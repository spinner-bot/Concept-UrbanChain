#include "app/console_app.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "core/network.h"
#include "io/line_io.h"
#include "io/map_io.h"
#include "nav/router.h"

namespace uc {
namespace {

bool loadNetwork(Network& network, const std::string& mapPath, const std::string& linePath,
                 std::string* error) {
    if (!io::loadMap(mapPath, network.map(), error)) {
        return false;
    }
    return io::loadLines(linePath, network, error);
}

std::string formatClock(double seconds) {
    int total = static_cast<int>(std::floor(seconds + 0.5));
    if (total < 0) {
        total = 0;
    }
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
    return std::string(buffer);
}

bool parseClock(const std::string& text, double* seconds) {
    int parts[3] = {0, 0, 0};
    int count = 0;
    std::size_t begin = 0;
    while (begin <= text.size() && count < 3) {
        const std::size_t end = text.find(':', begin);
        const std::string token = text.substr(begin, end == std::string::npos ? text.size() - begin
                                                                              : end - begin);
        if (token.empty()) {
            return false;
        }
        try {
            parts[count++] = std::stoi(token);
        } catch (...) {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    if (count < 2 || parts[0] < 0 || parts[1] < 0 || parts[1] >= 60 || parts[2] < 0 ||
        parts[2] >= 60) {
        return false;
    }
    *seconds = parts[0] * 3600.0 + parts[1] * 60.0 + parts[2];
    return true;
}

void printInfo(const Network& network) {
    std::cout << "stations: " << network.map().stationCount()
              << ", edges: " << network.map().edges().size()
              << ", lines: " << network.lines().size() << '\n';
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const Line& line = network.lines()[i];
        std::cout << "  line " << line.id() << ": " << line.name() << ", "
                  << line.stationCount() << " stations, length " << line.length(network.map())
                  << '\n';
    }
}

void printJourney(const Network& network, const Journey& journey) {
    if (!journey.found) {
        std::cout << "no route found\n";
        return;
    }
    std::cout << "arrive " << formatClock(journey.arriveTime) << ", transfers "
              << journey.transfers << '\n';
    for (std::size_t i = 0; i < journey.legs.size(); ++i) {
        const JourneyLeg& leg = journey.legs[i];
        const Line& line = network.lines()[static_cast<std::size_t>(leg.line)];
        std::cout << "  " << (i + 1) << ". " << line.name() << ": "
                  << network.map().station(leg.fromStation).id << " -> "
                  << network.map().station(leg.toStation).id << "  "
                  << formatClock(leg.boardTime) << " - " << formatClock(leg.alightTime)
                  << '\n';
    }
}

void printHelp() {
    std::cout << "commands:\n"
              << "  load <map.cuc3m> <lines.cuc3l>\n"
              << "  info\n"
              << "  route <fromStationId> <toStationId> <HH:MM[:SS]>\n"
              << "  help\n"
              << "  quit\n";
}

}  // namespace

int runConsoleApp(int argc, char** argv) {
    Network network;
    std::string error;
    bool loaded = false;
    if (argc >= 3) {
        loaded = loadNetwork(network, argv[1], argv[2], &error);
        if (!loaded) {
            std::cout << "load failed: " << error << '\n';
        }
    }

    std::cout << "Concept UrbanChain v0.3 console\n";
    if (loaded) {
        printInfo(network);
    } else {
        std::cout << "no map loaded; use: load <map.cuc3m> <lines.cuc3l>\n";
    }
    printHelp();

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command.empty()) {
            continue;
        }
        if (command == "quit" || command == "exit") {
            break;
        }
        if (command == "help") {
            printHelp();
            continue;
        }
        if (command == "load") {
            std::string mapPath;
            std::string linePath;
            if (!(input >> mapPath >> linePath)) {
                std::cout << "usage: load <map.cuc3m> <lines.cuc3l>\n";
                continue;
            }
            Network staged;
            std::string loadError;
            if (!loadNetwork(staged, mapPath, linePath, &loadError)) {
                std::cout << "load failed: " << loadError << '\n';
                continue;
            }
            network = static_cast<Network&&>(staged);
            loaded = true;
            std::cout << "loaded\n";
            printInfo(network);
            continue;
        }
        if (command == "info") {
            if (!loaded) {
                std::cout << "no map loaded\n";
                continue;
            }
            printInfo(network);
            continue;
        }
        if (command == "route") {
            if (!loaded) {
                std::cout << "no map loaded\n";
                continue;
            }
            std::string fromId;
            std::string toId;
            if (!(input >> fromId >> toId)) {
                std::cout << "usage: route <fromStationId> <toStationId> <HH:MM[:SS]>\n";
                continue;
            }
            std::string clockText;
            if (!(input >> clockText)) {
                clockText = "08:00";
            }
            const Index from = network.map().findStation(fromId);
            const Index to = network.map().findStation(toId);
            if (from == kInvalidIndex || to == kInvalidIndex) {
                std::cout << "unknown station id\n";
                continue;
            }
            double departTime = 0.0;
            if (!parseClock(clockText, &departTime)) {
                std::cout << "invalid time: " << clockText << '\n';
                continue;
            }
            Router router;
            printJourney(network, router.findRoute(network, from, to, departTime));
            continue;
        }
        std::cout << "unknown command: " << command << '\n';
    }
    return 0;
}

}  // namespace uc
