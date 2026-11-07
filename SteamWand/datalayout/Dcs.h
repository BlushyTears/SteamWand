// Known limits:
// - A World created inside a stored value after that value was constructed (for example
//   pushed into a std::vector member later) is not tracked as a child World.
// - A World behind a pointer or vector inside a stored value doesn't move with that value,
//   so it can stay tracked by the wrong World.
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
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>

struct World;
template<typename T>
struct Slab;
template<typename T, bool WithAtoms = false>
struct View;

namespace dcs_detail {
    // True when a T&& parameter really is a value we can move from: not a plain x, and not const.
    template<typename T>
    concept MovableValue = !std::is_reference_v<T> && !std::is_const_v<T>;

    inline std::atomic<uint32_t> next_type_id{ 0 };

    template<typename T>
    uint32_t type_id_of() {
        // First call for this T: counter advances, type_id gets the new number.
        // Every subsequent call: returns the same type_id (initialization happens once).
        static const uint32_t type_id = next_type_id++;
        return type_id;
    }

    inline bool has_bit(const uint64_t* bits, uint32_t index) {
        return (bits[index / 64] >> (index % 64)) & 1;
    }

    // Memory for values that never moves. Address space for `max_count` values is reserved
    // up front, and grow() commits real memory for the first `count` of them.
    template<typename T>
    struct Block {
        T* data;

        Block(uint32_t count, uint32_t max_count) {
            data = static_cast<T*>(VirtualAlloc(nullptr, max_count * sizeof(T), MEM_RESERVE, PAGE_NOACCESS));
            if (!data && max_count > 0) {
                throw std::bad_alloc();
            }
            grow(count);
        }

        // Committing memory that is already committed does nothing, so this always starts at the beginning.
        void grow(uint32_t count) {
            if (count > 0 && !VirtualAlloc(data, count * sizeof(T), MEM_COMMIT, PAGE_READWRITE)) {
                throw std::bad_alloc();
            }
        }

        ~Block() {
            VirtualFree(data, 0, MEM_RELEASE);
        }

        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
    };

    // Lets a World keep slabs of different types in one list.
    struct ISlab {
        virtual ~ISlab() = default;
        virtual void clear() = 0;
        virtual void apply_queued() = 0;
    };

    struct WorldState;

    // Every World gets a unique id. Atoms and WorldRefs keep that id and look their World up here,
    // so they follow it through moves and find nothing once it's destroyed.
    struct WorldTable {
        std::mutex mutex; // Worlds can be created on any thread.
        uint64_t next_id = 1;
        std::unordered_map<uint64_t, WorldState*> worlds;

        uint64_t add(WorldState* state) {
            std::lock_guard<std::mutex> lock(mutex);
            uint64_t id = next_id;
            ++next_id;
            worlds[id] = state;
            return id;
        }

        void remove(uint64_t id) {
            std::lock_guard<std::mutex> lock(mutex);
            worlds.erase(id);
        }

        // Returns nullptr if that World is gone.
        WorldState* find(uint64_t id) {
            if (id == 0) {
                return nullptr; // An empty WorldRef. Every plain removal asks, so skip the lock.
            }

            std::lock_guard<std::mutex> lock(mutex);
            if (!worlds.contains(id)) {
                return nullptr;
            }
            return worlds[id];
        }
    };
    inline WorldTable world_table;

    // The World running a value's code on this thread. Worlds created meanwhile are its children.
    inline thread_local WorldState* constructing_parent = nullptr;

    // Everything a World owns. Stays at one address while Worlds move, so views and children can point at it.
    struct WorldState {
        World* owner;
        uint64_t id;
        size_t active_views = 0; // Open loops. Running a value's constructor or destructor counts as one.
        std::vector<ISlab*> slabs;    // Indexed by type_id_of<T>(). Empty for types this World doesn't use.
        std::vector<World*> children; // Every World tracked as living inside this one.
        std::vector<std::shared_ptr<std::function<void(World&, float)>>> systems; // Run by World::update(), in order.

        explicit WorldState(World* world) : owner(world), id(world_table.add(this)) {
        }

        ~WorldState(); // Defined after World, which it needs.
    };

    // While a slab builds a value, or cleanup(), clear(), discard() or move_to() run, the World
    // counts as being iterated, so the value's code can only use the queue functions on it.
    struct ValueCodeScope {
        WorldState* state;
        WorldState* outer;

        explicit ValueCodeScope(WorldState* running) : state(running), outer(constructing_parent) {
            constructing_parent = state;
            ++state->active_views;
        }

        ~ValueCodeScope() {
            constructing_parent = outer;
            --state->active_views;
        }
    };
}

struct WorldRef {
    uint64_t id = 0; // No World has id 0, so an empty WorldRef finds nothing.

    World* get() const {
        dcs_detail::WorldState* state = dcs_detail::world_table.find(id);
        if (!state) {
            return nullptr;
        }
        return state->owner;
    }

    explicit operator bool() const {
        return get() != nullptr;
    }

    bool operator==(const WorldRef&) const = default;
};

template<typename T>
struct Atom {
    WorldRef world_ref;
    uint32_t slot = UINT32_MAX;
    uint32_t generation = 0;

    World* world() const {
        return world_ref.get();
    }

    bool is_valid() const;
    bool operator==(const Atom&) const = default;
};

template<typename T>
struct RawSlots {
    T* data = nullptr;
    const uint64_t* presence = nullptr;
    uint32_t extent = 0;

    bool is_live(uint32_t slot) const {
        return slot < extent && dcs_detail::has_bit(presence, slot);
    }
};

struct World {
private:
    uint32_t cap;                                        // Slot count for types without their own capacity.
    std::unique_ptr<dcs_detail::WorldState> world_state; // Never null. A moved-from World gets a fresh, empty one.
    dcs_detail::WorldState* parent = nullptr;            // The World this one lives inside, if any.
    friend struct dcs_detail::WorldState;

    // A World built while a slab constructs a value lives inside that value's World.
    void join_parent() {
        parent = dcs_detail::constructing_parent;
        if (parent) {
            parent->children.push_back(this);
        }
    }

    // Values can't be added or removed directly while a loop over this World is open,
    // or from inside the constructor or destructor of a value in this World.
    void check_not_iterating() const {
        if (world_state->active_views > 0) {
            throw std::logic_error("World is being iterated");
        }
    }

    // Throws if taking over `child` would make a World own itself.
    void check_can_own(const World& child) const {
        // Walk up from this World through every World it lives inside.
        const dcs_detail::WorldState* state = world_state.get();
        while (state) {
            if (state == child.world_state.get()) {
                throw std::logic_error("A World cannot own itself");
            }
            state = state->owner->parent;
        }
    }

    // Returns nullptr if this World has no slab for T yet.
    template<typename T>
    Slab<T>* find_slab() const {
        uint32_t type_id = dcs_detail::type_id_of<T>();
        if (type_id >= world_state->slabs.size()) {
            return nullptr;
        }
        return static_cast<Slab<T>*>(world_state->slabs[type_id]);
    }

    template<typename T>
    Slab<T>* make_slab(uint32_t capacity, uint32_t max_capacity) {
        if (find_slab<T>()) {
            throw std::logic_error("Capacity must be set before the first value of its type");
        }

        uint32_t type_id = dcs_detail::type_id_of<T>();
        if (type_id >= world_state->slabs.size()) {
            world_state->slabs.resize(type_id + 1);
        }

        Slab<T>* slab = new Slab<T>(world_state.get(), capacity, max_capacity);
        world_state->slabs[type_id] = slab;
        return slab;
    }

    // The slab to add a T to. The first T gets a slab with the World's capacity.
    template<typename T>
    Slab<T>* slab_for_adding() {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            slab = make_slab<T>(cap, cap);
        }
        return slab;
    }

public:
    explicit World(uint32_t capacity = 1024)
        : cap(capacity), world_state(std::make_unique<dcs_detail::WorldState>(this)) {
        join_parent();
    }

    // Destroying a World that is being iterated terminates the program.
    ~World() {
        discard();
        if (parent) {
            std::erase(parent->children, this);
        }
    }

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) : cap(other.cap) {
        // Usually a move into a slab, which puts `other` inside that slab's World.
        if (dcs_detail::constructing_parent) {
            dcs_detail::constructing_parent->owner->check_can_own(other);
            other.check_not_iterating();
        }

        world_state = std::make_unique<dcs_detail::WorldState>(&other);
        std::swap(world_state, other.world_state); // `other` gets the fresh, empty state.
        world_state->owner = this;
        join_parent();
    }

    World& operator=(World&& other) {
        if (&other == this) {
            return *this;
        }
        check_not_iterating();
        other.check_not_iterating();
        check_can_own(other);

        // `other` may live inside our old state, so take what we need before replacing it.
        std::unique_ptr<dcs_detail::WorldState> incoming = std::make_unique<dcs_detail::WorldState>(&other);
        std::swap(incoming, other.world_state); // `other` gets the fresh, empty state.
        incoming->owner = this;
        cap = other.cap;
        world_state = std::move(incoming);
        return *this;
    }

    WorldRef ref() const {
        return WorldRef{ world_state->id };
    }

    uint32_t capacity() const {
        return cap;
    }

    // Gives T a fixed slot count instead of the World's. Call before the first value of T.
    template<typename T>
    void set_fixed_capacity(uint32_t capacity) {
        make_slab<T>(capacity, capacity);
    }

    // Lets T grow in place from the World's capacity up to max_capacity. Call before the first value of T.
    // Address space is reserved up front and memory is committed as the slab fills, so values never move.
    template<typename T>
    void allow_growth(uint32_t max_capacity) {
        make_slab<T>(std::min(cap, max_capacity), max_capacity);
    }

    template<typename T, typename... Args>
    Atom<T> emplace(Args&&... args) {
        check_not_iterating();
        Slab<T>* slab = slab_for_adding<T>();
        uint32_t slot = slab->construct(std::forward<Args>(args)...);
        slab->show(slot);
        return slab->atom_at(slot);
    }

    template<typename T>
    Atom<T> add(const T& value) {
        return emplace<T>(value);
    }

    // For temporaries and std::move(x). A plain add(x) uses the copying one above.
    template<dcs_detail::MovableValue T>
    Atom<T> add(T&& value) {
        return emplace<T>(std::move(value));
    }

    // Like add(), but safe while iterating. The value appears at the next cleanup().
    template<typename T>
    Atom<T> queue_add(const T& value) {
        return queue_add<T>(T(value));
    }

    template<dcs_detail::MovableValue T>
    Atom<T> queue_add(T&& value) {
        Slab<T>* slab = slab_for_adding<T>();
        uint32_t slot = slab->construct(std::move(value));
        slab->queued_adds.push_back(slot);
        return slab->atom_at(slot);
    }

    // Moves a value into another World and returns its new Atom; the old Atom stops matching.
    // Neither World may be iterated meanwhile. Returns an empty Atom if `atom` isn't live here.
    template<typename T>
    Atom<T> move_to(const Atom<T>& atom, World& destination) {
        T* value = get(atom);
        if (!value) {
            return {};
        }
        if (&destination == this) {
            return atom;
        }

        check_not_iterating();
        dcs_detail::ValueCodeScope scope(world_state.get());
        uint32_t slot = atom.slot; // `atom` may live inside the value that's about to move.
        Atom<T> moved = destination.emplace<T>(std::move(*value));
        find_slab<T>()->destroy(slot);
        return moved;
    }

    // Like move_to(), but safe while iterating: the value moves at this World's next cleanup().
    // Use move_to() when you need the new Atom straight away. If `destination` is destroyed
    // before then, the value is destroyed too, just as if it had already moved there.
    template<typename T>
    bool queue_move_to(const Atom<T>& atom, World& destination) {
        if (&destination == this || !is_live(atom)) {
            return false;
        }

        find_slab<T>()->queued_removals.push_back({ atom, destination.ref() });
        return true;
    }

    World& attach_world(World& child) {
        return attach_world(std::move(child));
    }

    World& attach_world(World&& child) {
        return *get(emplace<World>(std::move(child)));
    }

    // Returns nullptr for a removed or still queued value, or if the Atom belongs to another World.
    template<typename T>
    T* get(const Atom<T>& atom) const {
        if (atom.world_ref != ref()) {
            return nullptr;
        }

        Slab<T>* slab = find_slab<T>();
        if (!slab || !slab->matches(atom)) {
            return nullptr;
        }

        return slab->values.data + atom.slot;
    }

    template<typename T>
    bool is_live(const Atom<T>& atom) const {
        return get(atom) != nullptr;
    }

    template<typename T>
    size_t size() const {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return 0;
        }
        return slab->live_count;
    }

    template<typename T>
    bool queue_free(const Atom<T>& atom) {
        if (!is_live(atom)) {
            return false;
        }

        find_slab<T>()->queued_removals.push_back({ atom });
        return true;
    }

    // Applies this World's queued adds and removals.
    void cleanup() {
        check_not_iterating();
        dcs_detail::ValueCodeScope scope(world_state.get());
        // By index, because a value's destructor can queue a new type.
        std::vector<dcs_detail::ISlab*>& slabs = world_state->slabs;
        for (size_t i = 0; i < slabs.size(); ++i) {
            if (slabs[i]) {
                slabs[i]->apply_queued();
            }
        }
    }

    // Also cleans up every World inside this one, recursively.
    void cleanup_tree() {
        cleanup();
        std::vector<World*>& children = world_state->children;
        for (size_t i = 0; i < children.size(); ++i) {
            children[i]->cleanup_tree();
        }
    }

    // A function that updates one World. Plain functions and lambdas both work.
    using System = std::function<void(World& world, float dt)>;

    // Adds a system that update() runs on this World. Systems run in the order they were added,
    // stay through discard() and move with the World.
    void add_system(System system) {
        world_state->systems.push_back(std::make_shared<System>(std::move(system)));
    }

    // Runs this World's systems, applies their queued changes with cleanup(),
    // then updates every World inside this one the same way.
    void update(float dt) {
        // A system or child can remove or move this World, which ends its update here.
        WorldRef self = ref();

        // A copy of the list, so a system can add another one. That one runs from the next update on.
        std::vector<std::shared_ptr<System>> systems = world_state->systems;
        for (const std::shared_ptr<System>& system : systems) {
            (*system)(*this, dt);
            if (self.get() != this) {
                return;
            }
        }
        cleanup();

        std::vector<World*>& children = world_state->children;
        for (size_t i = 0; i < children.size(); ++i) {
            children[i]->update(dt);
            if (self.get() != this) {
                return;
            }
        }
    }

    // Empties one slab, including its queued adds and removals.
    template<typename T>
    void clear() {
        check_not_iterating();
        Slab<T>* slab = find_slab<T>();
        if (slab) {
            dcs_detail::ValueCodeScope scope(world_state.get());
            slab->clear();
        }
    }

    // Empties every slab. Allocations are kept.
    void discard() {
        check_not_iterating();
        dcs_detail::ValueCodeScope scope(world_state.get());
        // By index, because a value's destructor can queue a new type.
        std::vector<dcs_detail::ISlab*>& slabs = world_state->slabs;
        for (size_t i = 0; i < slabs.size(); ++i) {
            if (slabs[i]) {
                slabs[i]->clear();
            }
        }
    }

    template<typename T>
    View<T> iter() const {
        return View<T>(world_state.get(), find_slab<T>());
    }

    template<typename T>
    View<T, true> iter_atoms() const {
        return View<T, true>(world_state.get(), find_slab<T>());
    }

    // Borrowed raw slots include holes. Check presence before accessing a value.
    template<typename T>
    RawSlots<T> raw_slots() const {
        Slab<T>* slab = find_slab<T>();
        if (!slab) {
            return {};
        }
        return { slab->values.data, slab->presence.data(), slab->slot_extent };
    }
};

template<typename T>
struct Slab : dcs_detail::ISlab {
    struct Removal {
        Atom<T> atom;
        WorldRef destination; // Where the value moves to. Empty if it's just destroyed.
    };

    dcs_detail::WorldState* state;        // The World this slab belongs to.
    uint32_t cap;                         // Usable slots right now.
    uint32_t max_cap;                     // cap can grow up to this. Equal to cap for a fixed slab.
    uint32_t slot_extent = 0;             // One past the highest slot used since the last clear().
    uint32_t live_count = 0;
    dcs_detail::Block<T> values;
    std::vector<uint32_t> generations;    // Bumped on every removal, so older Atoms stop matching.
    std::vector<uint64_t> presence;       // One bit per slot, set while the slot holds a visible value.
    std::vector<uint32_t> free_slots;     // Removed slots that can be reused.
    std::vector<uint32_t> queued_adds;    // Constructed slots that become visible in apply_queued().
    std::vector<Removal> queued_removals; // Removed or moved away in apply_queued().

    Slab(dcs_detail::WorldState* owner, uint32_t capacity, uint32_t max_capacity)
        : state(owner), cap(capacity), max_cap(max_capacity), values(capacity, max_capacity),
        generations(capacity), presence(presence_words(max_capacity)) {
        free_slots.reserve(capacity);
    }

    ~Slab() override {
        clear();
    }

    // Words needed for one presence bit per slot.
    static uint32_t presence_words(uint32_t slots) {
        return slots / 64 + 1;
    }

    bool is_live(uint32_t slot) const {
        return slot < slot_extent && dcs_detail::has_bit(presence.data(), slot);
    }

    bool matches(const Atom<T>& atom) const {
        return is_live(atom.slot) && generations[atom.slot] == atom.generation;
    }

    Atom<T> atom_at(uint32_t slot) const {
        return Atom<T>{ WorldRef{ state->id }, slot, generations[slot] };
    }

    // Makes room for more slots in place, or throws if the slab is at its maximum.
    void grow() {
        if (cap == max_cap) {
            throw std::length_error("Slab capacity exceeded");
        }
        // Grow by 1/8, so growth stays rare without wasting much memory.
        uint64_t step = std::max<uint64_t>(cap / 8, 64);
        uint32_t new_cap = static_cast<uint32_t>(std::min<uint64_t>(cap + step, max_cap));

        values.grow(new_cap);
        generations.resize(new_cap);
        free_slots.reserve(new_cap);
        cap = new_cap;
    }

    // Constructs a value in a free slot. It stays invisible until show().
    template<typename... Args>
    uint32_t construct(Args&&... args) {
        uint32_t slot;
        if (free_slots.empty()) {
            if (slot_extent == cap) {
                grow();
            }
            slot = slot_extent;
            ++slot_extent;
        }
        else {
            slot = free_slots.back();
            free_slots.pop_back();
        }

        // Worlds built inside this value become children of this slab's World.
        dcs_detail::ValueCodeScope scope(state);

        // The slot is raw memory. Placement new constructs a T in that slot;
        // assignment would require a T to already exist there.
        ::new (values.data + slot) T(std::forward<Args>(args)...);
        return slot;
    }

    void show(uint32_t slot) {
        presence[slot / 64] |= 1ULL << (slot % 64);
        ++live_count;
    }

    // Ends the lifetime of a live or queued value and frees its slot.
    void destroy(uint32_t slot) {
        if (is_live(slot)) {
            presence[slot / 64] &= ~(1ULL << (slot % 64));
            --live_count;
        }
        std::destroy_at(values.data + slot);
        ++generations[slot];
        free_slots.push_back(slot);
    }

    void apply_queued() override {
        for (uint32_t slot : queued_adds) {
            show(slot);
        }
        queued_adds.clear();

        // By index, because a value's destructor can queue more.
        for (size_t i = 0; i < queued_removals.size(); ++i) {
            Removal removal = queued_removals[i];
            // Skips values that are already gone, for example freed twice.
            if (matches(removal.atom)) {
                World* destination = removal.destination.get();
                if (destination) {
                    destination->emplace<T>(std::move(values.data[removal.atom.slot]));
                }
                destroy(removal.atom.slot);
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
        // By index, because a value's destructor can queue more.
        for (size_t i = 0; i < queued_adds.size(); ++i) {
            destroy(queued_adds[i]);
        }
        queued_adds.clear();
        queued_removals.clear();
        free_slots.clear();
        slot_extent = 0;
    }
};

template<typename T, bool WithAtoms>
struct View {
    dcs_detail::WorldState* state;
    Slab<T>* slab; // nullptr if the World has no T yet.

    View(dcs_detail::WorldState* world_state, Slab<T>* slab)
        : state(world_state), slab(slab) {
        ++state->active_views;
    }

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    ~View() {
        --state->active_views;
    }

    struct Entry {
        Atom<T> atom;
        T& value;
    };

    struct iterator {
        const View* view;
        uint32_t slot = UINT32_MAX;  // UINT32_MAX means the end.
        uint32_t next_word = 0;
        uint32_t word_count = 0;
        uint64_t remaining_bits = 0; // Live slots in the current word that are not visited yet.

        void move_to_next_live_slot() {
            // Skip words that have no live slots left.
            while (remaining_bits == 0) {
                if (next_word == word_count) {
                    // Nothing left, so this is now the end iterator.
                    slot = UINT32_MAX;
                    return;
                }
                remaining_bits = view->slab->presence[next_word];
                ++next_word;
            }

            // Select the first live slot, then remove its bit from the mask.
            uint32_t bit_index = static_cast<uint32_t>(std::countr_zero(remaining_bits));
            slot = (next_word - 1) * 64 + bit_index;
            remaining_bits &= remaining_bits - 1;
        }

        iterator& operator++() {
            move_to_next_live_slot();
            return *this;
        }

        // iter() gives you the value.
        T& operator*() const requires (!WithAtoms) {
            return view->slab->values.data[slot];
        }

        // iter_atoms() gives you the value and its Atom.
        Entry operator*() const requires WithAtoms {
            return Entry{ view->slab->atom_at(slot), view->slab->values.data[slot] };
        }

        bool operator==(const iterator& other) const {
            return slot == other.slot;
        }
    };

    iterator begin() const {
        iterator first{ this };
        if (slab) {
            first.word_count = Slab<T>::presence_words(slab->slot_extent);
        }
        first.move_to_next_live_slot();
        return first;
    }

    iterator end() const {
        return iterator{ this };
    }
};

inline dcs_detail::WorldState::~WorldState() {
    // Destroy the values first, while children can still unregister themselves.
    // Each slab leaves the list first, because a value's destructor can still queue or look things up.
    while (!slabs.empty()) {
        ISlab* slab = slabs.back();
        slabs.pop_back();
        delete slab;
    }
    // Children still listed outlived this World's values (for example held by a shared_ptr). Let them go.
    for (World* child : children) {
        child->parent = nullptr;
    }

    world_table.remove(id);
}

template<typename T>
bool Atom<T>::is_valid() const {
    World* owner = world();
    return owner && owner->is_live(*this);
}

static_assert(sizeof(Atom<int>) == 16 && std::is_trivially_copyable_v<Atom<int>>, "Atoms should stay small and cheap to copy");
