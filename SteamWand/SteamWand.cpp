#include "Dcs.h"
#include <iostream>

int main() {
    const size_t COUNT = 1000000;
    World world(COUNT);

    uint32_t e = world.create_entity();
    world.add_component<int>(e, 5);
    world.remove_component<int>(e);
    world.add_component<int>(e, 100);

    auto h_x = world.get_handle<int>(e);
    std::cout << "Initial handle value: " << *h_x << "\n";
    *h_x += 5;
    std::cout << "Modified handle value: " << *h_x << "\n";

    uint32_t zombie = world.create_entity();
    world.add_component<int>(zombie, 100);
    world.add_component<float>(zombie, 3.5f);
    world.add_component<Vec3>(zombie, { 2, 5, 10 });

    std::cout << "Normal zombie hp: " << *world.get_component<int>(zombie) << "\n";

    // Get entity by component O(N) for scanning potential components and O(1) for fetching the match
    world.iter_with_entity<Vec3>([&](uint32_t entity_id, Vec3& pos) {
        float* spd = world.get_component<float>(entity_id);
        if (spd)
            std::cout << "entity " << entity_id << " pos: " << pos << " speed: " << *spd << "\n";
        });

    auto target_h = world.get_handle<Vec3>(zombie);
    *target_h = { 1, 2, 3 };
    std::cout << "Updated target: " << *target_h << "\n";

    world.destroy_entity(zombie);

    return 0;
}