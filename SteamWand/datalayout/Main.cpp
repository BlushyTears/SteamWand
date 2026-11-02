#include "Dcs.h"
#include "Benchmarks.h"
#include "Snake.h"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

void basicExamples() {
    std::cout << "--- Basic SteamWand Examples ---\n";

    World world(1024);

    // Adding returns an Atom but you can opt to exclude if you don't care about the individual type
    // For instance if you made a bullet hell game you wouldn't care about a bullet,
    // but it can be a useful reference so that you don't have to iterate an entire world to find a specific field
    Atom<int32_t> intAtom = world.add<int32_t>(42);
    Atom<float> floatAtom1 = world.add<float>(3.14f);
    world.add<float>(3.15f);
    world.add<Vec3>({ 1.0f, 2.0f, 3.0f });

    struct PlayerData { int level; };
    world.add<PlayerData>({ 10 });

    // Safe Retrieval
    auto* val = world.get<int32_t>(intAtom);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    // Fast linear access
    RawSlots<int32_t> ints = world.raw_slots<int32_t>();
    if (ints.is_live(0)) {
        std::cout << "First int32 value in raw array: " << ints.data[0] << "\n";
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

void directWorldExample() {
    std::cout << "--- Direct World Access Example ---\n";
    World world(1024);

    Atom<int32_t> hit = world.add<int32_t>(100);
    world.add<int32_t>(200);

    World* owner = hit.world();
    if (owner) {
        for (auto& hp : owner->iter<int32_t>()) {
            hp += 10;
            std::cout << "HP: " << hp << " owned by World address: " << owner << "\n";
        }
    }
}

void universeExample() {
    World universe(10);

    struct Data { float hp; };

    World world(100);
    for (int i = 0; i < 50; i++) {
        Data d;
        d.hp = float(i);
        world.add<Data>(d);
    }
    World& nestedWorld = universe.attach_world(world);

    size_t worldCount = universe.size<World>();
    std::cout << "universe size is " << worldCount << std::endl;

    if (worldCount > 0) {
        for (auto& item : nestedWorld.iter<Data>()) {
            std::cout << "HP: " << item.hp << std::endl;
        }
    }
}

void multipleWorldsExample() {
    World character(10);

    // Jeans store armor, material and size.
    World jeans(100);
    jeans.add<float>(0.8f);
    jeans.add<std::string>("Denim");
    jeans.add<Vec2>({ 32, 34 });

    // Shirt only stores armor.
    World shirt(100);
    shirt.add<float>(0.2f);

    character.attach_world(jeans);
    character.attach_world(shirt);

    float totalArmor = 0.0f;

    for (auto& item : character.iter<World>()) {
        // Each clothing-world holds a single armor float
        for (auto& armor : item.iter<float>()) {
            std::cout << "Incrementing total armor: " << totalArmor << " By: " << armor << std::endl;
            totalArmor += armor;
            break;
        }
    }
}

struct Particle { Vec3 position; Vec3 velocity; float life; };
struct Enemy { std::string name; int hp; };
struct Room { World enemies{ 8 }; std::string name; };

// A slab that can grow: it starts with the World's 64 slots and grows up to 100000.
// It grows in place, so pointers and Atoms to existing particles stay valid.
void growableSlabExample() {
    std::cout << "--- Growable Slab Example ---\n";

    World effects(64);
    effects.set_max_capacity<Particle>(100000);

    Atom<Particle> first = effects.add(Particle{ { 0, 0, 0 }, { 1, 0, 0 }, 2.0f });
    Particle* firstPointer = effects.get(first);

    for (int i = 0; i < 5000; i++) {
        effects.add(Particle{ { 0, 0, 0 }, { 1, 0, 0 }, 2.0f });
    }

    std::cout << "Particles: " << effects.size<Particle>() << "\n";
    if (effects.get(first) == firstPointer) {
        std::cout << "First particle is still at the same address\n\n";
    }
    else {
        std::cout << "First particle moved in memory\n\n";
    }
}

// add() isn't allowed while iterating. Use queue_add() and queue_free() instead;
// both take effect at cleanup().
void spawnWhileIteratingExample() {
    std::cout << "--- Spawn While Iterating Example ---\n";

    World effects(16);
    effects.set_max_capacity<Particle>(100000);

    for (int i = 0; i < 100; i++) {
        float life = 1.0f;
        if (i % 2 == 1) {
            life = 2.0f;
        }
        effects.add(Particle{ { 0, 0, 0 }, { 1, 0, 0 }, life });
    }

    // iter_atoms() gives each particle together with its Atom.
    for (auto [atom, particle] : effects.iter_atoms<Particle>()) {
        particle.life -= 1.0f;

        if (particle.life <= 0.0f) {
            // The particle dies and leaves two sparks behind.
            effects.queue_free(atom);
            effects.queue_add(Particle{ particle.position, { 0, 2, 0 }, 1.0f });
            effects.queue_add(Particle{ particle.position, { 0, -2, 0 }, 1.0f });
        }
    }

    std::cout << "Before cleanup: " << effects.size<Particle>() << " particles\n";
    effects.cleanup();
    std::cout << "After cleanup: " << effects.size<Particle>() << " particles\n\n";
}

// When an effect is over, discard() throws away everything in its World at once.
// The memory is kept, so the next effect can reuse it.
void discardExample() {
    std::cout << "--- Discard Example ---\n";

    World explosion(16);
    explosion.set_capacity<Particle>(1000);

    for (int i = 0; i < 1000; i++) {
        explosion.add(Particle{ { 0, 0, 0 }, { 0, 1, 0 }, 1.0f });
    }
    std::cout << "Explosion particles: " << explosion.size<Particle>() << "\n";

    explosion.discard();
    std::cout << "After discard: " << explosion.size<Particle>() << "\n\n";
}

// A World inside your own type counts as a child World, so cleanup_tree() reaches it too.
void worldsInsideTypesExample() {
    std::cout << "--- Worlds Inside Types Example ---\n";

    World level(8);
    Atom<Room> hallAtom = level.add(Room{ .name = "Hall" });
    Room* hall = level.get(hallAtom);

    hall->enemies.add(Enemy{ "Goblin", 30 });
    Atom<Enemy> ghost = hall->enemies.add(Enemy{ "Ghost", 0 });
    hall->enemies.queue_free(ghost);

    level.cleanup_tree(); // also applies the ghost's removal inside the hall
    std::cout << hall->name << " enemies: " << hall->enemies.size<Enemy>() << "\n\n";
}

// move_to() moves a value into another World and returns its new Atom.
// The old Atom stops matching.
void moveBetweenWorldsExample() {
    std::cout << "--- Move Between Worlds Example ---\n";

    World hall(8);
    World vault(8);

    Atom<Enemy> goblin = hall.add(Enemy{ "Goblin", 30 });
    Atom<Enemy> movedGoblin = hall.move_to(goblin, vault);

    std::cout << "In the vault: " << vault.get(movedGoblin)->name << "\n";
    if (!goblin.is_valid()) {
        std::cout << "The old Atom no longer works\n";
    }

    // Inside a loop, queue the move instead. It happens at cleanup().
    for (auto [atom, enemy] : vault.iter_atoms<Enemy>()) {
        if (enemy.hp > 0) {
            vault.queue_move_to(atom, hall);
        }
    }
    vault.cleanup();

    std::cout << "Back in the hall: " << hall.size<Enemy>() << " enemy\n\n";
}

// A system is a function a World runs on its own data every update(). The same system can be
// given to several Worlds, and update() also runs the systems of the Worlds inside.
void poisonSystem(World& world, float) {
    for (auto [atom, enemy] : world.iter_atoms<Enemy>()) {
        enemy.hp -= 10;
        if (enemy.hp <= 0) {
            world.queue_free(atom);
        }
    }
}

void systemsExample() {
    std::cout << "--- Systems Example ---\n";

    World level(8);

    World swamp(8);
    swamp.add(Enemy{ "Frog", 20 });
    swamp.add(Enemy{ "Snake", 10 });
    swamp.add_system(poisonSystem);
    World& swampRoom = level.attach_world(swamp);

    World cave(8);
    cave.add(Enemy{ "Bat", 10 });
    World& caveRoom = level.attach_world(cave); // no poison in the cave

    // A lambda works too.
    level.add_system([](World& world, float) {
        std::cout << "Updating a level with " << world.size<World>() << " rooms\n";
    });

    for (int frame = 1; frame <= 2; frame++) {
        level.update(0.016f); // runs the level's systems, then the swamp's and the cave's
        std::cout << "Frame " << frame << ": swamp has " << swampRoom.size<Enemy>()
                  << " enemies, cave has " << caveRoom.size<Enemy>() << "\n";
    }
    std::cout << "\n";
}

void runExamples() {
    basicExamples();
    directWorldExample();
    multipleWorldsExample();
    universeExample();
    growableSlabExample();
    spawnWhileIteratingExample();
    discardExample();
    worldsInsideTypesExample();
    moveBetweenWorldsExample();
    systemsExample();
}

void runBenchmarks() {
    run_benchmarks();
}

void printUsage() {
    std::cout << "SteamWandDataLayout [--examples | --benchmarks | --snake | --help]\n"
                 "  --examples    Small data-layout examples (default).\n"
                 "  --benchmarks  World storage and equivalent vector work; use Release x64.\n"
                 "  --snake       Interactive console Snake demo.\n"
                 "Set arguments in Project Properties > Debugging > Command Arguments.\n";
}

}

// Some unrelated rendering stuff uncommented, i use put it here for testing purposes

//#include <Windows.h>
//#include <wrl.h>
//#include <d3d12.h>
//#include <dxgi1_6.h>
//#include <cassert>
//#include <cstdint>
//#include <vector>
//
//#pragma comment(lib, "dxgi.lib")
//
//#ifndef ReleaseCom
//#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
//#endif
//
//inline void ThrowIfFailed(HRESULT hr) {
//    if (FAILED(hr)) {
//        throw std::exception();
//    }
//}
//
//inline void LogAdapters()
//{
//    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
//
//        UINT createFactoryFlags = 0;
//    #if defined(_DEBUG)
//        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
//    #endif
//
//    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&mdxgiFactory)));
//
//    UINT i = 0;
//
//    IDXGIAdapter* adapter = nullptr;
//    std::vector< IDXGIAdapter*> adapterList;
//
//    while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
//    {
//        DXGI_ADAPTER_DESC desc;
//        adapter->GetDesc(&desc);
//
//        std::wstring text = L"Adapter: ";
//        text += desc.Description;
//        text += L"\n";
//
//        OutputDebugString(text.c_str());
//
//        adapterList.push_back(adapter);
//
//        ++i;
//    }
//
//    for (size_t i = 0; i < adapterList.size(); ++i) {
//        DXGI_ADAPTER_DESC desc;
//
//        adapterList[i]->GetDesc(&desc);
//
//        std::wcout << L"Adapter: " << desc.VendorId << std::endl;
//
//        ReleaseCom(adapterList[i]);
//    }
//}

int main(int argc, char* argv[]) {
    //run_benchmarks();
    multipleWorldsExample();
    growableSlabExample();
    spawnWhileIteratingExample();
    discardExample();
    worldsInsideTypesExample();
    moveBetweenWorldsExample();
    systemsExample();
}
