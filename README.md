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

Atom hp    = world.create_atom<int32_t>(100);
Atom speed = world.create_atom<float>(3.5f);
Atom pos   = world.create_atom<Vec3>({1, 2, 3});
```

Atoms can optionally be associated with an entity ID:

```cpp
Atom hp = world.create_atom<int32_t>(100, entity_id);
```

Reading and writing back through the handle:

```cpp
world.get<int32_t>(hp) += 10;
```

---

# Entities

Entities are typed handles backed by a fixed array of atoms, one per enum field. Define your entity's layout with a plain enum class ending in `COUNT`:

```cpp
enum class Zombie : uint8_t {
    HP,
    Speed,
    Wealth,
    Target,
    COUNT
};
```

Create an entity by supplying `Field` initializers for each component:

```cpp
World world(1024);

auto zombie = world.create_entity<Zombie>(
    Field<Zombie::HP,     int32_t>{ 100      },
    Field<Zombie::Speed,  float>  { 1.5f     },
    Field<Zombie::Wealth, int32_t>{ 50       },
    Field<Zombie::Target, Vec2>   {{ 10, 20 }}
);
```

Read and write components through the typed `get<T>` accessor:

```cpp
int32_t hp = zombie.get<int32_t>(Zombie::HP, world);
zombie.get<float>(Zombie::Speed, world) *= 2.0f;
```

---

# Cloning & Upgrading Entities

`clone_entity` copies atoms from one handle type to another by matching enum indices. Fields that exist in the source but not the target are dropped; new fields in the target are left uninitialized for you to fill in.

```cpp
enum class SuperZombie : uint8_t {
    HP,
    Speed,
    Wealth,
    Target,
    Power,   // new field not present in Zombie
    COUNT
};

// Clone all matching fields (HP, Speed, Wealth, Target) from Zombie → SuperZombie
auto boss = clone_entity<SuperZombie>(zombie, world);

// Initialize the new field manually
boss.atoms[(size_t)SuperZombie::Power] = world.create_atom<float>(9001.0f);

std::cout << boss.get<int32_t>(SuperZombie::HP, world)    << "\n"; // copied
std::cout << boss.get<float>  (SuperZombie::Power, world) << "\n"; // new
```

This pattern lets you "upgrade" entities at runtime without any migration cost.

---

# Freeing Entities

Queue an entity for removal, then compact the slab in one pass with `cleanup()`:

```cpp
zombie.free(world);
world.cleanup(); // swap-and-pop compaction happens here
```

`cleanup()` must be called explicitly — this keeps the cost visible and lets you batch many frees before paying for compaction.

---

# Direct Slab Access

When you need ECS-style bulk iteration, bypass per-atom accessors and walk the underlying array directly:

```cpp
Vec3*  positions = world.get_array<Vec3>();
size_t count     = world.size<Vec3>();

for (size_t i = 0; i < count; ++i) {
    positions[i].x += 1.0f;
}
```

> **Note:** the array includes both active and freed slots. Use this when you control slot layout or are doing bulk operations that don't require validity checks. See `benchmark.cpp` for examples.

---

# Multi-World & Mounting

Worlds are independent by default. You can share slabs between worlds using `mount()`:

```cpp
World physics_world(1024);
World render_world(1024);

render_world.mount(physics_world); // render_world can now read physics slabs
```

---

## Adding a New Type

1. Define your data type and its `operator<<`
2. Add it to `ATOM_TYPES` with the next sequential ID

```cpp
struct Health { float current, max; };

inline std::ostream& operator<<(std::ostream& os, const Health& h) {
    return os << h.current << "/" << h.max;
}

#define ATOM_TYPES(X) \
    X(1, int32_t,  Int32)  \
    X(2, uint32_t, UInt32) \
    X(3, float,    Float)  \
    X(4, Vec2,     Vec2)   \
    X(5, Vec3,     Vec3)   \
    X(6, bool,     Bool)   \
    X(7, char,     Char)   \
    X(8, Health,   Health) // added here
```

---

## OOP-Style Storage

If you prefer conventional objects, atoms can be stored as struct members. Note that few operations are currently supporting structs. Do note that this is not recommended for larger projects due to the limitations of OOP, but can be a nice way to just try things out.

```cpp
struct Character {
    Atom health;
    Atom strength;
    Atom speed;
};
```

---

## Why SteamWand

Cache-friendly per-type storage with zero archetype overhead. Entities are runtime-composed bags of atoms. No archetype migrations, no component registries: JavaScript-like freedom that respects the programmer with more freedom — not less.

---

## Planned Features

- Fast iteration across active slots only (skip free list)
- Reverse index: entity → atom lookup
- Component groups and archetype-style iteration helpers
- Multi-threaded slab iteration
- Coroutine-friendly task systems
- Dynamic slab expansion
- Serialization utilities
- Scripting integration

---

## License

Open source forever. Use it however you like — no strings attached.
