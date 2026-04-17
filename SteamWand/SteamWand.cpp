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
    std::cout << "--- Basic SteamWand Examples ---\n";

    // 1. Initialize a World with a starting capacity
    World world(1024);

    // 2. Add different types of components (Atoms)
    // Note: In this DCS model, every 'add' creates a new entry in that type's slab.
    world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });
    world.add<bool>(true);

    // 3. Bulk Access
    // You get a raw pointer to the start of the contiguous memory slab.
    int32_t* hps = world.get_array<int32_t>();
    std::cout << "First HP value: " << hps[0] << "\n";

    // 4. Checking Size
    std::cout << "Float slab size: " << world.size<float>() << "\n";

    // 5. Deletion (Queue and Cleanup)
    // Queue an index for removal.
    world.queue_free(0);

    std::cout << "Size before cleanup: " << world.size<int32_t>() << "\n";

    // Cleanup performs swap_and_pop to keep memory contiguous.
    world.cleanup();

    std::cout << "Size after cleanup: " << world.size<int32_t>() << "\n";
    std::cout << "--------------------------------\n\n";
}

void reverseLookupExample() {
    std::cout << "--- Reverse Lookup Example ---\n";
    World universe(1024);

    World* z1 = new World(1);
    World* z2 = new World(1);

    z1->add<int32_t>(100);
    z1->add<float>(1.5f);

    z2->add<int32_t>(200);
    z2->add<float>(3.0f);

    int32_t* hps = z1->get_array<int32_t>();

    for (uint32_t i = 0; i < (uint32_t)z1->size<int32_t>(); ++i) {
        World* owner = z1->get_world_at<int32_t>(i);
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
    }

    delete z1;
    delete z2;
    std::cout << "------------------------------\n\n";
}

// One major use case for this: Alternative to ecs backwards lookup
void worldOfWorldsExample() {
    std::cout << "--- World of Worlds Example (Refactored) ---\n";
    World universe(10);

    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.add<int32_t>(100 + i);
    }

    universe.add<World>(std::move(nested));

    World* nested_worlds = universe.get_array<World>();
    World& active = nested_worlds[0];

    int32_t* hps = active.get_array<int32_t>();
    for (size_t i = 0; i < active.size<int32_t>(); i++) {
        World* owner = active.get_world_at<int32_t>(i);
        std::cout << "Nested Entity " << i << " HP: " << hps[i] << " (Owner World: " << owner << ")\n";
    }
}

int main() {
    //basicExamples();
    //reverseLookupExample();
    worldOfWorldsExample();

    // To run benchmarks, uncomment these:
    //printf("\n=== Steamwand benchmarks ===\n");
    //steamwand_linear();
    //steamwand_query_parralel();
    //steamwand_multi_component();
    //steamwand_backwards_query();
    //steamwand_zombie();

    //printf("\n=== ARCHETYPE benchmarks ===\n");
    //archetype_linear();
    //archetype_multi();
    //archetype_query_parallel();
    //archetype_backwards_query();
    //archetype_zombie();

    return 0;
}