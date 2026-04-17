# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world.

---

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, or nest and move worlds around as components.
- **Pay for what you use:** Data is stored per type in aligned slabs, and nothing is allocated until you create it.
- **Compose freely:** Entities are lightweight typed handles that point to atoms, with no inheritance required.
- **Clone and upgrade at runtime:** Copy matching atoms between entity layouts and add new fields as needed.
- **Keep structure in your hands:** Store worlds, atoms, or higher-level objects however your game needs.

---

## Core Types

SteamWand centers around a few simple building blocks:

- `Atom`: a lightweight handle containing index, entity id, generation, and type id.
- `World`: owns type slabs, tracks entity ids, and manages deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and metadata.
- `EntityHandle<E>`: typed entity wrapper backed by a fixed array of atoms.
- `Field<Key, T>`: compile-time field initializer used to build entities.

---

## Data Types

The built-in example types in the current layout are:

```cpp
struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

inline std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    return os << "Vec2(" << v.x << ", " << v.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
}
```

The engine also supports a world-in-world example type:

```cpp
struct World;
struct ComponentExample {
    std::unique_ptr<World> data;
};
```

---

## Atom Types

Available atom types are declared through `ATOM_TYPES` and assigned fixed type ids:

```cpp
#define ATOM_TYPES(X) \
    X(1, int32_t,         Int32) \
    X(2, uint32_t,        UInt32) \
    X(3, float,           Float) \
    X(4, Vec2,            Vec2) \
    X(5, Vec3,            Vec3) \
    X(6, bool,            Bool) \
    X(7, char,            Char) \
    X(8, ComponentExample, ComponentExample)
```

Type ids are also exposed through `AtomType` and `TypeInfo<T>`.

---

## Creating Atoms

Atoms are created through the `World` and returned as lightweight handles.

```cpp
World world(1024);

Atom hp    = world.create_atom<int32_t>(100);
Atom speed = world.create_atom<float>(3.5f);
Atom pos   = world.create_atom<Vec3>({1.0f, 2.0f, 3.0f});
```

Atoms can optionally be associated with an entity id:

```cpp
Atom hp = world.create_atom<int32_t>(100, entity_id);
```

The handle stores metadata only; the value lives in the per-type slab.

---

## Entity Layouts

Entities are typed handles backed by a fixed array of atoms, one per enum field. Define your layout with an enum class ending in `COUNT`.

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

auto zombie = create_entity<Zombie>(world,
    Field<Zombie::HP,     int32_t>{ 100 },
    Field<Zombie::Speed,  float>  { 1.5f },
    Field<Zombie::Wealth, int32_t>{ 50 },
    Field<Zombie::Target, Vec2>   {{ 10.0f, 20.0f }}
);
```

Read and write components through the typed `get<T>` accessor:

```cpp
int32_t hp = zombie.get<int32_t>(Zombie::HP, world);
zombie.get<float>(Zombie::Speed, world) *= 2.0f;
```

---

## Cloning Entities

`clone_entity` copies atoms from one entity handle to another by matching enum indices. Matching fields are copied, and missing fields in the target remain empty until you fill them in.

```cpp
enum class SuperZombie : uint8_t {
    HP,
    Speed,
    Wealth,
    Target,
    Power,
    COUNT
};

auto boss = clone_entity<SuperZombie>(zombie, world);
boss.atoms[(size_t)SuperZombie::Power] = world.create_atom<float>(9001.0f);
```

This lets you upgrade entities at runtime without a migration step.

---

## Freeing and Cleanup

Entities are not destroyed immediately. Instead, their atoms are queued for removal and compacted during `cleanup()`.

```cpp
zombie.free(world);
world.cleanup();
```

This makes the cost explicit and lets you batch many removals before compaction.

---

## Direct Slab Access

When you want bulk iteration, you can bypass per-handle access and work directly on the underlying array.

```cpp
Vec3* positions = world.get_array<Vec3>();
size_t count = world.size<Vec3>();

for (size_t i = 0; i < count; ++i) {
    positions[i].x += 1.0f;
}
```

This is the fastest way to walk a type’s storage when you already control the layout.

---

## Multi-World Storage

Worlds are independent by default, but you can store one world inside another as a component.

```cpp
World universe(10);

World nested(100);
for (int i = 0; i < 5; ++i) {
    nested.create_atom<int32_t>(100 + i);
}

universe.create_atom<ComponentExample>({ std::make_unique<World>(std::move(nested)) });

World& active = *(universe.get_array<ComponentExample>().data);
int32_t* hps = active.get_array<int32_t>();
```

This is useful when you want world-level composition instead of a single global simulation space.

---

## How It Works

SteamWand uses per-type slabs with aligned allocations and simple metadata for entity id and generation tracking. The `Atom` handle stores the type id, index, entity id, and generation, while the `World` keeps a registry of slabs and a deferred deletion queue. Clone support currently relies on copy-constructibility of the underlying type.

---

## Adding a New Type

1. Define your data type and its `operator<<`.
2. Add it to `ATOM_TYPES` with the next sequential id.

```cpp
struct Health { float current, max; };

inline std::ostream& operator<<(std::ostream& os, const Health& h) {
    return os << h.current << "/" << h.max;
}
```

```cpp
#define ATOM_TYPES(X) \
    X(1, int32_t,         Int32) \
    X(2, uint32_t,        UInt32) \
    X(3, float,           Float) \
    X(4, Vec2,            Vec2) \
    X(5, Vec3,            Vec3) \
    X(6, bool,            Bool) \
    X(7, char,            Char) \
    X(8, ComponentExample, ComponentExample)
```

---

## Example Usage

### Cloning and upgrading

```cpp
void basicExamples() {
    World world(1024);

    auto regular = create_entity<Zombie>(world,
        Field<Zombie::HP, int32_t>{ 100 },
        Field<Zombie::Speed, float>{ 1.5f },
        Field<Zombie::Wealth, int32_t>{ 50 },
        Field<Zombie::Target, Vec2>{{ 10.0f, 20.0f }}
    );

    auto boss = clone_entity<SuperZombie>(regular, world);
    boss.atoms[(size_t)SuperZombie::Power] = world.create_atom<float>(9001.0f);
}
```

### World inside world

```cpp
void worldOfWorldsExample() {
    World universe(10);

    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.create_atom<int32_t>(100 + i);
    }

    universe.create_atom<ComponentExample>({ std::make_unique<World>(std::move(nested)) });

    World& active = *(universe.get_array<ComponentExample>().data);
    int32_t* hps = active.get_array<int32_t>();

    for (size_t i = 0; i < active.size<int32_t>(); i++) {
        std::cout << "Entity " << i << " HP: " << hps[i] << "\n";
    }
}
```

---

## Planned Features

- Dynamic queries across multiple atom types.
- Paged slabs for sparse or rare types.
- Runtime add/remove of components.
- Reverse lookup from entity id to atom.
- Active-slot iteration that skips freed entries.
- Multi-threaded slab iteration.
- Coroutine-friendly task systems.
- Serialization utilities.
- Scripting integration.

---

## License

Open source forever. Use it however you like — no strings attached.
