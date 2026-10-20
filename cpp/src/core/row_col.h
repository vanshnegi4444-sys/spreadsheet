#pragma once
#include "cell.h"
#include "cell_range.h"
#include <unordered_map>
#include <map>
#include <set>
#include <functional>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

struct RowData {
    float height = 25.0f;
    bool customHeight = false;
    bool hide = false;
    int style = -1;
    std::unordered_map<int, Cell> cells;
};

class Rows {
public:
    int len = 1000;
    float default_height = 25.0f;
    std::map<int, RowData> data;

    Rows(int l = 1000, float h = 25.0f) : len(l), default_height(h) {}

    float getHeight(int ri) const;
    void setHeight(int ri, float v);

    bool isHide(int ri) const;
    void setHide(int ri, bool v);
    void unhide(int idx);

    void setStyle(int ri, int style);

    float sumHeight(int min, int max, const std::set<int>* exceptSet = nullptr) const;
    float totalHeight() const;

    const Cell* getCell(int ri, int ci) const;
    Cell* getCell(int ri, int ci);
    Cell& getCellOrNew(int ri, int ci);

    std::pair<int, int> getCellMerge(int ri, int ci) const;

    // what: "all", "text", "format", "merge"
    void setCell(int ri, int ci, const Cell& cell, const std::string& what = "all");
    void setCellText(int ri, int ci, const std::string& text);

    void deleteCell(int ri, int ci, const std::string& what = "all");
    void deleteCells(const CellRange& cellRange, const std::string& what = "all");

    void insert(int sri, int n = 1);
    void deleteRows(int sri, int eri);

    void insertColumn(int sci, int n = 1);
    void deleteColumn(int sci, int eci);

    void copyPaste(const CellRange& srcCellRange, const CellRange& dstCellRange,
                   const std::string& what = "all", bool autofill = false,
                   std::function<void(int ri, int ci, const Cell& cell)> cb = nullptr);

    void cutPaste(const CellRange& srcCellRange, const CellRange& dstCellRange);

    std::pair<int, int> maxCell() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

struct ColData {
    float width = 100.0f;
    bool customWidth = false;
    bool hide = false;
    int style = -1;
};

class Cols {
public:
    int len = 1000;
    float default_width = 100.0f;
    float indexWidth = 60.0f;
    float minWidth = 60.0f;
    std::map<int, ColData> data;

    Cols(int l = 1000, float w = 100.0f, float iw = 60.0f, float mw = 60.0f)
        : len(l), default_width(w), indexWidth(iw), minWidth(mw) {}

    float getWidth(int ci) const;
    void setWidth(int ci, float width);

    bool isHide(int ci) const;
    void setHide(int ci, bool v);
    void unhide(int idx);

    void setStyle(int ci, int style);

    float sumWidth(int min, int max) const;
    float totalWidth() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace xspreadsheet
