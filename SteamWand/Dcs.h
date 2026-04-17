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

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

// This is optional to add but nice for complex datatypes
inline std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << "Vec2(" << v.x << ", " << v.y << ")"; }
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) { return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")"; }

struct World;

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
    virtual void swap_and_pop(uint32_t index) = 0;
    virtual World* get_world(uint32_t index) const = 0;
};

template<typename T>
struct Slab : public ISlab {
    struct Meta {
        World* owner_world;
        uint32_t gen;
    };

    T* data;
    Meta* meta;
    uint32_t cap;
    uint32_t live_count;

    Slab(uint32_t capacity) : cap(capacity), live_count(0) {
        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        meta = (Meta*)_aligned_malloc(cap * sizeof(Meta), 64);
        if (meta)
            memset(meta, 0, cap * sizeof(Meta));
    }

    ~Slab() {
        for (uint32_t i = 0; i < live_count; ++i) {
            data[i].~T();
        }
        _aligned_free(data);
        _aligned_free(meta);
    }

    void create(const T& val, World* owner) {
        uint32_t idx = live_count++;
        new (&data[idx]) T(val);
        meta[idx].owner_world = owner;
        meta[idx].gen++;
    }

    void create(T&& val, World* owner) {
        uint32_t idx = live_count++;
        new (&data[idx]) T(std::move(val));
        meta[idx].owner_world = owner;
        meta[idx].gen++;
    }

    void swap_and_pop(uint32_t index) override {
        uint32_t last_idx = --live_count;
        if (index != last_idx) {
            if constexpr (std::is_move_assignable_v<T>) {
                data[index] = std::move(data[last_idx]);
            }
            else {
                data[index].~T();
                new (&data[index]) T(std::move(data[last_idx]));
            }
            meta[index].owner_world = meta[last_idx].owner_world;
            meta[index].gen++;
        }
        else {
            data[index].~T();
            meta[index].gen++;
        }
    }

    World* get_world(uint32_t index) const override {
        return meta[index].owner_world;
    }

    void clear_all() override {
        for (uint32_t i = 0; i < live_count; ++i) {
            data[i].~T();
        }
        live_count = 0;
    }

    size_t count() const override {
        return (size_t)live_count;
    }
};

struct World {
    uint32_t default_cap;
    std::array<ISlab*, 256> registry{};
    std::vector<std::unique_ptr<ISlab>> owned_storage;
    std::vector<uint32_t> death_row;

    World(uint32_t capacity = 1024) : default_cap(capacity) {
        registry.fill(nullptr);
    }

    World(World&& other) noexcept
        : default_cap(other.default_cap),
        registry(other.registry),
        owned_storage(std::move(other.owned_storage)),
        death_row(std::move(other.death_row)) {
        other.registry.fill(nullptr);
    }

    World& operator=(World&& other) noexcept {
        if (this != &other) {
            owned_storage.clear();
            default_cap = other.default_cap;
            registry = other.registry;
            owned_storage = std::move(other.owned_storage);
            death_row = std::move(other.death_row);
            other.registry.fill(nullptr);
        }
        return *this;
    }

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    template<typename T>
    Slab<T>& slab() {
        uint8_t tid = TypeInfo<T>::type_id;
        if (!registry[tid]) {
            auto new_slab = std::make_unique<Slab<T>>(default_cap);
            registry[tid] = new_slab.get();
            owned_storage.push_back(std::move(new_slab));
        }
        return *static_cast<Slab<T>*>(registry[tid]);
    }

    template<typename T>
    void add(const T& val) {
        slab<T>().create(val, this);
    }

    template<typename T>
    void add(T&& val) {
        slab<std::decay_t<T>>().create(std::move(val), this);
    }

    template<typename T>
    T* get_array() {
        return slab<T>().data;
    }

    template<typename T>
    World* get_world_at(uint32_t idx) {
        return slab<T>().get_world(idx);
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
        if (death_row.empty())
            return;

        std::sort(death_row.begin(), death_row.end(), std::greater<uint32_t>());
        uint32_t last_processed = 0xFFFFFFFF;

        for (uint32_t index : death_row) {
            if (index == last_processed)
                continue;
            for (ISlab* s : registry) {
                if (s && s->count() > (size_t)index)
                    s->swap_and_pop(index);
            }
            last_processed = index;
        }
        death_row.clear();
    }
};