#include "Dcs.h"
#include <iostream>

struct Zombie {
    AtomBase* hp;
    AtomBase* speed;
};

int main() {
    World<1024> world;

    auto* hp_p = world.create(int(100));
    auto* spd_p = world.create(3.5f);
    auto* wealth_p = world.create(int(3));

    std::vector<AtomBase*> zombie = { hp_p, spd_p, wealth_p };

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = world.clone_entity(zombie);
    world.value_of<int>(super_zombie[2]) = 5;

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::cout << "\n=== Vec3 only ===\n";
    world.iter<Vec3>([](Vec3& v) {
        std::cout << v.x << "," << v.y << "," << v.z << "\n";
        });

    auto* newthing = world.create(int(100));
    int& x = world.get(newthing);
    std::cout << "original x before increment: " << x << std::endl;
    x += 5;
    std::cout << "original x after increment: " << x << std::endl;
}