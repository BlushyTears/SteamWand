# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world of potentially worlds if so wished for.

---

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

`World::iter<T>()` for a single type, `iter<A, B, ...>()` for multiple.:

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

`discard()` runs destructors on every live component but keeps the slab allocations, so refilling the World is fast. Any `Atom` you got from this World before calling `discard()` is now invalid and `get()` will return `nullptr` for it.

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
