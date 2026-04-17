#include <cstdio>
#include <vector>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include "Dcs.h"
#include "Benchmarks.h"

#define ITEMS 33000000
#define RUNS 100
#define NOW() std::chrono::high_resolution_clock::now()
#define MS(start, end) std::chrono::duration<double, std::milli>(end - start).count()

#if defined(_MSC_VER)
#define FORCE_VEC __pragma(loop(ivdep))
#define RESTRICT __restrict
#else
#define FORCE_VEC _Pragma("clang loop vectorize(enable)")
#define RESTRICT __restrict__
#endif

static void print_stats(const char* name, double total_time) {
    printf("%-30s Total (Setup + %d runs): %.2fms\n", name, RUNS, total_time);
    printf("--------------------------------------\n");
}

void steamwand_linear() {
    auto start = NOW();
    World world(ITEMS);
    for (int i = 0; i < ITEMS; i++) world.add<float>(float(i));

    float* RESTRICT data = world.get_array<float>();
    size_t count = world.size<float>();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (size_t i = 0; i < count; i++)
                data[i] = data[i] * 2.0f + 1.0f;
    }
    print_stats("Steamwand linear:", MS(start, NOW()));
}

void steamwand_query_parralel() {
    auto start = NOW();
    World world(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        world.add<Vec3>({ float(i), float(i * 2), float(i * 3) });
        world.add<float>(float(i) * 0.5f);
    }

    Vec3* RESTRICT pos = world.get_array<Vec3>();
    float* RESTRICT spd = world.get_array<float>();
    size_t n = world.size<Vec3>();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (size_t i = 0; i < n; i++) {
                pos[i].x += spd[i];
                pos[i].y += spd[i];
                pos[i].z += spd[i];
            }
    }
    print_stats("Steamwand query parallel:", MS(start, NOW()));
}

void steamwand_multi_component() {
    auto start = NOW();
    World world(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        world.add<Vec3>({ float(i), float(i * 2), float(i * 3) });
        world.add<float>(float(i) * 0.5f);
        world.add<int32_t>(i);
    }

    Vec3* RESTRICT pos = world.get_array<Vec3>();
    float* RESTRICT spd = world.get_array<float>();
    int32_t* RESTRICT hp = world.get_array<int32_t>();
    size_t count = world.size<Vec3>();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (size_t i = 0; i < count; i++) {
                pos[i].x += spd[i];
                pos[i].y += spd[i];
                pos[i].z += spd[i];
                hp[i]--;
            }
    }
    print_stats("Steamwand multi:", MS(start, NOW()));
}

void steamwand_backwards_query() {
    auto start = NOW();
    World world(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        world.add<Vec3>({ float(i), float(i * 2), float(i * 3) });
        if (i % 2 == 0) world.add<float>(float(i) * 0.5f);
        else world.add<bool>(true);
    }

    Vec3* RESTRICT pos = world.get_array<Vec3>();
    float* RESTRICT spd = world.get_array<float>();
    size_t n = world.size<float>();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (size_t i = 0; i < n; i++) {
                pos[i].x += spd[i];
                pos[i].y += spd[i];
                pos[i].z += spd[i];
            }
    }
    print_stats("Steamwand backwards:", MS(start, NOW()));
}

void steamwand_zombie() {
    auto start = NOW();
    World pool(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        pool.add<int32_t>(100);
        pool.add<float>(10.0f);
    }

    int32_t* RESTRICT hp = pool.get_array<int32_t>();
    float* RESTRICT dmg = pool.get_array<float>();
    size_t count = pool.size<int32_t>();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (size_t i = 0; i < count; i++) {
                hp[i] -= 1;
                dmg[i] *= 0.99f;
                if (hp[i] <= 0) {
                    hp[i] = 0;
                }
            }
    }
    print_stats("Steamwand zombie:", MS(start, NOW()));
}

void archetype_linear() {
    auto start = NOW();
    std::vector<float> data(ITEMS);
    for (int i = 0; i < ITEMS; i++) data[i] = float(i);

    for (int r = 0; r < RUNS; r++) {
        float* RESTRICT raw = data.data();
        size_t n = data.size();
        FORCE_VEC
            for (size_t i = 0; i < n; i++) raw[i] = raw[i] * 2.0f + 1.0f;
    }
    print_stats("vector linear:", MS(start, NOW()));
}

void archetype_query_parallel() {
    auto start = NOW();
    std::vector<Vec3> pos(ITEMS);
    std::vector<float> speed(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        pos[i] = { float(i), float(i * 2), float(i * 3) };
        speed[i] = float(i) * 0.5f;
    }

    for (int r = 0; r < RUNS; r++) {
        Vec3* RESTRICT p = pos.data();
        float* RESTRICT s = speed.data();
        FORCE_VEC
            for (size_t i = 0; i < ITEMS; i++) {
                p[i].x += s[i]; p[i].y += s[i]; p[i].z += s[i];
            }
    }
    print_stats("archetype query parallel:", MS(start, NOW()));
}

void archetype_multi() {
    auto start = NOW();
    std::vector<Vec3> pos(ITEMS);
    std::vector<float> speed(ITEMS);
    std::vector<int32_t> health(ITEMS);
    for (int i = 0; i < ITEMS; i++) {
        pos[i] = { float(i), float(i * 2), float(i * 3) };
        speed[i] = float(i) * 0.5f;
        health[i] = i;
    }

    for (int r = 0; r < RUNS; r++) {
        Vec3* RESTRICT p = pos.data();
        float* RESTRICT s = speed.data();
        int32_t* RESTRICT h = health.data();
        FORCE_VEC
            for (int i = 0; i < ITEMS; i++) {
                p[i].x += s[i]; p[i].y += s[i]; p[i].z += s[i];
                h[i]--;
            }
    }
    print_stats("archetype multi:", MS(start, NOW()));
}

void archetype_backwards_query() {
    auto start = NOW();
    std::vector<Vec3> ps_pos(ITEMS / 2);
    std::vector<float> ps_speed(ITEMS / 2);
    for (int i = 0; i < ITEMS / 2; i++) {
        ps_pos[i] = { float(i), float(i * 2), float(i * 3) };
        ps_speed[i] = float(i) * 0.5f;
    }

    for (int r = 0; r < RUNS; r++) {
        Vec3* RESTRICT p = ps_pos.data();
        float* RESTRICT s = ps_speed.data();
        size_t n = ps_pos.size();
        FORCE_VEC
            for (size_t i = 0; i < n; i++) {
                p[i].x += s[i]; p[i].y += s[i]; p[i].z += s[i];
            }
    }
    print_stats("archetype backwards:", MS(start, NOW()));
}

void archetype_zombie() {
    auto start = NOW();
    std::vector<int32_t> health(ITEMS, 100);
    std::vector<float> damage(ITEMS, 10.0f);

    int32_t* RESTRICT h = health.data();
    float* RESTRICT d = damage.data();

    for (int r = 0; r < RUNS; r++) {
        FORCE_VEC
            for (int i = 0; i < ITEMS; i++) {
                h[i] -= 1;
                d[i] *= 0.99f;
                if (h[i] <= 0) {
                    h[i] = 0;
                }
            }
    }
    print_stats("archetype zombie:", MS(start, NOW()));
}