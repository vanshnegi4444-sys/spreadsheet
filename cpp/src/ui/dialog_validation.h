#pragma once
#include "../core/sheet_data.h"

namespace xspreadsheet {

class DialogValidation {
public:
    bool isOpen = false;
    std::string rangeStr;
    std::string type = "list";
    std::string op = "be";
    char val1Buf[256] = "";
    char val2Buf[256] = "";
    bool required = false;

    void open(SheetData& sheet);
    void render(SheetData& sheet);
};

} // namespace xspreadsheet
