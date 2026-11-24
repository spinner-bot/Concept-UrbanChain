#include "io/map_io.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace uc {
namespace io {
namespace {

std::string trimLeft(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
        ++begin;
    }
    return text.substr(begin);
}

bool setError(std::string* error, std::size_t lineNumber, const std::string& message) {
    if (error != nullptr) {
        *error = "line " + std::to_string(lineNumber) + ": " + message;
    }
    return false;
}

void stripBom(std::string& line) {
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xef &&
        static_cast<unsigned char>(line[1]) == 0xbb &&
        static_cast<unsigned char>(line[2]) == 0xbf) {
        line.erase(0, 3);
    }
}

}  // namespace

bool loadMap(const std::string& path, Map& map, std::string* error) {
    std::ifstream input(std::filesystem::u8path(path), std::ios::in | std::ios::binary);
    if (!input) {
        if (error != nullptr) {
            *error = "cannot open map file: " + path;
        }
        return false;
    }

    Map loaded;
    std::string line;
    std::size_t lineNumber = 0;
    bool headerSeen = false;
    bool finished = false;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (lineNumber == 1) {
            stripBom(line);
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (!headerSeen) {
            if (keyword != "CUC3M") {
                return setError(error, lineNumber, "expected CUC3M header");
            }
            int version = 0;
            if (!(stream >> version) || version != 1) {
                return setError(error, lineNumber, "unsupported CUC3M version");
            }
            headerSeen = true;
            continue;
        }

        if (keyword == "END") {
            finished = true;
            break;
        }

        if (keyword == "BACKGROUND") {
            int layerCount = 0;
            if (!(stream >> layerCount)) {
                return setError(error, lineNumber, "BACKGROUND expects a layer count");
            }
            if (layerCount != 0) {
                return setError(error, lineNumber, "background layers are not supported in v3");
            }
            continue;
        }

        if (keyword == "STATION") {
            std::string id;
            double x = 0.0;
            double y = 0.0;
            if (!(stream >> id >> x >> y)) {
                return setError(error, lineNumber, "STATION expects id, x, y");
            }
            std::string name;
            std::getline(stream, name);
            name = trimLeft(name);
            if (name.empty()) {
                name = id;
            }
            Station station;
            station.id = id;
            station.name = name;
            station.position = Vec2{x, y};
            if (!loaded.addStation(station)) {
                return setError(error, lineNumber, "duplicate station id: " + id);
            }
            continue;
        }

        if (keyword == "EDGE") {
            std::string fromId;
            std::string toId;
            double cost = 0.0;
            if (!(stream >> fromId >> toId >> cost)) {
                return setError(error, lineNumber, "EDGE expects two station ids and a cost");
            }
            double length = -1.0;
            if (!(stream >> length)) {
                length = -1.0;
            }
            const Index from = loaded.findStation(fromId);
            const Index to = loaded.findStation(toId);
            if (from == kInvalidIndex || to == kInvalidIndex) {
                return setError(error, lineNumber, "EDGE references an unknown station");
            }
            if (!loaded.addEdge(from, to, cost, length)) {
                return setError(error, lineNumber, "invalid or duplicate EDGE");
            }
            continue;
        }

        return setError(error, lineNumber, "unknown keyword: " + keyword);
    }

    if (!headerSeen) {
        if (error != nullptr) {
            *error = "map file is empty or has no header";
        }
        return false;
    }
    if (!finished) {
        if (error != nullptr) {
            *error = "map file has no END marker";
        }
        return false;
    }

    map = static_cast<Map&&>(loaded);
    return true;
}

bool saveMap(const std::string& path, const Map& map, std::string* error) {
    std::ofstream output(std::filesystem::u8path(path),
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) {
        if (error != nullptr) {
            *error = "cannot open map file for writing: " + path;
        }
        return false;
    }

    output << "CUC3M 1\n";
    output << "BACKGROUND 0\n";
    output << std::setprecision(17);
    for (std::size_t i = 0; i < map.stations().size(); ++i) {
        const Station& station = map.stations()[i];
        output << "STATION " << station.id << ' ' << station.position.x << ' '
               << station.position.y << ' ' << station.name << '\n';
    }
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        const EdgeRecord& edge = map.edges()[i];
        output << "EDGE " << map.station(edge.from).id << ' ' << map.station(edge.to).id << ' '
               << edge.cost << ' ' << edge.length << '\n';
    }
    output << "END\n";
    if (!output) {
        if (error != nullptr) {
            *error = "failed to write map file: " + path;
        }
        return false;
    }
    return true;
}

}  // namespace io
}  // namespace uc
