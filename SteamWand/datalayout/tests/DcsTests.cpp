#include "../Dcs.h"
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <latch>
#include <process.h>
#include <string_view>
#include <thread>
#include <unordered_set>

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition))                                                                                    \
            throw std::runtime_error(#condition);                                                            \
    } while (false)

template<typename Error, typename F>
void rejects(F&& operation) {
    try {
        operation();
    }
    catch (const Error&) {
        return;
    }
    throw std::runtime_error("Expected rejection");
}

template<typename H>
concept FloatHandle = requires(World& world, H handle) { world.get<float>(handle); };
static_assert(!FloatHandle<Atom<int>>);
static_assert(!std::is_copy_constructible_v<Slab<int>>);
static_assert(!std::is_copy_assignable_v<Slab<int>>);
static_assert(!std::is_move_constructible_v<Slab<int>>);
static_assert(!std::is_constructible_v<Atom<int>, uint32_t>);
static_assert(std::is_same_v<decltype(*std::declval<View<const int>::iterator>()), const int&>);

struct Counted {
    static inline int alive = 0;
    int value;

    explicit Counted(int number) : value(number) {
        if (number < 0) {
            throw std::runtime_error("Construction failed");
        }
        ++alive;
    }

    ~Counted() {
        --alive;
    }

    Counted(const Counted&) = delete;
    Counted(Counted&&) = delete;
};

struct CopyOnly {
    static inline int copies = 0;
    CopyOnly() = default;

    CopyOnly(const CopyOnly&) {
        ++copies;
    }

    CopyOnly(CopyOnly&&) = delete;
};

struct alignas(256) Aligned {
    std::array<std::byte, 256> bytes;
};

void identity_and_reuse() {
    World world(2), other(2);
    Atom<int> empty;
    CHECK(!empty.is_valid() && !empty.world() && !world.get(empty));
    CHECK(!world.find_slab<int>());
    CHECK(world.raw_slots<int>().data == nullptr);
    {
        auto view = world.iter<int>();
        CHECK(view.begin() == view.end());
    }
    CHECK(!world.find_slab<int>());

    auto first = world.add<int>(11);
    auto second = world.add<int>(12);
    auto foreign = other.add<int>(33);
    CHECK(!other.get(first) && !other.queue_free(first) && *other.get(foreign) == 33);
    CHECK(first != second && first != foreign && first != empty);
    CHECK(second != foreign && second != empty && foreign != empty);
    rejects<std::length_error>([&] { world.add<int>(13); });
    CHECK(world.size<int>() == 2 && *world.get(first) == 11);

    int* address = world.get(first);
    CHECK(world.queue_free(first) && world.queue_free(first));
    world.cleanup();
    CHECK(!first.is_valid() && !world.get(first) && world.size<int>() == 1);
    auto replacement = world.add<int>(22);
    CHECK(world.get(replacement) == address && replacement != first);
    CHECK(!world.queue_free(first) && *world.get(replacement) == 22);
    CHECK(*world.get(second) == 12);

    world.queue_free(replacement);
    world.discard();
    auto after_discard = world.add<int>(44);
    world.cleanup();
    CHECK(!world.get(first) && !world.get(second) && !world.get(replacement));
    CHECK(*world.get(after_discard) == 44 && world.size<int>() == 1);
    world.clear<int>();
    CHECK(!world.get(after_discard));

    World queued(4);
    auto clear_first = queued.add<int>(1);
    auto remove_later = queued.add<float>(2.0f);
    auto clear_second = queued.add<int>(3);
    auto keep = queued.add<float>(4.0f);
    queued.queue_free(clear_first);
    queued.queue_free(remove_later);
    queued.queue_free(clear_second);
    queued.queue_free(clear_first);
    queued.clear<int>();
    auto after_clear = queued.add<int>(5);
    queued.cleanup();
    CHECK(!queued.get(clear_first) && !queued.get(clear_second));
    CHECK(!queued.get(remove_later) && *queued.get(keep) == 4.0f);
    CHECK(*queued.get(after_clear) == 5);

    World churn(1);
    auto old = churn.add<int>(0);
    churn.queue_free(old);
    churn.cleanup();
    for (int i = 0; i < 100000; ++i) {
        auto atom = churn.add<int>(i);
        CHECK(churn.size<int>() == 1 && !churn.get(old));
        churn.queue_free(atom);
        churn.cleanup();
    }
    CHECK(churn.size<int>() == 0 && churn.raw_slots<int>().extent == 1);
}

void construction_and_destruction() {
    rejects<std::invalid_argument>([] { World zero(0); });
    rejects<std::invalid_argument>([] { Slab<int> zero(0); });
    {
        World world(1);
        rejects<std::runtime_error>([&] { world.emplace<Counted>(-1); });
        CHECK(world.size<Counted>() == 0 && Counted::alive == 0);
        auto first = world.emplace<Counted>(1);
        world.queue_free(first);
        world.cleanup();
        rejects<std::runtime_error>([&] { world.emplace<Counted>(-1); });
        auto second = world.emplace<Counted>(2);
        CHECK(world.get(second)->value == 2 && Counted::alive == 1);
        world.queue_free(second);
    }
    CHECK(Counted::alive == 0);
    World world(4);
    CopyOnly value;
    world.add<CopyOnly>(value);
    CHECK(CopyOnly::copies == 1);
    auto aligned = world.emplace<Aligned>();
    CHECK(reinterpret_cast<uintptr_t>(world.get(aligned)) % alignof(Aligned) == 0);
    auto movable = world.add(std::make_unique<int>(7));
    CHECK(**world.get(movable) == 7);
    auto retained = world.emplace<Counted>(9);
    world.clear<CopyOnly>();
    CHECK(world.get(retained)->value == 9);
    world.discard();
    CHECK(!world.get(retained) && Counted::alive == 0);
}

void worlds_and_ownership() {
    Atom<int> expired;
    WorldRef expired_world;
    {
        World temporary;
        expired = temporary.add<int>(4);
        expired_world = temporary.ref();
    }
    CHECK(!expired.world() && !expired.is_valid() && !expired_world.get());

    alignas(World) std::byte storage[sizeof(World)];
    auto reused = std::construct_at(reinterpret_cast<World*>(storage), 2);
    auto old_address_handle = reused->add<int>(1);
    std::destroy_at(reused);
    reused = std::construct_at(reinterpret_cast<World*>(storage), 2);
    auto new_address_handle = reused->add<int>(2);
    CHECK(!reused->get(old_address_handle) && !old_address_handle.world());
    CHECK(new_address_handle != old_address_handle && *reused->get(new_address_handle) == 2);
    std::destroy_at(reused);

    World source(8);
    auto atom = source.add<int>(7);
    auto reference = source.ref();
    int* address = source.get(atom);
    World moved(std::move(source));
    CHECK(atom.world() == &moved && reference.get() == &moved && moved.get(atom) == address);
    source.add<int>(8);
    CHECK(!source.get(atom) && source.ref() != reference);
    World destination(4);
    auto replaced = destination.add<int>(99);
    destination = std::move(moved);
    CHECK(!destination.get(replaced) && atom.world() == &destination && destination.get(atom) == address);
    destination = std::move(destination);
    CHECK(*destination.get(atom) == 7);

    World root(8);
    World& room = root.attach_world(destination);
    CHECK(atom.world() == &room && reference.get() == &room && room.get(atom) == address);
    CHECK(root.size<int>() == 0 && room.size<int>() == 1);
    rejects<std::logic_error>([&] { root.attach_world(std::move(root)); });
    rejects<std::logic_error>([&] { room.add<World>(std::move(root)); });
    rejects<std::logic_error>([&] { room.emplace<World>(std::move(root)); });
    rejects<std::logic_error>([&] { room = std::move(root); });
    CHECK(*room.get(atom) == 7);
    room.queue_free(atom);
    root.cleanup();
    CHECK(room.is_live(atom));
    root.cleanup_tree();
    CHECK(!room.is_live(atom));
    auto nested = room.emplace<World>(2);
    auto inner = room.get(nested)->emplace<Counted>(1);
    CHECK(Counted::alive == 1);
    root.discard();
    CHECK(!inner.world() && !reference.get() && Counted::alive == 0);

    World parent(8);
    auto child_atom = parent.emplace<World>(2);
    auto child_value = parent.get(child_atom)->add<int>(42);
    parent = std::move(*parent.get(child_atom));
    CHECK(child_value.world() == &parent && *parent.get(child_value) == 42);
    CHECK(!parent.get(child_atom));
}

void iteration_and_mutation() {
    World world(130);
    std::array<Atom<int>, 130> atoms;
    for (int i = 0; i < 130; ++i) {
        atoms[i] = world.add<int>(i);
    }
    {
        auto view = world.iter_atoms<int>();
        auto copy = view;
        auto moved = std::move(copy);
        CHECK(copy.begin() == copy.end());
        for (auto [atom, value] : moved) {
            CHECK(world.get(atom) == &value && atom.world() == &world);
            if (value != 0 && value != 63 && value != 64 && value != 129) {
                world.queue_free(atom);
            }
        }
        rejects<std::logic_error>([&] { world.cleanup(); });
        rejects<std::logic_error>([&] { world.add<float>(1); });
        rejects<std::logic_error>([&] { world.clear<int>(); });
        rejects<std::logic_error>([&] { world.discard(); });
        rejects<std::logic_error>([&] { World other(std::move(world)); });
        rejects<std::logic_error>([&] { world = World(); });
        CHECK(world.size<int>() == 130 && world.size<float>() == 0);
    }
    world.cleanup();
    std::array<int, 4> expected{0, 63, 64, 129};
    size_t index = 0;
    const World& read = world;
    for (auto value : read.iter<int>()) {
        CHECK(index < expected.size() && value == expected[index++]);
    }
    CHECK(index == 4 && world.live_count<int>() == 4);
    CHECK(world.raw_slots<int>().is_live(129) && !world.raw_slots<int>().is_live(130));
    for (auto [atom, value] : read.iter_atoms<int>()) {
        CHECK(read.get(atom) == &value);
    }

    World parent(2);
    auto child = parent.emplace<World>(2);
    parent.get(child)->add<int>(1);
    {
        auto view = parent.get(child)->iter<int>();
        parent.queue_free(child);
        rejects<std::logic_error>([&] { parent.cleanup(); });
        rejects<std::logic_error>([&] { parent.cleanup_tree(); });
        rejects<std::logic_error>([&] { parent.clear<World>(); });
        rejects<std::logic_error>([&] { parent.discard(); });
        rejects<std::logic_error>([&] { World other(std::move(parent)); });
        CHECK(parent.is_live(child));
    }
    parent.cleanup();
    CHECK(!parent.is_live(child));
}

void sparse_iteration() {
    World world(320);
    std::array<Atom<int>, 320> atoms;
    for (int value = 0; value < 320; ++value) {
        atoms[value] = world.add<int>(value);
    }
    for (auto [atom, value] : world.iter_atoms<int>()) {
        if (value != 130 && value != 319) {
            world.queue_free(atom);
        }
    }
    world.cleanup();

    {
        auto view = world.iter<int>();
        auto current = view.begin();
        CHECK(*current == 130);
        auto copied = current;
        CHECK(copied == current);
        ++current;
        CHECK(*current == 319 && *copied == 130 && copied != current);
        ++current;
        CHECK(current == view.end());

        auto assigned = world.iter<int>();
        assigned = view;
        CHECK(*assigned.begin() == 130);
        assigned = std::move(view);
        CHECK(view.begin() == view.end() && *assigned.begin() == 130);
    }

    world.queue_free(atoms[130]);
    world.queue_free(atoms[319]);
    world.cleanup();
    {
        auto empty = world.iter<int>();
        CHECK(empty.begin() == empty.end());
    }

    auto replacement = world.add<int>(17);
    size_t visited = 0;
    for (auto [atom, value] : world.iter_atoms<int>()) {
        CHECK(atom == replacement && value == 17);
        ++visited;
    }
    CHECK(visited == 1);
}

void reentrant_mutation() {
    struct ConstructorMutation {
        explicit ConstructorMutation(World& world) {
            world.add<int>(1);
        }
    };
    struct ConstructorView {
        explicit ConstructorView(World& world) {
            auto view = world.iter<int>();
        }
    };
    struct DestructorQueue {
        World* world;
        Atom<int> atom;
        bool* rejected;

        ~DestructorQueue() {
            // Destructors must catch the rejection rather than let an exception escape.
            try {
                world->queue_free(atom);
            }
            catch (const std::logic_error&) {
                *rejected = true;
            }
        }
    };

    World world;
    rejects<std::logic_error>([&] { world.emplace<ConstructorMutation>(world); });
    rejects<std::logic_error>([&] { world.emplace<ConstructorView>(world); });
    CHECK(world.size<ConstructorMutation>() == 0 && world.size<ConstructorView>() == 0);
    CHECK(world.size<int>() == 0);

    // Failed construction must release the mutation guard so later operations work.
    auto atom = world.add<int>(7);
    bool rejected = false;
    world.emplace<DestructorQueue>(&world, atom, &rejected);
    world.clear<DestructorQueue>();
    CHECK(rejected);
    world.cleanup();
    CHECK(*world.get(atom) == 7);
}

constexpr int termination_exit_code = 86;

void run_termination_case(std::string_view name) {
    // A child process checks std::terminate without stopping the test suite.
    std::set_terminate([] { std::_Exit(termination_exit_code); });

    if (name == "view-destroy") {
        auto world = std::make_unique<World>();
        auto view = world->iter<int>();
        world.reset();
        std::_Exit(0);
    }
    else if (name == "child-view-destroy") {
        auto parent = std::make_unique<World>();
        auto child = parent->emplace<World>();
        auto view = parent->get(child)->iter<int>();
        parent.reset();
        std::_Exit(0);
    }
    else {
        throw std::runtime_error("Unknown termination case");
    }
}

void destruction_contracts(const char* executable) {
    for (const char* name : {"view-destroy", "child-view-destroy"}) {
        intptr_t result = _spawnl(_P_WAIT, executable, executable, "--termination-case", name,
                                  static_cast<const char*>(nullptr));
        CHECK(result == termination_exit_code);
    }
    std::cout << "World destruction checks passed.\n";
}

template<size_t I>
struct Marker {};

template<size_t... I>
void concurrent_registration(std::index_sequence<I...>) {
    std::array<uint32_t, sizeof...(I)> ids{};
    std::latch start(sizeof...(I));
    std::array<std::thread, sizeof...(I)> threads{std::thread([&] {
        start.arrive_and_wait();
        ids[I] = TypeInfo<Marker<I>>::id();
    })...};
    for (auto& thread : threads) {
        thread.join();
    }
    std::unordered_set<uint32_t> unique(ids.begin(), ids.end());
    CHECK(unique.size() == ids.size());
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--termination-case") {
            run_termination_case(argv[2]);
            return 0;
        }
        CHECK(argc == 1);
        identity_and_reuse();
        construction_and_destruction();
        worlds_and_ownership();
        iteration_and_mutation();
        sparse_iteration();
        reentrant_mutation();
        concurrent_registration(std::make_index_sequence<8>{});
        destruction_contracts(argv[0]);
        std::cout << "DCS checks passed (100,000 reuse cycles). Atom<int>: " << sizeof(Atom<int>)
                  << " bytes.\n";
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
