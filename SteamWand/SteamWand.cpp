#include "Dcs.h"
#include <iostream>

struct Zombie {
    AtomBase* hp;
    AtomBase* speed;
};

int main() {
    World<1024> world;

    auto* hp = world.create(int32_t(100));
    auto* spd = world.create(3.5f);
    auto* wealth = world.create(int(3));
    auto* pos = world.create(Vec3{3, 2, 1});

    std::vector<AtomBase*> zombie = { hp, spd, wealth };

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::vector<AtomBase*> super_zombie = clone_entity(world, zombie);
    world.value_of<int>(super_zombie[2]) = 5;

    //world.print(super_zombie[2]); 
    //world.print(pos);
    //world.create(Vec3{ 1, 2, 3 });
    //world.pop<Vec3>(2);

    //world.create(Vec3{ 7, 0, 5 });
    //world.create(Vec2{ 9, 4 });

    //world.free_entity(zombie);

    std::cout << "Normal zombie wealth: " << world.value_of<int>(zombie[2]) << "\n";

    std::cout << "\n=== Vec3 only ===\n";
    world.iter<Vec3>([](Vec3& v) {
        std::cout << v.x << "," << v.y << "," << v.z << "\n";
        });
}