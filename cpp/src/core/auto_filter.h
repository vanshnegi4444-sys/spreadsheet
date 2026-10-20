#pragma once
#include "cell_range.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

struct Filter {
    int ci = 0;
    std::string op = "in"; // "all", "in", "eq", "neq", etc.
    std::vector<std::string> values;

    bool includes(const std::string& v) const;
    nlohmann::json toJson() const;
    static Filter fromJson(const nlohmann::json& j);
};

struct Sort {
    int ci = 0;
    std::string order = "asc"; // "asc", "desc"

    bool asc() const { return order == "asc"; }
    bool desc() const { return order == "desc"; }

    nlohmann::json toJson() const;
    static Sort fromJson(const nlohmann::json& j);
};

class AutoFilter {
public:
    std::string ref;
    std::vector<Filter> filters;
    std::optional<Sort> sort;

    bool active() const { return !ref.empty(); }
    void clear();

    CellRange range() const;
    CellRange hrange() const;

    bool includes(int ri, int ci) const;

    Filter* getFilter(int ci);
    const Filter* getFilter(int ci) const;
    void addFilter(int ci, const std::string& op, const std::vector<std::string>& values);

    Sort* getSort(int ci);
    void setSort(int ci, const std::string& order);

    // returns map of string_value -> count
    std::map<std::string, int> items(int ci, std::function<std::string(int r, int c)> getCellText) const;

    // returns { rset (rows to hide), fset (matching rows) }
    void filteredRows(std::function<std::string(int r, int c)> getCellText,
                      std::set<int>& rset,
                      std::vector<int>& fset) const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace xspreadsheet
