#pragma once
#include <string>
#include <vector>
#include <functional>

namespace xspreadsheet {

struct CellRange {
    int sri = 0; // start row index
    int sci = 0; // start col index
    int eri = 0; // end row index
    int eci = 0; // end col index
    float w = 0.0f;
    float h = 0.0f;

    CellRange() = default;
    CellRange(int r1, int c1, int r2, int c2, float width = 0.0f, float height = 0.0f);

    void set(int r1, int c1, int r2, int c2);
    bool multiple() const;

    bool includes(int ri, int ci) const;
    bool includes(const std::string& ref) const;

    void each(std::function<void(int ri, int ci)> cb, std::function<bool(int ri)> rowFilter = nullptr) const;

    bool contains(const CellRange& other) const;
    bool within(const CellRange& other) const;
    bool disjoint(const CellRange& other) const;
    bool intersects(const CellRange& other) const;

    CellRange unionRange(const CellRange& other) const;
    std::vector<CellRange> difference(const CellRange& other) const;

    void size(int& rows, int& cols) const;
    int rowCount() const { return eri - sri + 1; }
    int colCount() const { return eci - sci + 1; }

    std::string toString() const;
    bool operator==(const CellRange& o) const;
    bool operator!=(const CellRange& o) const { return !(*this == o); }

    static CellRange valueOf(const std::string& ref);
};

} // namespace xspreadsheet
