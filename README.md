# SteamWand
#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an 'atom' that resides in a defined 'World'.
---
## Core Philosophy
- **Use one world or many worlds:** Either rapidly prototype with all data in one world, or manage many worlds
- **Pay for what you use:** Systems are decoupled, slabs are per-type for optimal cache locality, nothing is allocated until you ask for it
- **Compose freely:** Entities (or whatever you want to call them) are collections of typed atoms, no inheritance model needed
- **Recompile only when the type system changes:** Everything else is runtime
---
## Core Data Model
All data lives in a `World`: a collection of per-type slabs, one contiguous array per type registered in `AtomTypes`.
```cpp
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3>;
```
Adding a new type costs one word. Each slab is fixed-size, stack-allocated, and cache-friendly. `iter<T>` is a pure linear scan with no branching. Each atom carries a `uint8_t` tag for O(1) type dispatch.

---
## Entities & Composition
Entities are `vector<AtomBase*>`. Bags of typed atoms composed freely at runtime.
```cpp
World<1024> world;
std::vector<AtomBase*> zombie = {
    world.create(int32_t(100)),  // hp
    world.create(3.5f),          // speed
    world.create(uint32_t(3))    // wealth
};
```
A boss zombie can clone a normal zombie, eject unused atoms, and add new ones without caring about the underlying types:
```cpp
auto boss = world.clone_entity(zombie);
world.free_entity(zombie);
```
---
## API
```cpp
// Creation & destruction
world.create(Vec3{ 1, 2, 3 });
world.free<Vec3>(atom);
world.free_entity(entity);

// Typed access explicit
world.value_of<int32_t>(atom) = 5;

// Typed access deduced from context via proxy
int32_t x = world.get(atom);
world.get(atom) = 5;
int32_t& x = world.get(atom);

// Iteration
world.iter<Vec3>([](Vec3& v) { ... });

// Runtime print & type discovery
world.print(atom);

// Batch operations
world.pop<Vec3>(20);   // remove last N of a type
```
---
## get() vs value_of()
`get()` returns a proxy that deduces the type from context. No template argument needed when the type is unambiguous:
```cpp
int32_t x = world.get(atom);   // deduced
world.get(atom) = 5;           // deduced
```
When context is ambiguous (e.g. `auto`, `std::cout`), use `value_of` with an explicit type:
```cpp
auto& x = world.value_of<int32_t>(atom);
std::cout << world.value_of<int32_t>(atom);
```
---
## Memory Model
Atoms live inside fixed slab arrays inside `World`. nothing is heap-allocated per atom. No `delete` is ever needed. `world.free<T>(atom)` returns the slot to a freelist for reuse. If reuse isn't needed, it can be skipped entirely. Everything is cleaned up when `World` goes out of scope.

| Strategy | Use case |
|---|---|
| `free<T>(atom)` | Return a single slot to the freelist, O(1) |
| `free_entity(entity)` | Free all atoms in an entity |
| `pop<T>(n)` | Remove last N of a type |

---
## Scripting (Lua)
`dispatch` is the planned Lua bridge. Push any atom to Lua without knowing its type at the C++ call site, pull values back by tag. Game data changes without recompilation. Only new types in `AtomTypes` require a rebuild.

---
## Why this over ECS?
Cache-friendly per-type storage with zero archetype overhead and a scripting bridge that doesn't require recompilation. Entities are runtime-composed bags of atoms — no archetype migrations, no component registries.

---
## Planned
- **Concurrency:** Tagged task queues, decoupled per system, thread-safe slab access via coroutines
- **Defragmentation:** Background compaction when spare compute is available
- **Overflow:** Linked slab extension if capacity is exceeded at runtime
- **Queries:** Iterator-based multi-type querying via metaprogramming
- **Multi-Stream Buffer:** For handling larger amounts of repetitive data

---
## Adding a New Type
1. Define your datatype (type alias, enum, struct, union)
2. Add it to `AtomTypes`
3. Define `operator<<` if you want `print` to support it
```cpp
struct Health { float current, max; };
inline std::ostream& operator<<(std::ostream& os, const Health& h) {
    return os << h.current << "/" << h.max;
}
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3, Health>;
```
