#include "Dcs.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

#define ITEMS 33000000
#define RUNS  100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

static void print_stats(const char* name, double total_time) {
    std::cout << name << " Total (Setup + " << RUNS << " runs): " << total_time << "ms\n";
    std::cout << name << " Avg/Run: " << (total_time / RUNS) << "ms\n";
    std::cout << "--------------------------------------\n";
}

void linear_iteration() {
    auto start = NOW();
    World world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create(float(i));

    for (int r = 0; r < RUNS; r++) {
        world.iter<float>([](float& v) {
            v = (v * 2.0f) + 1.0f;
            });
    }

    auto end = NOW();
    print_stats("proto linear: ", MS(start, end));
}

void query_parallel_proto() {
    auto start = NOW();
    World world(ITEMS * 2);

    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) });
        world.create(float(i) * 0.5f);
    }

    for (int r = 0; r < RUNS; r++) {
        world.query_parallel<Vec3, float>([](Vec3& pos, float& spd) {
            pos.x += spd;
            pos.y += spd;
            pos.z += spd;
            });
    }

    auto end = NOW();
    print_stats("proto query parallel: ", MS(start, end));
}

void multi_query_single_world() {
    auto start = NOW();
    World world(ITEMS * 3);

    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) });
        world.create(float(i) * 0.5f);
        world.create(i);
    }

    Vec3* pos_raw = world.raw<Vec3>();
    float* spd_raw = world.raw<float>();
    int* hp_raw = world.raw<int32_t>();
    size_t count = world.slabFor<Vec3>().allocated_count();

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < count; i++) {
            pos_raw[i].x += spd_raw[i];
            pos_raw[i].y += spd_raw[i];
            pos_raw[i].z += spd_raw[i];
            hp_raw[i] -= 1;
        }
    }

    auto end = NOW();
    print_stats("proto multi parallel: ", MS(start, end));
}

void backwards_query_proto() {
    auto start = NOW();
    World world(ITEMS * 2);
    uint16_t entity_id = 0;

    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) }, entity_id);
        if (i % 2 == 0)
            world.create(float(i) * 0.5f, entity_id);
        else
            world.create(bool(true), entity_id);
        entity_id++;
    }

    for (int r = 0; r < RUNS; r++) {
        world.query_parallel<Vec3, float>([](Vec3& pos, float& spd) {
            pos.x += spd;
            pos.y += spd;
            pos.z += spd;
            });
    }

    auto end = NOW();
    print_stats("proto backwards query: ", MS(start, end));
}

void archetype_linear() {
    auto start = NOW();
    std::vector<float> data;
    data.reserve(ITEMS);
    for (int i = 0; i < ITEMS; i++)
        data.push_back(float(i));

    for (int r = 0; r < RUNS; r++) {
        for (size_t i = 0; i < data.size(); i++)
            data[i] = (data[i] * 2.0f) + 1.0f;
    }

    auto end = NOW();
    print_stats("archetype linear: ", MS(start, end));
}

void archetype_query() {
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

    auto end = NOW();
    print_stats("archetype query: ", MS(start, end));
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
            health[i] -= 1;
        }
    }

    auto end = NOW();
    print_stats("archetype multi: ", MS(start, end));
}

void backwards_query_archetype() {
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

    auto end = NOW();
    print_stats("archetype backwards query: ", MS(start, end));
}

struct ZombieWorld {
    World internal;
    ZombieWorld(size_t count) : internal(count * 3) {}
    void create_entity(int health, float damage, bool is_alive) {
        internal.create(health);
        internal.create(damage);
        internal.create(is_alive);
    }
    int* hp_raw() { return internal.raw<int>(); }
    float* dmg_raw() { return internal.raw<float>(); }
    size_t count() { return internal.slabFor<int>().allocated_count(); }
};

void zombie_update() {
    auto start = NOW();
    ZombieWorld world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create_entity(100, 10.0f, true);

    auto hps = world.internal.view<int>();
    auto dmgs = world.internal.view<float>();
    size_t count = hps.count;

    for (int r = 0; r < RUNS; r++) {
        int* hp_ptr = hps.data;
        float* dmg_ptr = dmgs.data;

        for (size_t i = 0; i < count; i++) {
            hp_ptr[i] -= 1;
            dmg_ptr[i] *= 0.99f;
        }
    }

    auto end = NOW();
    print_stats("proto zombie optimized: ", MS(start, end));
}

void zombie_update_archetype() {
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
            health[i] -= 1;
            damage[i] *= 0.99f;
        }
    }

    auto end = NOW();
    print_stats("archetype zombie: ", MS(start, end));
}