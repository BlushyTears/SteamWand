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

// Custom datatypes
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

// Optional and nice to customize your prints, but not necessary
inline std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << "Vec2(" << v.x << ", " << v.y << ")"; }
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) { return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")"; }

struct World;

struct Handle {
    uint32_t id;
    uint32_t gen;

    bool is_valid() const { 
        return id != 0xFFFFFFFF;
    }
    static Handle invalid() { 
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
    virtual void swap_and_pop(uint32_t index) = 0;
    virtual World* get_world(uint32_t index) const = 0;
};

template<typename T>
struct Slab : public ISlab {
    struct Meta {
        World* owner_world;
        uint32_t gen;
        uint32_t handle_id;
    };

    T* data;
    Meta* meta;
    uint32_t* sparse_map;
    uint32_t* gen_map;

    uint32_t cap;
    uint32_t live_count;
    uint32_t next_handle_id;

    Slab(uint32_t capacity) : cap(capacity), live_count(0), next_handle_id(0) {
        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        meta = (Meta*)_aligned_malloc(cap * sizeof(Meta), 64);
        sparse_map = (uint32_t*)_aligned_malloc(cap * sizeof(uint32_t), 64);
        gen_map = (uint32_t*)_aligned_malloc(cap * sizeof(uint32_t), 64);

        if (meta)
            memset(meta, 0, cap * sizeof(Meta));
        if (gen_map)
            memset(gen_map, 0, cap * sizeof(uint32_t));
        if (sparse_map)
            memset(sparse_map, 0xFF, cap * sizeof(uint32_t));
    }

    ~Slab() {
        for (uint32_t i = 0; i < live_count; ++i) {
            data[i].~T();
        }
        _aligned_free(data);
        _aligned_free(meta);
        _aligned_free(sparse_map);
        _aligned_free(gen_map);
    }

    Handle create(T&& val, World* owner) {
        uint32_t dense_idx = live_count++;
        uint32_t h_id = next_handle_id++;

        new (&data[dense_idx]) T(std::move(val));

        gen_map[h_id]++;
        sparse_map[h_id] = dense_idx;

        meta[dense_idx].owner_world = owner;
        meta[dense_idx].gen = gen_map[h_id];
        meta[dense_idx].handle_id = h_id;

        return { 
            h_id, gen_map[h_id]
        };
    }

    void swap_and_pop(uint32_t dense_index) override {
        uint32_t last_dense_idx = --live_count;

        uint32_t deleted_handle_id = meta[dense_index].handle_id;
        gen_map[deleted_handle_id]++;
        sparse_map[deleted_handle_id] = 0xFFFFFFFF;

        if (dense_index != last_dense_idx) {
            if constexpr (std::is_move_assignable_v<T>) {
                data[dense_index] = std::move(data[last_dense_idx]);
            }
            else {
                data[dense_index].~T();
                new (&data[dense_index]) T(std::move(data[last_dense_idx]));
            }

            uint32_t moved_handle_id = meta[last_dense_idx].handle_id;
            sparse_map[moved_handle_id] = dense_index;

            meta[dense_index] = meta[last_dense_idx];
        }
        else {
            data[dense_index].~T();
        }
    }

    T* resolve(Handle h) {
        if (h.id >= next_handle_id || gen_map[h.id] != h.gen) 
            return nullptr;

        uint32_t dense_idx = sparse_map[h.id];
        return (dense_idx == 0xFFFFFFFF) ? nullptr : &data[dense_idx];
    }

    World* get_world(uint32_t index) const override { 
        return meta[index].owner_world;
    }
    size_t count() const override { 
        return (size_t)live_count;
    }

    void clear_all() override {
        for (uint32_t i = 0; i < live_count; ++i) data[i].~T();
        live_count = 0;
        next_handle_id = 0;
        memset(gen_map, 0, cap * sizeof(uint32_t));
        memset(sparse_map, 0xFF, cap * sizeof(uint32_t));
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
    Handle add(const T& val) {
        T copy = val;
        return slab<T>().create(std::move(copy), this);
    }

    template<typename T>
    Handle add(T&& val) {
        return slab<std::decay_t<T>>().create(std::forward<T>(val), this);
    }

    template<typename T>
    T* get(Handle h) {
        return slab<T>().resolve(h);
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
                if (s && s->count() > (size_t)index) s->swap_and_pop(index);
            }
            last_processed = index;
        }
        death_row.clear();
    }
};