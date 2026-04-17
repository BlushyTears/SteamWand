#include "Dcs.h"
#include "Benchmarks.h"
#include <iostream>
#include <chrono>

#define ITEMS 33000000
#define RUNS  100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

enum class Zombie : uint8_t {
    HP,
    Speed,
    Wealth,
    Target,
    COUNT
};

enum class SuperZombie : uint8_t {
    HP,
    Speed,
    Wealth,
    Target,
    Power,
    COUNT
};

struct Character {
    Atom health;
    Atom strength;
    Atom speed;
};

// TODO 1: DYNAMIC QUERIES
//   world.query<int32_t, Vec3>([](int32_t& hp, Vec3& pos) { hp -= 10; });
//
// TODO 2: PAGED SLABS
//   Rare components waste N slots. Allocate pages on demand.
//
// TODO 3: COMPONENT ADD/REMOVE AT RUNTIME
//   world.add_component<Burning>(zombie_handle, { damage_per_sec });
//
// TODO 4: REVERSE LOOKUP (entity_id -> Atom)
//   Atom hp = world.find_atom(entity_id, TypeInfo<int32_t>::type_id);

void basicExamples() {
    std::cout << "--- Cloning & Upgrading Example ---\n";
    World world(1024);

    auto regular = create_entity<Zombie>(world,
        Field<Zombie::HP, int32_t>{ 100 },
        Field<Zombie::Speed, float>  { 1.5f },
        Field<Zombie::Wealth, int32_t>{ 50 },
        Field<Zombie::Target, Vec2>   {{ 10.0f, 20.0f }}
    );
    std::cout << "Regular Zombie HP: " << regular.get<int32_t>(Zombie::HP, world) << "\n";

    auto boss = clone_entity<SuperZombie>(regular, world);
    boss.atoms[(size_t)SuperZombie::Power] = world.create_atom<float>(9001.0f);

    std::cout << "Super HP    (copied): " << boss.get<int32_t>(SuperZombie::HP, world) << "\n";
    std::cout << "Super Power (new):    " << boss.get<float>(SuperZombie::Power, world) << "\n";

    regular.free(world);
    std::cout << "Regular freed. Float count: " << world.size<float>() << "\n";
    std::cout << "-----------------------------------\n\n";
}

void worldOfWorldsExample() {
    World universe(10);

    // 1. Create and populate a world
    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.create_atom<int32_t>(100 + i);
    }

    // 2. Wrap and move it directly into the Universe as a ComponentExample
    universe.create_atom<ComponentExample>({ std::make_unique<World>(std::move(nested)) });

    // 3. Access the data back out
    World& active = *(universe.get_array<ComponentExample>()[0].data);
    int32_t* hps = active.get_array<int32_t>();

    for (size_t i = 0; i < active.size<int32_t>(); i++) {
        std::cout << "Entity " << i << " HP: " << hps[i] << "\n";
    }
}

int main() {
    //basicExamples();
    worldOfWorldsExample();
    //printf("\n=== Steamwand benchmarks ===\n");
    //steamwand_linear();
    //steamwand_query_parralel();
    //steamwand_multi_component();
    //steamwand_backwards_query();
    //steamwand_zombie();

    //printf("\n=== ARCHETYPE benchmarks ===\n");
    //archetype_linear();
    //archetype_multi();
    //archetype_query_parallel();
    //archetype_backwards_query();
    //archetype_zombie();

    return 0;
}
