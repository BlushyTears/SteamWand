// Todo:
// - When combining worlds we need godot-like control either:
// - Option to combine worlds and destruct old worlds easily
//
// - Need actual safety checks for get and the likes (stupid clanker called it safe for no reason)
// - Cleanup should be automatic in world raii probably instead of explicit
// - queue_free should take atom not index to avoid freeing the wrong data
// - World struct should default to giving local array,
//      if you want entire slab it should be called global array or get full or something
//
// - Expose Atom in iteration. Add iter_atoms<T>() yielding (Atom, T&) and a
//   multi-type version. Snake's expire-loop and reverseLookupExample currently
//   work around this by going through get_slab<T>() directly. The collision
//   detection case (knowing which box was hit) also needs it.
//
// - Per-type clear: world.clear<T>(). Slab::clear_all exists but isn't reachable
//   from World for a specific type. Useful between scenes, in tests, when
//   resetting a sub-World.
//
// - world.is_live<T>(atom) as a direct validity check, instead of get<T>(atom)
//   and checking for null.
//
// - Recursive cleanup. world.cleanup() currently only cleans that World's
//   death_row, not nested Worlds'. Needed if the city/house/room pattern is
//   used heavily.
//
// - Save/load story. Probably user code, but the engine could expose a stable-
//   representation hook for a slab.
//
// - Filtering / predicates / optional components in iteration
//
// - Get world by atom
//
// Ideas but probably little plan to add these immediately:
// - First-class entity ID layer
// - Thread safety
// - Auto-cleanup in World destructor (changes when destructors run)
// - Built-in spatial index, scene graph, or transform hierarchy
// - Replace vector storage with fixed array + linked list

// - EXAMPLES -

//struct Vec2 { float x, y; };
//struct Vec3 { float x, y, z; };
//
//void basicExamples() {
//    std::cout << "--- Basic SteamWand Examples ---\n";
//
//    World world(1024);
//
//    // Adding returns an Atom but you can opt to exclude if you don't care about the individual type
//    // For instance if you made a bullet hell game you wouldn't care about a bullet,
//    // but it can be a useful reference so that you don't have to iterate an entire world to find a specific field
//    Atom intAtom = world.add<int32_t>(42);
//    Atom floatAtom1 = world.add<float>(3.14f);
//    world.add<float>(3.15f);
//    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });
//
//    struct PlayerData { int level; };
//    world.add<PlayerData>({ 10 });
//
//    // Safe Retrieval
//    auto* val = world.get<int32_t>(intAtom);
//    if (val) {
//        std::cout << "Retrieved value via handle: " << *val << "\n";
//    }
//
//    // Fast linear access
//    auto* ints = world.get_array<int32_t>();
//    if (ints) {
//        std::cout << "First int32 value in raw array: " << ints[0] << "\n";
//    }
//
//    // Deletion and Cleanup
//    // We now pass the Atom and the Type so the World targets the correct Slab
//    world.queue_free<int32_t>(intAtom);
//    world.queue_free<float>(floatAtom1);
//    world.cleanup();
//
//    auto* expiredVal = world.get<int32_t>(intAtom);
//
//    if (!expiredVal) {
//        std::cout << "Atom correctly invalidated after deletion/cleanup.\n";
//    }
//    else {
//        // This shouldn't be reached if cleanup worked
//        std::cout << "Value still exists: " << *expiredVal << "\n";
//    }
//
//    std::cout << "--------------------------------\n\n";
//}
//
//void reverseLookupExample() {
//    std::cout << "--- Reverse Lookup Example ---\n";
//    World world(1024);
//
//    world.add<int32_t>(100);
//    world.add<int32_t>(200);
//
//    // This loop needs the per-index world owner, which the iterator doesn't expose.
//    // Keeping the indexed form here is intentional.
//    int32_t* hps = world.get_array<int32_t>();
//    size_t count = world.size<int32_t>();
//
//    for (uint32_t i = 0; i < (uint32_t)count; i++) {
//        World* owner = world.get_slab<int32_t>().get_world(i);
//        // here you could check if you found a matching item and return that id
//        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World address: " << owner << "\n";
//    }
//}
//
//void universeExample() {
//    World universe(10);
//
//    struct Data { float hp; };
//
//    World world(100);
//    for (int i = 0; i < 50; i++) {
//        Data d;
//        d.hp = float(i);
//        world.add<Data>(d);
//    }
//    universe.attach_world(std::move(world));
//
//    size_t worldCount = universe.size<World>();
//    std::cout << "universe size is " << worldCount << std::endl;
//
//    if (worldCount > 0) {
//        // Grab the first world from the universe and iterate its Data.
//        World* worlds = universe.get_array<World>();
//        for (auto& item : worlds[0].iter<Data>()) {
//            std::cout << "HP: " << item.hp << std::endl;
//        }
//    }
//}
//
//void multipleWorldsExample() {
//    World character(10);
//
//    // Build clothing Worlds standalone, then attach them. The std::move at
//    // the call site signals ownership transfer � after attach_world, the
//    // local variable is empty.
//
//    // Jeans have 3 components
//    World jeans(100);
//    jeans.add<float>(0.8f);
//    jeans.add<std::string>("Denim");
//    jeans.add<Vec2>({ 32, 34 });
//
//    // Shirt only has 1 component
//    World shirt(100);
//    shirt.add<float>(0.2f);
//
//    character.attach_world(std::move(jeans));
//    character.attach_world(std::move(shirt));
//
//    float totalArmor = 0.0f;
//
//    for (auto& item : character.iter<World>()) {
//        // Each clothing-world holds a single armor float
//        for (auto& armor : item.iter<float>()) {
//            std::cout << "Incrementing total armor: " << totalArmor << " By: " << armor << std::endl;
//            totalArmor += armor;
//            break;
//        }
//    }
//}

// Source code:

#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <iostream>
#include <malloc.h>
#include <cstring>
#include <algorithm>
#include <tuple>
#include <cassert>
#include <functional>
#include <new>
#include <utility>

// Find the first set bit with a CPU instruction. The caller supplies a nonzero value.
#ifdef _MSC_VER
#include <intrin.h>
static inline uint32_t ctz64(uint64_t v) {
    unsigned long idx;
    _BitScanForward64(&idx, v);
    return (uint32_t)idx;
}
#else
static inline uint32_t ctz64(uint64_t v) {
    return (uint32_t)__builtin_ctzll(v);
}
#endif

struct World;

struct Atom {
    uint32_t id;

    bool is_valid() const {
        return id != 0xFFFFFFFF;
    }

    static Atom invalid() {
        Atom atom;
        atom.id = 0xFFFFFFFF;
        return atom;
    }

    bool operator==(const Atom& other) const {
        return id == other.id;
    }

    bool operator!=(const Atom& other) const {
        return id != other.id;
    }
};

namespace std {
    template<>
    struct hash<Atom> {
        size_t operator()(const Atom& a) const noexcept {
            hash<uint32_t> hash_id;
            return hash_id(a.id);
        }
    };
}

// Each type T needs a unique integer id (used as an index into World's
// registry of slabs). The trick: a `static` variable inside a template
// function gives you ONE variable per template instantiation, not one
// shared across them. So `static uint32_t tid` inside TypeInfo<int>::id()
// and TypeInfo<float>::id() are two separate variables.
//
// We want each to be initialized to a different number. The way to do
// that is have them all call into a shared counter that hands out fresh
// numbers. That's what TypeRegistry::next_id() is for: one counter,
// called once per type, the result cached in that type's static.



// Codex feedback:

//Yes.For this prototype, I’d focus on ownership, handle lifetimes, and predictable capacity limits.The basic slab -
//and -bitmask design is worth keeping.
//
//In priority order :
//
//1. Prevent accidental Slab copies.It owns raw allocations but is implicitly copyable.Writing auto slab =
//world.get_slab<T>() copies its owning pointers, potentially causing double - free.Deleting its copy operations costs
//nothing at runtime.Storage definition(steamwand / SteamWand / SteamWand / datalayout / Dcs.h:273)
//
//2. Fix owner pointers when moving a World.I confirmed that after attach_world(), a child’s components still point to
//the original child’s address.That pointer can eventually dangle.Moving a world should update those references.
//Move operations(steamwand / SteamWand / SteamWand / datalayout / Dcs.h:622)
//
//3. Handle capacity exhaustion properly.Deleted slots aren’t reused, so repeated spawning / despawning eventually fills
//a slab—even with very few live objects.Snake already follows that pattern.The capacity assertion disappears in
//release builds, allowing out - of - bounds writes.A defined failure on insertion would be a useful first fix.
//Insertion(steamwand / SteamWand / SteamWand / datalayout / Dcs.h:329)
//
//4. Define what makes an Atom remain valid.I reproduced an old handle resolving to a different object after discard()
//and another insertion.Generation counters would let you detect stale handles when slots are reused.
//
//5. Settle the multi - type matching rule before implementing slot reuse.iter<Position, Velocity>() matches equal array
//indices.Skipping one object’s velocity can therefore pair another object’s velocity with its position.That’s fine
//as an explicitly managed indexing scheme, but independent per - type free lists would undermine it.
//
//A few smaller improvements would also help : iter_atoms<T>() for loops that need handles, a separate live_count<T>(),
//and lookups that don’t allocate missing slabs.Multi - type iteration could also stop at the smallest populated extent,
//since an intersection cannot produce matches beyond it.




struct TypeRegistry {
    static uint32_t next_id() {
        static uint32_t counter = 0;
        uint32_t id = counter;
        ++counter;
        return id;
    }
};

template<typename T>
struct TypeInfo {
    static uint32_t id() {
        // First call for this T: counter advances, tid gets the new number.
        // Every subsequent call: returns the same tid (initialization happens once).
        static uint32_t tid = TypeRegistry::next_id();
        return tid;
    }
};

struct ISlab {
    virtual ~ISlab() {}
    virtual void clear_all() = 0;
    virtual size_t count() const = 0;
    virtual void remove_at(uint32_t index) = 0;
    virtual World* get_world(uint32_t index) const = 0;
};

template<typename T>
struct Slab : public ISlab {
    T* data;
    World** owners;
    uint64_t* presence;
    uint32_t cap;
    uint32_t next_idx;

    Slab(uint32_t capacity) {
        cap = capacity;
        next_idx = 0;

        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        owners = (World**)_aligned_malloc(cap * sizeof(World*), 64);

        uint32_t words = (cap + 63) / 64;
        presence = (uint64_t*)_aligned_malloc(words * sizeof(uint64_t), 64);

        memset(presence, 0, words * sizeof(uint64_t));
    }

    ~Slab() {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (is_live(i)) {
                data[i].~T();
            }
        }

        _aligned_free(data);
        _aligned_free(owners);
        _aligned_free(presence);
    }

    // Each presence word holds one bit for each of 64 slots.
    bool is_live(uint32_t i) const {
        uint32_t word = i / 64;
        uint32_t bit_index = i % 64;
        uint64_t bit = 1ULL << bit_index;
        return (presence[word] & bit) != 0;
    }

    size_t count() const override {
        return next_idx;
    }

    World* get_world(uint32_t index) const override {
        return owners[index];
    }

    T* resolve(Atom h) {
        if (h.id >= next_idx || !is_live(h.id)) {
            return nullptr;
        }
        return &data[h.id];
    }

    template<typename U>
    Atom create(U&& component, World* world) {
        assert(next_idx < cap && "Slab capacity exceeded");

        uint32_t id = next_idx;
        next_idx++;

        // The slot is raw memory. Placement new constructs a T in that slot;
        // assignment would require a T to already exist there.
        // forward preserves whether the caller is copying or moving the value.
        new (&data[id]) T(std::forward<U>(component));

        owners[id] = world;

        uint32_t word = id / 64;
        uint64_t bit = 1ULL << (id % 64);
        presence[word] |= bit;

        Atom atom;
        atom.id = id;
        return atom;
    }

    void remove_at(uint32_t index) override {
        if (index >= next_idx || !is_live(index)) {
            return;
        }

        data[index].~T();

        uint32_t word = index / 64;
        uint32_t bit_index = index % 64;
        uint64_t bit = 1ULL << bit_index;
        presence[word] &= ~bit;
    }

    void clear_all() override {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (is_live(i)) {
                data[i].~T();
            }
        }

        next_idx = 0;
        uint32_t words = (cap + 63) / 64;
        memset(presence, 0, words * sizeof(uint64_t));
    }
};

template<typename... Types>
struct View;

// Single-type view: iter<T>() yields a T&.
// Keeping this separate makes its iterator ordinary typed code.
template<typename T>
struct View<T> {
    typedef T& Reference;

    // Keep the existing public tuple so direct access to view.slabs still works.
    std::tuple<Slab<T>*> slabs;

    View(Slab<T>* slab) : slabs(slab) {}

    template<typename U>
    Slab<U>* get_slab() {
        return std::get<Slab<U>*>(slabs);
    }

    uint32_t get_max_idx() const {
        Slab<T>* slab = std::get<0>(slabs);
        return slab->next_idx;
    }

    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;          // Current word index; word_count means end.
        uint64_t live;       // Bits remaining AFTER the current slot.
        uint32_t slot;       // Current slot, valid while w < word_count.

        uint64_t mask_at(uint32_t word) const {
            Slab<T>* slab = std::get<0>(v->slabs);
            return slab->presence[word];
        }

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = mask_at(w);
                if (live != 0) {
                    return;
                }
                ++w;
            }
        }

        void pop_current() {
            // Select the first live slot, then remove its bit from the mask.
            slot = w * 64 + ctz64(live);
            // Subtracting one and ANDing clears only the lowest set bit.
            live &= live - 1;
        }

        iterator& operator++() {
            if (live == 0) {
                ++w;
                advance_to_next_live_word();
            }

            if (w >= word_count) {
                return *this;
            }

            pop_current();
            return *this;
        }

        T& operator*() const {
            Slab<T>* slab = std::get<0>(v->slabs);
            return slab->data[slot];
        }

        bool operator!=(const iterator& other) const {
            return w != other.w || live != other.live;
        }
    };

    iterator begin() const {
        uint32_t max_idx = get_max_idx();

        iterator it;
        it.v = this;
        it.word_count = (max_idx + 63) / 64;
        it.w = 0;
        it.live = 0;
        it.slot = 0;

        it.advance_to_next_live_word();
        if (it.w < it.word_count) {
            it.pop_current();
        }
        return it;
    }

    iterator end() const {
        uint32_t max_idx = get_max_idx();

        iterator it;
        it.v = this;
        it.word_count = (max_idx + 63) / 64;
        it.w = it.word_count;
        it.live = 0;
        it.slot = 0;
        return it;
    }
};

// Multi-type view: iter<A, B, ...>() yields a tuple of references.
// Types... is the requested type list; sizeof...(Types) is its length.
template<typename... Types>
struct View {
    typedef std::tuple<Types&...> Reference;

    std::tuple<Slab<Types>*...> slabs;

    View(Slab<Types>*... slab_ptrs) : slabs(slab_ptrs...) {}

    template<typename T>
    Slab<T>* get_slab() {
        return std::get<Slab<T>*>(slabs);
    }

    uint32_t get_max_idx() const {
        // Keep the original reduction here: changing it also changed the
        // compiler's register allocation in multi-type iteration.
        return std::apply([](auto*... slab) {
            return std::max({ slab->next_idx... });
        }, slabs);
    }

    // Deliberately repeat the small iterator instead of sharing it through
    // another layer of templates. Both versions can be read on their own.
    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;          // Current word index; word_count means end.
        uint64_t live;       // Bits remaining AFTER the current slot.
        uint32_t slot;       // Current slot, valid while w < word_count.

        uint64_t mask_at(uint32_t word) const {
            // This runs for every scanned word. Keep the original direct AND
            // across slabs; materializing an array here can add work.
            uint64_t mask = ~0ULL;
            std::apply([&](auto*... slab) {
                ((mask &= slab->presence[word]), ...);
            }, v->slabs);
            return mask;
        }

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = mask_at(w);
                if (live != 0) {
                    return;
                }
                ++w;
            }
        }

        void pop_current() {
            // Select the first live slot, then remove its bit from the mask.
            slot = w * 64 + ctz64(live);
            // Subtracting one and ANDing clears only the lowest set bit.
            live &= live - 1;
        }

        iterator& operator++() {
            if (live == 0) {
                ++w;
                advance_to_next_live_word();
            }

            if (w >= word_count) {
                return *this;
            }

            pop_current();
            return *this;
        }

        std::tuple<Types&...> operator*() const {
            // Build the references directly, as in the original hot path.
            return std::tuple<Types&...>(
                std::get<Slab<Types>*>(v->slabs)->data[slot]...
            );
        }

        bool operator!=(const iterator& other) const {
            return w != other.w || live != other.live;
        }
    };

    iterator begin() const {
        uint32_t max_idx = get_max_idx();

        iterator it;
        it.v = this;
        it.word_count = (max_idx + 63) / 64;
        it.w = 0;
        it.live = 0;
        it.slot = 0;

        it.advance_to_next_live_word();
        if (it.w < it.word_count) {
            it.pop_current();
        }
        return it;
    }

    iterator end() const {
        uint32_t max_idx = get_max_idx();

        iterator it;
        it.v = this;
        it.word_count = (max_idx + 63) / 64;
        it.w = it.word_count;
        it.live = 0;
        it.slot = 0;
        return it;
    }
};

struct World {
    struct PendingRemoval {
        uint32_t type_id;
        Atom atom;
    };

    // Hard cap: slabs are sized to `cap` at construction and do not grow.
    // Adding more than `cap` items of the same type asserts in Slab::create.
    uint32_t cap;

    // One owning slot per type. Indexed by TypeInfo<T>::id(). Empty slots
    // (types not yet used in this world) hold a null unique_ptr.
    std::vector<std::unique_ptr<ISlab>> registry;

    std::vector<PendingRemoval> death_row;

    World(uint32_t capacity = 1024) : cap(capacity) {}

    // Copying a World would have to deep-copy every slab and is not supported.
    // The explicit delete gives a readable error instead of a unique_ptr one.
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    template<typename T>
    Slab<T>& get_slab() {
        uint32_t tid = TypeInfo<T>::id();

        if (tid >= registry.size()) {
            registry.resize(tid + 1);
        }

        if (!registry[tid]) {
            registry[tid] = std::make_unique<Slab<T>>(cap);
        }

        return *static_cast<Slab<T>*>(registry[tid].get());
    }

    template<typename T>
    Atom add(T&& payload) {
        return get_slab<std::decay_t<T>>().create(
            std::forward<T>(payload), this
        );
    }

    // Needed because callers write add<T>(x). With an explicit template
    // argument, T&& is no longer a forwarding reference - it's a plain
    // rvalue reference that won't bind to lvalues. This overload catches them.
    template<typename T>
    Atom add(const T& val) {
        T copy = val;
        return get_slab<T>().create(std::move(copy), this);
    }

    void discard() {
        for (std::unique_ptr<ISlab>& slab : registry) {
            if (slab) {
                slab->clear_all();
            }
        }
        death_row.clear();
    }

    World& attach_world(World&& child) {
        Slab<World>& slab = get_slab<World>();
        Atom a = slab.create(std::move(child), this);
        return slab.data[a.id];
    }

    template<typename T>
    T* get(Atom h) {
        return get_slab<T>().resolve(h);
    }

    template<typename T>
    T* get_array() {
        return get_slab<T>().data;
    }

    template<typename T>
    size_t size() {
        uint32_t tid = TypeInfo<T>::id();

        if (tid >= registry.size() || !registry[tid]) {
            return 0;
        }
        return registry[tid]->count();
    }

    template<typename T>
    void queue_free(Atom h) {
        death_row.push_back({ TypeInfo<T>::id(), h });
    }

    void cleanup() {
        for (const PendingRemoval& pending : death_row) {
            if (pending.type_id >= registry.size()) {
                continue;
            }

            ISlab* slab = registry[pending.type_id].get();
            if (!slab) {
                continue;
            }

            slab->remove_at(pending.atom.id);
        }

        death_row.clear();
    }

    // Range-for entry point. One type yields T&, multiple yield std::tuple<T&...>.
    template<typename... Ts>
    View<Ts...> iter() {
        return View<Ts...>(&get_slab<Ts>()...);
    }
};