#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace nm {

// Minimal CSV reader. Handles quoted fields and auto-detects the delimiter
// (comma / semicolon / tab) so it copes with exports from spreadsheets and
// football databases alike.
class Csv {
public:
    static std::vector<std::string> splitLine(const std::string& line, char delim = ',') {
        std::vector<std::string> out;
        std::string field;
        bool inQuotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (inQuotes) {
                if (c == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        ++i;
                    } else {
                        inQuotes = false;
                    }
                } else {
                    field += c;
                }
            } else {
                if (c == '"') {
                    inQuotes = true;
                } else if (c == delim) {
                    out.push_back(trim(field));
                    field.clear();
                } else {
                    field += c;
                }
            }
        }
        out.push_back(trim(field));
        return out;
    }

    // Pick whichever of , ; \t appears most often on the header line.
    static char detectDelimiter(const std::string& header) {
        const char candidates[] = {',', ';', '\t'};
        char best = ',';
        size_t bestCount = 0;
        for (char d : candidates) {
            size_t n = 0;
            bool inQuotes = false;
            for (char c : header) {
                if (c == '"') inQuotes = !inQuotes;
                else if (c == d && !inQuotes) ++n;
            }
            if (n > bestCount) {
                bestCount = n;
                best = d;
            }
        }
        return best;
    }

    static bool read(const std::string& path, std::vector<std::vector<std::string>>& rows) {
        std::ifstream in(path);
        if (!in.is_open()) return false;
        std::string line;
        char delim = ',';
        bool first = true;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (first) {
                delim = detectDelimiter(line);
                first = false;
            }
            rows.push_back(splitLine(line, delim));
        }
        return true;
    }

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t");
        return s.substr(a, b - a + 1);
    }
};

}  // namespace nm
