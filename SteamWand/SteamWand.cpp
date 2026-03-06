#include "Dcs.h"
#include <iostream>

int main() {
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
    auto* target_p = world.create(Vec3{ 2, 5, 10 });

    std::vector<AtomBase*> zombie = { hp_p, spd_p, wealth_p, target_p };
    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = world.clone_entity(zombie);

    world.free_entity(zombie);

    world.value_of<int>(super_zombie[2]) = 5;
    std::cout << "Super zombie wealth: " << world.value_of<int>(super_zombie[2]) << "\n";

    auto target_h = world.get_handle<Vec3>(target_p);
    *target_h = { 1, 2, 3 };

    // optional back-querying — user opts in by creating a ReverseIndex
    ReverseIndex<Vec3> positions(COUNT);
    positions.insert(0, target_p);

    AtomBase* found = positions.get_atom(0);
    if (found)
        std::cout << "Found position: " << world.value_of<Vec3>(found) << "\n";

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

    return 0;
}