#include "Dcs.h"
#include "Benchmarks.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

constexpr uint32_t ITEMS = 1000000;
constexpr int RUNS = 100;
using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    double setupTime = 0.0;
    double loopTime = 0.0;
    double checksum = 0.0;
};

double elapsed_ms(Clock::time_point start) {
    Clock::time_point end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void print_results(const char* name, const BenchmarkResult& world, const BenchmarkResult& baseline) {
    if (!std::isfinite(world.checksum) || world.checksum != baseline.checksum) {
        throw std::runtime_error("Benchmark outputs differ");
    }

    std::printf("%s\n", name);
    std::printf("  World:  setup %8.2f ms, loop %8.2f ms\n", world.setupTime, world.loopTime);
    std::printf("  vector: setup %8.2f ms, loop %8.2f ms\n", baseline.setupTime, baseline.loopTime);
    std::printf("  Matching checksum: %.17g\n\n", world.checksum);
}

void scale(float* data) {
    for (int run = 0; run < RUNS; run++) {
        for (uint32_t i = 0; i < ITEMS; i++) {
            data[i] = data[i] * 2.0f + 1.0f;
        }
    }
}

double sum(const float* data) {
    double total = 0.0;
    for (uint32_t i = 0; i < ITEMS; i++) {
        total += data[i];
    }
    return total;
}

void benchmark_linear() {
    BenchmarkResult worldResult;
    BenchmarkResult vectorResult;

    Clock::time_point start = Clock::now();
    World world(ITEMS);
    for (uint32_t i = 0; i < ITEMS; i++) {
        world.add<float>(float(i));
    }

    float* values = world.raw_slots<float>().data;
    worldResult.setupTime = elapsed_ms(start);

    start = Clock::now();
    scale(values);
    worldResult.loopTime = elapsed_ms(start);
    worldResult.checksum = sum(values);

    start = Clock::now();
    std::vector<float> baseline(ITEMS);
    for (uint32_t i = 0; i < ITEMS; i++) {
        baseline[i] = float(i);
    }

    vectorResult.setupTime = elapsed_ms(start);

    start = Clock::now();
    scale(baseline.data());
    vectorResult.loopTime = elapsed_ms(start);
    vectorResult.checksum = sum(baseline.data());

    print_results("Dense float storage", worldResult, vectorResult);
}

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Body {
    Vec3 position;
    float speed;
    int32_t health;
};

Body initial_body(uint32_t i) {
    Body body;
    body.position.x = float(i);
    body.position.y = float(i * 2);
    body.position.z = float(i * 3);
    body.speed = float(i) * 0.5f;
    body.health = 1000;
    return body;
}

void move_bodies(Body* bodies) {
    for (int run = 0; run < RUNS; run++) {
        for (uint32_t i = 0; i < ITEMS; i++) {
            Body& body = bodies[i];
            body.position.x += body.speed;
            body.position.y += body.speed;
            body.position.z += body.speed;
            body.health--;
        }
    }
}

double sum(const Body* bodies) {
    double total = 0.0;
    for (uint32_t i = 0; i < ITEMS; i++) {
        const Body& body = bodies[i];
        total += double(body.position.x) + body.position.y + body.position.z + body.health;
    }
    return total;
}

void benchmark_direct_world() {
    BenchmarkResult worldResult;
    BenchmarkResult vectorResult;

    Clock::time_point start = Clock::now();
    World root(1);
    World child(ITEMS);
    for (uint32_t i = 0; i < ITEMS; i++) {
        child.add(initial_body(i));
    }

    WorldRef childRef = child.ref();
    root.attach_world(child);
    worldResult.setupTime = elapsed_ms(start);

    start = Clock::now();
    World* attachedWorld = childRef.get();
    Body* bodies = attachedWorld->raw_slots<Body>().data;
    move_bodies(bodies);
    worldResult.loopTime = elapsed_ms(start);
    worldResult.checksum = sum(bodies);

    start = Clock::now();
    std::vector<Body> baseline;
    baseline.reserve(ITEMS);
    for (uint32_t i = 0; i < ITEMS; i++) {
        baseline.push_back(initial_body(i));
    }

    vectorResult.setupTime = elapsed_ms(start);

    start = Clock::now();
    move_bodies(baseline.data());
    vectorResult.loopTime = elapsed_ms(start);
    vectorResult.checksum = sum(baseline.data());

    print_results("Direct World access and local records", worldResult, vectorResult);
}

}

void run_benchmarks() {
    std::printf("%u values, %d passes. Single thread; raw ranges are fully populated.\n\n", ITEMS, RUNS);
    benchmark_linear();
    benchmark_direct_world();
}
