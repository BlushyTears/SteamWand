#include "Dcs.h"
#include <vector>

struct Zombie {
    Slab<1024>::Atom* hp;
    Slab<1024>::Atom* speed;
};

int main() {
    Slab<1024> slab;

    auto* hp = slab.create(int32_t(100));
    auto* spd = slab.create(3.5f);
    auto* wealth = slab.create(3);

    std::vector<Slab<1024>::Atom*> zombie;
    zombie.push_back(hp);
    zombie.push_back(spd);
    zombie.push_back(wealth);

    std::cout << "Normal zombie wealth: " << zombie[2]->get<int32_t>() << std::endl;

    std::vector<Slab<1024>::Atom*> super_zombie = clone_entity(slab, zombie);
    super_zombie[2]->get<int32_t>() = 5;
    std::cout << "Super zombie wealth updated: " << super_zombie[2]->get<int32_t>() << std::endl;

    slab.create(Vec3{ 1, 2, 3 });
    slab.create(Vec3{ 7, 0, 5 });
    slab.create(Vec2{ 9, 4 });

    std::cout << "speed: " << spd->get<float>() << "\n";

    std::cout << "\n=== Vec3 only ===\n";
    slab.iter<Vec3>([](Vec3& v) {
        std::cout << v.x << "," << v.y << "," << v.z << "\n";
        });

    std::cout << "\n=== Vec2 only ===\n";
    slab.iter<Vec2>([](Vec2& v) {
        std::cout << v.x << "," << v.y << "\n";
        });

    auto find = [](auto& vec, uint8_t tag) -> Slab<1024>::Atom* {
        for (auto* a : vec)
            if (a->tag == tag) return a;
        return nullptr;
        };

    slab.free(hp);
}