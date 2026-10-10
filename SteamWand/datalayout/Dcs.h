// Todo:
// - Worlds embedded in other stored types bypass recursive cleanup and cycle checks.
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
#include <exception>
#include <memory>
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

    struct ISlab {
        virtual ~ISlab() = default;
        virtual void clear() = 0;
        virtual void remove(uint32_t slot, uint32_t generation) = 0;
    };

    // Everything a World owns. Shared so WorldRefs can follow it through moves.
    struct WorldState {
        struct Removal {
            uint32_t type_id;
            uint32_t slot;
            uint32_t generation;
        };

        static inline std::atomic<uint64_t> next_id{ 1 };

        World* owner;
        const uint64_t id = next_id++;
        size_t active_views = 0;
        bool mutation_in_progress = false;
        std::vector<std::unique_ptr<ISlab>> slabs; // Indexed by TypeInfo<T>::id().
        std::vector<Removal> pending_removals;     // Applied by World::cleanup().

        explicit WorldState(World* world) : owner(world) {
        }
    };
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
    std::weak_ptr<dcs_detail::WorldState> world_state;
    uint64_t id = 0;

    explicit WorldRef(const std::shared_ptr<dcs_detail::WorldState>& state)
        : world_state(state) {
        if (state) {
            id = state->id;
        }
    }
    friend struct World;
    template<typename, bool>
    friend struct View;

public:
    WorldRef() = default;

    World* get() const noexcept {
        std::shared_ptr<dcs_detail::WorldState> state = world_state.lock();
        if (!state) {
            return nullptr;
        }

        return state->owner;
    }

    explicit operator bool() const noexcept {
        return get() != nullptr;
    }

    bool operator==(const WorldRef& other) const noexcept {
        return id == other.id;
    }
};

template<typename T>
struct Atom {
private:
    static_assert(std::is_same_v<T, std::remove_cvref_t<T>>, "Atom type must be unqualified");
    WorldRef world_ref;
    uint32_t slot = UINT32_MAX;
    uint32_t generation = 0;

    Atom(WorldRef world, uint32_t slot, uint32_t generation)
        : world_ref(std::move(world)), slot(slot), generation(generation) {
    }
    friend struct World;
    template<typename, bool>
    friend struct View;

public:
    Atom() = default;

    World* world() const noexcept {
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
        if (slot >= extent) {
            return false;
        }

        uint32_t word = slot / 64;
        uint32_t bit_index = slot % 64;
        uint64_t bit = 1ULL << bit_index;
        return (presence[word] & bit) != 0;
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

    uint32_t cap;
    uint32_t slot_extent = 0;          // One past the highest slot used since the last clear().
    uint32_t live_count = 0;
    std::unique_ptr<T, Deallocate> data;
    std::vector<uint32_t> generations; // Bumped on every removal, so older Atoms stop matching.
    std::vector<uint32_t> free_slots;  // Removed slots that can be reused.
    std::vector<uint64_t> presence;    // One bit per slot, set while the slot holds a value.
    friend struct World;
    template<typename, bool>
    friend struct View;

    static T* allocate(uint32_t capacity) {
        size_t allocation_bytes = static_cast<size_t>(capacity) * sizeof(T);
        void* memory = ::operator new(allocation_bytes, std::align_val_t{ alignment });
        return static_cast<T*>(memory);
    }

    bool is_live(uint32_t slot) const noexcept {
        if (slot >= slot_extent) {
            return false;
        }

        uint32_t word = slot / 64;
        uint32_t bit_index = slot % 64;
        uint64_t bit = 1ULL << bit_index;
        return (presence[word] & bit) != 0;
    }

    bool matches(uint32_t slot, uint32_t generation) const noexcept {
        return is_live(slot) && generations[slot] == generation;
    }

    template<typename... Args>
    uint32_t emplace(Args&&... args) {
        uint32_t slot = slot_extent;
        bool reuse_slot = !free_slots.empty();

        if (reuse_slot) {
            slot = free_slots.back();
        }
        else {
            // After clear() the extent restarts at 0, so step over retired slots.
            while (slot < cap && generations[slot] == UINT32_MAX) {
                ++slot;
            }
        }
        if (slot >= cap) {
            throw std::length_error("Slab capacity exceeded");
        }

        T* values = data.get();
        // The slot is raw memory. Placement new constructs a T in that slot;
        // assignment would require a T to already exist there.
        ::new (&values[slot]) T(std::forward<Args>(args)...);

        if (reuse_slot) {
            free_slots.pop_back();
        }
        else {
            slot_extent = slot + 1;
        }

        uint32_t word = slot / 64;
        uint64_t bit = 1ULL << (slot % 64);
        presence[word] |= bit;
        ++live_count;
        return slot;
    }

    void remove(uint32_t slot, uint32_t generation) override {
        if (!matches(slot, generation)) {
            return;
        }
        if constexpr (std::is_same_v<T, World>) {
            data.get()[slot].check_tree_not_busy();
        }
        uint32_t word = slot / 64;
        uint64_t bit = 1ULL << (slot % 64);
        presence[word] &= ~bit;
        --live_count;
        std::destroy_at(data.get() + slot);
        // UINT32_MAX retires the slot instead of reviving an earlier handle.
        ++generations[slot];
        if (generations[slot] != UINT32_MAX) {
            free_slots.push_back(slot);
        }
    }

    void clear() override {
        for (uint32_t slot = 0; slot < slot_extent; ++slot) {
            if (is_live(slot)) {
                remove(slot, generations[slot]);
            }
        }
        free_slots.clear();
        slot_extent = 0;
    }

public:
    explicit Slab(uint32_t capacity)
        : cap(capacity), data(allocate(cap)), generations(cap),
        presence((size_t{ cap } + 63) / 64) {
        free_slots.reserve(cap);
    }

    ~Slab() override {
        for (uint32_t slot = 0; slot < slot_extent; ++slot) {
            if (is_live(slot)) {
                std::destroy_at(data.get() + slot);
            }
        }
    }

    Slab(const Slab&) = delete;
    Slab& operator=(const Slab&) = delete;
    Slab(Slab&&) = delete;
    Slab& operator=(Slab&&) = delete;

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
    std::shared_ptr<dcs_detail::WorldState> world_state;
    SlabPointer slab = nullptr;
    friend struct World;

    View(std::shared_ptr<dcs_detail::WorldState> state, SlabPointer slab)
        : world_state(std::move(state)), slab(slab) {
        if (world_state) {
            if (world_state->mutation_in_progress) {
                throw std::logic_error("World is being mutated");
            }
            ++world_state->active_views;
        }
    }

public:
    View(const View& other) : world_state(other.world_state), slab(other.slab) {
        if (world_state) {
            ++world_state->active_views;
        }
    }

    View(View&& other) noexcept : world_state(std::move(other.world_state)), slab(other.slab) {
        other.slab = nullptr;
    }

    // Copy-and-swap: `other` releases our old contents when it is destroyed.
    View& operator=(View other) noexcept {
        world_state.swap(other.world_state);
        std::swap(slab, other.slab);
        return *this;
    }

    ~View() {
        if (world_state) {
            --world_state->active_views;
        }
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

            uint64_t selected_bit = 1ULL << bit_index;
            remaining_bits &= ~selected_bit;
        }

    public:
        iterator& operator++() {
            move_to_next_live_slot();
            return *this;
        }

        decltype(auto) operator*() const {
            T& value = view->slab->data.get()[slot];
            if constexpr (WithAtoms) {
                WorldRef world(view->world_state);
                uint32_t generation = view->slab->generations[slot];
                Atom<Value> atom(std::move(world), slot, generation);
                return Entry{ std::move(atom), value };
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
    uint32_t cap;
    std::shared_ptr<dcs_detail::WorldState> world_state; // Null after a move until something is added.
    std::weak_ptr<dcs_detail::WorldState> parent_state;  // Set while this World is stored in another World.
    template<typename>
    friend struct Slab;

    struct MutationGuard {
        dcs_detail::WorldState& state;

        explicit MutationGuard(dcs_detail::WorldState& storage) : state(storage) {
            if (state.active_views || state.mutation_in_progress) {
                throw std::logic_error("World has an active view or mutation");
            }
            state.mutation_in_progress = true;
        }

        ~MutationGuard() {
            state.mutation_in_progress = false;
        }

        MutationGuard(const MutationGuard&) = delete;
        MutationGuard& operator=(const MutationGuard&) = delete;
    };

    dcs_detail::WorldState& ensure_state() {
        if (!world_state) {
            world_state = std::make_shared<dcs_detail::WorldState>(this);
        }
        return *world_state;
    }

    // Throws if this World, or any World stored inside it, has an active view or mutation.
    void check_tree_not_busy() const {
        if (!world_state) {
            return;
        }
        if (world_state->active_views || world_state->mutation_in_progress) {
            throw std::logic_error("World has an active view or mutation");
        }

        RawSlots<const World> children = raw_slots<World>();
        for (uint32_t slot = 0; slot < children.extent; ++slot) {
            if (children.is_live(slot)) {
                children.data[slot].check_tree_not_busy();
            }
        }
    }

    // Throws if taking over `child` would make a World own itself, or if `child` is busy.
    void check_can_own(const World& child) const {
        if (&child == this) {
            throw std::logic_error("A World cannot own itself");
        }

        std::shared_ptr<dcs_detail::WorldState> ancestor = parent_state.lock();
        while (ancestor) {
            if (ancestor == child.world_state) {
                throw std::logic_error("World ownership cycle");
            }
            ancestor = ancestor->owner->parent_state.lock();
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
    Slab<T>* lookup_slab() const {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>, "Storage type must be unqualified");
        if (!world_state) {
            return nullptr;
        }

        uint32_t type_id = TypeInfo<T>::id();
        if (type_id >= world_state->slabs.size()) {
            return nullptr;
        }

        return static_cast<Slab<T>*>(world_state->slabs[type_id].get());
    }

    template<typename T>
    T* lookup_value(const Atom<T>& atom) const {
        if (!world_state) {
            return nullptr;
        }
        if (atom.world_ref.id != world_state->id) {
            return nullptr;
        }

        Slab<T>* slab = lookup_slab<T>();
        if (!slab) {
            return nullptr;
        }
        if (!slab->matches(atom.slot, atom.generation)) {
            return nullptr;
        }

        return &slab->data.get()[atom.slot];
    }

    template<typename T>
    Slab<T>& ensure_slab() {
        dcs_detail::WorldState& state = ensure_state();
        uint32_t type_id = TypeInfo<T>::id();

        if (type_id >= state.slabs.size()) {
            state.slabs.resize(type_id + 1);
        }

        if (!state.slabs[type_id]) {
            state.slabs[type_id] = std::make_unique<Slab<T>>(cap);
        }

        return *static_cast<Slab<T>*>(state.slabs[type_id].get());
    }

    void drain_removals() {
        for (const dcs_detail::WorldState::Removal& removal : world_state->pending_removals) {
            // queue_free only queues live values, so their slab exists.
            dcs_detail::ISlab& slab = *world_state->slabs[removal.type_id];
            slab.remove(removal.slot, removal.generation);
        }
        world_state->pending_removals.clear();
    }

public:
    explicit World(uint32_t capacity = 1024)
        : cap(capacity), world_state(std::make_shared<dcs_detail::WorldState>(this)) {
        if (capacity == 0) {
            throw std::invalid_argument("World capacity must be positive");
        }
    }

    ~World() {
        try {
            check_tree_not_busy();
        }
        catch (...) {
            std::terminate();
        }
        // Reset before the values die, so their destructors see an empty World, not a half-destroyed one.
        world_state.reset();
    }

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) : cap(other.cap) {
        other.check_tree_not_busy();
        world_state = std::move(other.world_state);
        if (world_state) {
            world_state->owner = this;
        }
    }

    World& operator=(World&& other) {
        if (&other == this) {
            return *this;
        }
        check_tree_not_busy();
        check_can_own(other);

        // `other` may live inside our old state, so take what we need before replacing it.
        cap = other.cap;
        std::shared_ptr<dcs_detail::WorldState> incoming_state = std::move(other.world_state);
        if (incoming_state) {
            incoming_state->owner = this;
        }

        world_state = std::move(incoming_state);
        return *this;
    }

    WorldRef ref() const noexcept {
        return WorldRef(world_state);
    }

    uint32_t capacity() const noexcept {
        return cap;
    }

    template<typename T>
    Slab<T>* find_slab() {
        return lookup_slab<T>();
    }

    template<typename T>
    const Slab<T>* find_slab() const {
        return lookup_slab<T>();
    }

    template<typename T, typename... Args>
    Atom<T> emplace(Args&&... args) {
        if constexpr (std::is_same_v<T, World>) {
            // Calls check_world_arg once for each argument.
            (check_world_arg(args), ...);
        }

        dcs_detail::WorldState& state = ensure_state();
        MutationGuard guard(state);
        Slab<T>& slab = ensure_slab<T>();
        uint32_t slot = slab.emplace(std::forward<Args>(args)...);

        if constexpr (std::is_same_v<T, World>) {
            slab.data.get()[slot].parent_state = world_state;
        }

        return Atom<T>(ref(), slot, slab.generations[slot]);
    }

    template<typename T>
    Atom<std::remove_cvref_t<T>> add(T&& value) {
        return emplace<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    template<typename T>
    Atom<std::remove_cvref_t<T>> add(const T& value) {
        return emplace<std::remove_cvref_t<T>>(value);
    }

    World& attach_world(World& child) {
        return attach_world(std::move(child));
    }

    World& attach_world(World&& child) {
        Atom<World> atom = add<World>(std::move(child));
        World* attached_world = get(atom);
        return *attached_world;
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
        if (!slab) {
            return 0;
        }

        return slab->size();
    }

    template<typename T>
    bool queue_free(const Atom<T>& atom) {
        if (world_state && world_state->mutation_in_progress) {
            throw std::logic_error("World is being mutated");
        }

        if (!is_live(atom)) {
            return false;
        }

        dcs_detail::WorldState::Removal removal;
        removal.type_id = TypeInfo<T>::id();
        removal.slot = atom.slot;
        removal.generation = atom.generation;
        world_state->pending_removals.push_back(removal);
        return true;
    }

    void cleanup() {
        if (!world_state) {
            return;
        }

        MutationGuard guard(*world_state);
        drain_removals();
    }

    void cleanup_tree() {
        check_tree_not_busy();
        if (!world_state) {
            return;
        }

        MutationGuard guard(*world_state);
        drain_removals();

        RawSlots<World> children = raw_slots<World>();
        for (uint32_t slot = 0; slot < children.extent; ++slot) {
            if (children.is_live(slot)) {
                children.data[slot].cleanup_tree();
            }
        }
    }

    template<typename T>
    void clear() {
        if (!world_state) {
            return;
        }
        if constexpr (std::is_same_v<T, World>) {
            // Fail before anything is removed if a child World is in use.
            check_tree_not_busy();
        }

        MutationGuard guard(*world_state);
        Slab<T>* slab = find_slab<T>();
        if (slab) {
            slab->clear();
        }
        // Queued removals of T still name the old generations, so cleanup() skips them.
    }

    void discard() {
        check_tree_not_busy();
        if (!world_state) {
            return;
        }

        MutationGuard guard(*world_state);
        for (auto& slab : world_state->slabs) {
            if (slab) {
                slab->clear();
            }
        }
        world_state->pending_removals.clear();
    }

    template<typename T>
    View<T> iter() {
        return View<T>(world_state, find_slab<T>());
    }

    template<typename T>
    View<const T> iter() const {
        return View<const T>(world_state, find_slab<T>());
    }

    template<typename T>
    View<T, true> iter_atoms() {
        return View<T, true>(world_state, find_slab<T>());
    }

    template<typename T>
    View<const T, true> iter_atoms() const {
        return View<const T, true>(world_state, find_slab<T>());
    }

    // Borrowed raw slots include holes. Check presence before accessing a value.
    template<typename T>
    RawSlots<const T> raw_slots() const {
        const Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }

        RawSlots<const T> slots;
        slots.data = slab->data.get();
        slots.presence = slab->presence.data();
        slots.extent = slab->slot_extent;
        return slots;
    }

    template<typename T>
    RawSlots<T> raw_slots() {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }

        RawSlots<T> slots;
        slots.data = slab->data.get();
        slots.presence = slab->presence.data();
        slots.extent = slab->slot_extent;
        return slots;
    }
};

template<typename T>
bool Atom<T>::is_valid() const {
    World* owner = world();
    if (!owner) {
        return false;
    }

    return owner->is_live(*this);
}
