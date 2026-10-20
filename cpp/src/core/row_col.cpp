#include "row_col.h"
#include "alphabet.h"
#include <regex>
#include <algorithm>
#include <cmath>

namespace xspreadsheet {

static std::string updateFormulas(const std::string& text, int xn, int yn, std::function<bool(int, int)> condition = nullptr) {
    if (text.empty() || text[0] != '=') return text;
    static const std::regex cellRegex("([a-zA-Z]{1,3}\\d+)");
    std::string result;
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), cellRegex);
    auto words_end = std::sregex_iterator();
    size_t lastPos = 0;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        result += text.substr(lastPos, match.position() - lastPos);
        std::string cellRef = match.str();
        result += expr2expr(cellRef, xn, yn, condition);
        lastPos = match.position() + match.length();
    }
    result += text.substr(lastPos);
    return result;
}

float Rows::getHeight(int ri) const {
    if (isHide(ri)) return 0.0f;
    auto it = data.find(ri);
    if (it != data.end() && it->second.customHeight) {
        return it->second.height;
    }
    return default_height;
}

void Rows::setHeight(int ri, float v) {
    data[ri].height = v;
    data[ri].customHeight = true;
}

bool Rows::isHide(int ri) const {
    auto it = data.find(ri);
    return it != data.end() && it->second.hide;
}

void Rows::setHide(int ri, bool v) {
    data[ri].hide = v;
}

void Rows::unhide(int idx) {
    int index = idx;
    while (index > 0) {
        index -= 1;
        if (isHide(index)) {
            setHide(index, false);
        } else {
            break;
        }
    }
}

void Rows::setStyle(int ri, int style) {
    data[ri].style = style;
}

float Rows::sumHeight(int min, int max, const std::set<int>* exceptSet) const {
    float s = 0.0f;
    for (int i = min; i < max; ++i) {
        if (exceptSet && exceptSet->count(i)) continue;
        s += getHeight(i);
    }
    return s;
}

float Rows::totalHeight() const {
    return sumHeight(0, len);
}

const Cell* Rows::getCell(int ri, int ci) const {
    auto it = data.find(ri);
    if (it == data.end()) return nullptr;
    auto cit = it->second.cells.find(ci);
    if (cit == it->second.cells.end()) return nullptr;
    return &(cit->second);
}

Cell* Rows::getCell(int ri, int ci) {
    auto it = data.find(ri);
    if (it == data.end()) return nullptr;
    auto cit = it->second.cells.find(ci);
    if (cit == it->second.cells.end()) return nullptr;
    return &(cit->second);
}

Cell& Rows::getCellOrNew(int ri, int ci) {
    return data[ri].cells[ci];
}

std::pair<int, int> Rows::getCellMerge(int ri, int ci) const {
    const Cell* c = getCell(ri, ci);
    if (c) return c->merge;
    return {0, 0};
}

void Rows::setCell(int ri, int ci, const Cell& cell, const std::string& what) {
    Cell& target = getCellOrNew(ri, ci);
    if (what == "all") {
        target = cell;
    } else if (what == "text") {
        target.text = cell.text;
        target.value = cell.value;
    } else if (what == "format") {
        target.style = cell.style;
        if (cell.merge.first > 0 || cell.merge.second > 0) {
            target.merge = cell.merge;
        }
    }
}

void Rows::setCellText(int ri, int ci, const std::string& text) {
    Cell& cell = getCellOrNew(ri, ci);
    if (cell.editable) {
        cell.text = text;
        cell.evaluatedValue.clear();
        cell.isDirty = true;
    }
}

void Rows::deleteCell(int ri, int ci, const std::string& what) {
    auto it = data.find(ri);
    if (it == data.end()) return;
    auto cit = it->second.cells.find(ci);
    if (cit == it->second.cells.end()) return;

    if (!cit->second.editable) return;

    if (what == "all") {
        it->second.cells.erase(cit);
    } else if (what == "text") {
        cit->second.text.clear();
        cit->second.value.clear();
    } else if (what == "format") {
        cit->second.style = -1;
        cit->second.merge = {0, 0};
    } else if (what == "merge") {
        cit->second.merge = {0, 0};
    }
}

void Rows::deleteCells(const CellRange& cellRange, const std::string& what) {
    cellRange.each([&](int ri, int ci) {
        deleteCell(ri, ci, what);
    });
}

void Rows::insert(int sri, int n) {
    std::map<int, RowData> next;
    for (auto& pair : data) {
        int ri = pair.first;
        if (ri >= sri) {
            int nri = ri + n;
            for (auto& cpair : pair.second.cells) {
                cpair.second.text = updateFormulas(cpair.second.text, 0, n, [sri](int x, int y) {
                    return y >= sri;
                });
            }
            next[nri] = std::move(pair.second);
        } else {
            next[ri] = std::move(pair.second);
        }
    }
    data = std::move(next);
    len += n;
}

void Rows::deleteRows(int sri, int eri) {
    int n = eri - sri + 1;
    std::map<int, RowData> next;
    for (auto& pair : data) {
        int ri = pair.first;
        if (ri < sri) {
            next[ri] = std::move(pair.second);
        } else if (ri > eri) {
            int nri = ri - n;
            for (auto& cpair : pair.second.cells) {
                cpair.second.text = updateFormulas(cpair.second.text, 0, -n, [eri](int x, int y) {
                    return y > eri;
                });
            }
            next[nri] = std::move(pair.second);
        }
    }
    data = std::move(next);
    len = std::max(1, len - n);
}

void Rows::insertColumn(int sci, int n) {
    for (auto& pair : data) {
        std::unordered_map<int, Cell> nextCells;
        for (auto& cpair : pair.second.cells) {
            int ci = cpair.first;
            if (ci >= sci) {
                cpair.second.text = updateFormulas(cpair.second.text, n, 0, [sci](int x, int y) {
                    return x >= sci;
                });
                nextCells[ci + n] = std::move(cpair.second);
            } else {
                nextCells[ci] = std::move(cpair.second);
            }
        }
        pair.second.cells = std::move(nextCells);
    }
}

void Rows::deleteColumn(int sci, int eci) {
    int n = eci - sci + 1;
    for (auto& pair : data) {
        std::unordered_map<int, Cell> nextCells;
        for (auto& cpair : pair.second.cells) {
            int ci = cpair.first;
            if (ci < sci) {
                nextCells[ci] = std::move(cpair.second);
            } else if (ci > eci) {
                cpair.second.text = updateFormulas(cpair.second.text, -n, 0, [eci](int x, int y) {
                    return x > eci;
                });
                nextCells[ci - n] = std::move(cpair.second);
            }
        }
        pair.second.cells = std::move(nextCells);
    }
}

void Rows::copyPaste(const CellRange& srcCellRange, const CellRange& dstCellRange,
                     const std::string& what, bool autofill,
                     std::function<void(int, int, const Cell&)> cb) {
    int rn = srcCellRange.rowCount();
    int cn = srcCellRange.colCount();
    int drn = dstCellRange.rowCount();
    int dcn = dstCellRange.colCount();

    bool isAdd = true;
    int dn = 0;
    if (dstCellRange.eri < srcCellRange.sri || dstCellRange.eci < srcCellRange.sci) {
        isAdd = false;
        if (dstCellRange.eri < srcCellRange.sri) dn = drn;
        else dn = dcn;
    }

    for (int i = srcCellRange.sri; i <= srcCellRange.eri; ++i) {
        auto rowIt = data.find(i);
        if (rowIt == data.end()) continue;

        for (int j = srcCellRange.sci; j <= srcCellRange.eci; ++j) {
            auto cellIt = rowIt->second.cells.find(j);
            if (cellIt == rowIt->second.cells.end()) continue;

            for (int ii = dstCellRange.sri; ii <= dstCellRange.eri; ii += rn) {
                for (int jj = dstCellRange.sci; jj <= dstCellRange.eci; jj += cn) {
                    int nri = ii + (i - srcCellRange.sri);
                    int nci = jj + (j - srcCellRange.sci);
                    Cell ncell = cellIt->second;

                    if (autofill && !ncell.text.empty()) {
                        int step = (jj - dstCellRange.sci) + (ii - dstCellRange.sri) + 2;
                        if (!isAdd) step -= (dn + 1);

                        if (ncell.text[0] == '=') {
                            int xn = (srcCellRange.sri == dstCellRange.sri) ? (step - 1) : 0;
                            int yn = (srcCellRange.sri != dstCellRange.sri) ? (step - 1) : 0;
                            ncell.text = updateFormulas(ncell.text, xn, yn);
                        } else if ((rn <= 1 && cn > 1 && (dstCellRange.sri > srcCellRange.eri || dstCellRange.eri < srcCellRange.sri)) ||
                                   (cn <= 1 && rn > 1 && (dstCellRange.sci > srcCellRange.eci || dstCellRange.eci < srcCellRange.sci)) ||
                                   (rn <= 1 && cn <= 1)) {
                            // Increment trailing number in text e.g. "Item 1" or "10"
                            static const std::regex numRegex("([0-9]+)$");
                            std::smatch match;
                            if (std::regex_search(ncell.text, match, numRegex)) {
                                long long baseVal = std::stoll(match.str());
                                long long newVal = baseVal + step - 1;
                                ncell.text = ncell.text.substr(0, match.position()) + std::to_string(newVal);
                            }
                        }
                    }

                    setCell(nri, nci, ncell, what);
                    if (cb) cb(nri, nci, ncell);
                }
            }
        }
    }
}

void Rows::cutPaste(const CellRange& srcCellRange, const CellRange& dstCellRange) {
    std::map<int, RowData> next;
    for (auto& rpair : data) {
        int ri = rpair.first;
        for (auto& cpair : rpair.second.cells) {
            int ci = cpair.first;
            int nri = ri;
            int nci = ci;
            if (srcCellRange.includes(ri, ci)) {
                nri = dstCellRange.sri + (ri - srcCellRange.sri);
                nci = dstCellRange.sci + (ci - srcCellRange.sci);
            }
            next[nri].cells[nci] = std::move(cpair.second);
        }
        if (rpair.second.customHeight) {
            next[ri].height = rpair.second.height;
            next[ri].customHeight = true;
        }
        if (rpair.second.hide) next[ri].hide = true;
        if (rpair.second.style >= 0) next[ri].style = rpair.second.style;
    }
    data = std::move(next);
}

std::pair<int, int> Rows::maxCell() const {
    int maxR = 0;
    int maxC = 0;
    for (const auto& rpair : data) {
        if (rpair.first > maxR) maxR = rpair.first;
        for (const auto& cpair : rpair.second.cells) {
            if (cpair.first > maxC) maxC = cpair.first;
        }
    }
    return { maxR, maxC };
}

nlohmann::json Rows::toJson() const {
    nlohmann::json j;
    j["len"] = len;
    for (const auto& rpair : data) {
        nlohmann::json rj;
        if (rpair.second.customHeight) rj["height"] = rpair.second.height;
        if (rpair.second.hide) rj["hide"] = true;
        if (rpair.second.style >= 0) rj["style"] = rpair.second.style;
        if (!rpair.second.cells.empty()) {
            nlohmann::json cj;
            for (const auto& cpair : rpair.second.cells) {
                cj[std::to_string(cpair.first)] = cpair.second.toJson();
            }
            rj["cells"] = cj;
        }
        j[std::to_string(rpair.first)] = rj;
    }
    return j;
}

void Rows::fromJson(const nlohmann::json& j) {
    data.clear();
    if (j.contains("len") && j["len"].is_number()) {
        len = j["len"].get<int>();
    }
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key() == "len") continue;
        int ri = -1;
        try { ri = std::stoi(it.key()); } catch (...) { continue; }
        const auto& rj = it.value();
        RowData& rd = data[ri];
        if (rj.contains("height") && rj["height"].is_number()) {
            rd.height = rj["height"].get<float>();
            rd.customHeight = true;
        }
        if (rj.contains("hide") && rj["hide"].is_boolean()) {
            rd.hide = rj["hide"].get<bool>();
        }
        if (rj.contains("style") && rj["style"].is_number()) {
            rd.style = rj["style"].get<int>();
        }
        if (rj.contains("cells") && rj["cells"].is_object()) {
            for (auto cit = rj["cells"].begin(); cit != rj["cells"].end(); ++cit) {
                int ci = -1;
                try { ci = std::stoi(cit.key()); } catch (...) { continue; }
                rd.cells[ci] = Cell::fromJson(cit.value());
            }
        }
    }
}

float Cols::getWidth(int ci) const {
    if (isHide(ci)) return 0.0f;
    auto it = data.find(ci);
    if (it != data.end() && it->second.customWidth) {
        return it->second.width;
    }
    return default_width;
}

void Cols::setWidth(int ci, float width) {
    data[ci].width = std::max(minWidth, width);
    data[ci].customWidth = true;
}

bool Cols::isHide(int ci) const {
    auto it = data.find(ci);
    return it != data.end() && it->second.hide;
}

void Cols::setHide(int ci, bool v) {
    data[ci].hide = v;
}

void Cols::unhide(int idx) {
    int index = idx;
    while (index > 0) {
        index -= 1;
        if (isHide(index)) {
            setHide(index, false);
        } else {
            break;
        }
    }
}

void Cols::setStyle(int ci, int style) {
    data[ci].style = style;
}

float Cols::sumWidth(int min, int max) const {
    float s = 0.0f;
    for (int i = min; i < max; ++i) {
        s += getWidth(i);
    }
    return s;
}

float Cols::totalWidth() const {
    return sumWidth(0, len);
}

nlohmann::json Cols::toJson() const {
    nlohmann::json j;
    j["len"] = len;
    for (const auto& cpair : data) {
        nlohmann::json cj;
        if (cpair.second.customWidth) cj["width"] = cpair.second.width;
        if (cpair.second.hide) cj["hide"] = true;
        if (cpair.second.style >= 0) cj["style"] = cpair.second.style;
        j[std::to_string(cpair.first)] = cj;
    }
    return j;
}

void Cols::fromJson(const nlohmann::json& j) {
    data.clear();
    if (j.contains("len") && j["len"].is_number()) {
        len = j["len"].get<int>();
    }
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key() == "len") continue;
        int ci = -1;
        try { ci = std::stoi(it.key()); } catch (...) { continue; }
        const auto& cj = it.value();
        ColData& cd = data[ci];
        if (cj.contains("width") && cj["width"].is_number()) {
            cd.width = cj["width"].get<float>();
            cd.customWidth = true;
        }
        if (cj.contains("hide") && cj["hide"].is_boolean()) {
            cd.hide = cj["hide"].get<bool>();
        }
        if (cj.contains("style") && cj["style"].is_number()) {
            cd.style = cj["style"].get<int>();
        }
    }
}

} // namespace xspreadsheet
