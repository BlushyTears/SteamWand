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
        return { 0xFFFFFFFF };
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
            return hash<uint32_t>{}(a.id);
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
struct TypeRegistry {
    static uint32_t next_id() {
        static uint32_t counter = 0;
        return counter++;
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
    virtual ~ISlab() = default;
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

    Slab(uint32_t capacity) : cap(capacity), next_idx(0) {
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

    // True if slot `i` currently holds a live component.
    bool is_live(uint32_t i) const {
        return (presence[i / 64] & (1ULL << (i % 64))) != 0;
    }

    size_t count() const override {
        return (size_t)next_idx;
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

        // We allocated raw bytes for `data` (aligned_malloc), so data[id] doesn't
        // hold a real T yet - just uninitialized memory. The `new (ptr) T(...)`
        // syntax constructs T directly into that memory. Writing `data[id] = ...`
        // instead would assume a T already exists there and try to destruct it
        // first, which crashes for anything non-trivial like std::string.
        new (&data[id]) T(std::forward<U>(component));

        owners[id] = world;

        uint32_t word = id / 64;
        uint64_t bit = 1ULL << (id % 64);
        presence[word] |= bit;

        return Atom{ id };
    }

    void remove_at(uint32_t index) override {
        if (index >= next_idx || !is_live(index)) {
            return;
        }

        data[index].~T();
        presence[index / 64] &= ~(1ULL << (index % 64));
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

template <typename... Types>
struct View {
    std::tuple<Slab<Types>*...> slabs;

    View(Slab<Types>*... s) : slabs(s...) {}

    template <typename T>
    Slab<T>* get_slab() {
        return std::get<Slab<T>*>(slabs);
    }

    uint32_t get_max_idx() const {
        return std::apply([](auto*... s) {
            return std::max({ s->next_idx... });
            }, slabs);
    }

    // Range-for support below
    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;          // current word index
        uint64_t live;       // remaining live bits in current word
        uint32_t slot;       // current slot (valid while live != 0 or end)

        // For one type the live mask is just that slab's presence word.
        // For many it's the AND across every slab's presence word, so a bit
        // is set only where every type has a live slot at that index.
        uint64_t mask_at(uint32_t word) const {
            if constexpr (sizeof...(Types) == 1) {
                return std::get<0>(v->slabs)->presence[word];
            }
            else {
                uint64_t mask = ~0ULL;

                std::apply([&](auto*... s) {
                    ((mask &= s->presence[word]), ...);
                    }, v->slabs);

                return mask;
            }
        }

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = mask_at(w);
                if (live) {
                    return;
                }
                ++w;
            }
        }

        void pop_current() {
            slot = (w * 64) + ctz64(live);
            live &= (live - 1);
        }

        iterator& operator++() {
            if (!live) {
                ++w;
                advance_to_next_live_word();
            }
            if (w >= word_count) {
                return *this;
            }
            pop_current();
            return *this;
        }

        // The (parens) around the single-type expression keep decltype(auto)
        // returning T&, not T. Without them it would copy.
        decltype(auto) operator*() const {
            if constexpr (sizeof...(Types) == 1) {
                return (std::get<0>(v->slabs)->data[slot]);
            }
            else {
                return std::tuple<Types&...>(
                    std::get<Slab<Types>*>(v->slabs)->data[slot]...
                );
            }
        }

        bool operator!=(const iterator& other) const {
            return w != other.w || live != other.live;
        }
    };

    iterator begin() const {
        uint32_t max_idx = get_max_idx();
        uint32_t wc = (max_idx + 63) / 64;

        iterator it{ this, wc, 0, 0, 0 };
        it.advance_to_next_live_word();

        if (it.w < it.word_count) {
            it.pop_current();
        }
        return it;
    }

    iterator end() const {
        uint32_t max_idx = get_max_idx();
        uint32_t wc = (max_idx + 63) / 64;

        return iterator{ this, wc, wc, 0, 0 };
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
        for (auto& slab : registry) {
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
        for (const auto& pending : death_row) {
            if (pending.type_id >= registry.size()) {
                continue;
            }

            ISlab* s = registry[pending.type_id].get();
            if (!s) {
                continue;
            }

            s->remove_at(pending.atom.id);
        }

        death_row.clear();
    }

    // Range-for entry point. One type yields T&, multiple yield std::tuple<T&...>.
    template<typename... Ts>
    View<Ts...> iter() {
        return View<Ts...>(&get_slab<Ts>()...);
    }
};