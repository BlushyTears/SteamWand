// Known limits:
// - A World created inside a stored value after that value was constructed (for example
//   pushed into a std::vector member later) is not tracked as a child World.
//
// Future ideas:
// - Save/load story. Probably user code, but the engine could expose a stable-
//   representation hook for a slab.
// - Thread safety



#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

struct World;
template<typename T>
struct Slab;
template<typename T>
struct Atom;
template<typename T, bool WithAtoms = false>
struct View;

namespace dcs_detail {
    inline std::atomic<uint32_t> next_type_id{ 1 };

    inline bool has_bit(const uint64_t* bits, uint32_t index) noexcept {
        return (bits[index / 64] >> (index % 64)) & 1;
    }

    struct ISlab {
        virtual ~ISlab() = default;
        virtual void clear() = 0;
        virtual void apply_queued() = 0;
    };

    struct WorldState;

    // Lets Atoms and WorldRefs find their World by index and generation without owning it.
    struct WorldTable {
        struct Entry {
            WorldState* state = nullptr;
            uint32_t generation = 0;
        };

        std::mutex mutex;
        std::vector<Entry> entries;
        std::vector<uint32_t> free_indices;
    };
    inline WorldTable world_table;

    // The World whose slab is constructing a value on this thread. Worlds created meanwhile are its children.
    inline thread_local WorldState* constructing_parent = nullptr;

    struct ParentScope {
        WorldState* outer;

        explicit ParentScope(WorldState* state) : outer(std::exchange(constructing_parent, state)) {
        }

        ~ParentScope() {
            constructing_parent = outer;
        }
    };

    // Everything a World owns. Stays at one address while Worlds move, so views and children can point at it.
    struct WorldState {
        World* owner;
        uint32_t index = 0;
        uint32_t generation = 0;
        size_t active_views = 0;
        bool mutation_in_progress = false;
        std::vector<std::unique_ptr<ISlab>> slabs; // Indexed by TypeInfo<T>::id().
        std::vector<World*> children;              // Every tracked World living inside this World's values.

        explicit WorldState(World* world) : owner(world) {
            std::lock_guard<std::mutex> lock(world_table.mutex);
            if (world_table.free_indices.empty()) {
                index = static_cast<uint32_t>(world_table.entries.size());
                world_table.entries.emplace_back();
            }
            else {
                index = world_table.free_indices.back();
                world_table.free_indices.pop_back();
            }
            world_table.entries[index].state = this;
            generation = world_table.entries[index].generation;
        }

        ~WorldState() {
            // Destroy the values first, while children can still unregister themselves.
            slabs.clear();

            std::lock_guard<std::mutex> lock(world_table.mutex);
            WorldTable::Entry& entry = world_table.entries[index];
            entry.state = nullptr;
            // UINT32_MAX retires the index instead of reviving an earlier WorldRef.
            if (++entry.generation != UINT32_MAX) {
                world_table.free_indices.push_back(index);
            }
        }
    };

    inline World* find_world(uint32_t index, uint32_t generation) {
        std::lock_guard<std::mutex> lock(world_table.mutex);
        if (index >= world_table.entries.size() || world_table.entries[index].generation != generation) {
            return nullptr;
        }
        return world_table.entries[index].state->owner;
    }
}

template<typename T>
struct TypeInfo {
    static uint32_t id() {
        // First call for this T: counter advances, type_id gets the new number.
        // Every subsequent call: returns the same type_id (initialization happens once).
        static const uint32_t type_id = dcs_detail::next_type_id++;
        return type_id;
    }
};

struct WorldRef {
private:
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    explicit WorldRef(const dcs_detail::WorldState& state)
        : index(state.index), generation(state.generation) {
    }
    friend struct World;
    template<typename, bool>
    friend struct View;

public:
    WorldRef() = default;

    World* get() const {
        return dcs_detail::find_world(index, generation);
    }

    explicit operator bool() const {
        return get() != nullptr;
    }

    bool operator==(const WorldRef&) const noexcept = default;
};

template<typename T>
struct Atom {
private:
    static_assert(std::is_same_v<T, std::remove_cvref_t<T>>, "Atom type must be unqualified");
    WorldRef world_ref;
    uint32_t slot = UINT32_MAX;
    uint32_t generation = 0;

    Atom(WorldRef world, uint32_t slot, uint32_t generation)
        : world_ref(world), slot(slot), generation(generation) {
    }
    friend struct World;
    template<typename, bool>
    friend struct View;

public:
    Atom() = default;

    World* world() const {
        return world_ref.get();
    }

    bool is_valid() const;
    bool operator==(const Atom&) const noexcept = default;
};

template<typename T>
struct RawSlots {
    T* data = nullptr;
    const uint64_t* presence = nullptr;
    uint32_t extent = 0;

    bool is_live(uint32_t slot) const noexcept {
        return slot < extent && dcs_detail::has_bit(presence, slot);
    }
};

template<typename T>
struct Slab final : public dcs_detail::ISlab {
private:
    static_assert(std::is_nothrow_destructible_v<T>, "Stored destructors must not throw");
    static constexpr size_t alignment = std::max(size_t{ 64 }, alignof(T));

    struct Deallocate {
        void operator()(T* data) const noexcept {
            ::operator delete(data, std::align_val_t{ alignment });
        }
    };

    struct Removal {
        uint32_t slot;
        uint32_t generation;
    };

    uint32_t cap;
    uint32_t slot_extent = 0;             // One past the highest slot used since the last clear().
    uint32_t live_count = 0;
    std::unique_ptr<T, Deallocate> data;
    std::vector<uint32_t> generations;    // Bumped on every removal, so older Atoms stop matching.
    std::vector<uint32_t> free_slots;     // Removed slots that can be reused.
    std::vector<uint64_t> presence;       // One bit per slot, set while the slot holds a visible value.
    std::vector<uint32_t> queued_adds;    // Constructed slots that become visible in apply_queued().
    std::vector<Removal> queued_removals; // Removed in apply_queued().
    friend struct World;
    template<typename, bool>
    friend struct View;

    static T* allocate(uint32_t capacity) {
        size_t allocation_bytes = static_cast<size_t>(capacity) * sizeof(T);
        void* memory = ::operator new(allocation_bytes, std::align_val_t{ alignment });
        return static_cast<T*>(memory);
    }

    bool is_live(uint32_t slot) const noexcept {
        return slot < slot_extent && dcs_detail::has_bit(presence.data(), slot);
    }

    bool matches(uint32_t slot, uint32_t generation) const noexcept {
        return is_live(slot) && generations[slot] == generation;
    }

    // Constructs a value in a free slot. It stays invisible until show().
    template<typename... Args>
    uint32_t construct(Args&&... args) {
        bool reuse_slot = !free_slots.empty();
        uint32_t slot = reuse_slot ? free_slots.back() : slot_extent;
        // After clear() the extent restarts at 0, so step over retired slots.
        while (!reuse_slot && slot < cap && generations[slot] == UINT32_MAX) {
            ++slot;
        }
        if (slot >= cap) {
            throw std::length_error("Slab capacity exceeded");
        }

        // The slot is raw memory. Placement new constructs a T in that slot;
        // assignment would require a T to already exist there.
        ::new (data.get() + slot) T(std::forward<Args>(args)...);

        // Only claim the slot once construction succeeded.
        if (reuse_slot) {
            free_slots.pop_back();
        }
        else {
            slot_extent = slot + 1;
        }
        return slot;
    }

    void show(uint32_t slot) noexcept {
        presence[slot / 64] |= 1ULL << (slot % 64);
        ++live_count;
    }

    // Ends the lifetime of a live or queued value and frees its slot.
    void destroy(uint32_t slot) {
        if constexpr (std::is_same_v<T, World>) {
            data.get()[slot].check_tree_not_busy();
        }
        if (is_live(slot)) {
            presence[slot / 64] &= ~(1ULL << (slot % 64));
            --live_count;
        }
        std::destroy_at(data.get() + slot);
        // UINT32_MAX retires the slot instead of reviving an earlier handle.
        if (++generations[slot] != UINT32_MAX) {
            free_slots.push_back(slot);
        }
    }

    void apply_queued() override {
        for (uint32_t slot : queued_adds) {
            show(slot);
        }
        queued_adds.clear();

        for (const Removal& removal : queued_removals) {
            // Skips values that were freed twice or already removed by clear().
            if (matches(removal.slot, removal.generation)) {
                destroy(removal.slot);
            }
        }
        queued_removals.clear();
    }

    void clear() override {
        for (uint32_t slot = 0; slot < slot_extent; ++slot) {
            if (is_live(slot)) {
                destroy(slot);
            }
        }
        for (uint32_t slot : queued_adds) {
            destroy(slot);
        }
        queued_adds.clear();
        queued_removals.clear();
        free_slots.clear();
        slot_extent = 0;
    }

public:
    explicit Slab(uint32_t capacity)
        : cap(capacity), data(allocate(capacity)), generations(capacity),
        presence((size_t{ capacity } + 63) / 64) {
        free_slots.reserve(capacity);
    }

    ~Slab() override {
        clear();
    }

    uint32_t size() const noexcept {
        return live_count;
    }

    uint32_t capacity() const noexcept {
        return cap;
    }
};

template<typename T, bool WithAtoms>
struct View {
private:
    using Value = std::remove_const_t<T>;
    using SlabPointer = std::conditional_t<std::is_const_v<T>, const Slab<Value>*, Slab<Value>*>;
    dcs_detail::WorldState* state;
    SlabPointer slab;
    friend struct World;

    View(dcs_detail::WorldState* world_state, SlabPointer slab)
        : state(world_state), slab(slab) {
        if (state->mutation_in_progress) {
            throw std::logic_error("World is being mutated");
        }
        ++state->active_views;
    }

public:
    View(const View&) = delete;
    View& operator=(const View&) = delete;

    ~View() {
        --state->active_views;
    }

    struct Entry {
        Atom<Value> atom;
        T& value;
    };

    struct iterator {
    private:
        const View* view;
        uint32_t slot = 0;
        uint32_t word_index = 0;
        uint32_t word_count = 0;
        uint64_t remaining_bits = 0; // Live slots in the current word that are not visited yet.
        friend struct View;

        iterator(const View* view, bool at_end) : view(view) {
            if (view->slab) {
                size_t slot_extent = view->slab->slot_extent;
                word_count = static_cast<uint32_t>((slot_extent + 63) / 64);
            }

            if (at_end || word_count == 0) {
                word_index = word_count;
                return;
            }

            remaining_bits = view->slab->presence[0];
            move_to_next_live_slot();
        }

        void move_to_next_live_slot() {
            // Skip words that have no live slots left.
            while (remaining_bits == 0 && word_index + 1 < word_count) {
                ++word_index;
                remaining_bits = view->slab->presence[word_index];
            }

            if (remaining_bits == 0) {
                // Nothing left, so this is now the end iterator.
                word_index = word_count;
                return;
            }

            // Select the first live slot, then remove its bit from the mask.
            uint32_t bit_index = static_cast<uint32_t>(std::countr_zero(remaining_bits));
            slot = word_index * 64 + bit_index;
            remaining_bits &= remaining_bits - 1;
        }

    public:
        iterator& operator++() {
            move_to_next_live_slot();
            return *this;
        }

        decltype(auto) operator*() const {
            T& value = view->slab->data.get()[slot];
            if constexpr (WithAtoms) {
                Atom<Value> atom(WorldRef(*view->state), slot, view->slab->generations[slot]);
                return Entry{ atom, value };
            }
            else {
                return value;
            }
        }

        bool operator==(const iterator& other) const noexcept {
            return view == other.view &&
                word_index == other.word_index &&
                remaining_bits == other.remaining_bits;
        }
    };

    iterator begin() const {
        return iterator(this, false);
    }

    iterator end() const {
        return iterator(this, true);
    }
};

struct World {
private:
    uint32_t cap;                                        // Slot count for types without set_capacity().
    std::unique_ptr<dcs_detail::WorldState> world_state; // Never null. A moved-from World gets a fresh, empty one.
    dcs_detail::WorldState* parent = nullptr;            // The World this one lives inside, if any.
    template<typename>
    friend struct Slab;

    struct MutationGuard {
        dcs_detail::WorldState& state;

        explicit MutationGuard(dcs_detail::WorldState& storage, bool allow_views = false) : state(storage) {
            if (state.mutation_in_progress || (state.active_views && !allow_views)) {
                throw std::logic_error("World has an active view or mutation");
            }
            state.mutation_in_progress = true;
        }

        ~MutationGuard() {
            state.mutation_in_progress = false;
        }
    };

    // A World built while a slab constructs a value lives inside that value's World.
    void join_parent() {
        parent = dcs_detail::constructing_parent;
        if (parent) {
            parent->children.push_back(this);
        }
    }

    // Throws if this World, or any World inside it, has an active view or mutation.
    void check_tree_not_busy() const {
        if (world_state->active_views || world_state->mutation_in_progress) {
            throw std::logic_error("World has an active view or mutation");
        }
        for (const World* child : world_state->children) {
            child->check_tree_not_busy();
        }
    }

    // Throws if taking over `child` would make a World own itself, or if `child` is busy.
    void check_can_own(const World& child) const {
        if (&child == this) {
            throw std::logic_error("A World cannot own itself");
        }
        for (const dcs_detail::WorldState* ancestor = parent; ancestor; ancestor = ancestor->owner->parent) {
            if (ancestor == child.world_state.get()) {
                throw std::logic_error("World ownership cycle");
            }
        }
        child.check_tree_not_busy();
    }

    // emplace<World> gets a capacity or a World to move in. Only the World needs checking.
    template<typename Arg>
    void check_world_arg(Arg& arg) const {
        if constexpr (std::is_same_v<Arg, World>) {
            check_can_own(arg);
        }
    }

    template<typename T>
    Slab<T>* find_slab() const {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>, "Storage type must be unqualified");
        uint32_t type_id = TypeInfo<T>::id();
        if (type_id >= world_state->slabs.size()) {
            return nullptr;
        }

        return static_cast<Slab<T>*>(world_state->slabs[type_id].get());
    }

    template<typename T>
    Slab<T>& ensure_slab(uint32_t capacity) {
        std::vector<std::unique_ptr<dcs_detail::ISlab>>& slabs = world_state->slabs;
        uint32_t type_id = TypeInfo<T>::id();

        if (type_id >= slabs.size()) {
            slabs.resize(type_id + 1);
        }
        if (!slabs[type_id]) {
            slabs[type_id] = std::make_unique<Slab<T>>(capacity);
        }

        return *static_cast<Slab<T>*>(slabs[type_id].get());
    }

    template<typename T>
    T* lookup_value(const Atom<T>& atom) const {
        if (atom.world_ref != ref()) {
            return nullptr;
        }

        Slab<T>* slab = find_slab<T>();
        if (!slab || !slab->matches(atom.slot, atom.generation)) {
            return nullptr;
        }

        return slab->data.get() + atom.slot;
    }

    // Constructs a T in a free slot. Queued values are allowed during iteration
    // and become visible at the next cleanup().
    template<typename T, typename... Args>
    Atom<T> create(bool queued, Args&&... args) {
        if constexpr (std::is_same_v<T, World>) {
            // Calls check_world_arg once for each argument.
            (check_world_arg(args), ...);
        }

        MutationGuard guard(*world_state, queued);
        Slab<T>& slab = ensure_slab<T>(cap);
        uint32_t slot;
        {
            dcs_detail::ParentScope scope(world_state.get());
            slot = slab.construct(std::forward<Args>(args)...);
        }

        if (queued) {
            slab.queued_adds.push_back(slot);
        }
        else {
            slab.show(slot);
        }

        return Atom<T>(ref(), slot, slab.generations[slot]);
    }

    void apply_queued() {
        for (std::unique_ptr<dcs_detail::ISlab>& slab : world_state->slabs) {
            if (slab) {
                slab->apply_queued();
            }
        }
    }

public:
    explicit World(uint32_t capacity = 1024)
        : cap(capacity), world_state(std::make_unique<dcs_detail::WorldState>(this)) {
        if (capacity == 0) {
            throw std::invalid_argument("World capacity must be positive");
        }
        join_parent();
    }

    // Destroying a World that is being iterated or mutated terminates the program.
    ~World() {
        discard();
        if (parent) {
            std::erase(parent->children, this);
        }
    }

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other)
        : cap(other.cap),
        world_state(std::exchange(other.world_state, std::make_unique<dcs_detail::WorldState>(&other))) {
        world_state->owner = this;
        join_parent();
    }

    World& operator=(World&& other) {
        if (&other == this) {
            return *this;
        }
        check_tree_not_busy();
        check_can_own(other);

        // `other` may live inside our old state, so take what we need before replacing it.
        cap = other.cap;
        std::unique_ptr<dcs_detail::WorldState> incoming =
            std::exchange(other.world_state, std::make_unique<dcs_detail::WorldState>(&other));
        incoming->owner = this;
        world_state = std::move(incoming);
        return *this;
    }

    WorldRef ref() const noexcept {
        return WorldRef(*world_state);
    }

    uint32_t capacity() const noexcept {
        return cap;
    }

    // Gives T its own slot count instead of the World's. Call before the first value of T.
    template<typename T>
    void set_capacity(uint32_t capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("Capacity must be positive");
        }
        if (find_slab<T>()) {
            throw std::logic_error("set_capacity must come before the first value of its type");
        }

        MutationGuard guard(*world_state, true);
        ensure_slab<T>(capacity);
    }

    template<typename T, typename... Args>
    Atom<T> emplace(Args&&... args) {
        return create<T>(false, std::forward<Args>(args)...);
    }

    template<typename T>
    Atom<std::remove_cvref_t<T>> add(T&& value) {
        return emplace<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    template<typename T>
    Atom<std::remove_cvref_t<T>> add(const T& value) {
        return emplace<std::remove_cvref_t<T>>(value);
    }

    // Like add(), but safe while iterating. The value appears at the next cleanup().
    template<typename T>
    Atom<std::remove_cvref_t<T>> queue_add(T&& value) {
        return create<std::remove_cvref_t<T>>(true, std::forward<T>(value));
    }

    template<typename T>
    Atom<std::remove_cvref_t<T>> queue_add(const T& value) {
        return create<std::remove_cvref_t<T>>(true, value);
    }

    World& attach_world(World& child) {
        return attach_world(std::move(child));
    }

    World& attach_world(World&& child) {
        return *get(add<World>(std::move(child)));
    }

    template<typename T>
    const T* get(const Atom<T>& atom) const {
        return lookup_value(atom);
    }

    template<typename T>
    T* get(const Atom<T>& atom) {
        return lookup_value(atom);
    }

    template<typename T>
    bool is_live(const Atom<T>& atom) const {
        return get(atom) != nullptr;
    }

    template<typename T>
    size_t size() const {
        const Slab<T>* slab = find_slab<T>();
        return slab ? slab->size() : 0;
    }

    template<typename T>
    bool queue_free(const Atom<T>& atom) {
        if (world_state->mutation_in_progress) {
            throw std::logic_error("World is being mutated");
        }
        if (!is_live(atom)) {
            return false;
        }

        find_slab<T>()->queued_removals.push_back({ atom.slot, atom.generation });
        return true;
    }

    // Applies this World's queued adds and removals.
    void cleanup() {
        MutationGuard guard(*world_state);
        apply_queued();
    }

    // Also cleans up every World inside this one, recursively.
    void cleanup_tree() {
        cleanup();
        std::vector<World*>& children = world_state->children;
        for (size_t i = 0; i < children.size(); ++i) {
            children[i]->cleanup_tree();
        }
    }

    // Empties one slab, including its queued adds and removals.
    template<typename T>
    void clear() {
        MutationGuard guard(*world_state);
        if (Slab<T>* slab = find_slab<T>()) {
            slab->clear();
        }
    }

    // Empties every slab. Allocations are kept.
    void discard() {
        MutationGuard guard(*world_state);
        for (std::unique_ptr<dcs_detail::ISlab>& slab : world_state->slabs) {
            if (slab) {
                slab->clear();
            }
        }
    }

    template<typename T>
    View<T> iter() {
        return View<T>(world_state.get(), find_slab<T>());
    }

    template<typename T>
    View<const T> iter() const {
        return View<const T>(world_state.get(), find_slab<T>());
    }

    template<typename T>
    View<T, true> iter_atoms() {
        return View<T, true>(world_state.get(), find_slab<T>());
    }

    template<typename T>
    View<const T, true> iter_atoms() const {
        return View<const T, true>(world_state.get(), find_slab<T>());
    }

    // Borrowed raw slots include holes. Check presence before accessing a value.
    template<typename T>
    RawSlots<const T> raw_slots() const {
        const Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }
        return { slab->data.get(), slab->presence.data(), slab->slot_extent };
    }

    template<typename T>
    RawSlots<T> raw_slots() {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }
        return { slab->data.get(), slab->presence.data(), slab->slot_extent };
    }
};

template<typename T>
bool Atom<T>::is_valid() const {
    World* owner = world();
    return owner && owner->is_live(*this);
}

static_assert(sizeof(Atom<int>) == 16 && std::is_trivially_copyable_v<Atom<int>>, "Atoms should stay small and cheap to copy");
