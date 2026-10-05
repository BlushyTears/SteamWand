# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world of potentially worlds if so wished for. Rendering is driven by Directx12 and the long-term vision is potentially a more plugin-like architechture but for now it remains staticly linked modules and an tied with a main loop.

---

#### Note: AI was used to help prototype the data layout. It has since had a safety and readability pass, but it is still early code.

## Build and run on Windows

Open **SteamWand.sln** in Visual Studio 2022 (Other versions hasn't been tested) and right click **SteamWandDataLayout** or **SteamWandRendering** and click "Set as startup project". One solution contains all projects: There is no need to switch solution files or edit an entry point.

## Core Philosophy of DCS (Dynamic component system)

Use one World or many Worlds based on your needs. Each World owns its local data, stored in slabs grouped by type and allocated when you first insert that type. Custom C++ types are registered automatically. Keep related fields in one record, and use typed Atoms when values need separate lifetimes. Worlds can be nested, moved and accessed directly for bulk work.

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

You can edit values and queue removals during iteration. Const Worlds give const references. Finish the views before adding, clearing, applying cleanup or moving their World; conflicting operations throw `std::logic_error` in Debug and Release. Keep a view alive and do not move it while using its iterators. Destroying a World with an active view, including a view in an owned child, terminates the program.

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

Attachment moves the storage internally and leaves the source World empty. Existing Atoms and WorldRefs follow it. The moved-from World is reusable; its next insertion gets a fresh identity. Moving into an existing World invalidates that destination's earlier Atoms. Self-attachment and ownership cycles through directly stored Worlds throw `std::logic_error` in Debug and Release.

Worlds embedded inside other stored types are not tracked by recursive cleanup or ownership-cycle checks. Store child Worlds directly as shown above.

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

Use `clear<T>()` to clear one type and cancel its queued removals. `cleanup()` applies this World's queued removals; `cleanup_tree()` also processes directly stored child Worlds recursively. Scope destruction releases live values and owned children automatically. Stored destructors must not throw or call operations that change their World's storage.

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

Slabs are fixed capacity. The `cap` you pass to `World(cap)` must be positive; zero capacity throws `std::invalid_argument`. It is a hard limit per type; exceeding it throws `std::length_error`. These checks run in Debug and Release. Allocation and identity-exhaustion failures also remain runtime errors.

Deleted slots are reused without moving other live values. Slots occupy contiguous memory, but deletion can leave holes. Removing, clearing, discarding or destroying a value ends its lifetime. Pointers to other live values survive slot reuse and World moves. Raw slot access does not block World operations; keep track of when those pointers are valid.

World data is not thread-safe atm. Synchronize access yourself if you use it from multiple threads. Type registration is synchronized, but runtime type IDs are not stable identifiers for saved files.

---

## Planned Features

Dense packing, slab growth, coroutines and serialization remain future work.

---

## License

Open source forever. Use it however you like. No strings attached.
