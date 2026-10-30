# SteamWand

#### A modular, data-oriented C++ engine built on axiomatic composition to gain the benefits of conventional OOP benefits without the undesired inheritence bloat and the likes. Everything data-related is an atom that lives in a defined world of potentially worlds if so wished for. 

#### Rendering is driven by Directx12 and the long-term vision is likely a more plugin-like architechture (Use the parts you want) but for now it remains staticly linked modules and an tied with a main loop, aka no interfaces connecting them.

---

## AI Note:

#### The philosophy of SteamWand is LLM-based assistance without reliance/vibe-coding. In other words; AI is used to speed up the more boring/boilerplate-ish parts of the development without inducing AI-related tech debt.

---

## Build and run on Windows

Clone this repo and Open **SteamWand.sln** in Visual Studio 2022 (Other versions hasn't been tested yet) and right click **SteamWandDataLayout** or **SteamWandMain** and click "Set as startup project". SteamWandMain (the application, linking the SteamWandRendering engine library) should be picked by default so you should be able to skip this step if you just want it to work out of the box.

## Core Philosophy of DCS (Dynamic component system)

I always disliked the implicit relationship of an entity just being an ID and components being contigious lists with each entity owning 1 share of everything. For instance, what if you wanted the same entity to own two components such as two swords: Well either you have to create a new component for that 1 particular need- or you need to expand your component struct to have additional data. Both approaches just feel clunky.

There are numerous other problems like that with ECS that just made it feel like poor design - not to mention it being overkill for most games (A typical game might only have 100 or so active entities at its peak). I also don't like how you can't normally attatch entities with each other very easily (For instance if two enemies merge and combine their capabilities you'd probably have to copy the data of these two enemies, make a new enemy struct with this data, instantiate it, delete the old enemies completely if you were using ECS). 

I wanted to go against the norm with this system. The whole idea is that you revert the relationship and make things from the ground up, and compose them based on the requirements of your particular needs without having to compromise or perform hacky solutions. Each World owns its data locally, is stored in slabs grouped by type and allocated when you first insert that type. Custom C++ types are registered automatically without you having to do anything. Worlds can be nested, moved and accessed directly for bulk work, or you can have a more flexible system than OOP whilst still being extremely fast.

---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and a presence bitmap.
- `Atom<T>`: a typed handle into a slab, checked against its World and generation.
- `WorldRef`: a World reference that follows its storage through moves.
- `iter<T>()`: iteration over the live values of one type.

---

## Creating Atoms

`World::add<T>()` returns an `Atom<T>` referencing the stored value:

```cpp
struct PlayerData { int level; float speed; };

World world(1024);

Atom<int32_t> intAtom = world.add<int32_t>(42);
Atom<PlayerData> playerAtom = world.add<PlayerData>({10, 5.5f});
```

Use `emplace<T>(args...)` to construct a value directly in its slot.

---

## Safe Atom Access

```cpp
int32_t* value = world.get(intAtom);
if (value) {
    std::cout << "Value: " << *value << "\n";
}
```

Returns `nullptr` if the Atom has been freed or belongs to another World. Default Atoms are invalid. Reusing a slot changes its generation, so an earlier Atom cannot target its replacement. `world.is_live(atom)` and `atom.is_valid()` check whether the value still exists.

---

## Raw Slot Access

Access a World's local slots directly:

```cpp
RawSlots<int32_t> ints = world.raw_slots<int32_t>();
for (uint32_t i = 0; i < ints.extent; ++i) {
    if (ints.is_live(i)) {
        ints.data[i] += 10;
    }
}
```

The extent includes holes. `size<T>()` counts live values, so it is not a raw array bound.

---

## Iteration

`World::iter<T>()` returns a view over the live values of one type:

```cpp
// Single type
for (auto& hp : world.iter<int32_t>()) {
    hp -= 1;
}

struct Vec3 { float x, y, z; };

struct Body {
    int32_t hp;
    Vec3 position;
    float speed;
};

// Related fields belong to the same record.
for (auto& body : world.iter<Body>()) {
    if (body.hp > 0) {
        body.position.x += body.speed * 0.016f;
        body.hp -= 1;
    }
}
```

Independent slabs do not imply a relationship between values at matching slots. Values can also refer to one another through typed Atoms when they need separate lifetimes.

Use `iter_atoms<T>()` when a loop needs the handle:

```cpp
for (auto [atom, body] : world.iter_atoms<Body>()) {
    if (body.hp <= 0) {
        world.queue_free(atom);
    }
}
world.cleanup();
```

---

## Direct World Access

Get the World directly from an Atom when you want to operate on its local data:

```cpp
World* owner = intAtom.world();
if (owner) {
    for (auto& hp : owner->iter<int32_t>()) {
        hp += 10;
    }
}
```

`atom.world()` returns its World while that World exists, even if the individual value has been removed. A `WorldRef` refers to that World without a particular value:

```cpp
WorldRef room = world.ref();
World* owner = room.get();
```

A `WorldRef` follows the storage through moves, but does not keep the World alive. `get()` returns `nullptr` after its owner is destroyed. A raw World pointer is borrowed; get it again after moving the World.

---

## World of Worlds

Store Worlds inside other Worlds for ownership and grouping. Build a child World standalone, then attach it:

```cpp
World universe(10);

World nested(100);
nested.add<int32_t>(100);

World& nestedWorld = universe.attach_world(nested);   // nested is now empty
```

Attachment moves the storage internally and leaves the source World empty.

Worlds embedded inside other stored types are not tracked by recursive cleanup or ownership-cycle checks (yet). Store child Worlds directly as shown above.

---

## Atom Invalidation

```cpp
Atom<int32_t> a = world.add<int32_t>(42);
world.get(a);          // returns pointer

world.queue_free(a);
world.cleanup();
world.get(a);          // returns nullptr
```

---

## Discarding a World

When you want to throw out everything in a World and start fresh, use: `discard()`:

```cpp
World world(1024);
world.add<int32_t>(42);
world.add<std::string>("hello");

world.discard(); // every slab is now empty, allocations are kept

world.add<int32_t>(7); // reuses the same memory
```

`discard()` empties the World. Anything you added is gone, but the World itself is ready to use again. Atoms from before the discard no longer point at anything.

Use `clear<T>()` to clear one slab and cancel its queued removals. `cleanup()` applies this World's queued removals; `cleanup_tree()` also processes directly stored child Worlds recursively.

---

### Custom Types

```cpp
void example() {
    World world(1024);

    struct PlayerData {int level; float speed; int32_t score;};
    world.add<PlayerData>({10, 5.5f, 0});

    for (auto& pd : world.iter<PlayerData>()) {
        pd.speed += 0.1f;
        pd.score += pd.level;
    }
}
```

---

## Technical considerations

Slabs are fixed capacity. The `cap` you pass to `World(cap)` must be positive. It is a hard limit per type; exceeding it throws `std::length_error`, there are ideas of using a linked-list system to support bigger caps.

Deleted slots are reused without moving other live values. Slots occupy contiguous memory, but deletion will leave holes. Removing, clearing, discarding or destroying a value ends its lifetime. Pointers to other live values survive slot reuse and World moves. Raw slot access does not block World operations; keep track of when those pointers are valid.

World data is not thread-safe atm. Synchronize access yourself if you use it from multiple threads (One idea could be to give worlds a semaphore or equivalent to lock a world so only one thread can use it at once). Type registration is synchronized atm, but runtime type IDs are not stable identifiers for saved files.

---

## Planned Features

Dense packing, slab growth, maybe coroutines, interface for domain-specific functions and serialization remain future work.

---

## License

Open source forever. You may not (and should not since I'm new to game engines) use this as training data for your LLM without explicit permission.
