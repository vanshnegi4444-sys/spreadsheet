#include "format.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace xspreadsheet {

std::string Formatter::formatNumber(double num) {
    bool negative = (num < 0);
    double absNum = std::abs(num);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << absNum;
    std::string s = ss.str();

    auto dotPos = s.find('.');
    std::string intPart = s.substr(0, dotPos);
    std::string fracPart = (dotPos != std::string::npos) ? s.substr(dotPos) : "";

    std::string withCommas;
    int len = static_cast<int>(intPart.length());
    for (int i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0) {
            withCommas += ',';
        }
        withCommas += intPart[i];
    }
    return (negative ? "-" : "") + withCommas + fracPart;
}

std::string Formatter::format(const std::string& formatKey, const std::string& rawText) {
    if (rawText.empty()) return "";
    if (formatKey.empty() || formatKey == "normal" || formatKey == "text") {
        return rawText;
    }

    // Try parsing as double
    char* end = nullptr;
    double val = std::strtod(rawText.c_str(), &end);
    bool isNumeric = (end != rawText.c_str() && *end == '\0');

    if (formatKey == "number") {
        if (isNumeric) return formatNumber(val);
        return rawText;
    }
    if (formatKey == "percent") {
        if (isNumeric) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << (val * 100.0) << "%";
            return ss.str();
        }
        return rawText + "%";
    }
    if (formatKey == "rmb") {
        if (isNumeric) return "￥" + formatNumber(val);
        return "￥" + rawText;
    }
    if (formatKey == "usd") {
        if (isNumeric) return "$" + formatNumber(val);
        return "$" + rawText;
    }
    if (formatKey == "eur") {
        if (isNumeric) return "€" + formatNumber(val);
        return "€" + rawText;
    }
    // "date", "time", "datetime", "duration"
    return rawText;
}

const std::vector<FormatItem>& Formatter::getBaseFormats() {
    static const std::vector<FormatItem> formats = {
        { "normal", "format.normal", "string", "Normal" },
        { "text", "format.text", "string", "Plain Text" },
        { "number", "format.number", "number", "1,000.12" },
        { "percent", "format.percent", "number", "10.12%" },
        { "rmb", "format.rmb", "number", "￥10.00" },
        { "usd", "format.usd", "number", "$10.00" },
        { "eur", "format.eur", "number", "€10.00" },
        { "date", "format.date", "date", "2026-10-18" },
        { "time", "format.time", "date", "15:59:00" },
        { "datetime", "format.datetime", "date", "2026-10-18 15:59:00" },
        { "duration", "format.duration", "date", "24:01:00" }
    };
    return formats;
}

} // namespace xspreadsheet
