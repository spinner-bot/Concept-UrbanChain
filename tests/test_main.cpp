#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/network.h"
#include "ds/binary_heap.h"
#include "ds/dynamic_array.h"
#include "ds/hash_map.h"
#include "ds/queue.h"
#include "ds/sort.h"
#include "ds/stack.h"
#include "ds/union_find.h"
#include "edit/editor.h"
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

void testSort() {
    uc::DynamicArray<int> values;
    const int raw[] = {5, 1, 9, 3, 3, 7, 0, 8, 2, 6, 4, 4, 11, 10};
    for (int value : raw) {
        values.push_back(value);
    }
    uc::quickSort(values, [](int a, int b) { return a < b; });
    CHECK(values.size() == 14);
    for (std::size_t i = 1; i < values.size(); ++i) {
        CHECK(values[i - 1] <= values[i]);
    }
    CHECK(values[0] == 0);
    CHECK(values[values.size() - 1] == 11);
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

void testEditor() {
    uc::Editor editor;
    std::string error;

    const uc::Index a = editor.addStation("Alpha", 0.0, 0.0, &error);
    const uc::Index b = editor.addStation("Bravo", 1.0, 0.0, &error);
    const uc::Index c = editor.addStation("Charlie", 2.0, 0.0, &error);
    CHECK(a == 0);
    CHECK(b == 1);
    CHECK(c == 2);
    CHECK(editor.isDirty());
    CHECK(editor.network().map().findStation("S1") == a);

    CHECK(editor.addEdge(a, b, &error));
    CHECK(editor.addEdge(b, c, &error));
    CHECK(editor.network().map().hasEdge(a, b));

    const int line = editor.addLine("Editor Line", 0x123456u, &error);
    CHECK(line == 0);
    CHECK(editor.appendStop(line, a, &error));
    CHECK(editor.appendStop(line, b, &error));
    CHECK(editor.appendStop(line, c, &error));
    CHECK(editor.network().lines()[0].stationCount() == 3);
    CHECK(editor.validate(&error));

    CHECK(editor.undo());
    CHECK(editor.network().lines()[0].stationCount() == 2);
    CHECK(editor.redo());
    CHECK(editor.network().lines()[0].stationCount() == 3);

    CHECK(editor.moveStation(a, 5.0, 5.0, &error));
    CHECK(std::fabs(editor.network().map().station(a).position.x - 5.0) < 1e-9);
    CHECK(editor.undo());
    CHECK(std::fabs(editor.network().map().station(a).position.x - 0.0) < 1e-9);

    CHECK(editor.removeLine(0, &error));
    CHECK(editor.network().lines().empty());

    const std::string mapPath = "uc_test_editor.cuc3m";
    const std::string linePath = "uc_test_editor.cuc3l";
    CHECK(editor.save(mapPath, linePath, &error));
    CHECK(!editor.isDirty());

    uc::Editor loaded;
    CHECK(loaded.load(mapPath, linePath, &error));
    CHECK(!loaded.isDirty());
    CHECK(loaded.network().map().stationCount() == 3);
    CHECK(loaded.network().lines().empty());

    std::filesystem::remove(mapPath);
    std::filesystem::remove(linePath);
}

void testEditorDeletionAndLineUpdate() {
    uc::Editor editor;
    std::string error;
    const uc::Index a = editor.addStation("A", 0.0, 0.0, &error);
    const uc::Index b = editor.addStation("B", 1.0, 0.0, &error);
    const uc::Index c = editor.addStation("C", 2.0, 0.0, &error);
    const uc::Index d = editor.addStation("D", 3.0, 0.0, &error);
    CHECK(editor.addEdge(a, b, &error));
    CHECK(editor.addEdge(b, c, &error));
    CHECK(editor.addEdge(c, d, &error));
    CHECK(editor.addEdge(a, d, &error));

    const int line = editor.addLine("BC", 0x00ff00u, &error);
    CHECK(editor.appendStop(line, b, &error));
    CHECK(editor.appendStop(line, c, &error));

    CHECK(!editor.removeEdge(b, c, &error));
    CHECK(!editor.removeStation(b, &error));
    CHECK(editor.removeEdge(a, d, &error));
    CHECK(!editor.network().map().hasEdge(a, d));

    uc::Line updated = editor.network().lines()[0];
    updated.setName("Renamed");
    updated.ops().dwellSec = 45.0;
    CHECK(editor.replaceLine(line, updated, &error));
    CHECK(editor.network().lines()[0].name() == "Renamed");
    CHECK(std::fabs(editor.network().lines()[0].ops().dwellSec - 45.0) < 1e-9);
    CHECK(editor.undo());
    CHECK(editor.network().lines()[0].name() == "BC");

    CHECK(editor.removeLine(line, &error));
    CHECK(editor.removeEdge(b, c, &error));

    uc::Editor remapEditor;
    const uc::Index ra = remapEditor.addStation("A", 0.0, 0.0, &error);
    const uc::Index rb = remapEditor.addStation("B", 1.0, 0.0, &error);
    const uc::Index rc = remapEditor.addStation("C", 2.0, 0.0, &error);
    const uc::Index rd = remapEditor.addStation("D", 3.0, 0.0, &error);
    CHECK(remapEditor.addEdge(ra, rb, &error));
    CHECK(remapEditor.addEdge(rb, rc, &error));
    CHECK(remapEditor.addEdge(rc, rd, &error));
    const int remapLine = remapEditor.addLine("B-C", 0x0000ffu, &error);
    CHECK(remapEditor.appendStop(remapLine, rb, &error));
    CHECK(remapEditor.appendStop(remapLine, rc, &error));
    const int remapLine2 = remapEditor.addLine("C-D", 0x00ffffu, &error);
    CHECK(remapEditor.appendStop(remapLine2, rc, &error));
    CHECK(remapEditor.appendStop(remapLine2, rd, &error));
    CHECK(remapEditor.setTransferTime(rc, 0, 1, 90.0, &error));

    CHECK(remapEditor.removeStation(ra, &error));
    CHECK(remapEditor.network().map().stationCount() == 3);
    CHECK(remapEditor.network().lines()[0].stations()[0] == 0);
    CHECK(remapEditor.network().lines()[0].stations()[1] == 1);
    CHECK(remapEditor.network().lines()[1].stations()[0] == 1);
    CHECK(remapEditor.network().lines()[1].stations()[1] == 2);
    CHECK(remapEditor.network().map().hasEdge(0, 1));
    CHECK(remapEditor.network().map().hasEdge(1, 2));
    CHECK(std::fabs(remapEditor.network().transferTime(1, 0, 1) - 90.0) < 1e-9);
    CHECK(remapEditor.validate(&error));
}

}  // namespace

int main() {
    testDynamicArray();
    testHashMap();
    testBinaryHeap();
    testSort();
    testUnionFind();
    testQueueStack();
    testMapAndLine();
    testNetwork();
    testIoRoundTrip();
    testRouter();
    testEditor();
    testEditorDeletionAndLineUpdate();

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
