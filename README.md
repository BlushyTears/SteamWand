# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world of potentially worlds if so wished for.

---

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, or nest worlds inside other worlds as data.
- **Pay for what you use:** Data is stored per type in contiguous slabs, and nothing is allocated until you create it.
- **Keep ownership explicit:** Every stored value carries a generational `Atom` handle.
- **Compose freely:** Storage is runtime-driven, **any C++ type works out of the box** - no macros needed.
- **Stay flexible:** Worlds can be nested, moved, and accessed directly when you want maximum control.

---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and metadata.
- `Atom`: a lightweight generational reference with `id` and `gen`.

---

## Data Types

The built-in example types are:

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

## Dynamic Type System

```cpp
// Any C++ type works instantly but limited to one type per world due to limitations of type uniqueness.
world.add<int32_t>(42);
world.add<float>(3.14f);
world.add<Vec3>({1.0f, 2.0f, 3.0f});

// Custom types work without any setup
struct PlayerData { int level; float speed; };
world.add<PlayerData>({10, 5.5f});
```

---

## Creating Atoms

Values are created through `World::add<T>()`. Returns an `Atom` with id and generation:

```cpp
World world(1024);

Atom intAtom = world.add<int32_t>(42);
Atom floatAtom = world.add<float>(3.14f);
Atom vecAtom = world.add<Vec3>({1.0f, 2.0f, 3.0f});

// Custom types - zero configuration
struct PlayerData { int level; };
Atom playerAtom = world.add<PlayerData>({10});
```

---

## Safe Atom Access

Atoms resolve to live data with `World::get<T>()`:

```cpp
int32_t* value = world.get<int32_t>(intAtom);
if (value) {
    std::cout << "Value: " << *value << "\n";
}
```

Returns `nullptr` if atom is stale, deleted, or generation mismatch.

---

## Direct Slab Access

Bulk iteration bypasses atom lookup for maximum speed:

```cpp
int32_t* ints = world.get_array<int32_t>();
size_t count = world.size<int32_t>();

for (size_t i = 0; i < count; ++i) {
    ints[i] += 10;
}
```

---

## Ownership Lookup

Each atom tracks its owning world for reverse lookup:

```cpp
int32_t* ints = world.get_array<int32_t>();
for (size_t i = 0; i < world.size<int32_t>(); ++i) {
    World* owner = world.get_world_at<int32_t>(i);
    std::cout << "Index " << i << ": " << ints[i] << " (owner: " << owner << ")\n";
}
```

---

## World of Worlds

**Native hierarchical composition** - store worlds as atoms:

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
        std::cout << "Nested HP: " << hps[i] << "\n";
    }
}
```

---

## Generational Atoms

**Stale reference protection** via generation counters:

```cpp
Atom a = world.add<int32_t>(42);
int32_t* value = world.get<int32_t>(a);  // Works

world.queue_free(0);
world.cleanup();
int32_t* expired = world.get<int32_t>(a);  // nullptr
```

---

## Cleanup Model

Deferred deletion keeps it simple and explicit:

```cpp
world.queue_free(0);  // Mark index for removal
world.cleanup();      // Invalidate across all slabs
```

Uses generation marking (no compaction/swap-and-pop).

---

## Example Usage

### Zero-config custom types

```cpp
void basicExamples() {
    std::cout << "--- Basic SteamWand Examples ---\n";
    World world(1024);

    // Built-in + custom types - all work identically
    Atom intAtom = world.add<int32_t>(42);
    world.add<float>(3.14f);
    world.add<Vec3>({1.0f, 2.0f, 3.0f});
    
    struct PlayerData { int level; float speed; };
    world.add<PlayerData>({10, 5.5f});

    // Safe access
    int32_t* val = world.get<int32_t>(intAtom);
    if (val) std::cout << "Player level: " << *val << "\n";

    // Test invalidation
    world.queue_free(0);
    world.cleanup();
    if (!world.get<int32_t>(intAtom)) {
        std::cout << "Atom safely invalidated ✓\n";
    }
}
```

### Multiple independent worlds

```cpp
void multipleWorldsExample() {
    World overworld(1024), nether(1024);
    overworld.add<int32_t>(10);
    nether.add<int32_t>(666);

    World multiverse(10);
    multiverse.add<World>(std::move(overworld));
    multiverse.add<World>(std::move(nether));
    
    std::cout << "Multiverse manages 2 worlds\n";
}
```

---

## Current Characteristics

- **Dynamic typing**: Any C++ type works instantly via `TypeInfo<T>::id()`
- **No compaction**: Uses generation marking (sparse over time)
- **Deferred cleanup**: Explicit `queue_free()` + `cleanup()`
- **Zero boilerplate**: No macros, no registration

---

## Planned Features

- Non-disruptive defragmentation
- Multi-threaded access with coroutines
- Dynamic multi-type queries
- Serialization

---

## License

Open source forever. Use it however you like — no strings attached.