#pragma once
#include <string>
#include <functional>

namespace xspreadsheet {

// 0 -> "A", 25 -> "Z", 26 -> "AA", etc.
std::string stringAt(int index);

// "A" -> 0, "Z" -> 25, "AA" -> 26, etc.
int indexAt(const std::string& str);

// e.g. "B10" -> x = 1 (col), y = 9 (row)
bool expr2xy(const std::string& src, int& x, int& y);

// e.g. x = 1, y = 9 -> "B10"
std::string xy2expr(int x, int y);

// translate cell reference in src by (xn, yn)
std::string expr2expr(const std::string& src, int xn, int yn, std::function<bool(int, int)> condition = nullptr);

} // namespace xspreadsheet
