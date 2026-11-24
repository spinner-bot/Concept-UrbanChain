#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/network.h"
#include "ds/binary_heap.h"
#include "ds/dynamic_array.h"
#include "ds/hash_map.h"
#include "ds/queue.h"
#include "ds/stack.h"
#include "ds/union_find.h"
#include "io/line_io.h"
#include "io/map_io.h"
#include "nav/router.h"

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

void testIoRoundTrip() {
    uc::Network network;
    uc::Map& map = network.map();
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    const uc::Index a = map.findStation("A");
    const uc::Index b = map.findStation("B");
    const uc::Index c = map.findStation("C");
    CHECK(map.addEdge(a, b, 1.0, 1.5));
    CHECK(map.addEdge(b, c, 2.0, 2.5));

    uc::Line line = makeLine(a, b, c);
    line.ops().segmentRunSec[1] = 200.0;
    line.ops().dwellSec = 25.0;
    line.ops().headway[0].headwaySec = 240;
    network.addLine(line);
    CHECK(network.setTransferTime(b, 0, 1, 90.0));

    const std::string mapPath = "uc_test_roundtrip.cuc3m";
    const std::string linePath = "uc_test_roundtrip.cuc3l";
    std::string error;

    CHECK(uc::io::saveMap(mapPath, network.map(), &error));
    uc::Map loadedMap;
    CHECK(uc::io::loadMap(mapPath, loadedMap, &error));
    CHECK(loadedMap.stationCount() == 3);
    CHECK(static_cast<int>(loadedMap.edges().size()) == 2);
    CHECK(loadedMap.findStation("B") == b);
    CHECK(std::fabs(loadedMap.edgeCost(a, b) - 1.0) < 1e-9);
    CHECK(std::fabs(loadedMap.edgeLength(a, b) - 1.5) < 1e-9);
    CHECK(std::fabs(loadedMap.edgeCost(b, c) - 2.0) < 1e-9);
    CHECK(loadedMap.station(a).name == "A");

    uc::Network loadedNetwork;
    loadedNetwork.map() = loadedMap;
    CHECK(uc::io::saveLines(linePath, network, &error));
    CHECK(uc::io::loadLines(linePath, loadedNetwork, &error));
    CHECK(loadedNetwork.lines().size() == 1);
    const uc::Line& loadedLine = loadedNetwork.lines()[0];
    CHECK(loadedLine.name() == "L1");
    CHECK(loadedLine.color() == 0xff0000u);
    CHECK(loadedLine.stationCount() == 3);
    CHECK(std::fabs(loadedLine.ops().dwellSec - 25.0) < 1e-9);
    CHECK(std::fabs(loadedLine.ops().segmentRunSec[1] - 200.0) < 1e-9);
    CHECK(loadedLine.ops().headway[0].headwaySec == 240);
    CHECK(std::fabs(loadedNetwork.transferTime(loadedMap.findStation("B"), 0, 1) - 90.0) <
          1e-9);

    std::filesystem::remove(mapPath);
    std::filesystem::remove(linePath);
}

uc::Network makeTwoLineNetwork() {
    uc::Network network;
    uc::Map& map = network.map();
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addStation(makeStation("D", 3.0, 0.0));
    const uc::Index a = map.findStation("A");
    const uc::Index b = map.findStation("B");
    const uc::Index c = map.findStation("C");
    const uc::Index d = map.findStation("D");
    map.addEdge(a, b, 1.0);
    map.addEdge(b, c, 1.0);
    map.addEdge(c, d, 1.0);

    uc::Line first(0, "first", 0xff0000u);
    first.stations().push_back(a);
    first.stations().push_back(b);
    first.stations().push_back(c);
    first.ops().segmentRunSec.push_back(120.0);
    first.ops().segmentRunSec.push_back(120.0);
    first.ops().dwellSec = 30.0;
    first.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 300});
    network.addLine(first);

    uc::Line second(0, "second", 0x00ff00u);
    second.stations().push_back(c);
    second.stations().push_back(d);
    second.ops().segmentRunSec.push_back(120.0);
    second.ops().dwellSec = 30.0;
    second.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 600});
    network.addLine(second);
    network.setTransferTime(c, 0, 1, 120.0);
    return network;
}

void testRouter() {
    uc::Network network = makeTwoLineNetwork();
    const uc::Index a = network.map().findStation("A");
    const uc::Index c = network.map().findStation("C");
    const uc::Index d = network.map().findStation("D");

    uc::Router router;
    const uc::Journey direct = router.findRoute(network, a, c, 0.0);
    CHECK(direct.found);
    CHECK(std::fabs(direct.arriveTime - 450.0) < 1e-6);
    CHECK(direct.transfers == 0);
    CHECK(direct.legs.size() == 1);

    const uc::Journey transfer = router.findRoute(network, a, d, 0.0);
    CHECK(transfer.found);
    CHECK(std::fabs(transfer.arriveTime - 1020.0) < 1e-6);
    CHECK(transfer.transfers == 1);
    CHECK(transfer.legs.size() == 2);
    CHECK(transfer.legs[0].fromStation == a);
    CHECK(transfer.legs[0].toStation == c);
    CHECK(std::fabs(transfer.legs[0].boardTime - 150.0) < 1e-6);
    CHECK(std::fabs(transfer.legs[0].alightTime - 450.0) < 1e-6);
    CHECK(transfer.legs[1].fromStation == c);
    CHECK(transfer.legs[1].toStation == d);
    CHECK(std::fabs(transfer.legs[1].boardTime - 870.0) < 1e-6);
    CHECK(std::fabs(transfer.legs[1].alightTime - 1020.0) < 1e-6);

    const uc::Journey reverse = router.findRoute(network, d, a, 0.0);
    CHECK(reverse.found);
    CHECK(std::fabs(reverse.arriveTime - 1020.0) < 1e-6);

    const uc::Journey same = router.findRoute(network, a, a, 0.0);
    CHECK(same.found);
    CHECK(std::fabs(same.arriveTime - 0.0) < 1e-9);
    CHECK(same.legs.empty());

    const uc::Journey closed = router.findRoute(network, a, d, 100000.0);
    CHECK(!closed.found);

    uc::Network single;
    uc::Map& map = single.map();
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addStation(makeStation("D", 3.0, 0.0));
    const uc::Index sa = map.findStation("A");
    const uc::Index sb = map.findStation("B");
    const uc::Index sc = map.findStation("C");
    const uc::Index sd = map.findStation("D");
    map.addEdge(sa, sb, 1.0);
    map.addEdge(sb, sc, 1.0);
    map.addEdge(sc, sd, 1.0);
    uc::Line line(0, "line", 0xff0000u);
    line.stations().push_back(sa);
    line.stations().push_back(sb);
    line.stations().push_back(sc);
    line.stations().push_back(sd);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().dwellSec = 30.0;
    line.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 300});
    single.addLine(line);

    const uc::Journey oneLine = router.findRoute(single, sa, sd, 0.0);
    CHECK(oneLine.found);
    CHECK(std::fabs(oneLine.arriveTime - 600.0) < 1e-6);
    CHECK(oneLine.transfers == 0);
    CHECK(oneLine.legs.size() == 1);
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
    testIoRoundTrip();
    testRouter();

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
