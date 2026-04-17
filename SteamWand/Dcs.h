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

inline std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    return os << "Vec2(" << v.x << ", " << v.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
}

struct World;
struct ComponentExample {
    std::unique_ptr<World> data;
};

#define ATOM_TYPES(X) \
    X(1, int32_t,  Int32)  \
    X(2, uint32_t, UInt32) \
    X(3, float,    Float)  \
    X(4, Vec2,     Vec2)   \
    X(5, Vec3,     Vec3)   \
    X(6, bool,     Bool)   \
    X(7, char,     Char)   \
    X(8, ComponentExample,   ComponentExample)

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

struct Atom {
    uint32_t index;
    uint16_t entity_id;
    uint8_t generation;
    uint8_t type_id;

    bool valid() const noexcept {
        return type_id != 0;
    }
};

struct ISlab {
    virtual ~ISlab() = default;
    virtual void clear_all() = 0;
    virtual size_t count() const = 0;
    virtual void swap_and_pop(uint32_t index) = 0;
    virtual uint16_t get_eid(uint32_t index) const = 0;
    virtual Atom clone_at(uint32_t index, uint16_t new_eid) = 0;
};

template<typename T>
struct Slab : public ISlab {
    struct Meta {
        uint16_t eid;
        uint8_t gen;
    };

    T* data;
    Meta* meta;
    uint32_t cap;
    uint32_t live_count;

    Slab(uint32_t capacity) : cap(capacity), live_count(0) {
        data = (T*)_aligned_malloc(cap * sizeof(T), 64);
        meta = (Meta*)_aligned_malloc(cap * sizeof(Meta), 64);
        if (meta) memset(meta, 0, cap * sizeof(Meta));
    }

    ~Slab() {
        for (uint32_t i = 0; i < live_count; ++i) {
            data[i].~T();
        }
        _aligned_free(data);
        _aligned_free(meta);
    }

    Atom create(const T& val, uint16_t eid) {
        uint32_t idx = live_count++;
        new (&data[idx]) T(val);
        meta[idx].eid = eid;
        if (meta[idx].gen == 0) meta[idx].gen = 1;
        return { idx, eid, meta[idx].gen, TypeInfo<T>::type_id };
    }

    Atom create(T&& val, uint16_t eid) {
        uint32_t idx = live_count++;
        new (&data[idx]) T(std::move(val));
        meta[idx].eid = eid;
        if (meta[idx].gen == 0) meta[idx].gen = 1;
        return { idx, eid, meta[idx].gen, TypeInfo<T>::type_id };
    }

    void swap_and_pop(uint32_t index) override {
        uint32_t last_idx = --live_count;
        if (index != last_idx) {
            data[index] = std::move(data[last_idx]);
            meta[index].eid = meta[last_idx].eid;
            meta[index].gen++;
        }
        else {
            data[index].~T();
            meta[index].gen++;
        }
    }

    Atom clone_at(uint32_t index, uint16_t new_eid) override {
        if constexpr (std::is_copy_constructible_v<T>) {
            return create(data[index], new_eid);
        }
        return { 0, 0, 0, 0 };
    }

    uint16_t get_eid(uint32_t index) const override {
        return meta[index].eid;
    }

    void clear_all() override {
        for (uint32_t i = 0; i < live_count; ++i) {
            data[i].~T();
        }
        live_count = 0;
    }
    size_t count() const override { return live_count; }
};

struct World {
    uint32_t default_cap;
    uint16_t next_eid = 1;
    std::array<ISlab*, 256> registry{};
    std::vector<std::unique_ptr<ISlab>> owned_storage;
    std::vector<uint32_t> death_row;

    World(uint32_t capacity = 1024) : default_cap(capacity) {
        registry.fill(nullptr);
    }

    World(World&& other) noexcept
        : default_cap(other.default_cap),
        next_eid(other.next_eid),
        registry(other.registry),
        owned_storage(std::move(other.owned_storage)),
        death_row(std::move(other.death_row))
    {
        other.registry.fill(nullptr);
        other.next_eid = 1;
    }

    World& operator=(World&& other) noexcept {
        if (this != &other) {
            owned_storage.clear();
            default_cap = other.default_cap;
            next_eid = other.next_eid;
            registry = other.registry;
            owned_storage = std::move(other.owned_storage);
            death_row = std::move(other.death_row);
            other.registry.fill(nullptr);
            other.next_eid = 1;
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
    Atom create_atom(const T& val, uint16_t eid = 0) {
        if (eid == 0) eid = next_eid++;
        return slab<T>().create(val, eid);
    }

    template<typename T>
    Atom create_atom(T&& val, uint16_t eid = 0) {
        if (eid == 0) eid = next_eid++;
        return slab<T>().create(std::move(val), eid);
    }

    Atom clone_atom(Atom original) {
        if (!original.valid() || !registry[original.type_id]) return { 0, 0, 0, 0 };
        uint16_t new_eid = next_eid++;
        return registry[original.type_id]->clone_at(original.index, new_eid);
    }

    void queue_free(uint32_t index) {
        death_row.push_back(index);
    }

    void cleanup() {
        if (death_row.empty()) return;
        std::sort(death_row.begin(), death_row.end(), std::greater<uint32_t>());
        uint32_t last_processed = 0xFFFFFFFF;
        for (uint32_t index : death_row) {
            if (index == last_processed) continue;
            for (ISlab* s : registry) {
                if (s && s->count() > index) s->swap_and_pop(index);
            }
            last_processed = index;
        }
        death_row.clear();
    }

    template<typename T> T* get_array() { return slab<T>().data; }
    template<typename T> size_t size() { return slab<T>().live_count; }
};

template<auto Key, typename T>
struct Field {
    static constexpr decltype(Key) key = Key;
    using value_type = T;
    T value;
};

template<typename E>
struct EntityHandle {
    static constexpr size_t N = static_cast<size_t>(E::COUNT);
    std::array<Atom, N> atoms{};

    template<typename T>
    T& get(E field, World& world) {
        size_t idx = static_cast<size_t>(field);
        return world.slab<T>().data[atoms[idx].index];
    }

    void free(World& world) {
        for (const auto& atom : atoms) {
            if (atom.valid()) {
                world.queue_free(atom.index);
            }
        }
    }
};

template<typename E, typename... Fs>
EntityHandle<E> create_entity(World& world, Fs&&... fields) {
    EntityHandle<E> h;
    uint16_t eid = world.next_eid++;
    ([&] {
        using F = std::decay_t<Fs>;
        h.atoms[static_cast<size_t>(F::key)] = world.create_atom<typename F::value_type>(std::move(fields.value), eid);
        }(), ...);
    return h;
}

template<typename TargetEnum, typename SourceEnum>
EntityHandle<TargetEnum> clone_entity(const EntityHandle<SourceEnum>& source, World& world) {
    EntityHandle<TargetEnum> target;
    size_t copy_limit = std::min(static_cast<size_t>(TargetEnum::COUNT),
        static_cast<size_t>(SourceEnum::COUNT));
    for (size_t i = 0; i < copy_limit; i++) {
        if (source.atoms[i].valid()) {
            target.atoms[i] = world.clone_atom(source.atoms[i]);
        }
    }
    return target;
}