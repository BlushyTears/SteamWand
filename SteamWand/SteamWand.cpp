#include "Dcs.h"
#include <iostream>
#include "Benchmarks.h"

struct Zombie {
    AtomBase* hp;
    AtomBase* speed;
    AtomBase* playerTarget;
};

int main() {
    const size_t COUNT = 1000000;
    World world(COUNT);

    auto* atom = world.create(int(5));
    world.create(Vec3{ 1, 2, 3 });
    world.free<Vec3>(atom);

    int& x = world.get(atom);
    std::cout << x;

    auto* hp_p = world.create(int(100));
    auto* spd_p = world.create(3.5f);
    auto* wealth_p = world.create(int(3));
    auto* playerTarget_p = world.create(Vec3({2, 5, 10}));

    std::vector<AtomBase*> zombie = { hp_p, spd_p, wealth_p, playerTarget_p };

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = world.clone_entity(zombie);
    world.value_of<int>(super_zombie[2]) = 5;

    Vec3& playerTarget = world.get(playerTarget_p);
    playerTarget = {1, 2, 3};

    world.free_entity(zombie);

    std::cout << "Super zombie wealth: " << world.value_of<int>(super_zombie[2]) << "\n";

    std::cout << "\n=== Vec3 only ===\n";
    world.iter<Vec3>([](Vec3& v) {
        std::cout << v.x << "," << v.y << "," << v.z << "\n";
        });

    AtomBase* newthing = world.create(int(100));
    int& x = world.get(newthing);
    world.free<int>(newthing);

    std::cout << "original x before increment: " << x << std::endl;
    x += 5;
    std::cout << "original x after increment: " << x << std::endl;
}