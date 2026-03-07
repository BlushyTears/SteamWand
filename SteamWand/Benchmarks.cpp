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

    print_stats("proto linear:         ", times);
}

void backwards_query() {
    World world(ITEMS);

    // Create components in parallel order
    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) });
        if (i % 2 == 0) {
            world.create(float(i) * 0.5f);
        }
        else {
            world.create(bool(true));
        }
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

    // Create components in parallel order
    for (int i = 0; i < ITEMS; i++) {
        world.create(Vec3{ float(i), float(i * 2), float(i * 3) });
        world.create(float(i) * 0.5f);
        world.create(i);
    }

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();

        world.query_parallel<Vec3, float, int>([](Vec3& pos, float& spd, int& hp) {
            pos.x += spd;
            pos.y += spd;
            pos.z += spd;
            hp -= 1;
            });

        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("proto multi parallel: ", times);
}

void backwards_query_proto() {
    World world(ITEMS);
    uint16_t entity_id = 0;

    // Create entities with components
    for (int i = 0; i < ITEMS; i++) {
        if (i % 2 == 0) {
            world.create(Vec3{ float(i), float(i * 2), float(i * 3) }, entity_id);
            world.create(float(i) * 0.5f, entity_id);
        }
        else {
            world.create(Vec3{ float(i), float(i * 2), float(i * 3) }, entity_id);
            world.create(bool(true), entity_id);
        }
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
// Note: these are hardcoded to optimal parameters which don't reflect
// real circumstances. A real archetype ECS pays migration cost when
// components change, query overhead to find matching archetypes, and
// cannot handle dynamic composition at runtime.
// SteamWand suffers from none of these issues.
// 
// Second note: The ECS archetype is a generic, vibe-coded implementation
// just for testing against SteamWand. It could probably be made better in some ways
// Also, I haven't tried sparse set ecs yet, which probably
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

void archetype_queries() {
    ArchetypeECS ecs;
    ecs.ps_px.reserve(ITEMS / 2);
    ecs.ps_py.reserve(ITEMS / 2);
    ecs.ps_pz.reserve(ITEMS / 2);
    ecs.ps_speed.reserve(ITEMS / 2);
    ecs.po_px.reserve(ITEMS / 2);
    ecs.po_py.reserve(ITEMS / 2);
    ecs.po_pz.reserve(ITEMS / 2);

    std::vector<size_t> ps_indices;
    ps_indices.reserve(ITEMS / 2);

    for (int i = 0; i < ITEMS; i++) {
        if (i % 2 == 0) {
            ps_indices.push_back(ecs.ps_px.size());
            ecs.insert_pos_speed(float(i), float(i * 2), float(i * 3), float(i) * 0.5f);
        }
        else {
            ecs.insert_pos_only(float(i), float(i * 2), float(i * 3));
        }
    }

    std::shuffle(ps_indices.begin(), ps_indices.end(), std::mt19937(std::random_device{}()));

    std::vector<double> linear_times, query_times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (size_t i = 0; i < ecs.ps_px.size(); i++)
            ecs.ps_px[i] = (ecs.ps_px[i] * 2.0f) + 1.0f;
        for (size_t i = 0; i < ecs.po_px.size(); i++)
            ecs.po_px[i] = (ecs.po_px[i] * 2.0f) + 1.0f;
        auto end = NOW();
        linear_times.push_back(MS(start, end));


        start = NOW();
        for (size_t i = 0; i < ecs.ps_px.size(); i++) {
            ecs.ps_px[i] += ecs.ps_speed[i];
            ecs.ps_py[i] += ecs.ps_speed[i];
            ecs.ps_pz[i] += ecs.ps_speed[i];
        }
        end = NOW();
        query_times.push_back(MS(start, end));
    }

    print_stats("archetype linear:     ", linear_times);
    print_stats("archetype query:      ", query_times);
}

void archetype_multi() {
    std::vector<float> px, py, pz, speed;
    std::vector<int> health;

    px.reserve(ITEMS);
    py.reserve(ITEMS);
    pz.reserve(ITEMS);
    speed.reserve(ITEMS);
    health.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        px.push_back(float(i));
        py.push_back(float(i * 2));
        pz.push_back(float(i * 3));
        speed.push_back(float(i) * 0.5f);
        health.push_back(i);
    }

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();
        for (int i = 0; i < ITEMS; i++) {
            px[i] += speed[i];
            py[i] += speed[i];
            pz[i] += speed[i];
            health[i] -= 1;
        }
        auto end = NOW();
        times.push_back(MS(start, end));
    }

    print_stats("archetype multi: ", times);
}

void backwards_query_archetype() {
    struct Entity {
        Vec3 pos;
        float spd;
        bool has_speed;
    };

    std::vector<Entity> entities;
    entities.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        if (i % 2 == 0) {
            entities.push_back({ Vec3{float(i), float(i * 2), float(i * 3)}, float(i) * 0.5f, true });
        }
        else {
            entities.push_back({ Vec3{float(i), float(i * 2), float(i * 3)}, 0.0f, false });
        }
    }

    std::vector<double> times;

    for (int r = 0; r < RUNS; r++) {
        auto start = NOW();

        for (auto& e : entities) {
            if (e.has_speed) {
                e.pos.x += e.spd;
                e.pos.y += e.spd;
                e.pos.z += e.spd;
            }
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

    for (int i = 0; i < ITEMS; i++) {
        world.create_entity(100, 10.0f, true);
    }

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
    std::vector<bool> alive;

    health.reserve(ITEMS);
    damage.reserve(ITEMS);
    alive.reserve(ITEMS);

    for (int i = 0; i < ITEMS; i++) {
        health.push_back(100);
        damage.push_back(10.0f);
        alive.push_back(true);
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
