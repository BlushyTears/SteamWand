#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <iostream>
#include <array>
#include <malloc.h>
#include <cstring>
#include <algorithm>

struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

struct World;

struct Atom {
    uint32_t id;
    uint32_t gen;

    bool is_valid() const {
        return id != 0xFFFFFFFF;
    }

    static Atom invalid() {
        return { 0xFFFFFFFF, 0 };
    }
};

#define ATOM_TYPES(X) \
    X(1, int32_t,  Int32)  \
    X(2, uint32_t, UInt32) \
    X(3, float,    Float)  \
    X(4, Vec2,     Vec2)   \
    X(5, Vec3,     Vec3)   \
    X(6, bool,     Bool)   \
    X(7, char,     Char)   \
    X(8, World,    World)

enum class AtomType : uint8_t {
    Empty = 0,
#define X_ENUM(tid, ctype, name) name = tid,
    ATOM_TYPES(X_ENUM)
#undef X_ENUM
};

template<typename T>
struct TypeInfo;

#define DEFINE_TYPE_INFO(tid, ctype, name) \
template<> struct TypeInfo<ctype> { \
    static constexpr uint8_t type_id = uint8_t(tid); \
    static constexpr const char* type_name = #name; \
};
ATOM_TYPES(DEFINE_TYPE_INFO)

struct ISlab {
    virtual ~ISlab() = default;
    virtual void clear_all() = 0;
    virtual size_t count() const = 0;
    virtual void remove_at(uint32_t index) = 0;
    virtual World* get_world(uint32_t index) const = 0;
};

template<typename T>
struct Slab : public ISlab {
    struct Meta {
        void* owner;
        uint32_t gen;
    };

    T* data;
    Meta* meta;
    uint32_t* gens;

    uint32_t cap;
    uint32_t next_idx;

    Slab(uint32_t capacity) : cap(capacity), next_idx(0) {
        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        meta = (Meta*)_aligned_malloc(cap * sizeof(Meta), 64);
        gens = (uint32_t*)_aligned_malloc(cap * sizeof(uint32_t), 64);

        memset(gens, 0, cap * sizeof(uint32_t));
    }

    ~Slab() {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (gens[i] % 2 != 0) {
                data[i].~T();
            }
        }
        _aligned_free(data);
        _aligned_free(meta);
        _aligned_free(gens);
    }

    Atom create(T&& val, World* owner) {
        uint32_t idx = next_idx++;

        new (&data[idx]) T(std::move(val));

        gens[idx] = (gens[idx] + 1) | 1;
        meta[idx].owner = (void*)owner;
        meta[idx].gen = gens[idx];

        return { idx, gens[idx] };
    }

    void remove_at(uint32_t index) override {
        if (index < next_idx && (gens[index] % 2 != 0)) {
            data[index].~T();
            gens[index]++;
        }
    }

    T* resolve(Atom h) {
        if (h.id >= next_idx || gens[h.id] != h.gen) {
            return nullptr;
        }
        return &data[h.id];
    }

    World* get_world(uint32_t index) const override {
        return (World*)meta[index].owner;
    }

    size_t count() const override {
        return (size_t)next_idx;
    }

    void clear_all() override {
        for (uint32_t i = 0; i < next_idx; ++i) {
            if (gens[i] % 2 != 0) {
                data[i].~T();
            }
        }
        next_idx = 0;
        memset(gens, 0, cap * sizeof(uint32_t));
    }
};

struct World {
    uint32_t cap;
    std::array<ISlab*, 256> registry{};
    std::vector<std::unique_ptr<ISlab>> storage;
    std::vector<uint32_t> death_row;

    World(uint32_t capacity = 1024) : cap(capacity) {
        registry.fill(nullptr);
    }

    World(World&& other) noexcept : cap(other.cap) {
        registry = other.registry;
        storage = std::move(other.storage);
        death_row = std::move(other.death_row);
        other.registry.fill(nullptr);
    }

    World& operator=(World&& other) noexcept {
        if (this != &other) {
            storage.clear();
            cap = other.cap;
            registry = other.registry;
            storage = std::move(other.storage);
            death_row = std::move(other.death_row);
            other.registry.fill(nullptr);
        }
        return *this;
    }

    template<typename T>
    Slab<T>& get_slab() {
        uint8_t tid = TypeInfo<T>::type_id;
        if (!registry[tid]) {
            auto s = std::make_unique<Slab<T>>(cap);
            registry[tid] = s.get();
            storage.push_back(std::move(s));
        }
        return *static_cast<Slab<T>*>(registry[tid]);
    }

    template<typename T>
    Atom add(T&& val) {
        return get_slab<std::decay_t<T>>().create(std::forward<T>(val), this);
    }

    template<typename T>
    Atom add(const T& val) {
        T copy = val;
        return get_slab<T>().create(std::move(copy), this);
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
    World* get_world_at(uint32_t idx) {
        return get_slab<T>().get_world(idx);
    }

    template<typename T>
    size_t size() {
        uint8_t tid = TypeInfo<T>::type_id;
        return (tid < 256 && registry[tid]) ? registry[tid]->count() : 0;
    }

    void queue_free(uint32_t index) {
        death_row.push_back(index);
    }

    void cleanup() {
        if (death_row.empty()) return;
        for (uint32_t index : death_row) {
            for (ISlab* s : registry) {
                if (s) s->remove_at(index);
            }
        }
        death_row.clear();
    }
};