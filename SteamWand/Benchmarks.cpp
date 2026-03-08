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

static void print_stats(const char* name, std::vector<double>& times) {
    std::sort(times.begin(), times.end());
    double min = times.front();
    double median = times[times.size() / 2];
    double max = times.back();
    std::cout << name << "ms  median=" << median << "ms\n";
}

// ----------------------------------------------------------------
// Prototype tests
// ----------------------------------------------------------------

void linear_iteration() {
    World world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create(float(i));

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        world.iter<float>([](float& v) {
            v = (v * 2.0f) + 1.0f;
            });
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto linear: ", times);
}

void query_parallel_proto() {
    World world(ITEMS * 2);

    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) });
        world.create(float(i) * 0.5f);
    }

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        world.query_parallel<Vec3, float>([](Vec3& pos, float& spd) {
            pos.x += spd;
            pos.y += spd;
            pos.z += spd;
            });
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto query parallel: ", times);
}

void multi_query_single_world() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < count; i++) {
            pos_raw[i].x += spd_raw[i];
            pos_raw[i].y += spd_raw[i];
            pos_raw[i].z += spd_raw[i];
            hp_raw[i] -= 1;
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto multi parallel: ", times);
}

void backwards_query_proto() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        world.query_parallel<Vec3, float>([](Vec3& pos, float& spd) {
            pos.x += spd;
            pos.y += spd;
            pos.z += spd;
            });
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto backwards query: ", times);
}

// ----------------------------------------------------------------
// Archetype ECS tests
// ----------------------------------------------------------------

struct ArchetypeECS {
    std::vector<float> ps_px, ps_py, ps_pz, ps_speed;
    std::vector<float> po_px, po_py, po_pz;

    void insert_pos_speed(float px, float py, float pz, float spd) {
        ps_px.push_back(px);
        ps_py.push_back(py);
        ps_pz.push_back(pz);
        ps_speed.push_back(spd);
    }

    void insert_pos_only(float px, float py, float pz) {
        po_px.push_back(px);
        po_py.push_back(py);
        po_pz.push_back(pz);
    }
};

void archetype_linear() {
    std::vector<float> data;
    data.reserve(ITEMS);
    for (int i = 0; i < ITEMS; i++)
        data.push_back(float(i));

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < data.size(); i++)
            data[i] = (data[i] * 2.0f) + 1.0f;
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype linear: ", times);
}

void archetype_query() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < px.size(); i++) {
            px[i] += speed[i];
            py[i] += speed[i];
            pz[i] += speed[i];
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype query:      ", times);
}

void archetype_multi() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (int i = 0; i < ITEMS; i++) {
            pos[i].x += speed[i];
            pos[i].y += speed[i];
            pos[i].z += speed[i];
            health[i] -= 1;
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype multi: ", times);
}

void backwards_query_archetype() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < ps_px.size(); i++) {
            ps_px[i] += ps_speed[i];
            ps_py[i] += ps_speed[i];
            ps_pz[i] += ps_speed[i];
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype backwards query: ", times);
}

// Proto vs Archetype real life example 1v1 maxed out optimization
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
    ZombieWorld world(ITEMS);

    for (int i = 0; i < ITEMS; i++)
        world.create_entity(100, 10.0f, true);

    int* hp_raw = world.hp_raw();
    float* dmg_raw = world.dmg_raw();
    size_t count = world.count();

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < count; i++) {
            hp_raw[i] -= 1;
            dmg_raw[i] *= 0.99f;
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto zombie: ", times);
}

void zombie_update_archetype() {
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

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (int i = 0; i < ITEMS; i++) {
            health[i] -= 1;
            damage[i] *= 0.99f;
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype zombie: ", times);
}