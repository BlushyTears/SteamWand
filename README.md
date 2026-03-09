# SteamWand
#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an 'atom' that resides in a defined 'World'.
---
## Core Philosophy
- **Use one world or many worlds:** Either rapidly prototype with all data in one world, or manage many worlds
- **Pay for what you use:** Systems are decoupled, slabs are per-type for optimal cache locality, nothing is allocated until you ask for it
- **Compose freely:** Entities are collections of typed atoms, no inheritance model needed
- **Recompile only when the type system changes:** Everything else is runtime
- **The structure is enforced by the end-user:** You can store atoms like vectors, hashmaps, objects, custom data structures, nothing at all or however you like for keeping things organized in your game.
---

# Creating Atoms

Atoms are created through the `World` and returned as lightweight handles.

```cpp
World world(1024);

Atom hp = world.create_atom<int32_t>(100);
Atom speed = world.create_atom<float>(3.5f);
Atom position = world.create_atom<Vec3>({1, 2, 3});
```

Atoms can optionally be associated with an entity:

```cpp
Atom hp = world.create_atom<int32_t>(100, entity_id);
```

Reading and writing back through the handle:

```cpp
world.get<int32_t>(hp) += 10;
```

------------------------------------------------------------------------

# Entities

Entities are collections of atoms — just a `std::vector<Atom>`.

```cpp
std::vector<Atom> zombie = {
    world.create_atom<int32_t>(100),
    world.create_atom<float>(2.0f),
    world.create_atom<Vec3>({0, 0, 0})
};
```

Cloning:

```cpp
auto boss = world.clone_entity(zombie);
```

Freeing the original does not impact the clone:

```cpp
world.free_entity(zombie);
```

Because atoms store `entity_id`, entity tracking can also be implemented externally if you want to create your ECS system on top of this for instance.

Or if you like OOP, you can store atoms in a conventional object (Currently no copy struct is supported).

```cpp
struct Character {
    Atom health;
    Atom strength;
    Atom speed;
};
```

------------------------------------------------------------------------

# Direct Slab Access

When you need ECS-inspired access, you can bypass get() entirely and iterate the underlying array:

```cpp
Vec3* positions = world.get_array<Vec3>();
size_t count = world.size<Vec3>();

for (size_t i = 0; i < count; ++i) {
    positions[i].x += 1.0f;
}
```

Note: the array includes both active and freed slots. Use this when you control slot layout or are doing bulk operations that don't need validity checks (see benchmark.cpp for some examples on this).

------------------------------------------------------------------------

## Adding a New Type

1. Define your datatype
2. Add it to `ATOM_TYPES` with a unique sequential ID
3. Define `operator<<` if you want `print` to support it

```cpp
struct Health { float current, max; };

inline std::ostream& operator<<(std::ostream& os, const Health& h) {
    return os << h.current << "/" << h.max;
}

#define ATOM_TYPES(X) \
    X(1, int32_t, Int32) \
    X(2, uint32_t, UInt32) \
    X(3, float, Float) \
    X(4, Vec2, Vec2) \
    X(5, Vec3, Vec3) \
    X(6, bool, Bool) \
    X(7, char, Char) \
    X(8, Health, Health)  // added here
```

------------------------------------------------------------------------

## Why SteamWand
Cache-friendly per-type storage with zero archetype overhead. Entities are runtime-composed bags of atoms. No archetype migrations, no component registries: Javascript-like freedom that respects the programmer with more freedom — not less.

------------------------------------------------------------------------

## Planned Features
Future ideas include:

- fast iteration across active slots only (skip free list)
- reverse index: entity → atom lookup
- component groups and archetype-style iteration helpers
- multi-threaded slab iteration
- coroutine-friendly task systems
- dynamic slab expansion
- serialization utilities
- scripting integration

---

## License
Open source forever. Use it however you like — no strings attached.

------------------------------------------------------------------------
