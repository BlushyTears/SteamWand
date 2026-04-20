# SteamWand

#### A modular, data-driven C++ engine built on axiomatic composition. Everything is an atom that lives in a defined world of potentially worlds if so wished for.

---

## Core Philosophy

- **Use one world or many worlds:** Prototype with a single `World`, nest worlds inside other worlds or keep many decoupled worlds based on your needs.
- **Pay for what you use:** Data is stored per type in contiguous slabs, and nothing is allocated until you create it.
- **Compose freely:** Storage is runtime-driven, any C++ type works out of the without needing macros.
- **Stay flexible:** Worlds can be nested, moved, and accessed directly when you want maximum control.
- **Respects the programmer**: SteamWand aims to be an engine that lets the end-user do more, not less with infinite guardrails.

---

## Core Types

SteamWand centers around a small set of building blocks:

- `World`: owns type slabs, tracks storage, and handles deferred cleanup.
- `Slab<T>`: per-type storage with aligned allocation and metadata.
- `Atom`: a lightweight generational reference with `id` and `gen`.
- `View<Types...>`: multi-slab query system for type-safe iteration using bitmasks for sparse iteration.

---

## Creating Atoms

`World::add<T>()` returns an `Atom` with id and generation:

```cpp
World world(1024);

Atom intAtom = world.add<int32_t>(42);
Atom playerAtom = world.add<PlayerData>({10, 5.5f});
```

---

## Safe Atom Access

`World::get<T>()` resolves atoms safely:

```cpp
int32_t* value = world.get<int32_t>(intAtom);
if (value) {
    std::cout << "Value: " << *value << "\n";
}
```

Returns `nullptr` for stale/deleted atoms.

---

## Direct Slab Access

Fast bulk iteration for systems:

```cpp
int32_t* ints = world.get_array<int32_t>();
for (size_t i = 0; i < world.size<int32_t>(); ++i) {
    ints[i] += 10;
}
```

---

## Multi-Type Queries with `View`

`World::view<Types...>()` creates type-safe multi-slab iterators using bitmask presence tracking:

```cpp
// Example: Zombies with HP, Speed, Position
auto zombie_view = world.view<int32_t, Vec3, float>();

zombie_view.each([](int32_t& hp, Vec3& pos, float& speed) {
    if (hp > 0) {
        pos.x += speed * 0.016f;
        hp -= 1;
    }
});
```

---

## Reverse lookup:

```cpp
int32_t* ints = world.get_array<int32_t>();
for (size_t i = 0; i < world.size<int32_t>(); ++i) {
    World* owner = world.get_world_at<int32_t>(i);
    std::cout << "Index " << i << ": " << ints[i] << " (owner: " << owner << ")\n";
}
```

---

## World of Worlds

Store worlds as atoms for hierarchy (Similar concept to Godot scenes):

```cpp
World universe(10);
World nested(100);
nested.add<int32_t>(100);

Atom nestedAtom = universe.add<World>(std::move(nested));
World* active = universe.get<World>(nestedAtom);
```

---

## Generational Atoms

Prevents stale references:

```cpp
Atom a = world.add<int32_t>(42);
world.get<int32_t>(a);  // Works

world.queue_free(0);
world.cleanup();
world.get<int32_t>(a);  // now a nullptr
```

---

### Custom types

```cpp
void basicExamples() {
    World world(1024);
    
    struct PlayerData { int level; float speed; };
    world.add<PlayerData>({10, 5.5f});
    
    // View across custom + built-in types
    auto players_view = world.view<PlayerData, int32_t>();
    players_view.each([](PlayerData& pd, int32_t& score) {
        pd.speed += 0.1f;
        score += pd.level;
    });
}
```

---

## Current Characteristics

- **Dynamic typing**: Any C++ type via `TypeInfo<T>::id()`
- **Multi-type views**: `view<Types...>()` for queries using bitmask iteration
- **Generation marking**: No compaction, sparse over time
- **Zero boilerplate**: No macros, no registration

## Technical Constrains:
- Deleting atoms in a world over time creates gaps due to no established degramentation in place (The workarounds are either: **Solution 1:** Delete that world after a while and replace it with a new one (Which is a pretty fast thing to do), or **Solution 2:** avoid deleting and just disable/reset it if you want tightly packed data for optimal cache locality). In most cases these holes don't matter because it's very typical to discard data after it has been used (*Solution 1*) or just reset state, such as setting bool isAlive to false (*Solution 2*).

For this reason, I have opted to not have defragmentation implemented as it therefore would for implicit & ecs-inspired, multi-world access. (If World A's ground slabs is to affect World B's character transform slabs to discover what kind of ground the character is walking on, then it's important that the character slabs don't get re-organized).

---

## Planned Features

- Non-disruptive defragmentation (No pop and swap)
- Coroutines implementation
- Serialization
- Replacing lambdas with neat iterators

---

## License

Open source forever. Use it however you like — no strings attached.
