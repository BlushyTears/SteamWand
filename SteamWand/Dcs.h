#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <vector>
#include <iostream>
#include <cassert>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3>;

inline std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << "Vec2( " << v.x << "," << v.y << " )"; }
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) { return os << "Vec3( " << v.x << "," << v.y << "," << v.z << " )"; }

template<typename T, typename Tuple, size_t I = 0>
constexpr size_t type_index() {
    if constexpr (I >= std::tuple_size_v<Tuple>)
        static_assert(I < std::tuple_size_v<Tuple>, "T not in AtomTypes");
    else if constexpr (std::is_same_v<T, std::tuple_element_t<I, Tuple>>)
        return I;
    else
        return type_index<T, Tuple, I + 1>();
    return size_t(-1);
}

template<typename T>
constexpr uint8_t tag_of() {
    return static_cast<uint8_t>(type_index<T, AtomTypes>() + 1);
}

constexpr uint8_t EMPTY_TAG = 0;

struct AtomBase {
    uint8_t tag = EMPTY_TAG;
};

template<typename T, size_t Capacity>
struct TypedSlab {
    struct Atom {
        uint8_t tag = EMPTY_TAG;
        union {
            T value;
            size_t next_free;
        };

        Atom() : tag(EMPTY_TAG), next_free(0) {}
        ~Atom() {}

        T& get() {
            return value;
        }
        const T& get() const {
            return value;
        }
    };

    TypedSlab() {
        for (size_t i = 0; i < Capacity - 1; ++i) {
            slots[i].next_free = i + 1;
        }
        slots[Capacity - 1].next_free = size_t(-1);
    }

    Atom* create(const T& val) {
        if (next_available == size_t(-1)) return nullptr;

        size_t index = next_available;
        next_available = slots[index].next_free;

        occupied.set(index);
        slots[index].tag = tag_of<T>();
        slots[index].value = val;
        return &slots[index];
    }

    void free(Atom* atom) {
        size_t i = atom - slots.data();
        if (!occupied[i]) return;

        occupied.reset(i);
        slots[i].tag = EMPTY_TAG;
        slots[i].next_free = next_available;
        next_available = i;
    }

    void clear() {
        occupied.reset();
        next_available = 0;
        for (size_t i = 0; i < Capacity - 1; ++i) {
            slots[i].tag = EMPTY_TAG;
            slots[i].next_free = i + 1;
        }
        slots[Capacity - 1].next_free = size_t(-1);
    }

    void pop(size_t n = 1) {
        for (size_t i = Capacity; i-- > 0 && n > 0;) {
            if (occupied[i]) {
                free(&slots[i]);
                --n;
            }
        }
    }

    template<typename Fn>
    void iter(Fn&& fn) {
        for (size_t i = 0; i < Capacity; ++i)
            if (occupied[i])
                fn(slots[i].value);
    }

private:
    std::array<Atom, Capacity> slots{};
    std::bitset<Capacity> occupied{};
    size_t next_available = 0;
};

template<size_t Capacity>
struct World {
    using Slabs = decltype([]<size_t... I>(std::index_sequence<I...>) {
        return std::tuple<TypedSlab<std::tuple_element_t<I, AtomTypes>, Capacity>...>{};
    }(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{}));

    struct AtomProxy {
        World* world;
        AtomBase* atom;

        template<typename T>
        operator T& () {
            assert(atom->tag == tag_of<T>() && "Tag mismatch on get()");
            return world->value_of<T>(atom);
        }

        template<typename T>
        operator const T& () const {
            assert(atom->tag == tag_of<T>() && "Tag mismatch on get()");
            return world->value_of<T>(atom);
        }

        template<typename T>
        AtomProxy& operator=(const T& val) {
            assert(atom->tag == tag_of<T>() && "Tag mismatch on set()");
            world->value_of<T>(atom) = val;
            return *this;
        }
    };

    AtomProxy get(AtomBase* atom) {
        return { this, atom };
    }

    Slabs slabs;

    template<typename T>
    auto& slabFor() {
        return std::get<type_index<T, AtomTypes>()>(slabs);
    }

    template<typename T>
    T& atom_cast(AtomBase* atom) {
        return reinterpret_cast<typename TypedSlab<T, Capacity>::Atom*>(atom)->get();
    }

    template<typename T>
    AtomBase* create(const T& val) {
        return reinterpret_cast<AtomBase*>(slabFor<T>().create(val));
    }

    template<typename T>
    void free(AtomBase* atom) {
        slabFor<T>().free(reinterpret_cast<typename TypedSlab<T, Capacity>::Atom*>(atom));
    }

    template<typename T>
    void pop(size_t n = 1) {
        slabFor<T>().pop(n);
    }

    template<typename T, typename Fn>
    void iter(Fn&& fn) {
        slabFor<T>().iter(std::forward<Fn>(fn));
    }

    template<typename T>
    T& value_of(AtomBase* atom) {
        return atom_cast<T>(atom);
    }

    void free_entity(std::vector<AtomBase*>& entity) {
        for (auto* atom : entity)
            dispatch(atom, [&](auto& v) {
            using T = std::decay_t<decltype(v)>;
            free<T>(atom);
                });
        entity.clear();
    }

    std::vector<AtomBase*> clone_entity(const std::vector<AtomBase*>& entity) {
        std::vector<AtomBase*> result;
        for (auto* atom : entity)
            dispatch(atom, [&](auto& v) { result.push_back(create(v)); });
        return result;
    }

    void print(AtomBase* atom) {
        dispatch(atom, [](auto& v) {
            if constexpr (requires { std::cout << v; })
                std::cout << v;
            });
    }

    template<typename Fn>
    void dispatch(AtomBase* atom, Fn&& fn) {
        [&] <size_t... I>(std::index_sequence<I...>) {
            ((atom->tag == tag_of<std::tuple_element_t<I, AtomTypes>>()
                ? [&] { fn(atom_cast<std::tuple_element_t<I, AtomTypes>>(atom)); return true; }()
                : false) || ...);
        }(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{});
    }
};