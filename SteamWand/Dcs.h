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

struct TypeRegistry {
    static uint32_t next_id() {
        static uint32_t counter = 0;
        return counter++;
    }
};

template<typename T>
struct TypeInfo {
    static uint32_t id() {
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
    virtual bool validate(Atom h) const = 0;
};

template<typename T>
struct Slab : public ISlab {
    struct Meta {
        void* owner;
    };

    T* data;
    Meta* meta;
    uint64_t* presence;
    uint32_t cap;
    uint32_t next_idx;

    Slab(uint32_t capacity) : cap(capacity), next_idx(0) {
        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        meta = (Meta*)_aligned_malloc(cap * sizeof(Meta), 64);

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
        _aligned_free(meta);
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
        return (World*)meta[index].owner;
    }

    bool validate(Atom h) const override {
        return h.id < next_idx && is_live(h.id);
    }

    T* resolve(Atom h) {
        if (h.id >= next_idx || !is_live(h.id)) {
            return nullptr;
        }
        return &data[h.id];
    }

    template<typename U>
    Atom create(U&& component, World* world) {
        // Hard cap — slabs do not resize. Hitting this means the World was
        // constructed with too small a capacity for the workload.
        assert(next_idx < cap && "Slab capacity exceeded");

        uint32_t id = next_idx++;

        // Placement-new into uninitialized aligned storage. Plain assignment
        // would read the LHS as if it were a constructed T, which is UB for
        // non-trivial types like std::string.
        new (&data[id]) T(std::forward<U>(component));
        meta[id].owner = world;
        presence[id / 64] |= (1ULL << (id % 64));

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

    uint64_t get_combined_mask(uint32_t word_idx) const {
        uint64_t mask = ~0ULL;

        std::apply([&](auto*... s) {
            ((mask &= s->presence[word_idx]), ...);
            }, slabs);

        return mask;
    }

    template <typename Func>
    void each(Func func) {
        uint32_t max_idx = get_max_idx();
        uint32_t word_count = (max_idx + 63) / 64;

        for (uint32_t w = 0; w < word_count; ++w) {
            uint64_t live = get_combined_mask(w);

            while (live) {
                uint32_t bit = ctz64(live);
                uint32_t slot = (w * 64) + bit;

                func(std::get<Slab<Types>*>(slabs)->data[slot]...);

                live &= (live - 1);
            }
        }
    }

    // Range-for support. Yields std::tuple<Types&...> per row.
    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;          // current word index
        uint64_t live;       // remaining live bits in current word
        uint32_t slot;       // current slot (valid while live != 0 or end)

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = v->get_combined_mask(w);
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

        std::tuple<Types&...> operator*() const {
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

// Single-component range. Lighter than View<T> — no AND-mask, just one slab.
template <typename T>
struct SingleView {
    Slab<T>* slab;

    explicit SingleView(Slab<T>* s) : slab(s) {}

    struct iterator {
        Slab<T>* slab;
        uint32_t word_count;
        uint32_t w;
        uint64_t live;
        uint32_t slot;

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = slab->presence[w];
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

        T& operator*() const {
            return slab->data[slot];
        }

        bool operator!=(const iterator& other) const {
            return w != other.w || live != other.live;
        }
    };

    iterator begin() const {
        uint32_t wc = (slab->next_idx + 63) / 64;

        iterator it{ slab, wc, 0, 0, 0 };
        it.advance_to_next_live_word();

        if (it.w < it.word_count) {
            it.pop_current();
        }
        return it;
    }

    iterator end() const {
        uint32_t wc = (slab->next_idx + 63) / 64;

        return iterator{ slab, wc, wc, 0, 0 };
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
    Atom add(T&& component) {
        return get_slab<std::decay_t<T>>().create(
            std::forward<T>(component), this
        );
    }

    template<typename T>
    Atom add(const T& val) {
        T copy = val;
        return get_slab<T>().create(std::move(copy), this);
    }

    // Attach an already-built World as a child. Requires std::move at the
    // call site because the child's storage is moved into this World's slab:
    //
    //     World jeans(100);
    //     jeans.add<float>(0.8f);
    //     character.attach_world(std::move(jeans));   // jeans is now empty
    //
    // After this call the source World is in a moved-from state — don't use
    // it. The std::move at the call site is the signal that ownership has
    // transferred.
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
            if (!s || !s->validate(pending.atom)) {
                continue;
            }

            s->remove_at(pending.atom.id);
        }

        death_row.clear();
    }

    // Range-for entry point. One type yields T&, multiple yield std::tuple<T&...>.
    template<typename T>
    SingleView<T> iter() {
        return SingleView<T>(&get_slab<T>());
    }

    template<typename First, typename Second, typename... Rest>
    auto iter() {
        return View<First, Second, Rest...>(
            &get_slab<First>(),
            &get_slab<Second>(),
            &get_slab<Rest>()...
        );
    }
};