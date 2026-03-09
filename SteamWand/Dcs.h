#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <algorithm>
#include <type_traits>
#include <malloc.h>

// =============================================================
// Basic Types
// =============================================================

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

inline std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    return os << "Vec2(" << v.x << ", " << v.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
}

// =============================================================
// Type System - Single source of truth using X macro
// To add a new type: Add one line to ATOM_TYPES.
// =============================================================

#define ATOM_TYPES(X) \
    X(1, int32_t, Int32) \
    X(2, uint32_t, UInt32) \
    X(3, float, Float) \
    X(4, Vec2, Vec2) \
    X(5, Vec3, Vec3) \
    X(6, bool, Bool) \
    X(7, char, Char)

#define ATOM_TYPE_MAX_ID 7

enum class AtomType : uint8_t {
    Empty = 0,
#define X(tid, ctype, name) name = tid,
    ATOM_TYPES(X)
#undef X
    Count
};

constexpr uint8_t EMPTY_TYPE = 0;

template<typename T> struct TypeInfo;

#define DEFINE_TYPE_INFO(tid, ctype, name) \
template<> struct TypeInfo<ctype> { \
    static constexpr AtomType type = AtomType::name; \
    static constexpr uint8_t type_id = static_cast<uint8_t>(tid); \
    static constexpr size_t size = sizeof(ctype); \
};

ATOM_TYPES(DEFINE_TYPE_INFO)
#undef DEFINE_TYPE_INFO

struct Atom {
    uint32_t index;
    uint8_t  generation;
    uint8_t  type_id;
    uint16_t entity_id;

    bool valid() const { return type_id != EMPTY_TYPE; }

    bool operator==(const Atom& other) const {
        return index == other.index &&
            generation == other.generation &&
            type_id == other.type_id &&
            entity_id == other.entity_id;
    }

    bool operator!=(const Atom& other) const { 
        return !(*this == other);
    }
};

template<typename T>
class Slab {
public:
    Slab(uint32_t capacity)
        : cap(capacity)
        , count(0)
        , type_id(TypeInfo<T>::type_id) {

        data = static_cast<T*>(_aligned_malloc(capacity * sizeof(T), 64));
        assert(data && "Aligned allocation failed");

        generations = std::make_unique<uint8_t[]>(capacity);
        entity_ids = std::make_unique<uint16_t[]>(capacity);

        free_list.reserve(capacity);

        for (uint32_t i = 0; i < capacity; i++) {
            generations[i] = 0;
            entity_ids[i] = 0;
            free_list.push_back(i);
        }
    }

    ~Slab() {
        if (data) _aligned_free(data);
    }

    Slab(const Slab&) = delete;
    Slab(Slab&&) = delete;
    Slab& operator=(const Slab&) = delete;
    Slab& operator=(Slab&&) = delete;

    Atom create(const T& value, uint16_t entity_id = 0) {
        assert(!free_list.empty() && "Slab at capacity");

        uint32_t idx = free_list.back();
        free_list.pop_back();

        data[idx] = value;
        generations[idx] += 1;
        entity_ids[idx] = entity_id;
        count += 1;

        return Atom{ idx, generations[idx], type_id, entity_id };
    }

    void free(Atom id) {
        assert(id.index < cap);
        assert(generations[id.index] != 0);
        assert(id.generation == generations[id.index]);
        assert(id.type_id == type_id);

        generations[id.index] = 0;
        free_list.push_back(id.index);
        count -= 1;
    }

    bool valid(Atom id) const {
        return id.index < cap && id.type_id == type_id
            && generations[id.index] != 0 
            && id.generation == generations[id.index];
    }

    T& get(Atom id) {
        assert(valid(id) && "Accessing invalid Atom");
        return data[id.index];
    }

    const T& get(Atom id) const {
        assert(valid(id) && "Accessing invalid Atom");
        return data[id.index];
    }

    void clear() {
        for (uint32_t i = 0; i < cap; ++i) {
            if (generations[i] != 0) {
                generations[i] = 0;
                free_list.push_back(i);
            }
        }
        count = 0;
    }

    T* raw() { 
        return data;
    }
    const T* raw() const { 
        return data;
    }

    size_t size() const { 
        return count;
    }
    uint32_t capacity() const { 
        return cap;
    }

private:
    uint32_t cap;
    size_t count;
    uint8_t type_id;

    T* data = nullptr;

    std::unique_ptr<uint8_t[]> generations;
    std::unique_ptr<uint16_t[]> entity_ids;
    std::vector<uint32_t> free_list;
};

// =============================================================
// World - Container for all slabs optimized for cache locality
// =============================================================

class World {
public:
    World(uint32_t capacity = 1024) {
#define X(tid, ctype, name) \
        std::get<tid - 1>(slabs) = std::make_unique<Slab<ctype>>(capacity);
        ATOM_TYPES(X)
#undef X
    }

    template<typename T>
    Atom create(const T& value, uint16_t entity_id = 0) {
        return slab<T>().create(value, entity_id);
    }

    template<typename T>
    T& get(Atom id) {
        return slab<T>().get(id);
    }

    template<typename T>
    const T& get(Atom id) const {
        return slab<T>().get(id);
    }

    template<typename T>
    void free(Atom id) {
        if (valid<T>(id)) slab<T>().free(id);
    }

    void free(Atom id) {
        if (!id.valid()) return;
        switch (id.type_id) {
#define X(tid, ctype, name) \
            case tid: if (slab_ptr<ctype>()) slab_ptr<ctype>()->free(id); break;
            ATOM_TYPES(X)
#undef X
        default: break;
        }
    }

    template<typename T>
    bool valid(Atom id) const {
        return slab<T>().valid(id);
    }

    bool valid(Atom id) const {
        if (id.type_id == 0 || id.type_id > ATOM_TYPE_MAX_ID) return false;
        switch (id.type_id) {
#define X(tid, ctype, name) \
            case tid: return slab_ptr<ctype>() && slab_ptr<ctype>()->valid(id);
            ATOM_TYPES(X)
#undef X
        default: return false;
        }
    }

    template<typename T>
    T* raw() { 
        return slab<T>().raw();
    }

    template<typename T>
    const T* raw() const { 
        return slab<T>().raw();
    }

    template<typename T>
    size_t count() const {
        return slab<T>().size();
    }

    template<typename T>
    struct View {
        T* data;
        size_t count;

        T* begin() { 
            return data;
        }
        T* end() { 
            return data + count;
        }
        const T* begin() const { 
            return data;
        }
        const T* end() const { 
            return data + count;
        }
    };

    template<typename T>
    View<T> view() { 
        return { raw<T>(), count<T>() };
    }

    template<typename T>
    View<const T> view() const { 
        return { raw<T>(), count<T>() };
    }

    std::vector<Atom> clone_entity(const std::vector<Atom>& entity) {
        std::vector<Atom> result;
        result.reserve(entity.size());

        for (Atom id : entity) {
            dispatch(id, [&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                result.push_back(create<T>(v));
                });
        }

        return result;
    }

    void free_entity(const std::vector<Atom>& entity) {
        for (Atom id : entity) free(id);
    }

    uint16_t entity_id(Atom id) const { 
        return id.entity_id;
    }

    void print(Atom id) {
        if (!valid(id)) 
            return;
        dispatch(id, [](auto& v) { 
            std::cout << v << "\n";
            });
    }

    template<typename T>
    void clear() { slab<T>().clear(); }

    void clear_all() {
#define X(tid, ctype, name) if (slab_ptr<ctype>()) slab_ptr<ctype>()->clear();
        ATOM_TYPES(X)
#undef X
    }

    size_t total_atoms() const {
        size_t total = 0;
#define X(tid, ctype, name) if (slab_ptr<ctype>()) total += slab_ptr<ctype>()->size();
        ATOM_TYPES(X)
#undef X
            return total;
    }

    template<typename Fn>
    void dispatch(Atom id, Fn&& fn)
    {
        if (!valid(id)) 
            return;
        switch (id.type_id)
        {
#define X(tid, ctype, name) \
            case tid: \
                fn(slab_ptr<ctype>()->get(id)); \
                break;

            ATOM_TYPES(X)
#undef X
        default:
            break;
        }
    }

    template<typename Fn>
    void dispatch(Atom id, Fn&& fn) const
    {
        if (!valid(id)) 
            return;
        switch (id.type_id)
        {
#define X(tid, ctype, name) \
            case tid: \
                fn(slab_ptr<ctype>()->get(id)); \
                break;

            ATOM_TYPES(X)
#undef X
        default:
            break;
        }
    }

private:

    std::tuple<
#define X(tid, ctype, name) std::unique_ptr<Slab<ctype>>,
        ATOM_TYPES(X)
#undef X
        std::monostate> slabs;

    template<typename T>
    Slab<T>& slab() {
        return *std::get<TypeInfo<T>::type_id - 1>(slabs);
    }

    template<typename T>
    const Slab<T>& slab() const {
        return *std::get<TypeInfo<T>::type_id - 1>(slabs);
    }

    template<typename T>
    Slab<T>* slab_ptr() {
        return std::get<TypeInfo<T>::type_id - 1>(slabs).get();
    }

    template<typename T>
    const Slab<T>* slab_ptr() const {
        return std::get<TypeInfo<T>::type_id - 1>(slabs).get();
    }
};