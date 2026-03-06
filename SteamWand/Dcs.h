#pragma once
#include <vector>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <memory>
#include <algorithm>

// Example structs that can easily be implemented
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

constexpr uint8_t  EMPTY_TAG = 0;
constexpr uint32_t INVALID_ID = UINT32_MAX;

struct AtomBase {
    uint8_t tag = EMPTY_TAG;
};

template<typename T>
struct TypedSlab {
    struct Atom : AtomBase {
        T value{};
    };

    TypedSlab(size_t capacity) : cap(capacity) {
        slots = std::make_unique<Atom[]>(capacity);
        free_indices.reserve(capacity);
        for (size_t i = 0; i < capacity; i++)
            free_indices.push_back(i);
    }

    Atom* create(const T& val) {
        assert(!free_indices.empty() && "Slab at capacity");

        size_t idx = free_indices.back();
        free_indices.pop_back();
        slots[idx].tag = tag_of<T>();
        slots[idx].value = val;

        return &slots[idx];
    }

    void free(Atom* atom) {
        size_t i = atom - slots.get();
        assert(slots[i].tag != EMPTY_TAG && "Double free detected");
        slots[i].tag = EMPTY_TAG;
        free_indices.push_back(i);
    }

    template<typename Fn>
    void iter(Fn&& fn) {
        for (size_t i = 0; i < cap; ++i)
            if (slots[i].tag != EMPTY_TAG) 
                fn(slots[i].value);
    }

    T& get_value(AtomBase* atom) {
        return static_cast<Atom*>(atom)->value;
    }

private:
    size_t cap;
    std::unique_ptr<Atom[]> slots;
    std::vector<size_t> free_indices;
};

template<typename T>
struct ReverseIndex {
    ReverseIndex(size_t max_entities) {
        sparse.assign(max_entities, INVALID_ID);
    }

    void insert(uint32_t entity_id, AtomBase* atom) {
        assert(entity_id < sparse.size());
        assert(sparse[entity_id] == INVALID_ID);

        sparse[entity_id] = static_cast<uint32_t>(dense_entity.size());
        dense_entity.push_back(entity_id);
        dense_atom.push_back(atom);
    }

    void remove(uint32_t entity_id) {
        assert(entity_id < sparse.size());
        uint32_t idx = sparse[entity_id];
        uint32_t last = static_cast<uint32_t>(dense_entity.size()) - 1;

        if (idx != last) {
            dense_entity[idx] = dense_entity[last];
            dense_atom[idx] = dense_atom[last];
            sparse[dense_entity[idx]] = idx;
        }

        dense_entity.pop_back();
        dense_atom.pop_back();
        sparse[entity_id] = INVALID_ID;
    }

    AtomBase* get_atom(uint32_t entity_id) const {
        uint32_t idx = sparse[entity_id];
        if (idx == INVALID_ID) 
            return nullptr;
        return dense_atom[idx];
    }

    bool has(uint32_t entity_id) const {
        return entity_id < sparse.size() && sparse[entity_id] != INVALID_ID;
    }

    template<typename Fn>
    void iter(Fn&& fn) {
        for (size_t i = 0; i < dense_entity.size(); ++i)
            fn(dense_entity[i], dense_atom[i]);
    }

private:
    std::vector<uint32_t>  sparse;
    std::vector<uint32_t>  dense_entity;
    std::vector<AtomBase*> dense_atom;
};

struct World {
    template<typename... Ts>
    static std::tuple<TypedSlab<Ts>...> make_slabs_type(std::tuple<Ts...>);
    using SlabsTuple = decltype(make_slabs_type(std::declval<AtomTypes>()));

    template<typename... Ts>
    static std::tuple<ReverseIndex<Ts>...> make_indices_type(std::tuple<Ts...>);
    using IndicesTuple = decltype(make_indices_type(std::declval<AtomTypes>()));

    World(size_t capacity) {
        max_entities = capacity;
        slabs = std::make_unique<SlabsTuple>(init_slabs(capacity, std::make_index_sequence<std::tuple_size_v<AtomTypes>>{}));
        indices = std::make_unique<IndicesTuple>(init_indices(capacity, std::make_index_sequence<std::tuple_size_v<AtomTypes>>{}));
    }

    uint32_t create_entity() {
        assert(next_entity_id < max_entities && "Entity capacity exceeded");
        return next_entity_id++;
    }

    void destroy_entity(uint32_t entity_id) {
        destroy_components<0>(entity_id);
    }

    template<typename T>
    AtomBase* add_component(uint32_t entity_id, const T& val) {
        AtomBase* atom = slabFor<T>().create(val);
        indexFor<T>().insert(entity_id, atom);
        return atom;
    }

    template<typename T>
    void remove_component(uint32_t entity_id) {
        auto& idx = indexFor<T>();
        auto* atom = idx.get_atom(entity_id);
        assert(atom && "entity does not have this component");

        idx.remove(entity_id);
        slabFor<T>().free(reinterpret_cast<typename TypedSlab<T>::Atom*>(atom));
    }

    template<typename T, typename Fn>
    void iter(Fn&& fn) {
        slabFor<T>().iter(std::forward<Fn>(fn));
    }

    template<typename T, typename Fn>
    void iter_with_entity(Fn&& fn) {
        indexFor<T>().iter([&](uint32_t entity_id, AtomBase* atom) {
            fn(entity_id, slabFor<T>().get_value(atom));
        });
    }

    template<typename T>
    bool has_component(uint32_t entity_id) {
        return indexFor<T>().has(entity_id);
    }

    template<typename T>
    T* get_component(uint32_t entity_id) {
        AtomBase* atom = indexFor<T>().get_atom(entity_id);
        if (!atom)
            return nullptr;
        return &slabFor<T>().get_value(atom);
    }

    template<typename T>
    struct Handle {
        AtomBase* atom;
        World* world;

        T& operator*() {
            return world->value_of<T>(atom);
        }

        T* operator->() {
            return &world->value_of<T>(atom);
        }

        Handle& operator=(const T& val) {
            world->value_of<T>(atom) = val;
            return *this;
        }
    };

    template<typename T>
    Handle<T> get_handle(uint32_t entity_id) {
        AtomBase* atom = indexFor<T>().get_atom(entity_id);
        assert(atom && "entity does not have this component");
        return {atom, this};
    }

    template<typename Fn>
    void dispatch(AtomBase* atom, Fn&& fn) {
        [&] <size_t... I>(std::index_sequence<I...>) {
            ((atom->tag == tag_of<std::tuple_element_t<I, AtomTypes>>()
                ? [&] { fn(value_of<std::tuple_element_t<I, AtomTypes>>(atom)); return true; }()
                : false) || ...);
        }(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{});
    }

private:
    template<typename T>
    auto& slabFor() {
        return std::get<TypedSlab<T>>(*slabs);
    }

    template<typename T>
    auto& indexFor() {
        return std::get<ReverseIndex<T>>(*indices);
    }

    template<typename T>
    T& value_of(AtomBase* atom) {
        return slabFor<T>().get_value(atom);
    }

    template<typename T>
    void remove_atom(uint32_t entity_id) {
        auto& idx = indexFor<T>();
        auto* atom = idx.get_atom(entity_id);
        idx.remove(entity_id);
        slabFor<T>().free(reinterpret_cast<typename TypedSlab<T>::Atom*>(atom));
    }

    template<size_t I = 0>
    void destroy_components(uint32_t entity_id) {
        if constexpr (I < std::tuple_size_v<AtomTypes>) {
            using T = std::tuple_element_t<I, AtomTypes>;
            if (indexFor<T>().has(entity_id))
                remove_atom<T>(entity_id);
            destroy_components<I + 1>(entity_id);
        }
    }

    template<size_t... I>
    static SlabsTuple init_slabs(size_t cap, std::index_sequence<I...>) {
        return std::make_tuple(TypedSlab<std::tuple_element_t<I, AtomTypes>>(cap)...);
    }

    template<size_t... I>
    static IndicesTuple init_indices(size_t cap, std::index_sequence<I...>) {
        return std::make_tuple(ReverseIndex<std::tuple_element_t<I, AtomTypes>>(cap)...);
    }

    uint32_t next_entity_id = 0;
    size_t max_entities = 0;

    std::unique_ptr<SlabsTuple> slabs;
    std::unique_ptr<IndicesTuple> indices;
};
