#include "Dcs.h"
#include <iostream>
#include <chrono>
#include <vector>
#include "Benchmarks.h"
#include "Snake.h"

#define ITEMS 33000000
#define RUNS  100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

// Todo:
// - When combining worlds we need godot-like control either:
//      - 1. Flatten the worlds into 1 forever
//      - 2. Temporarily the parrent world caches its child worlds
//      - 3. Cache the worlds A and B into World C O(1) via pointer and 
//           limited add/delete capabilities to avoid race conditions (Superior option probably)
// 
// - Need actual safety checks for get and the likes (stupid clanker called it safe for no reason)
// - Cleanup should be automatic in world raii probably instead of explicit
// - queue_free should take atom not index to avoid freeing the wrong data
// - World struct should default to giving local array,
//      if you want entire slab it should be called global array or get full or something

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

void basicExamples() {
    std::cout << "--- Basic SteamWand Examples (Generational) ---\n";

    World world(1024);

    // Adding returns an Atom (a slot handle into the type's slab)
    Atom intAtom = world.add<int32_t>(42);
    Atom floatAtom1 = world.add<float>(3.14f);
    world.add<float>(3.15f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });

    struct PlayerData { int level; };
    world.add<PlayerData>({ 10 });

    // Safe Retrieval
    auto* val = world.get<int32_t>(intAtom);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    // Bulk Access
    auto* ints = world.get_array<int32_t>();
    if (ints) {
        std::cout << "First int32 value in raw array: " << ints[0] << "\n";
    }

    // Deletion and Cleanup
    // We now pass the Atom and the Type so the World targets the correct Slab
    world.queue_free<int32_t>(intAtom);
    world.queue_free<float>(floatAtom1);
    world.cleanup();

    auto* expiredVal = world.get<int32_t>(intAtom);

    if (!expiredVal) {
        std::cout << "Atom correctly invalidated after deletion/cleanup.\n";
    }
    else {
        // This shouldn't be reached if cleanup worked
        std::cout << "Value still exists: " << *expiredVal << "\n";
    }

    std::cout << "--------------------------------\n\n";
}

void reverseLookupExample() {
    std::cout << "--- Reverse Lookup Example ---\n";
    World world(1024);

    world.add<int32_t>(100);
    world.add<int32_t>(200);

    // This loop needs the per-index world owner, which the iterator doesn't expose.
    // Keeping the indexed form here is intentional.
    int32_t* hps = world.get_array<int32_t>();
    size_t count = world.size<int32_t>();

    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        World* owner = world.get_slab<int32_t>().get_world(i);
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World address: " << owner << "\n";
    }
}

void universeExample() {
    World universe(10);

    struct Data { float hp; };

    World& world = universe.emplace_world(100);

    for (int i = 0; i < 50; i++) {
        Data d;
        d.hp = float(i);
        world.add<Data>(d);
    }

    size_t worldCount = universe.size<World>();
    std::cout << "universe size is " << worldCount << std::endl;

    if (worldCount > 0) {
        // Grab the first world from the universe and iterate its Data.
        World* worlds = universe.get_array<World>();
        for (auto& item : worlds[0].iter<Data>()) {
            std::cout << "HP: " << item.hp << std::endl;
        }
    }
}

void multipleWorldsExample() {
    World character(10);

    // Jeans have 3 components
    World& jeans = character.emplace_world(100);
    jeans.add<float>(0.8f);
    jeans.add<std::string>("Denim");
    jeans.add<Vec2>({ 32, 34 });

    // Shirt only has 1 component
    World& shirt = character.emplace_world(100);
    shirt.add<float>(0.2f);

    float totalArmor = 0.0f;

    for (auto& item : character.iter<World>()) {
        // Each clothing-world holds a single armor float; sum the first one if present.
        for (auto& armor : item.iter<float>()) {
            std::cout << "Incrementing total armor: " << totalArmor << " By: " << armor << std::endl;
            totalArmor += armor;
            break; // mirror original behavior — only first float per item
        }
    }
}

int main() {
    basicExamples();
    reverseLookupExample();
    multipleWorldsExample();
    universeExample();
    //SnakeGame snake;
    //snake.run();

    //steamwand_linear();
    //steamwand_query_parallel();
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