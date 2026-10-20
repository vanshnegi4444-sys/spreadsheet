#include "sheet_data.h"
#include "alphabet.h"
#include "formula.h"
#include "format.h"
#include <algorithm>
#include <iostream>

namespace xspreadsheet {

SheetData::SheetData(const std::string& n) : name(n), selector(0, 0, 0, 0) {
    // Default style
    styles.push_back(Style());
}

void SheetData::pushHistoryState() {
    history.add(toJson());
}

int SheetData::addStyle(const Style& s) {
    for (size_t i = 0; i < styles.size(); ++i) {
        if (styles[i] == s) return static_cast<int>(i);
    }
    styles.push_back(s);
    return static_cast<int>(styles.size() - 1);
}

Style SheetData::getCellStyle(int ri, int ci) const {
    const Cell* c = rows.getCell(ri, ci);
    if (c && c->style >= 0 && c->style < static_cast<int>(styles.size())) {
        return styles[c->style];
    }
    // Check row-level style
    auto rit = rows.data.find(ri);
    if (rit != rows.data.end() && rit->second.style >= 0 && rit->second.style < static_cast<int>(styles.size())) {
        return styles[rit->second.style];
    }
    // Check column-level style
    auto cit = cols.data.find(ci);
    if (cit != cols.data.end() && cit->second.style >= 0 && cit->second.style < static_cast<int>(styles.size())) {
        return styles[cit->second.style];
    }
    return styles.empty() ? Style() : styles[0];
}

Style SheetData::getSelectedCellStyle() const {
    return getCellStyle(select_ri, select_ci);
}

static void applyStyleProperty(Style& s, const std::string& property, const std::string& value) {
    if (property == "format") {
        s.format = value;
    } else if (property == "font-bold") {
        s.font.bold = (value == "true" || value == "1");
    } else if (property == "font-italic") {
        s.font.italic = (value == "true" || value == "1");
    } else if (property == "font-name") {
        s.font.name = value;
    } else if (property == "font-size") {
        try { s.font.size = std::stof(value); } catch (...) {}
    } else if (property == "strike") {
        s.strike = (value == "true" || value == "1");
    } else if (property == "underline") {
        s.underline = (value == "true" || value == "1");
    } else if (property == "textwrap") {
        s.textwrap = (value == "true" || value == "1");
    } else if (property == "align") {
        s.align = value;
    } else if (property == "valign") {
        s.valign = value;
    } else if (property == "color") {
        s.color = value;
    } else if (property == "bgcolor") {
        s.bgcolor = value;
    }
}

void SheetData::setSelectedCellAttr(const std::string& property, const std::string& value) {
    pushHistoryState();

    if (property == "formula") {
        if (selector.multiple()) {
            int rn = selector.rowCount();
            int cn = selector.colCount();
            if (rn > 1) {
                for (int c = selector.sci; c <= selector.eci; ++c) {
                    Cell& cell = rows.getCellOrNew(selector.eri + 1, c);
                    cell.text = "=" + value + "(" + xy2expr(c, selector.sri) + ":" + xy2expr(c, selector.eri) + ")";
                    cell.isDirty = true;
                }
            } else if (cn > 1) {
                Cell& cell = rows.getCellOrNew(select_ri, selector.eci + 1);
                cell.text = "=" + value + "(" + xy2expr(selector.sci, select_ri) + ":" + xy2expr(selector.eci, select_ri) + ")";
                cell.isDirty = true;
            }
        } else {
            Cell& cell = rows.getCellOrNew(select_ri, select_ci);
            cell.text = "=" + value + "()";
            cell.isDirty = true;
        }
        return;
    }

    int totalCells = selector.rowCount() * selector.colCount();
    bool isWholeSheet = (selector.rowCount() >= rows.len && selector.colCount() >= cols.len);
    bool isWholeColumns = (selector.rowCount() >= rows.len);
    bool isWholeRows = (selector.colCount() >= cols.len);

    if (isWholeSheet) {
        // Fast path for Select All: Update default style and existing populated cells
        Style baseStyle = styles.empty() ? Style() : styles[0];
        applyStyleProperty(baseStyle, property, value);
        int newStyleId = addStyle(baseStyle);
        if (!styles.empty()) styles[0] = baseStyle;

        for (auto& rpair : rows.data) {
            for (auto& cpair : rpair.second.cells) {
                Style s = (cpair.second.style >= 0 && cpair.second.style < static_cast<int>(styles.size()))
                              ? styles[cpair.second.style] : baseStyle;
                applyStyleProperty(s, property, value);
                cpair.second.style = addStyle(s);
            }
        }
        return;
    }

    if (isWholeColumns) {
        // Fast path for Whole Column selection: Set column-level style
        for (int c = selector.sci; c <= selector.eci; ++c) {
            Style s = (cols.data[c].style >= 0 && cols.data[c].style < static_cast<int>(styles.size()))
                          ? styles[cols.data[c].style] : (styles.empty() ? Style() : styles[0]);
            applyStyleProperty(s, property, value);
            cols.setStyle(c, addStyle(s));
        }
        for (auto& rpair : rows.data) {
            for (auto& cpair : rpair.second.cells) {
                if (cpair.first >= selector.sci && cpair.first <= selector.eci) {
                    Style s = getCellStyle(rpair.first, cpair.first);
                    applyStyleProperty(s, property, value);
                    cpair.second.style = addStyle(s);
                }
            }
        }
        return;
    }

    if (isWholeRows) {
        // Fast path for Whole Row selection: Set row-level style
        for (int r = selector.sri; r <= selector.eri; ++r) {
            Style s = (rows.data[r].style >= 0 && rows.data[r].style < static_cast<int>(styles.size()))
                          ? styles[rows.data[r].style] : (styles.empty() ? Style() : styles[0]);
            applyStyleProperty(s, property, value);
            rows.setStyle(r, addStyle(s));
        }
        for (auto& rpair : rows.data) {
            if (rpair.first >= selector.sri && rpair.first <= selector.eri) {
                for (auto& cpair : rpair.second.cells) {
                    Style s = getCellStyle(rpair.first, cpair.first);
                    applyStyleProperty(s, property, value);
                    cpair.second.style = addStyle(s);
                }
            }
        }
        return;
    }

    if (totalCells > 2000) {
        // For large ranges, only populate/update existing cells to avoid memory explosion
        for (auto& rpair : rows.data) {
            if (rpair.first >= selector.sri && rpair.first <= selector.eri) {
                for (auto& cpair : rpair.second.cells) {
                    if (cpair.first >= selector.sci && cpair.first <= selector.eci) {
                        Style s = getCellStyle(rpair.first, cpair.first);
                        applyStyleProperty(s, property, value);
                        cpair.second.style = addStyle(s);
                    }
                }
            }
        }
        return;
    }

    // Normal selection path
    selector.each([&](int ri, int ci) {
        Cell& cell = rows.getCellOrNew(ri, ci);
        Style s = getCellStyle(ri, ci);
        applyStyleProperty(s, property, value);
        cell.style = addStyle(s);
    });
}

void SheetData::setSelectedCellBorder(const BorderStyle& borderStyle, const std::string& mode) {
    pushHistoryState();

    if (mode == "none") {
        selector.each([&](int ri, int ci) {
            Cell* c = rows.getCell(ri, ci);
            if (c && c->style >= 0 && c->style < static_cast<int>(styles.size())) {
                Style s = styles[c->style];
                s.border = CellBorder();
                c->style = addStyle(s);
            }
        });
        return;
    }

    selector.each([&](int ri, int ci) {
        Cell& cell = rows.getCellOrNew(ri, ci);
        Style s = getCellStyle(ri, ci);

        bool isTop = (ri == selector.sri);
        bool isBottom = (ri == selector.eri);
        bool isLeft = (ci == selector.sci);
        bool isRight = (ci == selector.eci);

        if (mode == "all") {
            s.border.top = borderStyle;
            s.border.bottom = borderStyle;
            s.border.left = borderStyle;
            s.border.right = borderStyle;
        } else if (mode == "outside") {
            if (isTop) s.border.top = borderStyle;
            if (isBottom) s.border.bottom = borderStyle;
            if (isLeft) s.border.left = borderStyle;
            if (isRight) s.border.right = borderStyle;
        } else if (mode == "inside") {
            if (!isBottom) s.border.bottom = borderStyle;
            if (!isRight) s.border.right = borderStyle;
        } else if (mode == "horizontal") {
            if (!isBottom) s.border.bottom = borderStyle;
        } else if (mode == "vertical") {
            if (!isRight) s.border.right = borderStyle;
        } else if (mode == "top") {
            if (isTop) s.border.top = borderStyle;
        } else if (mode == "bottom") {
            if (isBottom) s.border.bottom = borderStyle;
        } else if (mode == "left") {
            if (isLeft) s.border.left = borderStyle;
        } else if (mode == "right") {
            if (isRight) s.border.right = borderStyle;
        }

        cell.style = addStyle(s);
    });
}

void SheetData::setCellText(int ri, int ci, const std::string& text, bool recordHistory) {
    if (recordHistory) {
        pushHistoryState();
    }
    rows.setCellText(ri, ci, text);
    validations.validate(ri, ci, text);
    for (auto& rpair : rows.data) {
        for (auto& cpair : rpair.second.cells) {
            if (!cpair.second.text.empty() && cpair.second.text[0] == '=') {
                cpair.second.isDirty = true;
            }
        }
    }
}

std::string SheetData::getCellText(int ri, int ci) const {
    const Cell* c = rows.getCell(ri, ci);
    return c ? c->text : "";
}

std::string SheetData::getCellDisplayValue(int ri, int ci) {
    Cell* c = rows.getCell(ri, ci);
    if (!c || c->text.empty()) return "";

    std::string text = c->text;
    std::string evalVal = text;

    if (text[0] == '=') {
        if (!c->isDirty && !c->evaluatedValue.empty()) {
            evalVal = c->evaluatedValue;
        } else {
            evalVal = FormulaEngine::eval(text, [this](int x, int y) {
                const Cell* refC = this->rows.getCell(y, x);
                if (refC && !refC->text.empty() && refC->text[0] == '=') {
                    return this->getCellDisplayValue(y, x);
                }
                return this->getCellText(y, x);
            });
            c->evaluatedValue = evalVal;
            c->isDirty = false;
        }
    }

    Style s = getCellStyle(ri, ci);
    return Formatter::format(s.format, evalVal);
}

bool SheetData::isSingleSelected() const {
    return !selector.multiple();
}

bool SheetData::canMerge() const {
    return selector.multiple();
}

bool SheetData::canUnmerge() const {
    const Cell* c = rows.getCell(selector.sri, selector.sci);
    return c && (c->merge.first > 0 || c->merge.second > 0);
}

void SheetData::merge() {
    if (!canMerge()) return;
    pushHistoryState();

    int rn = selector.rowCount();
    int cn = selector.colCount();
    int sri = selector.sri;
    int sci = selector.sci;

    Cell& masterCell = rows.getCellOrNew(sri, sci);
    masterCell.merge = { rn - 1, cn - 1 };

    merges.add(selector);

    // Clear subordinate cells
    for (int r = sri; r <= selector.eri; ++r) {
        for (int c = sci; c <= selector.eci; ++c) {
            if (r == sri && c == sci) continue;
            rows.deleteCell(r, c, "all");
        }
    }
}

void SheetData::unmerge() {
    if (!canUnmerge()) return;
    pushHistoryState();

    int sri = selector.sri;
    int sci = selector.sci;
    Cell* c = rows.getCell(sri, sci);
    if (c) {
        c->merge = {0, 0};
    }
    merges.deleteWithin(selector);
}

void SheetData::copy() {
    clipboardRange = selector;
    clipboardIsCut = false;
    hasClipboard = true;
}

void SheetData::cut() {
    clipboardRange = selector;
    clipboardIsCut = true;
    hasClipboard = true;
}

bool SheetData::paste(const std::string& what) {
    if (!hasClipboard) return false;
    pushHistoryState();

    if (clipboardIsCut) {
        rows.cutPaste(clipboardRange, selector);
        merges.move(clipboardRange, selector.sri - clipboardRange.sri, selector.sci - clipboardRange.sci);
        clearClipboard();
    } else {
        rows.copyPaste(clipboardRange, selector, what, false, [&](int ri, int ci, const Cell& cell) {
            if (cell.merge.first > 0 || cell.merge.second > 0) {
                merges.add(CellRange(ri, ci, ri + cell.merge.first, ci + cell.merge.second));
            }
        });
    }
    return true;
}

bool SheetData::autofill(const CellRange& targetRange) {
    pushHistoryState();
    rows.copyPaste(selector, targetRange, "all", true, [&](int ri, int ci, const Cell& cell) {
        if (cell.merge.first > 0 || cell.merge.second > 0) {
            merges.add(CellRange(ri, ci, ri + cell.merge.first, ci + cell.merge.second));
        }
    });
    selector = targetRange;
    return true;
}

void SheetData::clearClipboard() {
    hasClipboard = false;
    clipboardIsCut = false;
}

void SheetData::insert(const std::string& type, int n) {
    pushHistoryState();
    if (type == "row") {
        rows.insert(selector.sri, n);
        merges.shift("row", selector.sri, n, [this](int ri, int ci, int rn, int cn) {
            Cell* c = rows.getCell(ri, ci);
            if (c) {
                c->merge.first += rn;
                c->merge.second += cn;
            }
        });
    } else if (type == "column") {
        rows.insertColumn(selector.sci, n);
        cols.len += n;
        merges.shift("column", selector.sci, n, [this](int ri, int ci, int rn, int cn) {
            Cell* c = rows.getCell(ri, ci);
            if (c) {
                c->merge.first += rn;
                c->merge.second += cn;
            }
        });
    }
}

void SheetData::deleteSelected(const std::string& type) {
    pushHistoryState();
    if (type == "row") {
        rows.deleteRows(selector.sri, selector.eri);
        int size = selector.rowCount();
        merges.shift("row", selector.sri, -size, [this](int ri, int ci, int rn, int cn) {
            Cell* c = rows.getCell(ri, ci);
            if (c) {
                c->merge.first += rn;
                c->merge.second += cn;
                if (c->merge.first <= 0 && c->merge.second <= 0) c->merge = {0, 0};
            }
        });
    } else if (type == "column") {
        rows.deleteColumn(selector.sci, selector.eci);
        int size = selector.colCount();
        cols.len = std::max(1, cols.len - size);
        merges.shift("column", selector.sci, -size, [this](int ri, int ci, int rn, int cn) {
            Cell* c = rows.getCell(ri, ci);
            if (c) {
                c->merge.first += rn;
                c->merge.second += cn;
                if (c->merge.first <= 0 && c->merge.second <= 0) c->merge = {0, 0};
            }
        });
    }
}

void SheetData::deleteCellText() {
    pushHistoryState();
    rows.deleteCells(selector, "text");
}

void SheetData::setFreeze(int ri, int ci) {
    pushHistoryState();
    freeze = { ri, ci };
}

float SheetData::freezeTotalWidth() const {
    return cols.sumWidth(0, freeze.second);
}

float SheetData::freezeTotalHeight() const {
    return rows.sumHeight(0, freeze.first);
}

void SheetData::autofilterToggle() {
    pushHistoryState();
    if (autoFilter.active()) {
        autoFilter.clear();
        exceptRowSet.clear();
        sortedRowMap.clear();
        unsortedRowMap.clear();
    } else {
        autoFilter.ref = selector.toString();
    }
}

void SheetData::resetAutoFilter() {
    if (!autoFilter.active()) return;
    std::vector<int> fset;
    autoFilter.filteredRows([this](int r, int c) {
        return this->getCellText(r, c);
    }, exceptRowSet, fset);

    if (autoFilter.sort) {
        int sortCi = autoFilter.sort->ci;
        bool asc = autoFilter.sort->asc();
        std::vector<int> sortedF = fset;
        std::sort(sortedF.begin(), sortedF.end(), [this, sortCi, asc](int a, int b) {
            std::string va = this->getCellText(a, sortCi);
            std::string vb = this->getCellText(b, sortCi);
            double na = 0, nb = 0;
            char* ea = nullptr; char* eb = nullptr;
            na = std::strtod(va.c_str(), &ea);
            nb = std::strtod(vb.c_str(), &eb);
            bool isNumA = (ea != va.c_str() && *ea == '\0');
            bool isNumB = (eb != vb.c_str() && *eb == '\0');
            if (isNumA && isNumB) {
                return asc ? (na < nb) : (na > nb);
            }
            return asc ? (va < vb) : (va > vb);
        });

        sortedRowMap.clear();
        unsortedRowMap.clear();
        for (size_t idx = 0; idx < fset.size(); ++idx) {
            sortedRowMap[fset[idx]] = sortedF[idx];
            unsortedRowMap[sortedF[idx]] = fset[idx];
        }
    }
}

int SheetData::mapVisualToLogicalRow(int visualRi) const {
    auto it = sortedRowMap.find(visualRi);
    if (it != sortedRowMap.end()) return it->second;
    return visualRi;
}

int SheetData::mapLogicalToVisualRow(int logicalRi) const {
    auto it = unsortedRowMap.find(logicalRi);
    if (it != unsortedRowMap.end()) return it->second;
    return logicalRi;
}

void SheetData::undo() {
    nlohmann::json current = toJson();
    nlohmann::json prev;
    if (history.undo(current, prev)) {
        fromJson(prev);
    }
}

void SheetData::redo() {
    nlohmann::json current = toJson();
    nlohmann::json next;
    if (history.redo(current, next)) {
        fromJson(next);
    }
}

nlohmann::json SheetData::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    if (freeze.first > 0 || freeze.second > 0) {
        j["freeze"] = xy2expr(freeze.second, freeze.first);
    }

    nlohmann::json sAry = nlohmann::json::array();
    for (const auto& s : styles) {
        sAry.push_back(s.toJson());
    }
    j["styles"] = sAry;

    j["merges"] = merges.toJson();
    j["rows"] = rows.toJson();
    j["cols"] = cols.toJson();
    j["validations"] = validations.toJson();
    if (autoFilter.active()) {
        j["autofilter"] = autoFilter.toJson();
    }
    return j;
}

void SheetData::fromJson(const nlohmann::json& j) {
    if (j.contains("name") && j["name"].is_string()) {
        name = j["name"].get<std::string>();
    }
    if (j.contains("freeze") && j["freeze"].is_string()) {
        int fx = 0, fy = 0;
        expr2xy(j["freeze"].get<std::string>(), fx, fy);
        freeze = { fy, fx };
    } else {
        freeze = { 0, 0 };
    }

    styles.clear();
    if (j.contains("styles") && j["styles"].is_array()) {
        for (const auto& sj : j["styles"]) {
            styles.push_back(Style::fromJson(sj));
        }
    }
    if (styles.empty()) styles.push_back(Style());

    if (j.contains("merges")) merges = Merges::fromJson(j["merges"]);
    if (j.contains("rows")) rows.fromJson(j["rows"]);
    if (j.contains("cols")) cols.fromJson(j["cols"]);
    if (j.contains("validations")) validations.fromJson(j["validations"]);
    if (j.contains("autofilter")) autoFilter.fromJson(j["autofilter"]);
}

} // namespace xspreadsheet
