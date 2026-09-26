# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world of potentially worlds if so wished for.

---

#### Note: Usage of AI was used to make the data layout for prototyping reasons, and it is therefore unstable. There are plans to hand-code a cleaner, more consise version of this system.

## Build and run on Windows

Open **SteamWand.sln** in Visual Studio 2022 (Other versions hasn't been tested) and right click any either of the two projects and click "Set as startup project".

In Solution Explorer, right-click the project you want to run and choose **Set as Startup Project**, then press **F5** (debug) or **Ctrl+F5** (run without debugging).

| Projects so far |
| **SteamWand.Rendering:** The existing Win32/DirectX 12 window 
| **SteamWand.DataLayout:** A console executable with small World/Atom examples by default.

One solution contains both projects; there is no need to switch solution files or edit an entry point. Each project can also be built independently by right-clicking it and choosing **Build**.

### Data-layout runner

Set **Project Properties > Configuration Properties > Debugging > Command Arguments** for SteamWand.DataLayout:

- No argument, or `--examples`: small examples for add/get, removal, owner lookup and nested worlds.
- `--benchmarks`: the existing ten benchmark functions (33 million items, 100 runs). Use Release x64; the full suite can allocate several GB.
- `--snake`: the existing interactive console Snake prototype.
- `--help`: list the options.

The data storage implementation has not been redesigned as part of the project split. The backwards-query benchmark now reads the populated speed slots with `spd[i]`; previously `spd[2 * i]` read uninitialized slots.

### Rendering runner

The renderer currently clears the window to blue. The unfinished `shader.hlsl` is retained as a resource and copied next to the executable; it is not compiled into a triangle pipeline yet. Renderer arguments such as `--warp`, `--width 1280` and `--height 720` go in that project's **Command Arguments** field. Debug rendering uses the Direct3D debug layer, which requires the Windows **Graphics Tools** optional feature; Release does not require that layer.

### Source and build layout

```text
SteamWand.sln
SteamWand.Windows.props              Shared compiler, output and debugger settings
SteamWand/
  rendering/
    SteamWand.Rendering.vcxproj      DirectX executable project
    Main.cpp                        wWinMain and existing renderer
    d3dx12.h
    shader.hlsl
  datalayout/
    SteamWand.DataLayout.vcxproj     Console executable project
    Main.cpp                        Examples and runner options
    Dcs.h                           Header-only World/Slab/Atom implementation
    Benchmarks.cpp / Benchmarks.h
    Snake.h
bin/x64/<Configuration>/<Project>/   Executables and runtime resources
obj/x64/<Configuration>/<Project>/   Intermediate build files
```

The two projects have separate output/intermediate directories and no dependency on each other. `Dcs.h` remains header-only and can later be included by the rendering code when the modules need to interact; no DLL or static-library wrapper is needed for it today. Shared build settings live in `SteamWand.Windows.props`, visible under **Solution Items** and in Property Manager.

From a **Developer PowerShell for VS 2022** (repository root):

```powershell
msbuild .\SteamWand.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild .\SteamWand.sln /m /p:Configuration=Release /p:Platform=x64
.\bin\x64\Debug\SteamWand.DataLayout\SteamWand.DataLayout.exe --examples
.\bin\x64\Release\SteamWand.Rendering\SteamWand.Rendering.exe
```

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, nest worlds inside other worlds or keep many decoupled worlds based on your needs.
- **Pay for what you use:** Data is stored per type in contiguous slabs, and nothing is allocated until you create it.
- **Compose freely:** Storage is runtime-driven, any C++ type works out of the box.
- **Stay flexible:** Worlds can be nested, moved, and accessed directly when you want maximum control.
- **Respects the programmer:** SteamWand aims to be an engine that lets the user do more, not less with infinite guardrails.
- **Takes lessons from ecs, oop, composition, DOD:** without necessarily being in any of those categories
---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and a presence bitmap.
- `Atom`: a lightweight handle into a slab.
- `iter<>()`: iteration over one or more component types.

---

## Creating Atoms

`World::add<T>()` returns an `Atom` referencing the slot the component lives in:

```cpp
World world(1024);

Atom intAtom = world.add<int32_t>(42);
Atom playerAtom = world.add<PlayerData>({10, 5.5f});
```

---

## Safe Atom Access

```cpp
int32_t* value = world.get<int32_t>(intAtom);
if (value) {
    std::cout << "Value: " << *value << "\n";
}
```

Returns `nullptr` if the atom has been freed.

---

## Direct Slab Access

Fast bulk iteration for systems:

```cpp
int32_t* ints = world.get_array<int32_t>();
for (size_t i = 0; i < world.size<int32_t>(); ++i) {
    ints[i] += 10;
}
```

---

## Iteration

`World::iter<T>()` for a single type, `iter<A, B, and so on>()` for multiple types:

```cpp
// Single type
for (auto& hp : world.iter<int32_t>()) {
    hp -= 1;
}

// Multiple types (yields only entries present in every slab):
for (auto [hp, pos, speed] : world.iter<int32_t, Vec3, float>()) {
    if (hp > 0) {
        pos.x += speed * 0.016f;
        hp -= 1;
    }
}
```

---

## Reverse Lookup

Each slot remembers which World it was added to:

```cpp
int32_t* ints = world.get_array<int32_t>();
for (size_t i = 0; i < world.size<int32_t>(); ++i) {
    World* owner = world.get_slab<int32_t>().get_world(i);
    std::cout << "Index " << i << ": " << ints[i] << " (owner: " << owner << ")\n";
}
```

---

## World of Worlds

Store worlds as components for hierarchy (similar concept to Godot scenes). Build a child World standalone, then attach it with `std::move`:

```cpp
World universe(10);

World nested(100);
nested.add<int32_t>(100);

universe.attach_world(std::move(nested));   // nested is now empty
```

`std::move` implies that the original world is discarded.

## Atom Invalidation

```cpp
Atom a = world.add<int32_t>(42);
world.get<int32_t>(a);          // returns pointer

world.queue_free<int32_t>(a);
world.cleanup();
world.get<int32_t>(a);          // returns nullptr
```

---

## Discarding a World

When you want to throw out everything in a World and start fresh, use: `discard()`:

```cpp
World world(1024);
world.add<int32_t>(42);
world.add<std::string>("hello");

world.discard(); // every slab is now empty, allocations are kept

world.add<int32_t>(7); // reuses the same memory
```

`discard()` empties the World. Anything you added is gone, but the World itself is ready to use again. Atoms from before the discard no longer point at anything.

---

### Custom Types

```cpp
void example() {
    World world(1024);

    struct PlayerData {int level; float speed;};
    world.add<PlayerData>({10, 5.5f});
    world.add<int32_t>(0);

    for (auto [pd, score] : world.iter<PlayerData, int32_t>()) {
        pd.speed += 0.1f;
        score += pd.level;
    }
}
```

---

## Current Characteristics

- **Dynamic typing**: Any C++ type via `TypeInfo<T>::id()`
- **Range-for iteration**: `iter<Types...>()` over single or multiple component types ecs-style
- **Bitmask query**: Multi-type iteration includes types via bitmask access
- **No boilerplate**: macros, type registration

## Technical considerations

- Slabs are fixed capacity. The `cap` you pass to `World(cap)` is a hard limit per slab; exceeding it asserts. Pointers from `get_array<T>()` and references from `iter` are stable for the World's lifetime. Most likely the plan is to use a fixed-sized array with linked list if we exceed the size.
- Deleted slots are not reclaimed — `next_idx` only goes up, leaving holes in the slab over time. Two workarounds when this matters:
  - **Discard the World.** Call `discard()` (or let it go out of scope) and start fresh. Cheap, common, and matches how most game state is naturally scoped (per scene, per level, per round).
  - **Don't delete — disable.** Set an `alive` flag on the component instead of removing it. Keeps the slab tightly packed for cache-friendly iteration.

Defragmentation isn't implemented at the moment because slot-correlation across slabs is part of the idea: components added together share an index similarly to how ecs does backwards searching. It's probably possible to move the elements of all slabs at the same time, but it needs more thought.

---

## Planned Features

- Non-disruptive defragmentation
- Coroutines
- Serialization
- Atom in iteration (so iter loops can know which slot they're on)

---

## License

Open source forever. Use it however you like — no strings attached.
