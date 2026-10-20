#include "alphabet.h"
#include <cctype>
#include <regex>

namespace xspreadsheet {

std::string stringAt(int index) {
    if (index < 0) return "";
    static const char alphabets[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string str = "";
    int cindex = index;
    while (cindex >= 26) {
        cindex /= 26;
        cindex -= 1;
        str += alphabets[cindex % 26];
    }
    str += alphabets[index % 26];
    return str;
}

int indexAt(const std::string& str) {
    int ret = 0;
    for (char c : str) {
        if (c >= 'A' && c <= 'Z') {
            ret = 26 * ret + (c - 'A' + 1);
        } else if (c >= 'a' && c <= 'z') {
            ret = 26 * ret + (c - 'a' + 1);
        }
    }
    return ret - 1;
}

bool expr2xy(const std::string& src, int& x, int& y) {
    std::string colStr = "";
    std::string rowStr = "";
    for (char c : src) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            rowStr += c;
        } else if (std::isalpha(static_cast<unsigned char>(c))) {
            colStr += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    if (colStr.empty() || rowStr.empty()) {
        x = -1;
        y = -1;
        return false;
    }
    x = indexAt(colStr);
    try {
        y = std::stoi(rowStr) - 1;
    } catch (...) {
        y = -1;
        return false;
    }
    return true;
}

std::string xy2expr(int x, int y) {
    if (x < 0 || y < 0) return "";
    return stringAt(x) + std::to_string(y + 1);
}

std::string expr2expr(const std::string& src, int xn, int yn, std::function<bool(int, int)> condition) {
    if (xn == 0 && yn == 0) return src;
    int x = 0, y = 0;
    if (!expr2xy(src, x, y)) return src;
    if (condition && !condition(x, y)) return src;
    return xy2expr(x + xn, y + yn);
}

} // namespace xspreadsheet
