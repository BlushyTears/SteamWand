#pragma once

// Another prototype idea: Single stream buffer for maximum performance

//#include <array>
//#include <cstdint>
//#include <cstring>
//#include <tuple>
//#include <type_traits>
//#include <iostream>
//
//struct Vec2 { float x, y; };
//struct Vec3 { float x, y, z; };
//
//using AtomTypes = std::tuple<int32_t, float, Vec2, Vec3, int>;
//
//template<typename T, typename Tuple, size_t I = 0>
//constexpr size_t type_index() {
//    if constexpr (I >= std::tuple_size_v<Tuple>) {
//        return -1;
//    }
//    else if constexpr (std::is_same_v<T, std::tuple_element_t<I, Tuple>>) {
//        return I;
//    }
//    else {
//        return type_index<T, Tuple, I + 1>();
//    }
//}
//
//template<typename T>
//constexpr uint8_t tag_of() {
//    return static_cast<uint8_t>(type_index<T, AtomTypes>() + 1);
//}
//
//template<size_t BufferSize>
//struct PackedAtomStream {
//    alignas(16) uint8_t buffer[BufferSize];
//    size_t head = 0;
//
//    static constexpr size_t get_size_by_tag(uint8_t tag) {
//        if (tag == 0) return 0;
//        static constexpr size_t sizes[] = {
//            0,
//            sizeof(std::tuple_element_t<0, AtomTypes>),
//            sizeof(std::tuple_element_t<1, AtomTypes>),
//            sizeof(std::tuple_element_t<2, AtomTypes>),
//            sizeof(std::tuple_element_t<3, AtomTypes>),
//            sizeof(std::tuple_element_t<4, AtomTypes>)
//        };
//        return sizes[tag];
//    }
//
//    static constexpr size_t get_align_by_tag(uint8_t tag) {
//        if (tag == 0) return 1;
//        static constexpr size_t aligns[] = {
//            1,
//            alignof(std::tuple_element_t<0, AtomTypes>),
//            alignof(std::tuple_element_t<1, AtomTypes>),
//            alignof(std::tuple_element_t<2, AtomTypes>),
//            alignof(std::tuple_element_t<3, AtomTypes>),
//            alignof(std::tuple_element_t<4, AtomTypes>)
//        };
//        return aligns[tag];
//    }
//
//    template<typename T>
//    bool write(const T& value) {
//        constexpr uint8_t tag = tag_of<T>();
//        constexpr size_t size = sizeof(T);
//        constexpr size_t alignment = alignof(T);
//
//        size_t tag_pos = head;
//        size_t data_pos = (tag_pos + 1 + (alignment - 1)) & ~(alignment - 1);
//
//        if (data_pos + size > BufferSize) return false;
//
//        buffer[tag_pos] = tag;
//        std::memcpy(&buffer[data_pos], &value, size);
//        head = data_pos + size;
//        return true;
//    }
//
//    template<typename T, typename Fn>
//    void iter(Fn&& fn) {
//        const uint8_t target_tag = tag_of<T>();
//        size_t cursor = 0;
//
//        while (cursor < head) {
//            uint8_t tag = buffer[cursor];
//            if (tag == 0) break;
//
//            size_t alignment = get_align_by_tag(tag);
//            size_t size = get_size_by_tag(tag);
//            size_t data_pos = (cursor + 1 + (alignment - 1)) & ~(alignment - 1);
//
//            if (tag == target_tag) {
//                fn(*reinterpret_cast<T*>(&buffer[data_pos]));
//            }
//
//            cursor = data_pos + size;
//        }
//    }
//
//    void clear() {
//        head = 0;
//        std::memset(buffer, 0, BufferSize);
//    }
//};