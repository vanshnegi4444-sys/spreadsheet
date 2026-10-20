#pragma once
#include "cell_range.h"
#include <vector>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

class Merges {
public:
    std::vector<CellRange> items;

    Merges() = default;

    void add(const CellRange& cr);
    void deleteWithin(const CellRange& cr);
    std::optional<CellRange> getFirstIncludes(int ri, int ci) const;
    bool intersects(const CellRange& cr) const;
    CellRange unionRange(const CellRange& cr) const;
    std::vector<CellRange> filterIntersects(const CellRange& cr) const;

    // type: "row" | "column"
    void shift(const std::string& type, int index, int n,
               std::function<void(int ri, int ci, int rn, int cn)> cbWithin = nullptr);

    void move(const CellRange& cr, int rn, int cn);

    nlohmann::json toJson() const;
    static Merges fromJson(const nlohmann::json& j);
};

} // namespace xspreadsheet
