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

    // 1. Adding now returns a Handle (Index + Generation)
    Handle intHandle = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });

    // 2. Safe Retrieval
    // This is safe even after cleanup() moves memory around
    int32_t* val = world.get<int32_t>(intHandle);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    // 3. Bulk Access (Same as before, still fast)
    int32_t* hps = world.get_array<int32_t>();
    std::cout << "First HP value in raw array: " << hps[0] << "\n";

    // 4. Deletion and Cleanup
    world.queue_free(0);
    world.cleanup();

    // 5. Post-Cleanup Safety Check
    int32_t* expiredVal = world.get<int32_t>(intHandle);
    if (!expiredVal) {
        std::cout << "Handle correctly invalidated after deletion/cleanup.\n";
    }

    std::cout << "--------------------------------\n\n";
}

void reverseLookupExample() {
    std::cout << "--- Reverse Lookup Example ---\n";
    World* z1 = new World(1);

    z1->add<int32_t>(100);
    z1->add<int32_t>(200);

    int32_t* hps = z1->get_array<int32_t>();

    for (uint32_t i = 0; i < (uint32_t)z1->size<int32_t>(); ++i) {
        World* owner = z1->get_world_at<int32_t>(i);
        // We can still use raw indices for bulk iteration loops
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
    }

    delete z1;
    std::cout << "------------------------------\n\n";
}

// Way to do reverse lookup (Useful for things like collision checking against many potential targets):
void worldOfWorldsExample() {
    std::cout << "--- World of Worlds Example (Refactored) ---\n";
    World universe(10);

    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.add<int32_t>(100 + i);
    }

    Handle nestedHandle = universe.add<World>(std::move(nested));
    World* active = universe.get<World>(nestedHandle);

    if (active) {
        int32_t* hps = active->get_array<int32_t>();
        for (size_t i = 0; i < active->size<int32_t>(); i++) {
            World* owner = active->get_world_at<int32_t>(i);
            std::cout << "Nested Entity " << i << " HP: " << hps[i] << " (Owner World: " << owner << ")\n";
        }
    }
}

int main() {
    //basicExamples();
    //reverseLookupExample();
    //worldOfWorldsExample();
    
    printf("\n=== Steamwand benchmarks ===\n");
    steamwand_linear();
    steamwand_query_parralel();
    steamwand_multi_component();
    steamwand_backwards_query();
    steamwand_zombie();
    
    archetype_linear();
    archetype_query_parallel();
    archetype_multi();
    archetype_backwards_query();
    archetype_zombie();

    return 0;
}