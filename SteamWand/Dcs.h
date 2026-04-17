#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <iostream>
#include <malloc.h>
#include <cstring>
#include <algorithm>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
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
    std::vector<ISlab*> registry;
    std::vector<std::unique_ptr<ISlab>> storage;
    std::vector<uint32_t> death_row;

    World(uint32_t capacity = 1024) : cap(capacity) {}

    World(World&& other) noexcept :
        cap(other.cap),
        registry(std::move(other.registry)),
        storage(std::move(other.storage)),
        death_row(std::move(other.death_row)) {
    }

    World& operator=(World&& other) noexcept {
        if (this != &other) {
            cap = other.cap;
            registry = std::move(other.registry);
            storage = std::move(other.storage);
            death_row = std::move(other.death_row);
        }
        return *this;
    }

    template<typename T>
    Slab<T>& get_slab() {
        uint32_t tid = TypeInfo<T>::id();
        if (tid >= registry.size()) {
            registry.resize(tid + 1, nullptr);
        }
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
    size_t size() {
        uint32_t tid = TypeInfo<T>::id();
        return (tid < registry.size() && registry[tid]) ? registry[tid]->count() : 0;
    }

    void queue_free(uint32_t index) {
        death_row.push_back(index);
    }

    void cleanup() {
        if (death_row.empty())
            return;
        for (uint32_t index : death_row) {
            for (ISlab* s : registry) {
                if (s) s->remove_at(index);
            }
        }
        death_row.clear();
    }
};