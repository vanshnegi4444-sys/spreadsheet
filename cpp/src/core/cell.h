#pragma once
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

struct Cell {
    std::string text;
    std::string value;
    std::string evaluatedValue;
    bool isDirty = true;
    int style = -1;
    std::pair<int, int> merge = {0, 0}; // [rows_span_extra, cols_span_extra]
    bool editable = true;
    bool printable = true;

    nlohmann::json toJson() const {
        nlohmann::json j;
        if (!text.empty()) j["text"] = text;
        if (!value.empty()) j["value"] = value;
        if (style >= 0) j["style"] = style;
        if (merge.first > 0 || merge.second > 0) {
            j["merge"] = { merge.first, merge.second };
        }
        if (!editable) j["editable"] = false;
        if (!printable) j["printable"] = false;
        return j;
    }

    static Cell fromJson(const nlohmann::json& j) {
        Cell c;
        if (j.contains("text") && j["text"].is_string()) c.text = j["text"].get<std::string>();
        if (j.contains("value") && j["value"].is_string()) c.value = j["value"].get<std::string>();
        if (j.contains("style") && j["style"].is_number()) c.style = j["style"].get<int>();
        if (j.contains("merge") && j["merge"].is_array() && j["merge"].size() >= 2) {
            c.merge = { j["merge"][0].get<int>(), j["merge"][1].get<int>() };
        }
        if (j.contains("editable") && j["editable"].is_boolean()) c.editable = j["editable"].get<bool>();
        if (j.contains("printable") && j["printable"].is_boolean()) c.printable = j["printable"].get<bool>();
        return c;
    }
};

} // namespace xspreadsheet
