#include "cell_range.h"
#include "alphabet.h"
#include <algorithm>

namespace xspreadsheet {

CellRange::CellRange(int r1, int c1, int r2, int c2, float width, float height)
    : sri(r1), sci(c1), eri(r2), eci(c2), w(width), h(height) {}

void CellRange::set(int r1, int c1, int r2, int c2) {
    sri = r1;
    sci = c1;
    eri = r2;
    eci = c2;
}

bool CellRange::multiple() const {
    return (eri - sri > 0) || (eci - sci > 0);
}

bool CellRange::includes(int ri, int ci) const {
    return sri <= ri && ri <= eri && sci <= ci && ci <= eci;
}

bool CellRange::includes(const std::string& ref) const {
    int x = 0, y = 0;
    if (!expr2xy(ref, x, y)) return false;
    return includes(y, x);
}

void CellRange::each(std::function<void(int, int)> cb, std::function<bool(int)> rowFilter) const {
    for (int i = sri; i <= eri; ++i) {
        if (!rowFilter || rowFilter(i)) {
            for (int j = sci; j <= eci; ++j) {
                cb(i, j);
            }
        }
    }
}

bool CellRange::contains(const CellRange& other) const {
    return sri <= other.sri && sci <= other.sci && eri >= other.eri && eci >= other.eci;
}

bool CellRange::within(const CellRange& other) const {
    return sri >= other.sri && sci >= other.sci && eri <= other.eri && eci <= other.eci;
}

bool CellRange::disjoint(const CellRange& other) const {
    return sri > other.eri || sci > other.eci || other.sri > eri || other.sci > eci;
}

bool CellRange::intersects(const CellRange& other) const {
    return sri <= other.eri && sci <= other.eci && other.sri <= eri && other.sci <= eci;
}

CellRange CellRange::unionRange(const CellRange& other) const {
    return CellRange(
        std::min(sri, other.sri),
        std::min(sci, other.sci),
        std::max(eri, other.eri),
        std::max(eci, other.eci)
    );
}

std::vector<CellRange> CellRange::difference(const CellRange& other) const {
    std::vector<CellRange> ret;
    auto addRet = [&](int r1, int c1, int r2, int c2) {
        if (r1 <= r2 && c1 <= c2) {
            ret.emplace_back(r1, c1, r2, c2);
        }
    };

    int dsr = other.sri - sri;
    int dsc = other.sci - sci;
    int der = eri - other.eri;
    int dec = eci - other.eci;

    if (dsr > 0) {
        addRet(sri, sci, other.sri - 1, eci);
        if (der > 0) {
            addRet(other.eri + 1, sci, eri, eci);
            if (dsc > 0) addRet(other.sri, sci, other.eri, other.sci - 1);
            if (dec > 0) addRet(other.sri, other.eci + 1, other.eri, eci);
        } else {
            if (dsc > 0) addRet(other.sri, sci, eri, other.sci - 1);
            if (dec > 0) addRet(other.sri, other.eci + 1, eri, eci);
        }
    } else if (der > 0) {
        addRet(other.eri + 1, sci, eri, eci);
        if (dsc > 0) addRet(sri, sci, other.eri, other.sci - 1);
        if (dec > 0) addRet(sri, other.eci + 1, other.eri, eci);
    }

    if (dsc > 0) {
        addRet(sri, sci, eri, other.sci - 1);
        if (dec > 0) {
            addRet(sri, other.eci + 1, eri, eci);
            if (dsr > 0) addRet(sri, other.sci, other.sri - 1, other.eci);
            if (der > 0) addRet(other.sri + 1, other.sci, eri, other.eci);
        } else {
            if (dsr > 0) addRet(sri, other.sci, other.sri - 1, eci);
            if (der > 0) addRet(other.sri + 1, other.sci, eri, eci);
        }
    } else if (dec > 0) {
        addRet(eri, other.eci + 1, eri, eci);
        if (dsr > 0) addRet(sri, sci, other.sri - 1, other.eci);
        if (der > 0) addRet(other.eri + 1, sci, eri, other.eci);
    }

    return ret;
}

void CellRange::size(int& rows, int& cols) const {
    rows = eri - sri + 1;
    cols = eci - sci + 1;
}

std::string CellRange::toString() const {
    std::string ref = xy2expr(sci, sri);
    if (multiple()) {
        ref += ":" + xy2expr(eci, eri);
    }
    return ref;
}

bool CellRange::operator==(const CellRange& o) const {
    return sri == o.sri && sci == o.sci && eri == o.eri && eci == o.eci;
}

CellRange CellRange::valueOf(const std::string& ref) {
    auto colonPos = ref.find(':');
    if (colonPos == std::string::npos) {
        int x = 0, y = 0;
        expr2xy(ref, x, y);
        return CellRange(y, x, y, x);
    }
    std::string startRef = ref.substr(0, colonPos);
    std::string endRef = ref.substr(colonPos + 1);
    int sx = 0, sy = 0, ex = 0, ey = 0;
    expr2xy(startRef, sx, sy);
    expr2xy(endRef, ex, ey);
    return CellRange(std::min(sy, ey), std::min(sx, ex), std::max(sy, ey), std::max(sx, ex));
}

} // namespace xspreadsheet
