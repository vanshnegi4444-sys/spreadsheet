#pragma once
#include <string>
#include <vector>

namespace xspreadsheet {

struct FormatItem {
    std::string key;
    std::string titleKey;
    std::string type; // "string", "number", "date"
    std::string label;
};

class Formatter {
public:
    static std::string formatNumber(double num);
    static std::string format(const std::string& formatKey, const std::string& rawText);
    static const std::vector<FormatItem>& getBaseFormats();
};

} // namespace xspreadsheet
