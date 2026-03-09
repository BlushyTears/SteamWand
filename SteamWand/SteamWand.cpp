#include "Dcs.h"
#include <iostream>
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <optional>
#include <tuple>
#include "Benchmarks.h"

#define ITEMS 33000000
#define RUNS  100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()


void basicExamples() {
    const size_t COUNT = 1000000;
    World world(COUNT);

    // Basic create & free
    Atom atom = world.create<int>(5);
    world.free<int>(atom);

    // Entity with multiple components
    Atom hp_p = world.create<int>(100);
    Atom spd_p = world.create<float>(3.5f);
    Atom wealth_p = world.create<int>(3);
    Atom target_p = world.create<int>(5);

    std::vector<Atom> zombie = { hp_p, spd_p, wealth_p, target_p };
    std::cout << "Normal zombie wealth: " << world.get<int>(zombie[2]) << "\n";

    std::vector<Atom> super_zombie = world.clone_entity(zombie);

    world.free_entity(zombie);

    world.get<int>(super_zombie[2]) = 5;
    std::cout << "Super zombie wealth: " << world.get<int>(super_zombie[2]) << "\n";

    // - Cache friendly way to manage a list of vec3's and ints if they're related -
    World vecWorld(COUNT);

    for (int i = 0; i < 4; i++) {
        vecWorld.create<Vec3>(Vec3{ float(1 * i), float(2 * i), float(3 * i) });
        vecWorld.create<int>(5);
    }

    // Iterate and modify Vec3 using raw pointer + count
    Vec3* vecs = vecWorld.raw<Vec3>();
    size_t vec_count = vecWorld.count<Vec3>();
    for (size_t i = 0; i < vec_count; i++) {
        vecs[i].x += 1.0f;
        vecs[i].y += 2.0f;
        vecs[i].z += 3.0f;
    }

    world.free_entity(super_zombie);

    // Clear Vec3 here to demonstrate the potential
    vecWorld.clear<Vec3>();

    // Print remaining Vec3 (should be nothing) - using raw pointer + count
    Vec3* remaining_vecs = vecWorld.raw<Vec3>();
    size_t remaining_count = vecWorld.count<Vec3>();
    for (size_t i = 0; i < remaining_count; i++) {
        std::cout << remaining_vecs[i] << "\n";
    }

    // This will print ints (still exist) - using raw pointer + count
    int* ints = vecWorld.raw<int>();
    size_t int_count = vecWorld.count<int>();
    for (size_t i = 0; i < int_count; i++) {
        std::cout << ints[i] << "\n";
    }

    // Print using runtime dispatch
    //world.print(hp_p);
}

int main() {
    //printf("Running benchmarks with %d items, %d runs each\n", ITEMS, RUNS);

    printf("\n=== Steamwand benchmarks ===\n");
    linear_iteration_v3();
    query_parallel_v3();
    multi_component_v3();
    backwards_query_v3();
    zombie_v3();

    printf("\n=== ARCHETYPE benchmarks ===\n");
    archetype_linear();
    archetype_multi();
    archetype_query_parallel();
    archetype_backwards_query();
    archetype_zombie();

    return 0;
}