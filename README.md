# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world.

---

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, or nest worlds inside other worlds as data.
- **Pay for what you use:** Data is stored per type in contiguous slabs, and nothing is allocated until you create it.
- **Compose freely:** Storage is runtime-driven, and the engine stays low-level and explicit.
- **Keep ownership explicit:** Each stored value carries a handle with generation tracking.
- **Stay flexible:** Worlds can be nested, moved, and accessed directly when you want maximum control.

---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and metadata.
- `Handle`: a lightweight generational reference with `id` and `gen`.
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

## Creating Data

Values are created through `World::add<T>()`. In this version of SteamWand, `add()` returns a `Handle` that includes both the handle id and generation.

```cpp
World world(1024);

Handle intHandle = world.add<int32_t>(42);
Handle floatHandle = world.add<float>(3.14f);
Handle vecHandle = world.add<Vec3>({1.0f, 2.0f, 3.0f});
```

You can also move values into storage when the type supports it:

```cpp
World nested(100);
Handle nestedHandle = world.add<World>(std::move(nested));
```

This makes nested-world composition a native pattern rather than a special case.

---

## Safe Handle Access

Handles can be resolved back into live data with `World::get<T>()`.

```cpp
int32_t* value = world.get<int32_t>(intHandle);
if (value) {
    std::cout << "Retrieved value: " << *value << "\n";
}
```

If the handle is stale, deleted, or no longer matches the current generation, `get<T>()` returns `nullptr`.

---

## Direct Slab Access

When you want bulk iteration, you can bypass handle lookup and work directly on the underlying contiguous array.

```cpp
World world(1024);
world.add<int32_t>(100);
world.add<int32_t>(200);
world.add<int32_t>(300);

int32_t* hps = world.get_array<int32_t>();
size_t count = world.size<int32_t>();

for (size_t i = 0; i < count; ++i) {
    hps[i] += 10;
}
```

This is the fastest way to walk a type’s storage when you control the layout.

---

## Ownership Lookup

Each stored value keeps metadata for its owning world. This is useful for reverse lookup, debugging, and nested-world workflows.

```cpp
World* z1 = new World(1);

z1->add<int32_t>(100);
z1->add<int32_t>(200);

int32_t* hps = z1->get_array<int32_t>();

for (uint32_t i = 0; i < (uint32_t)z1->size<int32_t>(); ++i) {
    World* owner = z1->get_world_at<int32_t>(i);
    std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
}
```

This is the current equivalent of reverse lookup: component data knows its owning `World`.

---

## World of Worlds

Worlds can be stored inside other worlds, which makes hierarchical simulation easy.

```cpp
World universe(10);

World nested(100);
for (int i = 0; i < 5; i++) {
    nested.add<int32_t>(100 + i);
}

Handle nestedHandle = universe.add<World>(std::move(nested));
World* active = universe.get<World>(nestedHandle);

if (active) {
    int32_t* hps = active->get_array<int32_t>();
    for (size_t i = 0; i < active->size<int32_t>(); i++) {
        World* owner = active->get_world_at<int32_t>(i);
        std::cout << "Nested Entity " << i << " HP: " << hps[i] << " (Owner World: " << owner << ")\n";
    }
}
```

This pattern is especially useful if you want a world to act as a component of another world.

---

## Cleanup Model

Removal is deferred. You queue indices for deletion, then compact all slabs in one pass with `cleanup()`.

```cpp
World world(1024);
world.add<int32_t>(42);
world.add<float>(3.14f);

world.queue_free(0);
world.cleanup();
```

The cleanup pass uses `swap_and_pop`, which keeps slab storage contiguous and makes iteration cache-friendly.

---

## Generational Handles

SteamWand now uses generational handles to reduce stale references. When an item is removed, its generation changes, and old handles no longer resolve.

That means a handle can survive code flow, but only remains valid if the underlying object is still the same live allocation. This is especially useful when cleanup reorders dense storage.

```cpp
Handle h = world.add<int32_t>(42);
int32_t* value = world.get<int32_t>(h);

world.queue_free(0);
world.cleanup();

int32_t* expired = world.get<int32_t>(h); // nullptr if invalidated
```

---

## Example Usage

### Basic storage

```cpp
void basicExamples() {
    std::cout << "--- Basic SteamWand Examples (Generational) ---\n";

    World world(1024);

    Handle intHandle = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<Vec3>({1.0f, 2.0f, 3.0f});

    int32_t* val = world.get<int32_t>(intHandle);
    if (val) {
        std::cout << "Retrieved value via handle: " << *val << "\n";
    }

    int32_t* hps = world.get_array<int32_t>();
    std::cout << "First HP value in raw array: " << hps << "\n";

    world.queue_free(0);
    world.cleanup();

    int32_t* expiredVal = world.get<int32_t>(intHandle);
    if (!expiredVal) {
        std::cout << "Handle correctly invalidated after deletion/cleanup.\n";
    }
}
```

### Reverse lookup

```cpp
void reverseLookupExample() {
    std::cout << "--- Reverse Lookup Example ---\n";
    World* z1 = new World(1);

    z1->add<int32_t>(100);
    z1->add<int32_t>(200);

    int32_t* hps = z1->get_array<int32_t>();

    for (uint32_t i = 0; i < (uint32_t)z1->size<int32_t>(); ++i) {
        World* owner = z1->get_world_at<int32_t>(i);
        std::cout << "Index " << i << " HP: " << hps[i] << " owned by World: " << owner << "\n";
    }

    delete z1;
}
```

### Nested world

```cpp
void worldOfWorldsExample() {
    std::cout << "--- World of Worlds Example (Refactored) ---\n";
    World universe(10);

    World nested(100);
    for (int i = 0; i < 5; i++) {
        nested.add<int32_t>(100 + i);
    }

    Handle nestedHandle = universe.add<World>(std::move(nested));
    World* active = universe.get<World>(nestedHandle);

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

## Benchmarks

The repository also includes benchmark entry points for performance testing.

```cpp
printf("\n=== Steamwand benchmarks ===\n");
steamwand_linear();
steamwand_query_parralel();
steamwand_multi_component();
steamwand_backwards_query();
steamwand_zombie();

archetype_linear();
archetype_query_parallel();
archetype_multi();
archetype_backwards_query();
archetype_zombie();
```

These are useful for comparing SteamWand’s slab-based design against archetype-based approaches.

---

## Current Limitations

- `queue_free(index)` removes by slab index, so it is best used with care when multiple slabs share the same index layout.
- Ownership metadata is stored per slab entry, not per logical entity abstraction.
- `World` currently acts as both a container and a storable type, which is powerful but intentionally low-level.
- Handle validity depends on generation matching, so stale references fail safely instead of pointing at moved data.
- The current code favors direct runtime storage over a higher-level ECS component graph.

---

## Planned Features

- Safer entity abstraction on top of world-owned slabs.
- Runtime add/remove of components.
- Better reverse lookup from stored data to higher-level identifiers.
- Active-slot iteration that skips freed entries.
- Multi-threaded slab iteration.
- Coroutine-friendly task systems.
- Serialization utilities.
- Scripting integration.
- Dynamic queries across multiple atom types.

---

## License

Open source forever. Use it however you like — no strings attached.