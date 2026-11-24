#include <cstdio>

#include "core/network.h"

namespace {

uc::Station makeStation(const char* id, const char* name, double x, double y) {
    uc::Station station;
    station.id = id;
    station.name = name;
    station.position = uc::Vec2{x, y};
    return station;
}

void buildDemoNetwork(uc::Network& network) {
    uc::Map& map = network.map();
    map.addStation(makeStation("A", "Alpha", 0.0, 0.0));
    map.addStation(makeStation("B", "Bravo", 1.0, 0.0));
    map.addStation(makeStation("C", "Charlie", 2.0, 0.0));
    map.addStation(makeStation("D", "Delta", 3.0, 0.0));

    const uc::Index a = map.findStation("A");
    const uc::Index b = map.findStation("B");
    const uc::Index c = map.findStation("C");
    const uc::Index d = map.findStation("D");
    map.addEdge(a, b, 1.0);
    map.addEdge(b, c, 1.0);
    map.addEdge(c, d, 1.0);

    uc::Line line(0, "Demo Line", 0xff3333u);
    line.stations().push_back(a);
    line.stations().push_back(b);
    line.stations().push_back(c);
    line.stations().push_back(d);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().segmentRunSec.push_back(120.0);
    line.ops().headway.push_back(uc::HeadwayPeriod{0, 86400, 300});
    network.addLine(line);
}

}  // namespace

int main() {
    uc::Network network;
    buildDemoNetwork(network);

    std::string error;
    if (!network.validate(&error)) {
        std::printf("network validation failed: %s\n", error.c_str());
        return 1;
    }

    std::printf("Concept UrbanChain v0.3 skeleton\n");
    std::printf("stations: %d, edges: %d, lines: %d\n",
                network.map().stationCount(),
                static_cast<int>(network.map().edges().size()),
                static_cast<int>(network.lines().size()));
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const uc::Line& line = network.lines()[i];
        std::printf("line %d: %s, %d stations, length %.3f\n",
                    line.id(),
                    line.name().c_str(),
                    line.stationCount(),
                    line.length(network.map()));
    }
    return 0;
}
