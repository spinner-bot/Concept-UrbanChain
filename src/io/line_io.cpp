#include "io/line_io.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

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

bool loadLines(const std::string& path, Network& network, std::string* error) {
    std::ifstream input(std::filesystem::u8path(path), std::ios::in | std::ios::binary);
    if (!input) {
        if (error != nullptr) {
            *error = "cannot open line file: " + path;
        }
        return false;
    }

    DynamicArray<Line> loadedLines;
    DynamicArray<TransferRecord> loadedTransfers;
    std::string line;
    std::size_t lineNumber = 0;
    bool headerSeen = false;
    bool finished = false;
    int currentLine = -1;

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
            if (keyword != "CUC3L") {
                return setError(error, lineNumber, "expected CUC3L header");
            }
            int version = 0;
            if (!(stream >> version) || version != 1) {
                return setError(error, lineNumber, "unsupported CUC3L version");
            }
            headerSeen = true;
            continue;
        }

        if (keyword == "END") {
            finished = true;
            break;
        }

        if (keyword == "LINE") {
            std::string colorToken;
            if (!(stream >> colorToken)) {
                return setError(error, lineNumber, "LINE expects a color");
            }
            std::uint32_t color = 0;
            try {
                color = static_cast<std::uint32_t>(std::stoul(colorToken, nullptr, 0));
            } catch (...) {
                return setError(error, lineNumber, "invalid line color: " + colorToken);
            }
            std::string name;
            std::getline(stream, name);
            name = trimLeft(name);
            Line entry(static_cast<int>(loadedLines.size()),
                       name.empty() ? "line" : name,
                       color);
            loadedLines.push_back(static_cast<Line&&>(entry));
            currentLine = static_cast<int>(loadedLines.size()) - 1;
            continue;
        }

        if (currentLine < 0 && keyword != "TRANSFER") {
            return setError(error, lineNumber, "line data before LINE keyword");
        }

        if (keyword == "STOPS") {
            Line& target = loadedLines[static_cast<std::size_t>(currentLine)];
            std::string stationId;
            while (stream >> stationId) {
                const Index station = network.map().findStation(stationId);
                if (station == kInvalidIndex) {
                    return setError(error, lineNumber, "unknown station: " + stationId);
                }
                target.stations().push_back(station);
            }
            continue;
        }

        if (keyword == "RUN") {
            Line& target = loadedLines[static_cast<std::size_t>(currentLine)];
            double seconds = 0.0;
            while (stream >> seconds) {
                target.ops().segmentRunSec.push_back(seconds);
            }
            continue;
        }

        if (keyword == "DWELL") {
            Line& target = loadedLines[static_cast<std::size_t>(currentLine)];
            if (!(stream >> target.ops().dwellSec)) {
                return setError(error, lineNumber, "DWELL expects seconds");
            }
            continue;
        }

        if (keyword == "HEADWAY") {
            Line& target = loadedLines[static_cast<std::size_t>(currentLine)];
            HeadwayPeriod period;
            if (!(stream >> period.startSec >> period.endSec >> period.headwaySec)) {
                return setError(error, lineNumber, "HEADWAY expects start, end, interval");
            }
            target.ops().headway.push_back(period);
            continue;
        }

        if (keyword == "TRANSFER") {
            std::string stationId;
            TransferRecord record;
            if (!(stream >> stationId >> record.fromLine >> record.toLine >> record.seconds)) {
                return setError(error, lineNumber,
                                "TRANSFER expects station, from line, to line, seconds");
            }
            record.station = network.map().findStation(stationId);
            if (record.station == kInvalidIndex) {
                return setError(error, lineNumber, "unknown station: " + stationId);
            }
            loadedTransfers.push_back(record);
            continue;
        }

        return setError(error, lineNumber, "unknown keyword: " + keyword);
    }

    if (!headerSeen) {
        if (error != nullptr) {
            *error = "line file is empty or has no header";
        }
        return false;
    }
    if (!finished) {
        if (error != nullptr) {
            *error = "line file has no END marker";
        }
        return false;
    }

    Network staged;
    staged.map() = network.map();
    for (std::size_t i = 0; i < loadedLines.size(); ++i) {
        staged.addLine(static_cast<Line&&>(loadedLines[i]));
    }
    for (std::size_t i = 0; i < loadedTransfers.size(); ++i) {
        const TransferRecord& record = loadedTransfers[i];
        if (!staged.setTransferTime(record.station, record.fromLine, record.toLine,
                                    record.seconds)) {
            if (error != nullptr) {
                *error = "line " + std::to_string(i + 1) + ": invalid transfer record";
            }
            return false;
        }
    }
    if (!staged.validate(error)) {
        return false;
    }

    network = static_cast<Network&&>(staged);
    return true;
}

bool saveLines(const std::string& path, const Network& network, std::string* error) {
    std::ofstream output(std::filesystem::u8path(path),
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) {
        if (error != nullptr) {
            *error = "cannot open line file for writing: " + path;
        }
        return false;
    }

    output << "CUC3L 1\n";
    output << std::setprecision(17);
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const Line& line = network.lines()[i];
        output << "LINE 0x" << std::hex << std::uppercase << std::setw(6) << std::setfill('0')
               << line.color() << std::dec << std::nouppercase << std::setfill(' ') << ' '
               << line.name() << '\n';
        output << "STOPS";
        for (std::size_t j = 0; j < line.stations().size(); ++j) {
            output << ' ' << network.map().station(line.stations()[j]).id;
        }
        output << '\n';
        output << "RUN";
        for (std::size_t j = 0; j < line.ops().segmentRunSec.size(); ++j) {
            output << ' ' << line.ops().segmentRunSec[j];
        }
        output << '\n';
        output << "DWELL " << line.ops().dwellSec << '\n';
        for (std::size_t j = 0; j < line.ops().headway.size(); ++j) {
            const HeadwayPeriod& period = line.ops().headway[j];
            output << "HEADWAY " << period.startSec << ' ' << period.endSec << ' '
                   << period.headwaySec << '\n';
        }
    }

    const DynamicArray<TransferRecord> transfers = network.transferRecords();
    for (std::size_t i = 0; i < transfers.size(); ++i) {
        const TransferRecord& record = transfers[i];
        output << "TRANSFER " << network.map().station(record.station).id << ' '
               << record.fromLine << ' ' << record.toLine << ' ' << record.seconds << '\n';
    }
    output << "END\n";
    if (!output) {
        if (error != nullptr) {
            *error = "failed to write line file: " + path;
        }
        return false;
    }
    return true;
}

}  // namespace io
}  // namespace uc
