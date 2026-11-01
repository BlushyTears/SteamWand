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
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
// Declared here so this header doesn't need <windows.h> and its min/max macros.
extern "C" __declspec(dllimport) void* __stdcall VirtualAlloc(void* address, size_t size, unsigned long type, unsigned long protect);
extern "C" __declspec(dllimport) int __stdcall VirtualFree(void* address, size_t size, unsigned long type);
#else
#include <sys/mman.h> // Only so the data layout can be tested off Windows.
#endif

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

    // Growable memory is committed in steps of this size. A multiple of the page size on every target.
    constexpr size_t commit_step = 64 * 1024;

    inline size_t round_to_step(size_t bytes) noexcept {
        return (bytes + commit_step - 1) / commit_step * commit_step;
    }

#if defined(_WIN32)
    inline void* reserve_pages(size_t bytes) {
        return VirtualAlloc(nullptr, bytes, 0x2000 /* MEM_RESERVE */, 0x01 /* PAGE_NOACCESS */);
    }

    inline bool commit_pages(void* at, size_t bytes) {
        return VirtualAlloc(at, bytes, 0x1000 /* MEM_COMMIT */, 0x04 /* PAGE_READWRITE */) != nullptr;
    }

    inline void release_pages(void* at, size_t) {
        VirtualFree(at, 0, 0x8000 /* MEM_RELEASE */);
    }
#else
    inline void* reserve_pages(size_t bytes) {
        void* memory = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        return memory == MAP_FAILED ? nullptr : memory;
    }

    inline bool commit_pages(void* at, size_t bytes) {
        return mprotect(at, bytes, PROT_READ | PROT_WRITE) == 0;
    }

    inline void release_pages(void* at, size_t bytes) {
        munmap(at, bytes);
    }
#endif

    // Memory for `count` values that never moves. A growable block reserves address space for
    // `max_count` values up front and commits more as it grows. Integer arrays start zeroed.
    template<typename U>
    struct Block {
        static constexpr size_t alignment = (std::max)(size_t{ 64 }, alignof(U));

        U* data = nullptr;
        size_t reserved = 0;  // Reserved bytes of a growable block. 0 means a fixed heap block.
        size_t committed = 0; // Usable bytes of a growable block.

        Block(uint32_t count, uint32_t max_count) {
            if (max_count > count) {
                reserved = round_to_step(size_t{ max_count } * sizeof(U));
                data = static_cast<U*>(reserve_pages(reserved));
                if (!data) {
                    throw std::bad_alloc();
                }
                try {
                    grow(count);
                }
                catch (...) {
                    release_pages(data, reserved);
                    throw;
                }
            }
            else {
                size_t bytes = size_t{ count } * sizeof(U);
                data = static_cast<U*>(::operator new(bytes, std::align_val_t{ alignment }));
                if constexpr (std::is_integral_v<U>) {
                    std::memset(data, 0, bytes);
                }
            }
        }

        // Makes room for `count` values. Newly committed memory is zeroed by the OS.
        void grow(uint32_t count) {
            if (!reserved) {
                return; // A fixed block is already as big as it gets.
            }
            size_t needed = round_to_step(size_t{ count } * sizeof(U));
            if (needed > committed) {
                if (!commit_pages(reinterpret_cast<char*>(data) + committed, needed - committed)) {
                    throw std::bad_alloc();
                }
                committed = needed;
            }
        }

        ~Block() {
            if (reserved) {
                release_pages(data, reserved);
            }
            else {
                ::operator delete(data, std::align_val_t{ alignment });
            }
        }

        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
    };

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

        ~WorldState(); // Defined after World, which it needs.
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

    struct Removal {
        uint32_t slot;
        uint32_t generation;
    };

    uint32_t cap;                                // Usable slots right now.
    uint32_t max_cap;                            // cap can grow up to this. Equal to cap for a fixed slab.
    uint32_t slot_extent = 0;                    // One past the highest slot used since the last clear().
    uint32_t live_count = 0;
    dcs_detail::Block<T> data;
    dcs_detail::Block<uint32_t> generations;     // Bumped on every removal, so older Atoms stop matching.
    dcs_detail::Block<uint64_t> presence;        // One bit per slot, set while the slot holds a visible value.
    std::vector<uint32_t> free_slots;            // Removed slots that can be reused.
    std::vector<uint32_t> queued_adds;           // Constructed slots that become visible in apply_queued().
    std::vector<Removal> queued_removals;        // Removed in apply_queued().
    friend struct World;
    template<typename, bool>
    friend struct View;

    static uint32_t words(uint32_t slots) noexcept {
        return (slots + 63) / 64;
    }

    bool is_live(uint32_t slot) const noexcept {
        return slot < slot_extent && dcs_detail::has_bit(presence.data, slot);
    }

    bool matches(uint32_t slot, uint32_t generation) const noexcept {
        return is_live(slot) && generations.data[slot] == generation;
    }

    // Makes room for at least `slots` slots in place, or throws if the slab is at its maximum.
    void grow(uint32_t slots) {
        if (slots > max_cap) {
            throw std::length_error("Slab capacity exceeded");
        }
        // Grow by 1/8, and by at least one commit step, so growth stays rare without wasting much memory.
        uint64_t step = (std::max)(uint64_t{ cap } / 8, uint64_t{ (std::max)(size_t{ 1 }, dcs_detail::commit_step / sizeof(T)) });
        uint32_t new_cap = static_cast<uint32_t>((std::min)(uint64_t{ max_cap }, (std::max)(uint64_t{ slots }, cap + step)));

        data.grow(new_cap);
        generations.grow(new_cap);
        presence.grow(words(new_cap));
        free_slots.reserve(new_cap);
        cap = new_cap;
    }

    // Constructs a value in a free slot. It stays invisible until show().
    template<typename... Args>
    uint32_t construct(Args&&... args) {
        bool reuse_slot = !free_slots.empty();
        uint32_t slot = reuse_slot ? free_slots.back() : slot_extent;
        // After clear() the extent restarts at 0, so step over retired slots.
        while (!reuse_slot && slot < cap && generations.data[slot] == UINT32_MAX) {
            ++slot;
        }
        if (slot >= cap) {
            grow(slot + 1);
        }

        // The slot is raw memory. Placement new constructs a T in that slot;
        // assignment would require a T to already exist there.
        ::new (data.data + slot) T(std::forward<Args>(args)...);

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
        presence.data[slot / 64] |= 1ULL << (slot % 64);
        ++live_count;
    }

    // Ends the lifetime of a live or queued value and frees its slot.
    void destroy(uint32_t slot) {
        if constexpr (std::is_same_v<T, World>) {
            data.data[slot].check_tree_not_busy();
        }
        if (is_live(slot)) {
            presence.data[slot / 64] &= ~(1ULL << (slot % 64));
            --live_count;
        }
        std::destroy_at(data.data + slot);
        // UINT32_MAX retires the slot instead of reviving an earlier handle.
        if (++generations.data[slot] != UINT32_MAX) {
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
    Slab(uint32_t capacity, uint32_t max_capacity)
        : cap(capacity), max_cap(max_capacity), data(capacity, max_capacity),
        generations(capacity, max_capacity), presence(words(capacity), words(max_capacity)) {
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

            remaining_bits = view->slab->presence.data[0];
            move_to_next_live_slot();
        }

        void move_to_next_live_slot() {
            // Skip words that have no live slots left.
            while (remaining_bits == 0 && word_index + 1 < word_count) {
                ++word_index;
                remaining_bits = view->slab->presence.data[word_index];
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
            T& value = view->slab->data.data[slot];
            if constexpr (WithAtoms) {
                Atom<Value> atom(WorldRef(*view->state), slot, view->slab->generations.data[slot]);
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
    friend struct dcs_detail::WorldState;

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

    // Throws if storing `arg` here would make a World own itself: `arg` is this World,
    // one of its ancestors, or a value that has this World inside it.
    template<typename Arg>
    void check_arg(const Arg& arg) const {
        if constexpr (std::is_same_v<Arg, World>) {
            check_can_own(arg);
        }

        uintptr_t begin = reinterpret_cast<uintptr_t>(std::addressof(arg));
        for (const World* world = this; world; world = world->parent ? world->parent->owner : nullptr) {
            uintptr_t at = reinterpret_cast<uintptr_t>(world);
            if (at >= begin && at < begin + sizeof(Arg)) {
                throw std::logic_error("A World cannot own itself");
            }
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
    Slab<T>& ensure_slab(uint32_t capacity, uint32_t max_capacity) {
        std::vector<std::unique_ptr<dcs_detail::ISlab>>& slabs = world_state->slabs;
        uint32_t type_id = TypeInfo<T>::id();

        if (type_id >= slabs.size()) {
            slabs.resize(type_id + 1);
        }
        if (!slabs[type_id]) {
            slabs[type_id] = std::make_unique<Slab<T>>(capacity, max_capacity);
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

        return slab->data.data + atom.slot;
    }

    // Constructs a T in a free slot. Queued values are allowed during iteration
    // and become visible at the next cleanup().
    template<typename T, typename... Args>
    Atom<T> create(bool queued, Args&&... args) {
        (check_arg(args), ...);

        MutationGuard guard(*world_state, queued);
        Slab<T>& slab = ensure_slab<T>(cap, cap);
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

        return Atom<T>(ref(), slot, slab.generations.data[slot]);
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
    // With a larger max_capacity the slab starts at `capacity` and grows in place up to it:
    // address space is reserved up front and memory is committed as the slab fills, so values never move.
    template<typename T>
    void set_capacity(uint32_t capacity, uint32_t max_capacity = 0) {
        if (capacity == 0) {
            throw std::invalid_argument("Capacity must be positive");
        }
        if (max_capacity != 0 && max_capacity < capacity) {
            throw std::invalid_argument("max_capacity must be at least capacity");
        }
        if (find_slab<T>()) {
            throw std::logic_error("set_capacity must come before the first value of its type");
        }

        MutationGuard guard(*world_state, true);
        ensure_slab<T>(capacity, (std::max)(capacity, max_capacity));
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

    // Moves a value into another World and returns its new Atom; the old Atom stops matching.
    // Neither World may be iterated meanwhile. Returns an empty Atom if `atom` isn't live here.
    template<typename T>
    Atom<T> move_to(const Atom<T>& atom, World& destination) {
        if (&destination == this) {
            return atom;
        }
        T* value = get(atom);
        if (!value) {
            return {};
        }

        MutationGuard guard(*world_state);
        Atom<T> moved = destination.add(std::move(*value));
        find_slab<T>()->destroy(atom.slot);
        return moved;
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
        return { slab->data.data, slab->presence.data, slab->slot_extent };
    }

    template<typename T>
    RawSlots<T> raw_slots() {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }
        return { slab->data.data, slab->presence.data, slab->slot_extent };
    }
};

inline dcs_detail::WorldState::~WorldState() {
    // Destroy the values first, while children can still unregister themselves.
    slabs.clear();
    // Children still listed outlived this World's values (for example held by a shared_ptr). Let them go.
    for (World* child : children) {
        child->parent = nullptr;
    }

    std::lock_guard<std::mutex> lock(world_table.mutex);
    WorldTable::Entry& entry = world_table.entries[index];
    entry.state = nullptr;
    // UINT32_MAX retires the index instead of reviving an earlier WorldRef.
    if (++entry.generation != UINT32_MAX) {
        world_table.free_indices.push_back(index);
    }
}

template<typename T>
bool Atom<T>::is_valid() const {
    World* owner = world();
    return owner && owner->is_live(*this);
}

static_assert(sizeof(Atom<int>) == 16 && std::is_trivially_copyable_v<Atom<int>>, "Atoms should stay small and cheap to copy");
