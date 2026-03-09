#include <cstdio>
#include <optional>
#include <tuple>
#include <vector>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include "Dcs.h"

#define ITEMS 33000000
#define RUNS 100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

static void print_stats(const char* name, double total_time) {
    printf("%s Total (Setup + %d runs): %.2fms\n", name, RUNS, total_time);
    printf("%s Avg/Run: %.2fms\n", name, total_time / RUNS);
    printf("--------------------------------------\n");
}

struct ZombieWorld {
    World internal;
    std::vector<std::tuple<Atom, Atom, Atom>> entities;

    ZombieWorld(size_t count) : internal((uint32_t)(count * 3)) {
        entities.reserve(count);
    }

    void create_entity(int health, float damage, bool is_alive) {
        Atom h = internal.create<int32_t>(health);
        Atom d = internal.create<float>(damage);
        Atom a = internal.create<bool>(is_alive);
        entities.emplace_back(h, d, a);
    }

    int32_t* hp_raw() { return internal.raw<int32_t>(); }
    float* dmg_raw() { return internal.raw<float>(); }
    size_t count() { return internal.count<int32_t>(); }
};

void linear_iteration_v3() {
    auto start = NOW();
    World world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create<float>(float(i));

    for (int r = 0; r < RUNS; r++) {
        auto view = world.view<float>();
        for (size_t i = 0; i < view.count; i++)
            view.data[i] = view.data[i] * 2.0f + 1.0f;
    }

    print_stats("v3 linear: ", MS(start, NOW()));
}

void query_parallel_v3() {
    auto start = NOW();
    World world(ITEMS * 2);

    for (int i = 0; i < ITEMS; i++) {
        world.create<Vec3>({ float(i), float(i * 2), float(i * 3) });
        world.create<float>(float(i) * 0.5f);
    }

    for (int r = 0; r < RUNS; r++) {
        auto pos = world.view<Vec3>();
        auto spd = world.view<float>();
        size_t n = std::min(pos.count, spd.count);
        for (size_t i = 0; i < n; i++) {
            pos.data[i].x += spd.data[i];
            pos.data[i].y += spd.data[i];
            pos.data[i].z += spd.data[i];
        }
    }

    print_stats("v3 query parallel: ", MS(start, NOW()));
}

void multi_component_v3() {
    auto start = NOW();
    World world(ITEMS * 3);

    for (int i = 0; i < ITEMS; i++) {
        world.create<Vec3>({ float(i), float(i * 2), float(i * 3) });
        world.create<float>(float(i) * 0.5f);
        world.create<int32_t>(i);
    }

    Vec3* pos = world.raw<Vec3>();
    float* spd = world.raw<float>();
    int32_t* hp = world.raw<int32_t>();
    size_t count = world.count<Vec3>();

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < count; i++) {
            pos[i].x += spd[i];
            pos[i].y += spd[i];
            pos[i].z += spd[i];
            hp[i]--;
        }
    }

    print_stats("v3 multi: ", MS(start, NOW()));
}

void backwards_query_v3() {
    auto start = NOW();
    World world(ITEMS * 2);
    uint16_t eid = 0;

    for (int i = 0; i < ITEMS; i++) {
        world.create<Vec3>({ float(i), float(i * 2), float(i * 3) }, eid);
        if (i % 2 == 0) world.create<float>(float(i) * 0.5f, eid);
        else world.create<bool>(true, eid);
        eid++;
    }

    for (int r = 0; r < RUNS; r++) {
        auto pos = world.view<Vec3>();
        auto spd = world.view<float>();
        size_t n = std::min(pos.count, spd.count);
        for (size_t i = 0; i < n; i++) {
            pos.data[i].x += spd.data[i];
            pos.data[i].y += spd.data[i];
            pos.data[i].z += spd.data[i];
        }
    }

    print_stats("v3 backwards: ", MS(start, NOW()));
}

void zombie_v3() {
    auto start = NOW();
    ZombieWorld world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create_entity(100, 10.0f, true);

    int32_t* hp = world.hp_raw();
    float* dmg = world.dmg_raw();
    size_t count = world.count();

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < count; i++) {
            hp[i]--;
            dmg[i] *= 0.99f;
        }
    }

    print_stats("v3 zombie: ", MS(start, NOW()));
}

void archetype_linear() {
    auto start = NOW();
    std::vector<float> data;
    data.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        data.push_back(float(i));

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < data.size(); i++)
            data[i] = data[i] * 2.0f + 1.0f;
    }

    print_stats("archetype linear: ", MS(start, NOW()));
}

void archetype_query_parallel() {
    auto start = NOW();
    std::vector<float> px, py, pz, speed;
    px.reserve(ITEMS);
    py.reserve(ITEMS);
    pz.reserve(ITEMS);
    speed.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        px.push_back(float(i));
        py.push_back(float(i * 2));
        pz.push_back(float(i * 3));
        speed.push_back(float(i) * 0.5f);
    }

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < px.size(); i++) {
            px[i] += speed[i];
            py[i] += speed[i];
            pz[i] += speed[i];
        }
    }

    print_stats("archetype query parallel: ", MS(start, NOW()));
}

void archetype_multi() {
    auto start = NOW();
    std::vector<Vec3> pos;
    std::vector<float> speed;
    std::vector<int> health;

    pos.reserve(ITEMS);
    speed.reserve(ITEMS);
    health.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        pos.push_back({ float(i), float(i * 2), float(i * 3) });
        speed.push_back(float(i) * 0.5f);
        health.push_back(i);
    }

    for (int r = 0; r < RUNS; r++) {
        for (int i = 0; i < ITEMS; i++) {
            pos[i].x += speed[i];
            pos[i].y += speed[i];
            pos[i].z += speed[i];
            health[i]--;
        }
    }

    print_stats("archetype multi: ", MS(start, NOW()));
}

void archetype_backwards_query() {
    auto start = NOW();
    std::vector<float> ps_px, ps_py, ps_pz, ps_speed;
    std::vector<float> po_px, po_py, po_pz;

    ps_px.reserve(ITEMS / 2);
    ps_py.reserve(ITEMS / 2);
    ps_pz.reserve(ITEMS / 2);
    ps_speed.reserve(ITEMS / 2);
    po_px.reserve(ITEMS / 2);
    po_py.reserve(ITEMS / 2);
    po_pz.reserve(ITEMS / 2);

    for (int i = 0; i < ITEMS; i++) {
        if (i % 2 == 0) {
            ps_px.push_back(float(i));
            ps_py.push_back(float(i * 2));
            ps_pz.push_back(float(i * 3));
            ps_speed.push_back(float(i) * 0.5f);
        }
        else {
            po_px.push_back(float(i));
            po_py.push_back(float(i * 2));
            po_pz.push_back(float(i * 3));
        }
    }

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < ps_px.size(); i++) {
            ps_px[i] += ps_speed[i];
            ps_py[i] += ps_speed[i];
            ps_pz[i] += ps_speed[i];
        }
    }

    print_stats("archetype backwards: ", MS(start, NOW()));
}

void archetype_zombie() {
    auto start = NOW();
    std::vector<int> health;
    std::vector<float> damage;
    std::vector<uint8_t> alive;

    health.reserve(ITEMS);
    damage.reserve(ITEMS);
    alive.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        health.push_back(100);
        damage.push_back(10.0f);
        alive.push_back(1);
    }

    for (int r = 0; r < RUNS; r++) {
        for (int i = 0; i < ITEMS; i++) {
            health[i]--;
            damage[i] *= 0.99f;
        }
    }

    print_stats("archetype zombie: ", MS(start, NOW()));
}
