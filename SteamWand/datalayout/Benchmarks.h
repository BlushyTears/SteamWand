#pragma once

void steamwand_linear();
void steamwand_query_parallel();
void steamwand_multi_component();
void steamwand_backwards_query();
void steamwand_zombie();

// If you still want the baseline comparisons:
void archetype_linear();
void archetype_query_parallel();
void archetype_multi();
void archetype_backwards_query();
void archetype_zombie();


// investigate if this code is better using c++11 standards rather than c++ 20 features

/*

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

template<size_t... Is> struct seq {};
template<size_t N, size_t... Is>
struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};
template<size_t... Is>
struct gen_seq<0, Is...> : seq<Is...> {};

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
    virtual ~ISlab() {}
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
        assert(next_idx < cap && "Slab capacity exceeded");

        uint32_t id = next_idx++;

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

    uint32_t get_max_idx_impl() const {
        return 0;
    }

    template<typename T, typename... Args>
    uint32_t get_max_idx_impl(Slab<T>* s, Slab<Args>*... args) const {
        uint32_t rest = get_max_idx_impl(args...);
        return s->next_idx > rest ? s->next_idx : rest;
    }

    template<size_t... Is>
    uint32_t get_max_idx_helper(seq<Is...>) const {
        return get_max_idx_impl(std::get<Is>(slabs)...);
    }

    uint32_t get_max_idx() const {
        return get_max_idx_helper(gen_seq<sizeof...(Types)>{});
    }

    template<size_t... Is>
    uint64_t get_combined_mask_helper(uint32_t word_idx, seq<Is...>) const {
        uint64_t mask = ~0ULL;
        int dummy[] = { 0, (mask &= std::get<Is>(slabs)->presence[word_idx], 0)... };
        (void)dummy;
        return mask;
    }

    uint64_t get_combined_mask(uint32_t word_idx) const {
        return get_combined_mask_helper(word_idx, gen_seq<sizeof...(Types)>{});
    }

    template <typename Func, size_t... Is>
    void each_helper(Func& func, uint32_t slot, seq<Is...>) {
        func(std::get<Is>(slabs)->data[slot]...);
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

                each_helper(func, slot, gen_seq<sizeof...(Types)>{});

                live &= (live - 1);
            }
        }
    }

    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;
        uint64_t live;
        uint32_t slot;

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
            if (w < word_count) {
                pop_current();
            }
            return *this;
        }

        template<size_t... Is>
        std::tuple<Types&...> deref_helper(seq<Is...>) const {
            return std::tuple<Types&...>(std::get<Is>(v->slabs)->data[slot]...);
        }

        std::tuple<Types&...> operator*() const {
            return deref_helper(gen_seq<sizeof...(Types)>{});
        }

        bool operator!=(const iterator& other) const {
            return w != other.w || live != other.live;
        }
    };

    iterator begin() const {
        uint32_t wc = (get_max_idx() + 63) / 64;

        iterator it{ this, wc, 0, 0, 0 };
        it.advance_to_next_live_word();

        if (it.w < it.word_count) {
            it.pop_current();
        }
        return it;
    }

    iterator end() const {
        uint32_t wc = (get_max_idx() + 63) / 64;

        return iterator{ this, wc, wc, 0, 0 };
    }
};

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
            if (w < word_count) {
                pop_current();
            }
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

    uint32_t cap;

    std::vector<std::unique_ptr<ISlab>> registry;

    std::vector<PendingRemoval> death_row;

    World(uint32_t capacity = 1024) : cap(capacity) {}

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
            registry[tid] = std::unique_ptr<Slab<T>>(new Slab<T>(cap));
        }

        return *static_cast<Slab<T>*>(registry[tid].get());
    }

    template<typename T>
    Atom add(T&& component) {
        return get_slab<typename std::decay<T>::type>().create(
            std::forward<T>(component), this
        );
    }

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
            if (!s || !s->validate(pending.atom)) {
                continue;
            }

            s->remove_at(pending.atom.id);
        }

        death_row.clear();
    }

    template<typename T>
    SingleView<T> iter() {
        return SingleView<T>(&get_slab<T>());
    }

    template<typename First, typename Second, typename... Rest>
    View<First, Second, Rest...> iter() {
        return View<First, Second, Rest...>(
            &get_slab<First>(),
            &get_slab<Second>(),
            &get_slab<Rest>()...
        );
    }
};

*/


// and c++26:

/*

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

    bool operator==(const Atom& other) const = default;
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

        std::fill_n(presence, words, 0);
    }

    ~Slab() {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (is_live(i)) {
                std::destroy_at(&data[i]);
            }
        }

        _aligned_free(data);
        _aligned_free(meta);
        _aligned_free(presence);
    }

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
        return (h.id < next_idx && is_live(h.id)) ? &data[h.id] : nullptr;
    }

    template<typename U>
    Atom create(U&& component, World* world) {
        assert(next_idx < cap && "Slab capacity exceeded");

        uint32_t id = next_idx++;

        std::construct_at(&data[id], std::forward<U>(component));
        meta[id].owner = world;
        presence[id / 64] |= (1ULL << (id % 64));

        return Atom{ id };
    }

    void remove_at(uint32_t index) override {
        if (index < next_idx && is_live(index)) {
            std::destroy_at(&data[index]);
            presence[index / 64] &= ~(1ULL << (index % 64));
        }
    }

    void clear_all() override {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (is_live(i)) {
                std::destroy_at(&data[i]);
            }
        }

        next_idx = 0;
        uint32_t words = (cap + 63) / 64;
        std::fill_n(presence, words, 0);
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

                // C++26 Pack Indexing allows cleaner access than std::get<I>
                std::apply([&](auto*... s) {
                    func(s->data[slot]...);
                }, slabs);

                live &= (live - 1);
            }
        }
    }

    struct iterator {
        const View* v;
        uint32_t word_count;
        uint32_t w;
        uint64_t live;
        uint32_t slot;

        void advance_to_next_live_word() {
            while (w < word_count) {
                live = v->get_combined_mask(w);
                if (live) return;
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
            if (w < word_count) {
                pop_current();
            }
            return *this;
        }

        auto operator*() const {
            return std::apply([this](auto*... s) {
                return std::forward_as_tuple(s->data[slot]...);
            }, v->slabs);
        }

        bool operator!=(const iterator& other) const = default;
    };

    iterator begin() const {
        uint32_t wc = (get_max_idx() + 63) / 64;
        iterator it{ this, wc, 0, 0, 0 };
        it.advance_to_next_live_word();
        if (it.w < it.word_count) it.pop_current();
        return it;
    }

    iterator end() const {
        uint32_t wc = (get_max_idx() + 63) / 64;
        return iterator{ this, wc, wc, 0, 0 };
    }
};

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
                if (live) return;
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
            if (w < word_count) {
                pop_current();
            }
            return *this;
        }

        T& operator*() const {
            return slab->data[slot];
        }

        bool operator!=(const iterator& other) const = default;
    };

    iterator begin() const {
        uint32_t wc = (slab->next_idx + 63) / 64;
        iterator it{ slab, wc, 0, 0, 0 };
        it.advance_to_next_live_word();
        if (it.w < it.word_count) it.pop_current();
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

    uint32_t cap;
    std::vector<std::unique_ptr<ISlab>> registry;
    std::vector<PendingRemoval> death_row;

    World(uint32_t capacity = 1024) : cap(capacity) {}

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
        return get_slab<std::remove_cvref_t<T>>().create(
            std::forward<T>(component), this
        );
    }

    template<typename T>
    Atom add(const T& val) {
        return get_slab<T>().create(T(val), this);
    }

    void discard() {
        for (auto& slab : registry) {
            if (slab) slab->clear_all();
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
        return (tid >= registry.size() || !registry[tid]) ? 0 : registry[tid]->count();
    }

    template<typename T>
    void queue_free(Atom h) {
        death_row.push_back({ TypeInfo<T>::id(), h });
    }

    void cleanup() {
        std::erase_if(death_row, [this](const auto& pending) {
            if (pending.type_id < registry.size()) {
                if (auto* s = registry[pending.type_id].get(); s && s->validate(pending.atom)) {
                    s->remove_at(pending.atom.id);
                }
            }
            return true;
        });
    }

    template<typename T>
    SingleView<T> iter() {
        return SingleView<T>(&get_slab<T>());
    }

    template<typename... Ts>
    auto iter() {
        return View<Ts...>(&get_slab<Ts>()...);
    }
};

*/