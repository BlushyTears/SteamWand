#include "Dcs.h"
#include <iostream>
#include <chrono>
#include <vector>
#include "Benchmarks.h"

#define ITEMS 33000000
#define RUNS  100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

void basicExamples() {
    std::cout << "--- Basic SteamWand Examples (Generational) ---\n";

    World world(1024);

    // 1. Adding now returns an Atom (Index + Generation)
    // Any type works now without needing to edit a macro
    Atom intAtom = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<float>(3.15f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });

    // Custom types work out of the box
    struct PlayerData { int level; };
    world.add<PlayerData>({ 10 });

    // 2. Safe Retrieval
    int32_t* val = world.get<int32_t>(intAtom);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    // 3. Bulk Access
    int32_t* ints = world.get_array<int32_t>();
    if (ints) {
        std::cout << "First int32 value in raw array: " << ints[0] << "\n";
    }

    // 4. Deletion and Cleanup
    world.queue_free(0);
    world.cleanup();

    // 5. Post-Cleanup Safety Check
    int32_t* expiredVal = world.get<int32_t>(intAtom);
    if (!expiredVal) {
        std::cout << "Atom correctly invalidated after deletion/cleanup.\n";
    }

    std::cout << "--------------------------------\n\n";
}

void reverseLookupExample() {
    std::cout << "--- Reverse Lookup Example ---\n";
    World world(1024);

    world.add<int32_t>(100);
    world.add<int32_t>(200);

    int32_t* hps = world.get_array<int32_t>();
    size_t count = world.size<int32_t>();

    for (uint32_t i = 0; i < (uint32_t)count; ++i) {
        // Retrieve the world pointer associated with this specific index
        World* owner = world.get_slab<int32_t>().get_world(i);
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
    }

    std::cout << "------------------------------\n\n";
}

void multipleWorldsExample() {
    // Two independent top-level worlds
    World overworld(1024);
    World nether(1024);

    overworld.add<int32_t>(10);
    nether.add<int32_t>(666);

    // A world containing multiple other worlds
    World multiverse(10);

    World room1(100);
    room1.add<Vec2>({ 1.0f, 1.0f });

    World room2(100);
    room2.add<Vec2>({ 5.0f, 5.0f });

    // Move them in
    multiverse.add<World>(std::move(room1));
    multiverse.add<World>(std::move(room2));

    std::cout << "Multiverse now manages 2 World components.\n";
}

int main() {
    basicExamples();
    reverseLookupExample();
    multipleWorldsExample();

//    steamwand_linear();
//    steamwand_query_parralel();
//    steamwand_multi_component();
//    steamwand_backwards_query();
//    steamwand_zombie();
//
////f you still want the baseline comparisons:
//    archetype_linear();
//    archetype_query_parallel();
//    archetype_multi();
//    archetype_backwards_query();
//    archetype_zombie();

    return 0;
}