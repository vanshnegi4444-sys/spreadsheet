#include "dialog_print.h"
#include "../core/alphabet.h"
#include <imgui.h>
#include <fstream>

namespace xspreadsheet {

void DialogPrint::render(SheetData& sheet) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(380, 280), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Print / Export Preview##Dialog", &isOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Sheet: %s", sheet.name.c_str());
        auto [maxR, maxC] = sheet.rows.maxCell();
        ImGui::Text("Active content range: A1:%s%d",
                    stringAt(maxC).c_str(), maxR + 1);

        ImGui::Separator();
        const char* paperSizes[] = { "A3 ( 11.69'' x 16.54'' )", "A4 ( 8.27'' x 11.69'' )", "A5 ( 5.83'' x 8.27'' )", "B4 ( 9.84'' x 13.90'' )", "B5 ( 6.93'' x 9.84'' )" };
        ImGui::Combo("Paper Size", &paperSizeIdx, paperSizes, 5);

        const char* orientations[] = { "Landscape", "Portrait" };
        ImGui::Combo("Orientation", &orientationIdx, orientations, 2);

        ImGui::Separator();
        ImGui::InputText("Export Path", exportPathBuf, sizeof(exportPathBuf));

        if (ImGui::Button("Export to CSV", ImVec2(120, 0))) {
            std::ofstream out(exportPathBuf);
            if (out.is_open()) {
                for (int r = 0; r <= maxR; ++r) {
                    for (int c = 0; c <= maxC; ++c) {
                        out << sheet.getCellText(r, c);
                        if (c < maxC) out << ",";
                    }
                    out << "\n";
                }
            }
            isOpen = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 0))) {
            isOpen = false;
        }
    }
    ImGui::End();
}

} // namespace xspreadsheet
