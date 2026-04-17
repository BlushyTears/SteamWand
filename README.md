# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world.

---

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, or nest worlds inside other worlds as data.
- **Pay for what you use:** Data is stored per type in contiguous slabs, and nothing is allocated until you create it.
- **Keep ownership explicit:** Every stored value carries a generational `Atom` handle.
- **Compose freely:** Storage is runtime-driven, and the engine stays low-level and explicit.
- **Stay flexible:** Worlds can be nested, moved, and accessed directly when you want maximum control.

---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and metadata.
- `Atom`: a lightweight generational reference with `id` and `gen`.
- `TypeInfo<T>`: maps C++ types to compact runtime type ids.
- `AtomType`: enum of registered atom types.
- `ISlab`: shared interface used by the world registry.

---

## Data Types

The built-in example types in the current layout are:

```cpp
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
```

Optional stream output helpers are included for nicer debug printing:

```cpp
inline std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    return os << "Vec2(" << v.x << ", " << v.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
}
```

---

## Atom Types

Available atom types are declared through `ATOM_TYPES` and assigned fixed type ids:

```cpp
#define ATOM_TYPES(X) \
    X(1, int32_t,  Int32)  \
    X(2, uint32_t, UInt32) \
    X(3, float,    Float)  \
    X(4, Vec2,     Vec2)   \
    X(5, Vec3,     Vec3)   \
    X(6, bool,     Bool)   \
    X(7, char,     Char)   \
    X(8, World,    World)
```

`AtomType::Empty = 0` represents an invalid or uninitialized slot.

---

## Creating Atoms

Values are created through `World::add<T>()`. `add()` returns an `Atom` that includes both the id and generation.

```cpp
World world(1024);

Atom intAtom = world.add<int32_t>(42);
Atom floatAtom = world.add<float>(3.14f);
Atom vecAtom = world.add<Vec3>({1.0f, 2.0f, 3.0f});
```

You can also move values into storage when the type supports it:

```cpp
World nested(100);
Atom nestedAtom = world.add<World>(std::move(nested));
```

---

## Safe Atom Access

Atoms can be resolved back into live data with `World::get<T>()`.

```cpp
int32_t* value = world.get<int32_t>(intAtom);
if (value) {
    std::cout << "Retrieved value: " << *value << "\n";
}
```

If the atom is stale, deleted, or no longer matches the current generation, `get<T>()` returns `nullptr`.

---

## Direct Slab Access

When you want bulk iteration, you can bypass atom lookup and work directly on the underlying contiguous array.

```cpp
int32_t* hps = world.get_array<int32_t>();
size_t count = world.size<int32_t>();

for (size_t i = 0; i < count; ++i) {
    hps[i] += 10;
}
```

This is the fastest way to walk a type's storage when you control the layout.

---

## Ownership Lookup

Each stored value keeps metadata for its owning world. This is useful for reverse lookup, debugging, and nested-world workflows.

```cpp
int32_t* hps = world.get_array<int32_t>();

for (uint32_t i = 0; i < (uint32_t)world.size<int32_t>(); ++i) {
    World* owner = world.get_world_at<int32_t>(i);
    std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
}
```

---

## World of Worlds

Worlds can be stored inside other worlds, which makes hierarchical simulation easy.

```cpp
World universe(10);

World nested(100);
for (int i = 0; i < 5; i++) {
    nested.add<int32_t>(100 + i);
}

Atom nestedAtom = universe.add<World>(std::move(nested));
World* active = universe.get<World>(nestedAtom);

if (active) {
    int32_t* hps = active->get_array<int32_t>();
    for (size_t i = 0; i < active->size<int32_t>(); i++) {
        World* owner = active->get_world_at<int32_t>(i);
        std::cout << "Nested Entity " << i << " HP: " << hps[i] << " (Owner World: " << owner << ")\n";
    }
}
```

---

## Cleanup Model

Removal is deferred. You queue indices for deletion, then mark them invalid with `cleanup()`.

```cpp
world.queue_free(0);
world.cleanup();
```

The cleanup pass marks slots as invalid via generation counters, keeping storage contiguous.

---

## Generational Atoms

SteamWand uses generational atoms to prevent stale references. When an item is removed, its generation changes, and old atoms no longer resolve.

```cpp
Atom a = world.add<int32_t>(42);
int32_t* value = world.get<int32_t>(a);  // Works

world.queue_free(0);
world.cleanup();

int32_t* expired = world.get<int32_t>(a);  // nullptr - invalidated
```

---

## Example Usage

### Basic storage

```cpp
void basicExamples() {
    std::cout << "--- Basic SteamWand Examples (Generational) ---\n";

    World world(1024);

    Atom intAtom = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<Vec3>({1.0f, 2.0f, 3.0f});

    int32_t* val = world.get<int32_t>(intAtom);
    if (val) {
        std::cout << "Retrieved value via atom: " << *val << "\n";
    }

    int32_t* hps = world.get_array<int32_t>();
    std::cout << "First HP value in raw array: " << hps << "\n";

    world.queue_free(0);
    world.cleanup();

    int32_t* expiredVal = world.get<int32_t>(intAtom);
    if (!expiredVal) {
        std::cout << "Atom correctly invalidated after deletion/cleanup.\n";
    }
}
```

### Nested world

```cpp
void worldOfWorldsExample() {
    std::cout << "--- World of Worlds Example ---\n";
    World universe(10);

    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.add<int32_t>(100 + i);
    }

    Atom nestedAtom = universe.add<World>(std::move(nested));
    World* active = universe.get<World>(nestedAtom);

    if (active) {
        int32_t* hps = active->get_array<int32_t>();
        for (size_t i = 0; i < active->size<int32_t>(); i++) {
            World* owner = active->get_world_at<int32_t>(i);
            std::cout << "Nested Entity " << i << " HP: " << hps[i] << " (Owner World: " << owner << ")\n";
        }
    }
}
```

---

## Current Limitations

- `queue_free(index)` removes by array index (not handle id).
- No compaction/swap-and-pop (yet) - uses generation marking instead.
- `World` acts as both container and storable type.
- Atom validity depends on generation matching.

---

## Planned Features

- Compaction with swap-and-pop.
- Entity abstraction layer.
- Active-slot iteration.
- Multi-threaded access.
- Dynamic queries.
- Serialization.

---

## License

Open source forever. Use it however you like — no strings attached.