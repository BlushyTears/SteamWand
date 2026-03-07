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
## Core Data Model
All data lives in a `World`: a collection of per-type slabs, one contiguous array per type registered in `AtomTypes`.

```cpp
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3>;

------------------------------------------------------------------------

# Core Data Model

All data lives inside a `World`. The world owns **one slab per type**
defined in `AtomTypes`.

``` cpp
using AtomTypes = std::tuple<int32_t, uint32_t, float, Vec2, Vec3, bool>;
```

Each type gets its own slab:

    World
     ├─ TypedSlab<int32_t>
     ├─ TypedSlab<uint32_t>
     ├─ TypedSlab<float>
     ├─ TypedSlab<Vec2>
     ├─ TypedSlab<Vec3>
     └─ TypedSlab<bool>

Each slab stores several arrays:

    values[]
    tags[]
    generations[]
    handles[]
    freelist

This produces a **structure-of-arrays layout** optimized for iteration.

------------------------------------------------------------------------

# Atom Structure

Atoms are lightweight handles referencing slab storage.

``` cpp
struct AtomBase {
    uint32_t index;
    uint8_t  tag;
    uint8_t  generation;
    uint16_t entity_id;
};
```

Fields:

  Field        Purpose
  ------------ -----------------------------
  index        Slot index in slab
  tag          Runtime type identifier
  generation   Detects stale handles
  entity_id    Optional entity association

Handles remain stable until freed.

------------------------------------------------------------------------

# Creating Atoms

Atoms are created through the `World`.

``` cpp
World world(1024);

AtomBase* hp = world.create(int32_t(100));
AtomBase* speed = world.create(float(3.5f));
AtomBase* position = world.create(Vec3{1,2,3});
```

Atoms can optionally be associated with an entity:

``` cpp
AtomBase* hp = world.create(int32_t(100), entity_id);
```

------------------------------------------------------------------------

# Typed Access

Values are accessed using their type.

``` cpp
world.value_of<int32_t>(hp) = 200;
```

Handles provide convenient proxy access:

``` cpp
auto h = world.get_handle<Vec3>(position);
*h = {5,0,3};

if (h.valid())
{
    auto entity = h.entity_id();
}
```

------------------------------------------------------------------------

# Iteration

Fast iteration across slabs:

``` cpp
world.iter<Vec3>([](Vec3& pos)
{
    pos.x += 1;
});
```

Index-aware iteration:

``` cpp
world.iter_with_index<Vec3>([](Vec3& pos, uint32_t index)
{
    pos.y += index;
});
```

These loops scan **only active slots**.

------------------------------------------------------------------------

# Entities

Entities can be simple collections of atoms.

``` cpp
std::vector<AtomBase*> zombie = {
    world.create(int32_t(100)),
    world.create(float(2.0f)),
    world.create(Vec3{0,0,0})
};
```

Cloning entities:

``` cpp
auto boss = world.clone_entity(zombie);
```

Freeing entities:

``` cpp
world.free_entity(zombie);
```

Because atoms store `entity_id`, entity tracking can also be implemented
externally.

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
positions.iter([&](uint32_t entity, AtomBase* atom)
{
    Vec3& pos = world.value_of<Vec3>(atom);
});
```

Reverse indices are optional and independent of the world.

------------------------------------------------------------------------

# Component Groups

Component groups allow lightweight archetype-style structures.

``` cpp
auto entity = world.create_entity(
    Vec3{0,0,0},
    float(10),
    bool(true)
);
```

Group iteration:

``` cpp
world.iter_group(groups, [](Vec3& pos, float& speed, bool& active)
{
    if(active)
        pos.x += speed;
});
```

This provides structured iteration without archetype migration.

------------------------------------------------------------------------

# Parallel Queries

Lockstep iteration across multiple slabs:

``` cpp
world.query_parallel<Vec3, float>([](Vec3& pos, float& speed)
{
    pos.x += speed;
});
```

Useful when components share aligned indices.

------------------------------------------------------------------------

# Memory Model

Slabs allocate fixed arrays at world creation:

``` cpp
World world(1024);
```

Each slab receives capacity `1024`.

Atoms are reused using a freelist.

Memory operations:

  Operation     Description
  ------------- -------------------------
  create        allocate slot
  free          return slot to freelist
  clear         reset entire slab
  free_entity   free multiple atoms

No per-atom `new` or `delete` is ever performed.

------------------------------------------------------------------------

# Generation Safety

Handles include generation numbers.

When an atom is freed:

    generation++

Access checks verify:

    handle.generation == slab.generation[index]

This prevents use-after-free errors.

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

# Adding a New Type

1.  Define the datatype

``` cpp
struct Health
{
    float current;
    float max;
};
```

2.  Add it to `AtomTypes`

``` cpp
using AtomTypes = std::tuple<
    int32_t,
    uint32_t,
    float,
    Vec2,
    Vec3,
    bool,
    Health
>;
```

3.  Optionally implement printing

``` cpp
std::ostream& operator<<(std::ostream& os, const Health& h)
{
    return os << h.current << "/" << h.max;
}
```

Recompile once to update the type system.

------------------------------------------------------------------------

# Why SteamWand?

Compared to typical ECS frameworks:

  Feature                      SteamWand
  ---------------------------- -----------
  Per-type slabs
  Runtime dispatch
  Generation-safe handles
  Optional entity model
  Minimal engine assumptions
  No archetype migration

SteamWand focuses on **simple, predictable, data-oriented storage**
rather than a full gameplay framework.

------------------------------------------------------------------------

# Planned Features

Future ideas include:

-   multi-threaded slab iteration
-   coroutine-friendly task systems
-   dynamic slab expansion
-   compile-time query helpers
-   serialization utilities
-   scripting integration

------------------------------------------------------------------------

# License

Open source forever. Use it however you like - no strings attatched.
