#include "merge.h"

namespace xspreadsheet {

void Merges::add(const CellRange& cr) {
    deleteWithin(cr);
    items.push_back(cr);
}

void Merges::deleteWithin(const CellRange& cr) {
    std::vector<CellRange> next;
    for (const auto& it : items) {
        if (!it.within(cr)) {
            next.push_back(it);
        }
    }
    items = std::move(next);
}

std::optional<CellRange> Merges::getFirstIncludes(int ri, int ci) const {
    for (const auto& it : items) {
        if (it.includes(ri, ci)) {
            return it;
        }
    }
    return std::nullopt;
}

bool Merges::intersects(const CellRange& cr) const {
    for (const auto& it : items) {
        if (it.intersects(cr)) {
            return true;
        }
    }
    return false;
}

CellRange Merges::unionRange(const CellRange& cr) const {
    CellRange res = cr;
    for (const auto& it : items) {
        if (it.intersects(res)) {
            res = it.unionRange(res);
        }
    }
    return res;
}

std::vector<CellRange> Merges::filterIntersects(const CellRange& cr) const {
    std::vector<CellRange> res;
    for (const auto& it : items) {
        if (it.intersects(cr)) {
            res.push_back(it);
        }
    }
    return res;
}

void Merges::shift(const std::string& type, int index, int n,
                   std::function<void(int, int, int, int)> cbWithin) {
    for (auto& range : items) {
        if (type == "row") {
            if (range.sri >= index) {
                range.sri += n;
                range.eri += n;
            } else if (range.sri < index && index <= range.eri) {
                range.eri += n;
                if (cbWithin) cbWithin(range.sri, range.sci, n, 0);
            }
        } else if (type == "column") {
            if (range.sci >= index) {
                range.sci += n;
                range.eci += n;
            } else if (range.sci < index && index <= range.eci) {
                range.eci += n;
                if (cbWithin) cbWithin(range.sri, range.sci, 0, n);
            }
        }
    }
}

void Merges::move(const CellRange& cr, int rn, int cn) {
    for (auto& it : items) {
        if (it.within(cr)) {
            it.sri += rn;
            it.eri += rn;
            it.sci += cn;
            it.eci += cn;
        }
    }
}

nlohmann::json Merges::toJson() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& it : items) {
        j.push_back(it.toString());
    }
    return j;
}

Merges Merges::fromJson(const nlohmann::json& j) {
    Merges m;
    if (j.is_array()) {
        for (const auto& item : j) {
            if (item.is_string()) {
                m.items.push_back(CellRange::valueOf(item.get<std::string>()));
            }
        }
    }
    return m;
}

} // namespace xspreadsheet
