#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <tuple>
#include <type_traits>

#include <vector>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

using AtomTypes = std::tuple<int32_t, float, Vec2, Vec3, int>;

template<typename T, typename Tuple, size_t I = 0>
constexpr size_t type_index() {
    if constexpr (I >= std::tuple_size_v<Tuple>) {
        static_assert(I < std::tuple_size_v<Tuple>, "T not in type list");
        return -1;
    }
    else if constexpr (std::is_same_v<T, std::tuple_element_t<I, Tuple>>) {
        return I;
    }
    else {
        return type_index<T, Tuple, I + 1>();
    }
}

template<typename T>
constexpr uint8_t tag_of() {
    return static_cast<uint8_t>(type_index<T, AtomTypes>() + 1);
}

constexpr uint8_t EMPTY_TAG = 0;
// vibe coded, but basically just calculates ideal size (By default i was hard coding it to 18 which isn't ideal)
constexpr size_t ATOM_DATA_SIZE = []<size_t... I>(std::index_sequence<I...>) {
    return std::max({ sizeof(std::tuple_element_t<I, AtomTypes>)... });
}(std::make_index_sequence<std::tuple_size_v<AtomTypes>>{});

struct AtomData {
    alignas(16) std::byte buf[ATOM_DATA_SIZE];

    template<typename T>
    void write(const T& val) {
        static_assert(sizeof(T) <= ATOM_DATA_SIZE);
        std::memcpy(buf, &val, sizeof(T));
    }

    template<typename T>
    T& read() {
        static_assert(sizeof(T) <= ATOM_DATA_SIZE);
        return *reinterpret_cast<T*>(buf);
    }

    template<typename T>
    const T& read() const {
        static_assert(sizeof(T) <= ATOM_DATA_SIZE);
        return *reinterpret_cast<const T*>(buf);
    }
};

template<size_t Capacity>
struct Slab {
    struct Atom {
        uint8_t tag = EMPTY_TAG;
        AtomData data{};

        template<typename T>
        T& get() { 
            return data.read<T>();
        }

        template<typename T>
        const T& get() const { 
            return data.read<T>();
        }
    };

    template<typename T>
    Atom* create(const T& value) {
        for (size_t i = 0; i < Capacity; ++i) {
            if (!occupied[i]) {
                occupied.set(i);
                slots[i].tag = tag_of<T>();
                slots[i].data.write(value);
                active[active_count++] = i;
                return &slots[i];
            }
        }
        return nullptr;
    }

    void free(Atom* atom) {
        size_t i = atom - slots.data();
        occupied.reset(i);
        slots[i].tag = EMPTY_TAG;
        for (size_t j = 0; j < active_count; ++j) {
            if (active[j] == i) {
                active[j] = active[--active_count];
                return;
            }
        }
    }

    // Arena allocator-style free
    void clear() {
        occupied.reset();
        active_count = 0;
    }

    template<typename T, typename Fn>
    void iter(Fn&& fn) {
        constexpr uint8_t filter = tag_of<T>();
        for (size_t j = 0; j < active_count; ++j) {
            auto& slot = slots[active[j]];
            if (slot.tag == filter)
                fn(slot.template get<T>());
        }
    }

private:
    std::array<Atom, Capacity> slots;
    std::bitset<Capacity> occupied;
    std::array<size_t, Capacity> active{};
    size_t active_count = 0;
};

// Ugly, vibecoded functions to copy either atoms or entities
template<size_t Capacity, size_t... I>
typename Slab<Capacity>::Atom* clone_atom(Slab<Capacity>& slab, typename Slab<Capacity>::Atom* atom, std::index_sequence<I...>) {
    typename Slab<Capacity>::Atom* result = nullptr;
    ((atom->tag == tag_of<std::tuple_element_t<I, AtomTypes>>()
        ? (result = slab.create(atom->get<std::tuple_element_t<I, AtomTypes>>()), true)
        : false) || ...);
    return result;
}

template<size_t Capacity>
std::vector<typename Slab<Capacity>::Atom*> clone_entity(Slab<Capacity>& slab, const std::vector<typename Slab<Capacity>::Atom*>& entity) {
    std::vector<typename Slab<Capacity>::Atom*> result;
    for (auto* atom : entity)
        result.push_back(clone_atom(slab, atom, std::make_index_sequence<std::tuple_size_v<AtomTypes>>{}));
    return result;
}