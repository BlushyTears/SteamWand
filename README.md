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

# Core Data Model

All data lives inside a `World`. The world owns **one slab per type**
defined in `AtomTypes`.

``` cpp
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3, bool>;
```
------------------------------------------------------------------------

# Creating Atoms

Atoms are created through the `World`.

``` cpp
World world(1024);

AtomBase* hp = world.create(int32_t(100));
AtomBase* speed = world.create(float(3.5f));
AtomBase* position = world.create(Vec3{1,2,3});
```

Atoms can optionally be associated with an entity (Or not, you can just store the address directly in a vector or something):

``` cpp
AtomBase* hp = world.create(int32_t(100), entity_id);
```

------------------------------------------------------------------------

# Iteration

Fast iteration across slabs:

``` cpp
world.iter<Vec3>([](Vec3& pos){
    pos.x += 1;
});
```

Index-aware iteration:

``` cpp
world.iter_with_index<Vec3>([](Vec3& pos, uint32_t index) {
    if (index % 2 == 0) pos.x += 1.0f;
});
```

These loops only scan **active slots**.

------------------------------------------------------------------------

# Entities

Entities represent a collection of atoms (Am considering molecyles over entities).

``` cpp
std::vector<AtomBase*> zombie = {
    world.create(int32_t(100)),
    world.create(float(2.0f)),
    world.create(Vec3{0,0,0})
};
```
Cloning the same entities:

``` cpp
auto boss = world.clone_entity(zombie);
```

Freeing original entities does not impact the copy:

``` cpp
world.free_entity(zombie);
```

Because atoms store `entity_id`, entity tracking can also be implemented
externally. 

Or if you like OOP, you can store atoms in a conventional object. The sky is the limit.
Note: Currently only vectors are supported for helper functions such as free, clone etc.

``` cpp
struct Character {
    AtomBase* health;
    AtomBase* strength;
    AtomBase* speed;
};
```

------------------------------------------------------------------------

# Reverse Index

`ReverseIndex<T>` provides fast lookup from **entity → component**.

``` cpp
ReverseIndex<Vec3> positions(1024);

positions.insert(entity_id, atom);

AtomBase* found = positions.get_atom(entity_id);
```

Iteration with entity context:

``` cpp
positions.iter([&](uint32_t entity, AtomBase* atom) {
    Vec3& pos = world.value_of<Vec3>(atom);
});
```

Reverse indices are optional and independent of the world.

------------------------------------------------------------------------

# Component Groups

Component groups allow loosely coupled archetype-style structures.

``` cpp
auto entity = world.create_entity(
    Vec3{0,0,0},
    float(10),
    bool(true)
);
```

Group iteration:

``` cpp
world.iter_group(groups, [](Vec3& pos, float& speed, bool& active) {
    if(active)
        pos.x += speed;
});
```

This provides structured iteration without archetype-like migration as seen below.

------------------------------------------------------------------------

# Parallel Queries

Lockstep iteration across multiple slabs:

``` cpp
world.query_parallel<Vec3, float>([](Vec3& pos, float& speed){
    pos.x += speed;
});
```

Useful when components share aligned indices.

------------------------------------------------------------------------

# Printing

Atoms can be printed generically:

``` cpp
world.print(atom);
```

Custom types can implement:

``` cpp
std::ostream& operator<<(std::ostream&, const T&)
```

------------------------------------------------------------------------

## Adding a New Type

1. Define your datatype (type alias, enum, struct, union)
2. Add it to `AtomTypes`
3. Define `operator<<` if you want `print` to support it

```cpp
struct Health { float current, max; };

inline std::ostream& operator<<(std::ostream& os, const Health& h) { return os << h.current << "/" << h.max; }

using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3, Health>; // health added here

```
------------------------------------------------------------------------

## Why SteamWand
Cache-friendly per-type storage with zero archetype overhead and a scripting bridge that does not require recompilation. Entities are runtime-composed bags of atoms. No archetype migrations, no component registries: Javascript-like freedom that respects the programmer with more tools - not less.

------------------------------------------------------------------------

## Planned Features
Future ideas include:

- multi-threaded slab iteration
- coroutine-friendly task systems
- dynamic slab expansion
- compile-time query helpers
- serialization utilities
- scripting integration

---

## License
Open source forever. Use it however you like — no strings attached.

------------------------------------------------------------------------
