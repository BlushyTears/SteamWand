#include "Dcs.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

void linearIteration(int items = 1000000) {
    World world(items);

    std::vector<AtomBase*> pointers;
    pointers.reserve(items);

    for (size_t i = 0; i < items; ++i) {
        pointers.push_back(world.create(float(i)));
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pointers.begin(), pointers.end(), g);

    auto start_iter = std::chrono::high_resolution_clock::now();
    world.iter<float>([&](float& v) {
        v = (v * 2.0f) + 1.0f;
        });
    auto end_iter = std::chrono::high_resolution_clock::now();

    auto start_value = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < items; ++i) {
        float& v = world.value_of<float>(pointers[i]);
        v = (v * 0.5f) - 0.5f;
    }
    auto end_value = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> dur_iter = end_iter - start_iter;
    std::chrono::duration<double, std::milli> dur_value = end_value - start_value;

    std::cout << "--- HARDWARE REALITY TEST (1M ITEMS) ---" << std::endl;
    std::cout << "Slab .iter (Linear):         " << dur_iter.count() << "ms" << std::endl;
    std::cout << "Pointer value_of (Shuffled): " << dur_value.count() << "ms" << std::endl;

    std::cout << "\nSample result: " << world.value_of<float>(pointers[0]) << std::endl;
}