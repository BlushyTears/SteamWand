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

void runExamples() {
    basicExamples();
    directWorldExample();
    multipleWorldsExample();
    universeExample();
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
}
