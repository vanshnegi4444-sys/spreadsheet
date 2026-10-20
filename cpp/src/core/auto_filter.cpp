#include "auto_filter.h"
#include <algorithm>

namespace xspreadsheet {

bool Filter::includes(const std::string& v) const {
    if (op == "all") return true;
    if (op == "in") {
        return std::find(values.begin(), values.end(), v) != values.end();
    }
    if (op == "eq" && !values.empty()) return v == values[0];
    if (op == "neq" && !values.empty()) return v != values[0];
    return false;
}

nlohmann::json Filter::toJson() const {
    nlohmann::json j;
    j["ci"] = ci;
    j["operator"] = op;
    j["value"] = values;
    return j;
}

Filter Filter::fromJson(const nlohmann::json& j) {
    Filter f;
    if (j.contains("ci") && j["ci"].is_number()) f.ci = j["ci"].get<int>();
    if (j.contains("operator") && j["operator"].is_string()) f.op = j["operator"].get<std::string>();
    if (j.contains("value")) {
        if (j["value"].is_array()) {
            for (const auto& v : j["value"]) {
                if (v.is_string()) f.values.push_back(v.get<std::string>());
            }
        } else if (j["value"].is_string()) {
            f.values.push_back(j["value"].get<std::string>());
        }
    }
    return f;
}

nlohmann::json Sort::toJson() const {
    nlohmann::json j;
    j["ci"] = ci;
    j["order"] = order;
    return j;
}

Sort Sort::fromJson(const nlohmann::json& j) {
    Sort s;
    if (j.contains("ci") && j["ci"].is_number()) s.ci = j["ci"].get<int>();
    if (j.contains("order") && j["order"].is_string()) s.order = j["order"].get<std::string>();
    return s;
}

void AutoFilter::clear() {
    ref.clear();
    filters.clear();
    sort.reset();
}

CellRange AutoFilter::range() const {
    if (ref.empty()) return CellRange();
    return CellRange::valueOf(ref);
}

CellRange AutoFilter::hrange() const {
    CellRange r = range();
    r.eri = r.sri;
    return r;
}

bool AutoFilter::includes(int ri, int ci) const {
    if (!active()) return false;
    return hrange().includes(ri, ci);
}

Filter* AutoFilter::getFilter(int ci) {
    for (auto& f : filters) {
        if (f.ci == ci) return &f;
    }
    return nullptr;
}

const Filter* AutoFilter::getFilter(int ci) const {
    for (const auto& f : filters) {
        if (f.ci == ci) return &f;
    }
    return nullptr;
}

void AutoFilter::addFilter(int ci, const std::string& op, const std::vector<std::string>& values) {
    Filter* f = getFilter(ci);
    if (!f) {
        Filter newF;
        newF.ci = ci;
        newF.op = op;
        newF.values = values;
        filters.push_back(std::move(newF));
    } else {
        f->op = op;
        f->values = values;
    }
}

Sort* AutoFilter::getSort(int ci) {
    if (sort && sort->ci == ci) return &(*sort);
    return nullptr;
}

void AutoFilter::setSort(int ci, const std::string& order) {
    if (order.empty()) {
        sort.reset();
    } else {
        sort = Sort{ ci, order };
    }
}

std::map<std::string, int> AutoFilter::items(int ci, std::function<std::string(int r, int c)> getCellText) const {
    std::map<std::string, int> m;
    if (active()) {
        CellRange cr = range();
        for (int ri = cr.sri + 1; ri <= cr.eri; ++ri) {
            std::string text = getCellText(ri, ci);
            m[text]++;
        }
    }
    return m;
}

void AutoFilter::filteredRows(std::function<std::string(int r, int c)> getCellText,
                             std::set<int>& rset,
                             std::vector<int>& fset) const {
    rset.clear();
    fset.clear();
    if (!active()) return;

    CellRange cr = range();
    for (int ri = cr.sri + 1; ri <= cr.eri; ++ri) {
        bool match = true;
        for (const auto& filter : filters) {
            std::string cellText = getCellText(ri, filter.ci);
            if (!filter.includes(cellText)) {
                match = false;
                break;
            }
        }
        if (!match) {
            rset.insert(ri);
        } else {
            fset.push_back(ri);
        }
    }
}

nlohmann::json AutoFilter::toJson() const {
    nlohmann::json j;
    if (active()) {
        j["ref"] = ref;
        nlohmann::json fAry = nlohmann::json::array();
        for (const auto& f : filters) fAry.push_back(f.toJson());
        j["filters"] = fAry;
        if (sort) j["sort"] = sort->toJson();
    }
    return j;
}

void AutoFilter::fromJson(const nlohmann::json& j) {
    clear();
    if (j.contains("ref") && j["ref"].is_string()) {
        ref = j["ref"].get<std::string>();
        if (j.contains("filters") && j["filters"].is_array()) {
            for (const auto& fj : j["filters"]) {
                filters.push_back(Filter::fromJson(fj));
            }
        }
        if (j.contains("sort") && j["sort"].is_object()) {
            sort = Sort::fromJson(j["sort"]);
        }
    }
}

} // namespace xspreadsheet
