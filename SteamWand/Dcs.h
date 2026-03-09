#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <algorithm>
#include <type_traits>

// =============================================================
// Custom types defined here alongside how you want them printed
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
// To register a new type: Add one line to ATOM_TYPES. 
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
    static constexpr uint8_t type_id = uint8_t(tid); \
    static constexpr size_t size = sizeof(ctype); \
};

ATOM_TYPES(DEFINE_TYPE_INFO)
#undef DEFINE_TYPE_INFO

struct Atom {
    uint32_t index;
    uint16_t entity_id;
    uint8_t generation;
    uint8_t type_id;

    bool valid() const noexcept {
        return type_id != EMPTY_TYPE;
    }

    bool operator==(const Atom& other) const noexcept {
        return index == other.index
            && generation == other.generation
            && type_id == other.type_id
            && entity_id == other.entity_id;
    }

    bool operator!=(const Atom& other) const noexcept {
        return !(*this == other);
    }
};

template<typename T>
struct Slab {
    Slab(uint32_t capacity) noexcept
        : slots((Slot*)_aligned_malloc(capacity * sizeof(Slot), 64))
        , cap(capacity)
        , count(0)
        , free_head(0)
        , type_id(TypeInfo<T>::type_id) {
        for (uint32_t i = 0; i < cap - 1; i++) {
            slots[i].next_free = i + 1;
            slots[i].generation = 0;
            slots[i].entity_id = 0;
        }

        slots[cap - 1].next_free = UINT32_MAX;
        slots[cap - 1].generation = 0;
        slots[cap - 1].entity_id = 0;
    }

    ~Slab() { 
        clear_all();
    }

    Slab(const Slab&) = delete;
    Slab(Slab&&) = delete;
    Slab& operator=(const Slab&) = delete;
    Slab& operator=(Slab&&) = delete;

    Atom create(const T& value, uint16_t entity_id = 0) noexcept {
        assert(free_head != UINT32_MAX && "Slab at capacity");

        uint32_t idx = free_head;
        Slot& s = slots[idx];

        free_head = s.next_free;
        std::construct_at(&s.data, value);
        s.generation += 1;
        s.entity_id = entity_id;
        count += 1;

        return Atom{ idx, entity_id, s.generation, type_id };
    }

    void free(Atom id) noexcept {
        assert(id.index < cap);
        assert(slots[id.index].generation != 0);
        assert(id.generation == slots[id.index].generation);
        assert(id.type_id == type_id);

        slots[id.index].data.~T();
        slots[id.index].next_free = free_head;
        free_head = id.index;
        count -= 1;
    }

    bool valid(Atom id) const noexcept {
        return id.index < cap
            && id.type_id == type_id
            && slots[id.index].generation != 0
            && id.generation == slots[id.index].generation;
    }

    T& get(Atom id) noexcept {
        assert(valid(id) && "Accessing invalid Atom");
        return slots[id.index].data;
    }

    const T& get(Atom id) const noexcept {
        assert(valid(id) && "Accessing invalid Atom");
        return slots[id.index].data;
    }

    void clear_all() noexcept {
        for (uint32_t i = 0; i < cap; ++i) {
            if (slots[i].generation != 0) {
                slots[i].data.~T();
                slots[i].next_free = free_head;
                free_head = i;
                slots[i].generation = 0;
            }
        }
        count = 0;
    }

    T* get_array() noexcept {
        return &slots[0].data;
    }
    const T* get_array() const noexcept {
        return &slots[0].data;
    }

    size_t size() const noexcept {
        return count;
    }
    uint32_t capacity() const noexcept {
        return cap;
    }

private:
    struct Slot {
        T data;
        uint32_t next_free;
        uint16_t entity_id;
        uint8_t generation;
    };

    struct AlignedDeleter {
        void operator()(void* p) const noexcept {
            _aligned_free(p);
        }
    };

    std::unique_ptr<Slot[], AlignedDeleter> slots;
    uint32_t cap;
    uint32_t count;
    uint32_t free_head;
    uint8_t type_id;
};

struct World {
    World(uint32_t capacity = 1024) noexcept {
#define X(tid, ctype, name) \
            std::get<tid - 1>(slabs) = std::make_unique<Slab<ctype>>(capacity);
        ATOM_TYPES(X)
#undef X
    }

    template<typename T>
    Atom create_atom(const T& value, uint16_t entity_id = 0) noexcept {
        return slab<T>().create(value, entity_id);
    }

    template<typename T>
    T& get(Atom id) noexcept {
        return slab<T>().get(id);
    }

    template<typename T>
    void free(Atom id) noexcept {
        if (valid<T>(id))
            slab<T>().free(id);
    }

    void free(Atom id) noexcept {
        if (!id.valid())
            return;
        switch (id.type_id) {
#define X(tid, ctype, name) \
            case tid: \
                if (slab_ptr<ctype>()) { \
                    slab_ptr<ctype>()->free(id); \
                } \
            break;
            ATOM_TYPES(X)
#undef X
        default:
            break;
        }
    }

    template<typename T>
    bool valid(Atom id) const noexcept {
        return slab<T>().valid(id);
    }

    bool valid(Atom id) const noexcept {
        if (id.type_id == 0 || id.type_id > ATOM_TYPE_MAX_ID)
            return false;

        switch (id.type_id) {
#define X(tid, ctype, name) \
            case tid: \
                return slab_ptr<ctype>() && slab_ptr<ctype>()->valid(id);
            ATOM_TYPES(X)
        default:
            return false;
        }
    }

    template<typename T>
    T* get_array() noexcept {
        return slab<T>().get_array();
    }

    template<typename T>
    const T* get_array() const noexcept {
        return slab<T>().get_array();
    }

    template<typename T>
    size_t size() const noexcept {
        return slab<T>().size();
    }

    std::vector<Atom> clone_entity(const std::vector<Atom>& entity) {
        std::vector<Atom> result;
        result.reserve(entity.size());

        for (Atom id : entity) {
            dispatch(id, [&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                result.push_back(create_atom<T>(v));
            });
        }
        return result;
    }

    void free_entity(const std::vector<Atom>& entity) noexcept {
        for (Atom id : entity)
            free(id);
    }

    uint16_t entity_id(Atom id) const noexcept {
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
    void clear_all() noexcept {
        slab<T>().clear_all();
    }

    void clear_world() noexcept {
#define X(tid, ctype, name) if (slab_ptr<ctype>()) slab_ptr<ctype>()->clear_all();
        ATOM_TYPES(X)
#undef X
    }

    template<typename Fn>
    void dispatch(Atom id, Fn&& fn) {
        if (!valid(id))
            return;
        switch (id.type_id) {
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
    Slab<T>& slab() noexcept {
        return *std::get<TypeInfo<T>::type_id - 1>(slabs);
    }

    template<typename T>
    const Slab<T>& slab() const noexcept {
        return *std::get<TypeInfo<T>::type_id - 1>(slabs);
    }

    template<typename T>
    Slab<T>* slab_ptr() noexcept {
        return std::get<TypeInfo<T>::type_id - 1>(slabs).get();
    }

    template<typename T>
    const Slab<T>* slab_ptr() const noexcept {
        return std::get<TypeInfo<T>::type_id - 1>(slabs).get();
    }
};