#include "Dcs.h"
#include <iostream>
#include "Benchmarks.h"
#include <chrono>

#define ITEMS 25000000
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

void basicExamples() {
    const size_t COUNT = 1000000;
    World world(COUNT);

    auto* atom = world.create(int(5));
    world.free<int>(atom);

    auto* newthing = world.create(int(100));
    auto h_x = world.get_handle<int>(newthing);
    std::cout << "Initial handle value: " << *h_x << "\n";
    *h_x += 5;
    std::cout << "Modified handle value: " << *h_x << "\n";

    auto* hp_p = world.create(int(100));
    auto* spd_p = world.create(3.5f);
    auto* wealth_p = world.create(int(3));
    auto* target_p = world.create(int(5));

    std::vector<AtomBase*> zombie = { hp_p, spd_p, wealth_p, target_p };
    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = world.clone_entity(zombie);

    world.free_entity(zombie);

    world.value_of<int>(super_zombie[2]) = 5;
    std::cout << "Super zombie wealth: " << world.value_of<int>(super_zombie[2]) << "\n";

    // - Cache friendly way to manage a list of vec3's -
    World vecWorld(COUNT);

    for (int i = 0; i < 4; i++) {
        auto pos = vecWorld.create(Vec3{ float(1 * i), float(2 * i), float(3 * i) });
        auto pos2 = vecWorld.create(int(5));
    }

    vecWorld.iter<Vec3>([](Vec3& v) {
        v.x += 1.0f;
        v.y += 2.0f;
        v.z += 3.0f;
        });

    world.free_entity(super_zombie);
    // Clear integers here to demostrate the potential
    vecWorld.clear<int>();

    // Loop unrolling can be a good idea since cout is slow and hurts cache performance
    vecWorld.iter<Vec3>([](Vec3& v) {
        std::cout << v << "\n";
        });

    // This will print nothing since it got cleared
    vecWorld.iter<int>([](int& v) {
        std::cout << v << "\n";
        });
}

int main() {
    //basicExamples();
    linear_iteration();
    backwards_query();
    multi_query_single_world();
    backwards_query_proto();
    std::cout << "\nArchetype:\n\n";
    archetype_queries();
    archetype_multi();
    backwards_query_archetype();

    std::cout << "\Zombie 1v1 prototype vs archetype maxed out performance:\n\n";
    
    zombie_update();
    zombie_update_archetype();
    return 0;
}