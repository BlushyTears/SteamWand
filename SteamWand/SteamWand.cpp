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

    // Adding now returns an Atom (Index + Generation)
    // Any type works now without needing to edit a macro
    Atom intAtom = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<float>(3.15f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });

    // Custom types work out of the box
    struct PlayerData { int level; };
    world.add<PlayerData>({ 10 });

    // Safe Retrieval
    int32_t* val = world.get<int32_t>(intAtom);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    // Bulk Access
    int32_t* ints = world.get_array<int32_t>();
    if (ints) {
        std::cout << "First int32 value in raw array: " << ints[0] << "\n";
    }

    // Deletion and Cleanup
    world.queue_free(0);
    world.cleanup();

    // Post-Cleanup Safety Check
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

    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        // Retrieve the world pointer associated with this specific index
        World* owner = world.get_slab<int32_t>().get_world(i);
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World address: " << owner << "\n";
    }
}

void multipleWorldsExample() {
    World character(10);

    // Jeans have 3 components
    World jeans(100);
    jeans.add<float>(0.8f);
    jeans.add<std::string>("Denim");
    jeans.add<Vec2>({ 32, 34 });

    // Shirt only has 1 component
    World shirt(100);
    shirt.add<float>(0.2f);

    character.add<World>(std::move(jeans));
    character.add<World>(std::move(shirt));

    World* items = character.get_array<World>();
    size_t itemCount = character.size<World>();
    float totalArmor = 0.0f;

    for (uint32_t i = 0; i < itemCount; i++) {
        // We ask the sub-world which is character: "Give me your armor array"
        float* armorPtr = items[i].get_array<float>();

        if (armorPtr && items[i].size<float>() > 0) {
            std::cout << "Incrementing total armor: " << totalArmor << " By: " << armorPtr[0] << std::endl;
            totalArmor += armorPtr[0];
        }
    }
}

int main() {
    basicExamples();
    reverseLookupExample();
    multipleWorldsExample();

    //steamwand_linear();
    //steamwand_query_parralel();
    //steamwand_multi_component();
    //steamwand_backwards_query();
    //steamwand_zombie();

    //archetype_linear();
    //archetype_query_parallel();
    //archetype_multi();
    //archetype_backwards_query();
    //archetype_zombie();

    return 0;
}