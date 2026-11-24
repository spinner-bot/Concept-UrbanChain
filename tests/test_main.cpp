#include <cmath>
#include <cstdio>
#include <string>

#include "core/network.h"
#include "ds/binary_heap.h"
#include "ds/dynamic_array.h"
#include "ds/hash_map.h"
#include "ds/queue.h"
#include "ds/stack.h"
#include "ds/union_find.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

void testDynamicArray() {
    uc::DynamicArray<int> values;
    CHECK(values.empty());
    for (int i = 0; i < 100; ++i) {
        values.push_back(i);
    }
    CHECK(values.size() == 100);
    CHECK(values[42] == 42);

    uc::DynamicArray<int> copy = values;
    CHECK(copy.size() == 100);
    CHECK(copy[99] == 99);

    copy.resize(10);
    CHECK(copy.size() == 10);
    CHECK(copy.capacity() >= 10);
}

void testHashMap() {
    uc::HashMap<std::string, int, uc::Fnv1aHash> names;
    CHECK(names.insert("alpha", 1));
    CHECK(!names.insert("alpha", 2));
    CHECK(names.contains("alpha"));
    CHECK(*names.find("alpha") == 1);
    CHECK(names.find("missing") == nullptr);

    for (int i = 0; i < 1000; ++i) {
        names.insert("station-" + std::to_string(i), i);
    }
    CHECK(names.size() == 1001);
    CHECK(*names.find("station-999") == 999);

    uc::HashMap<std::uint64_t, double, uc::IntHash> buckets;
    buckets.set(12345u, 1.5);
    CHECK(*buckets.find(12345u) == 1.5);
}

void testBinaryHeap() {
    uc::BinaryMinHeap heap;
    heap.push(5.0, 5);
    heap.push(1.0, 1);
    heap.push(3.0, 3);
    heap.push(2.0, 2);
    CHECK(heap.pop().key == 1.0);
    CHECK(heap.pop().key == 2.0);
    CHECK(heap.pop().key == 3.0);
    CHECK(heap.pop().key == 5.0);
    CHECK(heap.empty());
}

void testUnionFind() {
    uc::UnionFind sets(5);
    CHECK(sets.count() == 5);
    CHECK(sets.unite(0, 1));
    CHECK(sets.unite(1, 2));
    CHECK(!sets.unite(0, 2));
    CHECK(sets.connected(0, 2));
    CHECK(!sets.connected(0, 3));
    CHECK(sets.count() == 3);
}

void testQueueStack() {
    uc::Queue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);
    CHECK(queue.front() == 1);
    queue.pop();
    CHECK(queue.front() == 2);
    CHECK(queue.size() == 2);

    uc::Stack<int> stack;
    stack.push(1);
    stack.push(2);
    CHECK(stack.top() == 2);
    stack.pop();
    CHECK(stack.top() == 1);
}

uc::Station makeStation(const char* id, double x, double y) {
    uc::Station station;
    station.id = id;
    station.name = id;
    station.position = uc::Vec2{x, y};
    return station;
}

uc::Line makeLine(uc::Index a, uc::Index b, uc::Index c) {
    uc::Line line(0, "L1", 0xff0000u);
    line.stations().push_back(a);
    line.stations().push_back(b);
    line.stations().push_back(c);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 300});
    return line;
}

void testMapAndLine() {
    uc::Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    CHECK(!map.addStation(makeStation("A", 9.0, 9.0)));

    const uc::Index a = map.findStation("A");
    const uc::Index b = map.findStation("B");
    const uc::Index c = map.findStation("C");
    CHECK(map.addEdge(a, b, 1.0));
    CHECK(map.addEdge(b, c, 2.0, 3.0));
    CHECK(!map.addEdge(a, b, 1.0));
    CHECK(map.hasEdge(a, b));
    CHECK(!map.hasEdge(a, c));
    CHECK(std::fabs(map.edgeCost(b, c) - 2.0) < 1e-9);
    CHECK(std::fabs(map.edgeLength(b, c) - 3.0) < 1e-9);
    CHECK(map.isConnected());

    uc::Line line = makeLine(a, b, c);
    std::string error;
    CHECK(line.validate(map, &error));
    CHECK(std::fabs(line.length(map) - 4.0) < 1e-9);

    uc::Line repeated(0, "bad", 0xffffffu);
    repeated.stations().push_back(a);
    repeated.stations().push_back(b);
    repeated.stations().push_back(a);
    repeated.ops().segmentRunSec.push_back(120.0);
    repeated.ops().segmentRunSec.push_back(120.0);
    repeated.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 300});
    CHECK(!repeated.validate(map, &error));
}

void testNetwork() {
    uc::Network network;
    uc::Map& map = network.map();
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    const uc::Index a = map.findStation("A");
    const uc::Index b = map.findStation("B");
    const uc::Index c = map.findStation("C");
    map.addEdge(a, b, 1.0);
    map.addEdge(b, c, 1.0);

    const int lineId = network.addLine(makeLine(a, b, c));
    CHECK(lineId == 0);
    CHECK(network.line(lineId)->name() == "L1");
    CHECK(network.line(1) == nullptr);

    CHECK(network.setTransferTime(b, 0, 1, 180.0));
    CHECK(std::fabs(network.transferTime(b, 0, 1) - 180.0) < 1e-9);
    CHECK(std::fabs(network.transferTime(b, 1, 0) - uc::kDefaultTransferSeconds) < 1e-9);

    std::string error;
    CHECK(network.validate(&error));
}

}  // namespace

int main() {
    testDynamicArray();
    testHashMap();
    testBinaryHeap();
    testUnionFind();
    testQueueStack();
    testMapAndLine();
    testNetwork();

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
