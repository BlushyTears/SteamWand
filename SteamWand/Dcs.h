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

using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3, bool>;

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
constexpr uint8_t tag_of() noexcept {
    return static_cast<uint8_t>(type_index<T, AtomTypes>() + 1);
}

constexpr uint8_t  EMPTY_TAG = 0;
constexpr uint32_t INVALID_ID = UINT32_MAX;

struct AtomBase {
    uint32_t index;
    uint8_t  tag;
    uint8_t  generation;
    uint16_t entity_id;
};

template<typename T>
struct TypedSlab {
    using value_type = T;

    TypedSlab(uint32_t capacity) : cap(capacity) {
        values = std::make_unique<T[]>(capacity);
        tags = std::make_unique<uint8_t[]>(capacity);
        handles = std::make_unique<AtomBase[]>(capacity);
        generations = std::make_unique<uint8_t[]>(capacity);
        free_indices.reserve(capacity);
        for (uint32_t i = 0; i < capacity; i++) {
            tags[i] = EMPTY_TAG;
            generations[i] = 0;
            handles[i].index = i;
            handles[i].tag = tag_of<T>();
            handles[i].generation = 0;
            handles[i].entity_id = 0;
            free_indices.emplace_back(i);
        }
    }

    AtomBase* create(const T& val, uint16_t entity_id = 0) noexcept {
        assert(!free_indices.empty() && "Slab at capacity");

        uint32_t idx = free_indices.back();
        free_indices.pop_back();
        values[idx] = val;
        tags[idx] = tag_of<T>();
        generations[idx]++;

        handles[idx].index = idx;
        handles[idx].tag = tag_of<T>();
        handles[idx].generation = generations[idx];
        handles[idx].entity_id = entity_id;
        return &handles[idx];
    }

    void free(AtomBase* atom) noexcept {
        assert(tags[atom->index] != EMPTY_TAG && "Double free detected");
        assert(atom->generation == generations[atom->index] && "Stale handle detected");

        tags[atom->index] = EMPTY_TAG;
        free_indices.emplace_back(atom->index);
    }

    template<typename Fn>
    void iter(Fn&& fn) const noexcept {
        for (uint32_t i = 0; i < cap; ++i)
            if (tags[i] != EMPTY_TAG)
                fn(values[i]);
    }

    void clear() noexcept {
        for (uint32_t i = 0; i < cap; ++i) {
            if (tags[i] != EMPTY_TAG) {
                tags[i] = EMPTY_TAG;
                generations[i]++;
                free_indices.emplace_back(i);
            }
        }
    }

    bool valid(AtomBase* atom) const noexcept {
        return atom->index < cap &&
            tags[atom->index] != EMPTY_TAG &&
            atom->generation == generations[atom->index];
    }

    T& get_value(AtomBase* atom) noexcept {
        assert(valid(atom) && "Accessing invalid atom");
        return values[atom->index];
    }

    const T& get_value(AtomBase* atom) const noexcept {
        assert(valid(atom) && "Accessing invalid atom");
        return values[atom->index];
    }

    uint32_t index_of(AtomBase* atom) const noexcept {
        assert(valid(atom) && "Invalid atom");
        return atom->index;
    }

    uint16_t entity_id_of(AtomBase* atom) const noexcept {
        assert(valid(atom) && "Invalid atom");
        return atom->entity_id;
    }

    T* raw() const noexcept {
        return values.get();
    }

    size_t allocated_count() const noexcept {
        return cap - free_indices.size();
    }

    void reserve(size_t additional) noexcept {
        free_indices.reserve(free_indices.size() + additional);
    }

private:
    uint32_t cap;
    std::unique_ptr<T[]>        values;
    std::unique_ptr<uint8_t[]>  tags;
    std::unique_ptr<uint8_t[]>  generations;
    std::unique_ptr<AtomBase[]> handles;
    std::vector<uint32_t>       free_indices;
};

template<typename T>
struct ReverseIndex {
    ReverseIndex(size_t max_entities) {
        sparse.assign(max_entities, INVALID_ID);
    }

    void insert(uint32_t entity_id, AtomBase* atom) noexcept {
        assert(entity_id < sparse.size());
        assert(sparse[entity_id] == INVALID_ID);

        sparse[entity_id] = static_cast<uint32_t>(dense_entity.size());
        dense_entity.push_back(entity_id);
        dense_atom.push_back(atom);
    }

    void remove(uint32_t entity_id) noexcept {
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

    AtomBase* get_atom(uint32_t entity_id) const noexcept {
        uint32_t idx = sparse[entity_id];
        if (idx == INVALID_ID)
            return nullptr;
        return dense_atom[idx];
    }

    bool has(uint32_t entity_id) const noexcept {
        return entity_id < sparse.size() && sparse[entity_id] != INVALID_ID;
    }

    template<typename Fn>
    void iter(Fn&& fn) const noexcept {
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

    World(uint32_t capacity) {
        slabs = std::make_unique<SlabsTuple>(init_slabs(capacity, std::make_index_sequence<std::tuple_size_v<AtomTypes>>{}));
    }

    template<typename T>
    AtomBase* create(const T& val, uint16_t entity_id = 0) noexcept {
        return slabFor<T>().create(val, entity_id);
    }

    template<typename T>
    void free(AtomBase* atom) noexcept {
        if (valid<T>(atom))
            slabFor<T>().free(atom);
    }

    template<typename T, typename Fn>
    void iter(Fn&& fn) const noexcept {
        slabFor<T>().iter(std::forward<Fn>(fn));
    }

    template<typename T>
    bool valid(AtomBase* atom) const noexcept {
        return slabFor<T>().valid(atom);
    }

    template<typename T>
    T& value_of(AtomBase* atom) noexcept {
        return slabFor<T>().get_value(atom);
    }

    template<typename T>
    const T& value_of(AtomBase* atom) const noexcept {
        return slabFor<T>().get_value(atom);
    }

    template<typename T>
    uint32_t index_of(AtomBase* atom) const noexcept {
        return slabFor<T>().index_of(atom);
    }

    template<typename T>
    uint16_t entity_id_of(AtomBase* atom) const noexcept {
        return slabFor<T>().entity_id_of(atom);
    }

    template<typename T>
    T* raw() const noexcept {
        return slabFor<T>().raw();
    }

    template<typename T>
    void clear() noexcept {
        slabFor<T>().clear();
    }

    void print(AtomBase* atom) const {
        dispatch(atom, [](auto& v) {
            std::cout << v << "\n";
            });
    }

    std::vector<AtomBase*> clone_entity(const std::vector<AtomBase*>& entity) {
        std::vector<AtomBase*> result;
        result.reserve(entity.size());
        for (auto* atom : entity)
            dispatch(atom, [&](auto& v) { result.push_back(create(v)); });
        return result;
    }

    void free_entity(std::vector<AtomBase*>& entity) noexcept {
        for (auto* atom : entity)
            dispatch(atom, [&](auto& v) {
            using T = std::decay_t<decltype(v)>;
            free<T>(atom);
                });
        entity.clear();
    }

    template<typename T>
    struct Handle {
        AtomBase* atom;
        World* world;

        T& operator*() const noexcept {
            return world->value_of<T>(atom);
        }

        T* operator->() const noexcept {
            return &world->value_of<T>(atom);
        }

        Handle& operator=(const T& val) noexcept {
            world->value_of<T>(atom) = val;
            return *this;
        }

        bool valid() const noexcept {
            return world->valid<T>(atom);
        }

        uint16_t entity_id() const noexcept {
            return world->entity_id_of<T>(atom);
        }
    };

    template<typename T>
    Handle<T> get_handle(AtomBase* atom) const noexcept {
        return { atom, const_cast<World*>(this) };
    }

    template<typename Fn>
    void dispatch(AtomBase* atom, Fn&& fn) const {
        if (!atom) return;
        [&] <size_t... I>(std::index_sequence<I...>) {
            ((atom->tag == tag_of<std::tuple_element_t<I, AtomTypes>>()
                ? [&] { fn(value_of<std::tuple_element_t<I, AtomTypes>>(atom)); return true; }()
                : false) || ...);
        }(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{});
    }

    template<typename T>
    auto& slabFor() const noexcept {
        return std::get<TypedSlab<T>>(*slabs);
    }

    template<typename... Ts, typename Fn>
    void query_parallel(Fn&& fn) const noexcept {
        size_t count = (std::min)({ slabFor<Ts>().allocated_count()... });
        [&] <size_t... I>(std::index_sequence<I...>) {
            for (size_t i = 0; i < count; i++) {
                fn(slabFor<std::tuple_element_t<I, std::tuple<Ts...>>>().raw()[i]...);
            }
        }(std::index_sequence_for<Ts...>{});
    }

private:
    template<size_t... I>
    SlabsTuple init_slabs(uint32_t cap, std::index_sequence<I...>) {
        return std::make_tuple(TypedSlab<std::tuple_element_t<I, AtomTypes>>(cap)...);
    }

    std::unique_ptr<SlabsTuple> slabs;
};