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
#include <limits>
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
    template<typename Integer>
    Integer next_id() {
        static std::atomic<Integer> counter{1};
        Integer id = counter.load(std::memory_order_relaxed);
        while (true) {
            if (id == std::numeric_limits<Integer>::max()) {
                throw std::overflow_error("DCS identity space exhausted");
            }

            Integer next = id + 1;
            bool id_claimed = counter.compare_exchange_weak(id, next, std::memory_order_relaxed);
            if (id_claimed) {
                return id;
            }
        }
    }

    inline uint32_t checked_capacity(uint32_t capacity) {
        if (!capacity) {
            throw std::invalid_argument("World capacity must be positive");
        }
        return capacity;
    }

}

struct TypeRegistry {
    static uint32_t next_id() {
        return dcs_detail::next_id<uint32_t>();
    }
};

template<typename T>
struct TypeInfo {
    static uint32_t id() {
        // First call for this T: counter advances, type_id gets the new number.
        // Every subsequent call: returns the same type_id (initialization happens once).
        static const uint32_t type_id = TypeRegistry::next_id();
        return type_id;
    }
};

namespace dcs_detail {
    struct ISlab {
        virtual ~ISlab() = default;
        virtual void clear() = 0;
        virtual void remove(uint32_t slot, uint32_t generation) = 0;
        virtual void check_children() const = 0;
        virtual void cleanup_children() = 0;
    };

    struct WorldState {
        struct Removal {
            uint32_t type_id;
            uint32_t slot;
            uint32_t generation;
        };

        World* owner;
        const uint64_t id = next_id<uint64_t>();
        size_t views = 0;
        bool changing = false;
        std::vector<std::unique_ptr<ISlab>> registry;
        std::vector<Removal> death_row;

        explicit WorldState(World* world) : owner(world), registry(0), death_row(0) {
        }
    };

    struct MutationGuard {
        WorldState& state;

        explicit MutationGuard(WorldState& storage) : state(storage) {
            if (state.views || state.changing) {
                throw std::logic_error("World has an active view or mutation");
            }
            state.changing = true;
        }

        ~MutationGuard() {
            state.changing = false;
        }

        MutationGuard(const MutationGuard&) = delete;
        MutationGuard& operator=(const MutationGuard&) = delete;
    };
}

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
    template<typename>
    friend struct Atom;
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
    static constexpr size_t alignment = std::max(size_t{64}, alignof(T));

    struct Deallocate {
        void operator()(T* data) const noexcept {
            ::operator delete(data, std::align_val_t{alignment});
        }
    };

    uint32_t cap;
    uint32_t next_idx = 0;
    uint32_t live_count = 0;
    std::unique_ptr<T, Deallocate> data;
    std::vector<uint32_t> generations;
    std::vector<uint32_t> free_slots;
    std::vector<uint64_t> presence;
    friend struct World;
    template<typename, bool>
    friend struct View;

    static T* allocate(uint32_t capacity) {
        if (capacity > std::numeric_limits<size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }

        size_t allocation_bytes = static_cast<size_t>(capacity) * sizeof(T);
        void* memory = ::operator new(allocation_bytes, std::align_val_t{alignment});
        return static_cast<T*>(memory);
    }

    bool is_live(uint32_t slot) const noexcept {
        if (slot >= next_idx) {
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
        uint32_t slot = next_idx;
        bool reuse_slot = !free_slots.empty();

        if (reuse_slot) {
            slot = free_slots.back();
        }
        else {
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
            next_idx = slot + 1;
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
            data.get()[slot].check_mutation_tree();
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
        for (uint32_t slot = 0; slot < next_idx; ++slot) {
            if (is_live(slot)) {
                remove(slot, generations[slot]);
            }
        }
        free_slots.clear();
        next_idx = 0;
    }

    void check_children() const override {
        if constexpr (std::is_same_v<T, World>) {
            for (uint32_t slot = 0; slot < next_idx; ++slot) {
                if (is_live(slot)) {
                    data.get()[slot].check_mutation_tree();
                }
            }
        }
    }

    void cleanup_children() override {
        if constexpr (std::is_same_v<T, World>) {
            for (uint32_t slot = 0; slot < next_idx; ++slot) {
                if (is_live(slot)) {
                    data.get()[slot].cleanup_tree();
                }
            }
        }
    }

public:
    explicit Slab(uint32_t capacity)
        : cap(dcs_detail::checked_capacity(capacity)), data(allocate(cap)), generations(cap), free_slots(0),
          presence((size_t{cap} + 63) / 64) {
        free_slots.reserve(cap);
    }

    ~Slab() override {
        for (uint32_t slot = 0; slot < next_idx; ++slot) {
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
            if (world_state->changing) {
                throw std::logic_error("World is being mutated");
            }
            ++world_state->views;
        }
    }

public:
    View(const View& other) : world_state(other.world_state), slab(other.slab) {
        if (world_state) {
            ++world_state->views;
        }
    }

    View(View&& other) noexcept : world_state(std::move(other.world_state)), slab(other.slab) {
        other.slab = nullptr;
    }

    View& operator=(View other) noexcept {
        world_state.swap(other.world_state);
        std::swap(slab, other.slab);
        return *this;
    }

    ~View() {
        if (world_state) {
            --world_state->views;
        }
    }

    struct Entry {
        Atom<Value> atom;
        T& value;
    };

    struct iterator {
    private:
        const View* view;
        uint32_t word_index = 0;
        uint32_t word_count = 0;
        uint32_t slot = 0;
        uint64_t remaining_bits = 0;
        friend struct View;

        iterator(const View* view, bool at_end) : view(view) {
            if (view->slab) {
                size_t slot_extent = view->slab->next_idx;
                word_count = static_cast<uint32_t>((slot_extent + 63) / 64);
            }

            if (at_end) {
                word_index = word_count;
            }
            else {
                advance_to_next_live_word();
                if (word_index < word_count) {
                    pop_current();
                }
            }
        }

        void advance_to_next_live_word() {
            while (word_index < word_count) {
                remaining_bits = view->slab->presence[word_index];
                if (remaining_bits != 0) {
                    return;
                }

                ++word_index;
            }
        }

        void pop_current() {
            // Select the first live slot, then remove its bit from the mask.
            uint32_t bit_index = static_cast<uint32_t>(std::countr_zero(remaining_bits));
            slot = word_index * 64 + bit_index;

            uint64_t selected_bit = 1ULL << bit_index;
            remaining_bits &= ~selected_bit;
        }

    public:
        iterator& operator++() {
            if (remaining_bits == 0 && word_index < word_count) {
                ++word_index;
                advance_to_next_live_word();
            }

            if (word_index < word_count) {
                pop_current();
            }

            return *this;
        }

        decltype(auto) operator*() const {
            T& value = view->slab->data.get()[slot];
            if constexpr (WithAtoms) {
                WorldRef world(view->world_state);
                uint32_t generation = view->slab->generations[slot];
                Atom<Value> atom(world, slot, generation);
                return Entry{std::move(atom), value};
            }
            else {
                return (value);
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
    std::shared_ptr<dcs_detail::WorldState> world_state;
    std::weak_ptr<dcs_detail::WorldState> parent_state;
    template<typename>
    friend struct Slab;

    dcs_detail::WorldState& storage() {
        if (!world_state) {
            world_state = std::make_shared<dcs_detail::WorldState>(this);
        }
        return *world_state;
    }

    void check_mutation_tree() const {
        if (!world_state) {
            return;
        }
        if (world_state->views || world_state->changing) {
            throw std::logic_error("World has an active view or mutation");
        }
        for (const auto& slab : world_state->registry) {
            if (slab) {
                slab->check_children();
            }
        }
    }

    void check_child(World& child) const {
        if (&child == this) {
            throw std::logic_error("A World cannot own itself");
        }
        std::shared_ptr<dcs_detail::WorldState> parent = parent_state.lock();
        while (parent) {
            if (parent == child.world_state) {
                throw std::logic_error("World ownership cycle");
            }
            if (!parent->owner) {
                break;
            }

            parent = parent->owner->parent_state.lock();
        }
        child.check_mutation_tree();
    }

    template<typename U>
    void check_child(U&) const {
    }

    void release() noexcept {
        if (world_state) {
            world_state->owner = nullptr;
            world_state->changing = true;
            world_state.reset();
        }
    }

    template<typename T>
    Slab<T>* lookup_slab() const {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>, "Storage type must be unqualified");
        if (!world_state) {
            return nullptr;
        }

        uint32_t type_id = TypeInfo<T>::id();
        if (type_id >= world_state->registry.size()) {
            return nullptr;
        }

        return static_cast<Slab<T>*>(world_state->registry[type_id].get());
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
        dcs_detail::WorldState& state = storage();
        uint32_t type_id = TypeInfo<T>::id();

        if (type_id >= state.registry.size()) {
            size_t required_entries = static_cast<size_t>(type_id) + 1;
            state.registry.resize(required_entries);
        }

        if (!state.registry[type_id]) {
            state.registry[type_id] = std::make_unique<Slab<T>>(cap);
        }

        return *static_cast<Slab<T>*>(state.registry[type_id].get());
    }

    void drain_removals() {
        for (const dcs_detail::WorldState::Removal& pending : world_state->death_row) {
            if (pending.type_id >= world_state->registry.size()) {
                continue;
            }

            dcs_detail::ISlab* slab = world_state->registry[pending.type_id].get();
            if (!slab) {
                continue;
            }

            slab->remove(pending.slot, pending.generation);
        }
        world_state->death_row.clear();
    }

public:
    explicit World(uint32_t capacity = 1024)
        : cap(dcs_detail::checked_capacity(capacity)),
          world_state(std::make_shared<dcs_detail::WorldState>(this)) {
    }

    ~World() {
        try {
            check_mutation_tree();
        }
        catch (...) {
            std::terminate();
        }
        release();
    }

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) : cap(other.cap) {
        other.check_mutation_tree();
        world_state = std::move(other.world_state);
        if (world_state) {
            world_state->owner = this;
        }
    }

    World& operator=(World&& other) {
        if (&other == this) {
            return *this;
        }
        check_mutation_tree();
        check_child(other);
        uint32_t incoming_capacity = other.cap;
        auto incoming_state = std::move(other.world_state);
        if (incoming_state) {
            incoming_state->owner = this;
        }
        release();
        world_state = std::move(incoming_state);
        cap = incoming_capacity;
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
        dcs_detail::WorldState& state = storage();
        if constexpr (std::is_same_v<T, World>) {
            (check_child(args), ...);
        }

        dcs_detail::MutationGuard guard(state);
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
    size_t live_count() const {
        const Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return 0;
        }

        return slab->size();
    }

    template<typename T>
    size_t size() const {
        return live_count<T>();
    }

    template<typename T>
    bool queue_free(const Atom<T>& atom) {
        if (world_state && world_state->changing) {
            throw std::logic_error("World is being mutated");
        }

        if (!is_live(atom)) {
            return false;
        }

        dcs_detail::WorldState::Removal pending;
        pending.type_id = TypeInfo<T>::id();
        pending.slot = atom.slot;
        pending.generation = atom.generation;
        world_state->death_row.push_back(pending);
        return true;
    }

    void cleanup() {
        if (!world_state) {
            return;
        }

        dcs_detail::MutationGuard guard(*world_state);
        drain_removals();
    }

    void cleanup_tree() {
        check_mutation_tree();
        if (!world_state) {
            return;
        }

        dcs_detail::MutationGuard guard(*world_state);
        drain_removals();
        for (auto& slab : world_state->registry) {
            if (slab) {
                slab->cleanup_children();
            }
        }
    }

    template<typename T>
    void clear() {
        if (!world_state) {
            return;
        }

        Slab<T>* slab = find_slab<T>();
        dcs_detail::MutationGuard guard(*world_state);
        if (slab) {
            slab->check_children();
            slab->clear();
        }

        uint32_t type_id = TypeInfo<T>::id();
        auto& pending_removals = world_state->death_row;
        size_t removals_kept = 0;

        for (const auto& pending : pending_removals) {
            if (pending.type_id == type_id) {
                continue;
            }

            pending_removals[removals_kept] = pending;
            ++removals_kept;
        }

        pending_removals.resize(removals_kept);
    }

    void discard() {
        check_mutation_tree();
        if (!world_state) {
            return;
        }

        dcs_detail::MutationGuard guard(*world_state);
        for (auto& slab : world_state->registry) {
            if (slab) {
                slab->clear();
            }
        }
        world_state->death_row.clear();
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
        slots.extent = slab->next_idx;
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
        slots.extent = slab->next_idx;
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
