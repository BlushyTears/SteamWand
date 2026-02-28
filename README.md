# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an 'atom' that resides in a defined 'World'.

---

## Core Philosophy
- **Use one world or many worlds** - Either rapidly prototype with all data mashed into one world, or manage many worlds 
- **Pay for what you use** — systems are decoupled, slabs are per-type for optimal cache locality, nothing is allocated until you ask for it
- **Compose freely** — entities (Or whatever you want to call it is up to you) are a collection of typed atoms, no inheritance model needed.
- **Recompile only when the type system changes** — everything else is runtime
---

## Core Data Model
All data lives in a `World` — a collection of per-type slabs, one contiguous array per type registered in `AtomTypes`.
```cpp
using AtomTypes = std::tuple<int32_t, float, Vec2, Vec3, int>;
```
Adding a new type costs one word. Each slab is fixed-size, stack-allocated, and cache-friendly. `iter<T>` is a pure linear scan with no branching.

---

## Entities & Composition
Entities are `vector<AtomBase*>` — bags of typed atoms composed freely at runtime.
```cpp
World<1024> world;
std::vector<AtomBase*> zombie = {
    world.create(int32_t(100)),  // hp
    world.create(3.5f),          // speed
    world.create(int(3))         // wealth
};
```
A boss zombie can clone a normal zombie, eject unused atoms, and add new ones — all without knowing types at the call site:
```cpp
auto boss = clone_entity(world, zombie);
world.free_entity(zombie);
```

---

## API
```cpp
// Creation & destruction
world.create(Vec3{ 1, 2, 3 });
world.free<Vec3>(atom);
world.free_entity(entity);
// Typed access
world.value_of<int>(atom) = 5;
// Iteration — pure linear scan per type
world.iter<Vec3>([](Vec3& v) { ... });
// Runtime type discovery
world.print(atom); (In C++, The user still needs to truly determine if it's an int or a float for instance which is the biggest bottleneck currently. In Lua, it should be possible to use mappings to get around the unknown types).
// Memory management
world.clear();        // arena-style nuke, O(1)
world.pop<Vec3>(20);  // remove last N of a type, also O(1)
```

---

## Memory Model
| Strategy | Use case |
|---|---|
| `clear()` | Frame-local entities, O(1) reset |
| `free_entity()` | Individual removal via swap-and-pop |
| Optionally create multiple worlds or just one big world for prototyping | Persistence tiers — permanent vs transient |
---
## Scripting (Lua)
`dispatch` is the Lua bridge — push any atom to Lua without knowing its type at the C++ call site, pull values back by tag. Game data changes without recompilation. Only new types in `AtomTypes` require a rebuild.

## Why this over ECS?
SteamWand is a Cache-friendly per-type storage with zero archetype overhead and a scripting bridge that doesn't require recompilation. Additionally you get javascript-like flexibility that is unheard of from conventional approaches. The main downside (At least right now) on the C++ side, you need to keep track of the types. Print does help figuring out. In the future a static analyzer might be nice for this purpose.

---

## Planned
- **Automatic type deduction** — Somehow help with determining the type when we call get on an Atom (One idea is a static analyzer)
- **Concurrency** — tagged task queues, decoupled per system, thread-safe slab access, using coroutines
- **Defragmentation** — background compaction when spare compute is available
- **Overflow** — linked slab extension if capacity is exceeded at runtime
- **Queries** — iterator-based multi-type querying via metaprogramming
- **Multi-Stream Buffer** for handling larger amount of repetitive data (The current one in Idea.h is only single)
  
---

## Adding a New Type
1. Define your struct
2. Add it to `AtomTypes`
3. Define `operator<<` if you want `print` to support it
```cpp
struct Health { float current, max; };
inline std::ostream& operator<<(std::ostream& os, const Health& h) { 
    return os << h.current << "/" << h.max; 
}
using AtomTypes = std::tuple<int32_t, float, Vec2, Vec3, int, Health>; // Health added to the back here
```
