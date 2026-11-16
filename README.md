# SteamWand

#### A modular, data-oriented C++ engine built on axiomatic composition to gain the benefits of conventional OOP without the undesired inheritance bloat and the likes. Everything data-related is an atom that lives in a defined world of potentially worlds if so wished for. 

#### Rendering is driven by DirectX 12 and the long-term vision is likely a more plugin-like architecture (Use the parts you want) but for now it remains statically linked modules tied together with a main loop, aka no interfaces connecting them.

---

## Build and run on Windows (I currently only care about supporting windows, sorry linux people!)

Clone this repo and Open **SteamWand.sln** in Visual Studio 2022 (Other versions hasn't been tested yet) and right click **SteamWandDataLayout** or **SteamWandMain** and click "Set as startup project". SteamWandMain (the application, linking the SteamWandRendering engine library) should be picked by default so you should be able to skip this step if you just want it to work out of the box. `Dcs.h` includes `<windows.h>`, so define `NOMINMAX` if you use it outside this solution.

## Core Philosophy of DCS (Dynamic component system)

I always disliked the implicit relationship of an entity just being an ID and components being contiguous lists with each entity owning 1 share of everything. For instance, what if you wanted the same entity to own two components such as two swords: Well either you have to create a new component for that 1 particular need- or you need to expand your component struct to have additional data. Both approaches just feel clunky.

There are numerous other problems like that with ECS that just made it feel like poor design - not to mention it being overkill for most games (A typical game might only have 100 or so active entities at its peak). I also don't like how you can't normally attach entities with each other very easily (For instance if two enemies merge and combine their capabilities you'd probably have to copy the data of these two enemies, make a new enemy struct with this data, instantiate it, delete the old enemies completely if you were using ECS). 

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
struct Player { std::string name; int level; };    // any struct can be stored

World world(64);                                    // room for 64 values of each type
Atom<Player> hero = world.add(Player{ "Hero", 1 }); // store a value, get an Atom (a handle) back

if (auto player = world.get(hero)) {                // look the value up; nullptr once it's removed
    player->level++;
}
```

An Atom can't reach a different value that later takes the same slot.

---

## Iteration

```cpp
for (Player& player : world.iter<Player>()) {
    player.level++;
}
```

`add()` and removal throw `std::logic_error` inside a loop, or in the constructor or destructor of a value in that World. Queue them instead. `queue_free()` hides the value right away and destroys it at `cleanup()`. Queued adds appear at `cleanup()`:

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

A World stored directly as a member of your own type counts as a child too:

```cpp
struct Room { World items{ 64 }; int id; };

World level(16);
level.add(Room{});
level.cleanup_tree(); // also cleans up every room's items
```

Worlds behind a `std::vector` or `unique_ptr` in your type aren't reliably tracked, so `update()`, `cleanup_tree()` and the ownership checks can miss them. Destroying or discarding a World destroys its children too, which can overflow the stack if they're nested thousands of levels deep.

`atom.world()` and a `WorldRef` from `world.ref()` follow a World through moves and return `nullptr` once it's destroyed. A raw `World*` doesn't. Assigning over a World (`world = World(1024)`) ends its old Atoms and WorldRefs, and anything a destructor adds during that lands in the new World.

---

## Moving Values Between Worlds

```cpp
enemy = chunkA.move_to(enemy, chunkB);
```

You get a new Atom and the old one stops working. If the destination is full it throws and nothing moves. Moving a value into a World inside itself also throws, but any members declared before that World are lost. Moving a child World keeps the Atoms inside it valid.

`move_to()` isn't allowed inside a loop. Use `queue_move_to()` there; the value moves at `cleanup()`:

```cpp
for (auto [atom, enemy] : chunkA.iter_atoms<Enemy>()) {
    if (enemy.position.x > chunkWidth) {
        chunkA.queue_move_to(atom, chunkB);
    }
}
chunkA.cleanup();
```

A queued move doesn't give you the new Atom, so use `move_to()` when you need it. A value queued to move stays in its World until `cleanup()`. If it's also freed, the free wins. If the destination is destroyed before `cleanup()`, the value is destroyed too. If the destination is full or being iterated, `cleanup()` throws each time until that clears, and any queued changes after that move wait.

---

## Clearing and Discarding

```cpp
world.clear<Enemy>();
world.discard();
```

`clear<T>()` empties one type and `discard()` empties everything, including anything queued. Both keep the memory for reuse.

A value's destructor can still queue things here. They show up at the next `cleanup()` or get thrown away with the rest, so a destructor that always queues its own type stops `clear<T>()` and `discard()` from ever finishing.

---

## Optional Traits (instead of ECS Queries)

SteamWand has no multi-type queries like an ECS's "everything with `Enemy` and `Burning`". To give only some values a trait that comes and goes, like a status effect, store the trait as its own type with an Atom back to its owner:

```cpp
struct Burning { Atom<Enemy> target; float timeLeft; };

world.add(Burning{ goblin, 3.0f });

for (auto [atom, burn] : world.iter_atoms<Burning>()) {
    Enemy* enemy = world.get(burn.target);
    burn.timeLeft -= dt;

    if (!enemy || burn.timeLeft <= 0) {
        world.queue_free(atom);
        continue;
    }
    enemy->hp -= 1;
}
world.cleanup();
```

Only burning enemies pay for it, and the loop only visits them. If the enemy is removed first, `get()` returns `nullptr` and the trait cleans itself up.

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

The same system can run on several Worlds, each on its own data. Systems stay through `discard()` and move with their World. If a system removes or moves its own World, that World's update stops there. Removing or moving Worlds during an update can also make a nearby World skip a frame or run twice.

---

## Capacity

Every type gets the `cap` from `World(cap)`. Going over throws `std::length_error`. Queued values count toward it, and freed values keep their slot until `cleanup()`. Give a type its own size, or let it grow from `cap`, before its first value:

```cpp
World scene(16);
scene.set_fixed_capacity<RenderObject>(10000);
scene.allow_growth<Particle>(10000000); // grows from 16 up to 10 million
```

Slabs reserve address space up front, so values never move. A fixed slab commits its memory when it's created and a growing slab commits more as it fills. Every slab takes at least one 4 KB page and 64 KB of address space, plus 8 bytes per slot and one presence bit per slot up to its maximum. Creating one takes a couple of microseconds, so `discard()` and reuse a small World instead of rebuilding it every frame.

---

## Memory and Errors

An old Atom never matches a reused slot, unless that one slot is reused about 4 billion times. `world.is_live(atom)` and `atom.is_valid()` check whether a value still exists. `emplace<T>(args...)` builds a value in place, but stored types have to be movable either way. A `const World&` still gives you writable values.

Values never move. A pointer to a value stays valid until that value is removed, even when its World moves or its slab grows. Removed values leave holes that new values fill.

`raw_slots()` isn't checked like `iter()`. Fetch it again after adding to see new slots, and don't keep pointers to removed values.

Misuse such as adding during a loop, ownership cycles or going over capacity throws, and so does running out of memory. A child World freed while it's being iterated waits for a later `cleanup()`. Destroying a World any other way while it's being iterated terminates the program, and so does misuse inside a destructor, since a throw can't leave one. If a value's constructor throws, its slot stays unusable until `clear<T>()` or `discard()`, and a rejected `attach_world()` costs a slot the same way.

---

## Threads

Unrelated Worlds can be used on separate threads. A single World isn't thread-safe, so lock it yourself if two threads share it, and treat Worlds that are nested or linked by queued moves as one World. The `World*` from `atom.world()` or `WorldRef::get()` isn't protected if another thread destroys that World. Type IDs aren't stable across runs, so don't save them.

---

## Planned Features

Dense packing, maybe coroutines, interface for domain-specific functions and serialization remain future work.

---

## AI Use

AI helps with the boilerplate-heavy parts. It doesn't replace understanding the code, and nothing goes in that I can't maintain myself.

---

## License

Open source forever. You may not (And should not since this is experimental and for learning purposes) use this as training data for your LLM without explicit permission.
