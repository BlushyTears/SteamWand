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

- `World`: owns one slab per type and handles deferred cleanup.
- `Slab<T>`: contiguous storage for one type, with a presence bitmap.
- `Atom<T>`: a 16-byte handle to a value, checked against its World and generation.
- `WorldRef`: a handle to a World that follows it through moves.

---

## Adding and Getting Values

```cpp
struct PlayerData { int level; float speed; };

World world(1024);

Atom<int32_t> score = world.add<int32_t>(42);
Atom<PlayerData> player = world.add<PlayerData>({ 10, 5.5f });
Atom<PlayerData> other = world.emplace<PlayerData>(3, 1.0f);

if (PlayerData* p = world.get(player)) {
    p->level++;
}
```

`get()` returns `nullptr` once the value is removed, or if the Atom belongs to another World. Removed slots get reused, but an old Atom never matches the new value. `world.is_live(atom)` and `atom.is_valid()` check whether a value still exists.

---

## Iteration

```cpp
for (PlayerData& p : world.iter<PlayerData>()) {
    p.speed += 0.1f;
}
```

`add()` and removal aren't allowed inside a loop and throw `std::logic_error`. Queue them instead; they take effect at `cleanup()`. `queue_add()` still returns the new Atom right away:

```cpp
for (auto [atom, enemy] : world.iter_atoms<Enemy>()) {
    if (enemy.hp <= 0) {
        world.queue_free(atom);
        world.queue_add(Coin{ enemy.position });
    }
}
world.cleanup();
```

Slabs don't relate to each other, so slot 3 of one type has nothing to do with slot 3 of another. Put related fields in one struct, or link values with Atoms.

---

## Raw Slots

```cpp
RawSlots<int32_t> ints = world.raw_slots<int32_t>();
for (uint32_t i = 0; i < ints.extent; ++i) {
    if (ints.is_live(i)) {
        ints.data[i] += 10;
    }
}
```

`extent` includes holes, so loop to `extent`, not `size<T>()`.

---

## World of Worlds

```cpp
World universe(10);

World nested(100);
nested.add<int32_t>(100);

World& attached = universe.attach_world(nested); // nested is now empty
```

A World inside your own type counts as a child too:

```cpp
struct Room { World items{ 64 }; int id; };

World level(16);
level.add(Room{});
level.cleanup_tree(); // also cleans up every room's items
```

A World created inside a value after it was added (for example pushed into a `std::vector<World>` member later) isn't tracked. Destroying or discarding a World destroys its children too.

Get a value's World with `atom.world()`, or keep a `WorldRef` from `world.ref()`. Both follow the World through moves and return `nullptr` once it's destroyed. A raw `World*` doesn't follow moves.

---

## Moving Values Between Worlds

```cpp
enemy = chunkA.move_to(enemy, chunkB);
```

You get a new Atom and the old one stops working. If the destination is full it throws and nothing moves. Moving a child World keeps the Atoms inside it valid.

`move_to()` isn't allowed inside a loop. Use `queue_move_to()` there; the value moves at `cleanup()`:

```cpp
for (auto [atom, enemy] : chunkA.iter_atoms<Enemy>()) {
    if (enemy.position.x > chunkWidth) {
        chunkA.queue_move_to(atom, chunkB);
    }
}
chunkA.cleanup();
```

A queued move doesn't give you the new Atom, so use `move_to()` when you need it.

---

## Clearing and Discarding

```cpp
world.clear<Enemy>();
world.discard();
```

`clear<T>()` empties one type and `discard()` empties everything, including anything queued. Both keep the memory for reuse.

---

## Systems

A system is a function a World runs on its own data. `update()` runs a World's systems in the order they were added, calls `cleanup()`, then updates every World inside it the same way:

```cpp
void poison(World& world, float dt) {
    for (auto [atom, enemy] : world.iter_atoms<Enemy>()) {
        enemy.hp -= 10;
        if (enemy.hp <= 0) {
            world.queue_free(atom);
        }
    }
}

swamp.add_system(poison);
cave.add_system(poison);
level.add_system([&renderer](World& world, float dt) { renderer.draw(world); });

level.update(dt);
```

The same system can run on several Worlds, each on its own data. Systems stay through `discard()` and move with their World.

---

## Capacity

Every type gets the `cap` from `World(cap)`. Going over throws `std::length_error`, and queued values count toward it. Give a type its own size, or let it grow from `cap`, before its first value:

```cpp
World scene(16);
scene.set_capacity<RenderObject>(10000);
scene.set_max_capacity<Particle>(10'000'000); // grows from 16 up to 10M
```

Growing slabs reserve address space up front and commit memory as they fill, so values never move. They commit at least ~200 KB, so keep small types fixed.

---

## Memory and Errors

Values never move. A pointer to a value stays valid until that value is removed, even when its World moves or its slab grows. Removed values leave holes that new values fill.

`raw_slots()` isn't checked like `iter()`. Fetch it again after adding to see new slots, and don't keep pointers to removed values.

Misuse such as adding during a loop, ownership cycles or going over capacity throws. Destroying a World while it's being iterated terminates the program.

---

## Threads

Separate Worlds can be used on separate threads. A single World isn't thread-safe, so lock it yourself if two threads share it. Type IDs aren't stable across runs, so don't save them.

---

## Planned Features

Dense packing, maybe coroutines, interface for domain-specific functions and serialization remain future work.

---

## License

Open source forever. You may not (and should not since I'm new to game engines) use this as training data for your LLM without explicit permission.
