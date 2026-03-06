# SteamWand
#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an 'atom' that resides in a defined 'World'.
---
## Core Philosophy
- **Use one world or many worlds:** Either rapidly prototype with all data in one world, or manage many worlds
- **Pay for what you use:** Systems are decoupled, slabs are per-type for optimal cache locality, nothing is allocated until you ask for it
- **Compose freely:** Entities are uint32_t IDs with typed atoms attached, no inheritance model needed
- **Recompile only when the type system changes:** Everything else is runtime
- **The structure is enforced by the end-user:** You can store entity IDs like vectors, hashmaps, objects, custom data structures, nothing at all or however you like for keeping things organized in your game.
---
## Core Data Model
All data lives in a `World`: a collection of per-type slabs, one contiguous array per type registered in `AtomTypes`.
```cpp
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3>;
```
Adding a new type costs one word. Each slab is fixed-size and cache-friendly. `iter<T>` is a pure linear scan with no branching. Each atom carries a `uint8_t` tag for O(1) type dispatch. A `ReverseIndex` per type allows O(1) back-querying from entity ID to component with no impact on iteration performance.

---
## Entities & Composition
Entities are `uint32_t` IDs. Components are typed atoms attached to them at runtime.
```cpp
World world(1024);

uint32_t zombie = world.create_entity();
world.add_component<int32_t>(zombie, 100);
world.add_component<float>(zombie, 3.5f);
world.add_component<uint32_t>(zombie, 3);
```
---
## API
```cpp
// Entity lifecycle
uint32_t e = world.create_entity();
world.destroy_entity(e);

// Component add & remove
world.add_component<Vec3>(e, { 1, 2, 3 });
world.remove_component<Vec3>(e);

// Direct access
T* val = world.get_component<Vec3>(e);
bool exists = world.has_component<Vec3>(e);

// Handle (reference proxy)
auto h = world.get_handle<Vec3>(e);
*h = { 1, 2, 3 };

// Iteration (value only, fastest)
world.iter<Vec3>([](Vec3& v) { ... });

// Iteration with entity ID for back-querying
world.iter_with_entity<Vec3>([&](uint32_t entity_id, Vec3& pos) {
    float* spd = world.get_component<float>(entity_id);
});
```
---
## Back-querying
The `ReverseIndex` sits alongside each slab as a cold side-table. During `iter_with_entity` you get the entity ID for free with each component, and can then fetch sibling components in O(1). The slab iteration itself is unaffected.

```cpp
world.iter_with_entity<Vec3>([&](uint32_t entity_id, Vec3& pos) {
    float* spd = world.get_component<float>(entity_id);
    if (spd)
        pos.x += *spd;
});
```
Use whichever component type has fewer entities as the iteration entry point to keep the loop short.

---
## Memory Model
Slabs are heap-allocated once at `World` construction. Nothing is allocated per atom. `remove_component<T>` returns the slot to a freelist for reuse. Everything is cleaned up when `World` goes out of scope.

| Strategy | Use case |
|---|---|
| `remove_component<T>(e)` | Return a single slot to the freelist, O(1) |
| `destroy_entity(e)` | Remove all components attached to an entity |

---
## Scripting (Lua)
`dispatch` is the planned Lua bridge. Push any atom to Lua without knowing its type at the C++ call site, pull values back by tag. Game data changes without recompilation. Only new types in `AtomTypes` require a rebuild.

---
## Why this over ECS?
Cache-friendly per-type storage with zero archetype overhead and a scripting bridge that doesn't require recompilation. Entities are runtime-composed bags of atoms. No archetype migrations, no component registries: Javascript-like freedom.

---
## Planned
- **Concurrency:** Tagged task queues, decoupled per system, thread-safe slab access via coroutines
- **Overflow:** Linked slab extension if capacity is exceeded at runtime (Linked list style probs)
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
