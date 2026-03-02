#include "Dcs.h"
#include <iostream>

int main() {
    const size_t COUNT = 1000000;
    World world(COUNT);

    auto* atom = world.create(int(5));
    world.create(Vec3{ 1, 2, 3 });

    world.free<int>(atom);

    auto* newthing = world.create(int(100));
    auto h_x = world.get_handle<int>(newthing);

    std::cout << "Initial handle value: " << *h_x << "\n";
    *h_x += 5;
    std::cout << "Modified handle value: " << *h_x << "\n";

    auto* hp_p = world.create(int(100));
    auto* spd_p = world.create(3.5f);
    auto* wealth_p = world.create(int(3));
    auto* playerTarget_p = world.create(Vec3{ 2, 5, 10 });

    std::vector<AtomBase*> zombie = { hp_p, spd_p, wealth_p, playerTarget_p };

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = world.clone_entity(zombie);
    world.value_of<int>(super_zombie[2]) = 5;

    auto target_h = world.get_handle<Vec3>(playerTarget_p);
    *target_h = { 1, 2, 3 };

    return 0;
}