#pragma once
#include <vector>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <memory>
#include <algorithm>

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

struct AtomBase {
    uint8_t tag = 0;
};

template<typename T>
struct TypedSlab {
    struct Atom : AtomBase {
        T value{};
        T& get() { 
            return value;
        }
    };

    TypedSlab(size_t capacity) : cap(capacity) {
        slots = std::make_unique<Atom[]>(capacity);
        occupied.assign(capacity, 0); // Using vector<uint8_t> as a fast bool array
        free_indices.reserve(capacity);
        for (size_t i = capacity; i-- > 0; ) {
            free_indices.push_back(i);
        }
    }

    Atom* create(const T& val) {
        if (free_indices.empty()) return nullptr;
        size_t idx = free_indices.back();
        free_indices.pop_back();
        slots[idx].tag = tag_of<T>();
        slots[idx].value = val;
        occupied[idx] = 1;
        return &slots[idx];
    }

    void free(Atom* atom) {
        size_t i = atom - slots.get();
        if (occupied[i]) {
            occupied[i] = 0;
            free_indices.push_back(i);
        }
    }

    template<typename Fn>
    void iter(Fn&& fn) {
        for (size_t i = 0; i < cap; ++i)
            if (occupied[i]) fn(slots[i].value);
    }

    T& get_value(AtomBase* atom) {
        return static_cast<Atom*>(atom)->get();
    }

private:
    size_t cap;
    std::unique_ptr<Atom[]> slots;
    std::vector<uint8_t> occupied;
    std::vector<size_t> free_indices;
};

struct World {
    // Helper to generate the tuple type automatically from AtomTypes
    template<typename... Ts>
    static std::tuple<TypedSlab<Ts>...> make_slabs_type(std::tuple<Ts...>);
    using SlabsTuple = decltype(make_slabs_type(std::declval<AtomTypes>()));

    World(size_t capacity) : slabs(init_slabs(capacity, std::make_index_sequence<std::tuple_size_v<AtomTypes>>{})) {}

    template<typename T>
    auto& slabFor() {
        return std::get<TypedSlab<T>>(slabs);
    }

    template<typename T>
    AtomBase* create(const T& val) {
        return reinterpret_cast<AtomBase*>(slabFor<T>().create(val));
    }

    template<typename T>
    void free(AtomBase* atom) {
        slabFor<T>().free(reinterpret_cast<typename TypedSlab<T>::Atom*>(atom));
    }

    template<typename T, typename Fn>
    void iter(Fn&& fn) {
        slabFor<T>().iter(std::forward<Fn>(fn));
    }

    template<typename T>
    T& value_of(AtomBase* atom) {
        return slabFor<T>().get_value(atom);
    }

    struct AtomProxy {
        World* world;
        AtomBase* atom;
        template<typename T> operator T& () { 
            return world->value_of<T>(atom);
        }
        template<typename T> AtomProxy& operator=(const T& val) { 
            world->value_of<T>(atom) = val;
            return *this;
        }
    };

    AtomProxy get(AtomBase* atom) { 
        return { this, atom };
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

    // clanker goes brr brr
    template<typename Fn>
    void dispatch(AtomBase* atom, Fn&& fn) {
        [&] <size_t... I>(std::index_sequence<I...>) {
            ((atom->tag == tag_of<std::tuple_element_t<I, AtomTypes>>()
                ? [&] { fn(value_of<std::tuple_element_t<I, AtomTypes>>(atom)); return true; }()
                : false) || ...);
        }(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{});
    }

private:
    template<size_t... I>
    static SlabsTuple init_slabs(size_t cap, std::index_sequence<I...>) {
        return std::make_tuple(TypedSlab<std::tuple_element_t<I, AtomTypes>>(cap)...);
    }

    SlabsTuple slabs;
};