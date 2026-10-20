#include "dialog_sort_filter.h"
#include "../locale/locale.h"
#include <imgui.h>
#include <algorithm>

namespace xspreadsheet {

void DialogSortFilter::open(SheetData& sheet, int ci) {
    isOpen = true;
    targetCi = ci;
    searchBuf[0] = '\0';
    valueCounts = sheet.autoFilter.items(ci, [&](int r, int c) {
        return sheet.getCellText(r, c);
    });

    checkedValues.clear();
    const Filter* existing = sheet.autoFilter.getFilter(ci);
    for (const auto& pair : valueCounts) {
        if (existing) {
            checkedValues[pair.first] = existing->includes(pair.first);
        } else {
            checkedValues[pair.first] = true;
        }
    }
}

void DialogSortFilter::render(SheetData& sheet) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(280, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Sort & Filter##Dialog", &isOpen, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button("Sort A -> Z", ImVec2(-1, 0))) {
            sheet.autoFilter.setSort(targetCi, "asc");
            sheet.resetAutoFilter();
            isOpen = false;
        }
        if (ImGui::Button("Sort Z -> A", ImVec2(-1, 0))) {
            sheet.autoFilter.setSort(targetCi, "desc");
            sheet.resetAutoFilter();
            isOpen = false;
        }

        ImGui::Separator();
        ImGui::InputTextWithHint("##FilterSearch", "Search values...", searchBuf, sizeof(searchBuf));

        // Select All / Clear All
        bool allChecked = true;
        for (const auto& pair : checkedValues) {
            if (!pair.second) { allChecked = false; break; }
        }
        if (ImGui::Checkbox("(Select All)", &allChecked)) {
            for (auto& pair : checkedValues) {
                pair.second = allChecked;
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("ValuesList", ImVec2(0, 150), true);
        std::string searchFilter = searchBuf;
        std::transform(searchFilter.begin(), searchFilter.end(), searchFilter.begin(), ::tolower);

        for (auto& pair : valueCounts) {
            std::string label = pair.first.empty() ? "(Blanks)" : pair.first;
            std::string lowerLabel = label;
            std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);
            if (!searchFilter.empty() && lowerLabel.find(searchFilter) == std::string::npos) {
                continue;
            }

            bool checked = checkedValues[pair.first];
            std::string fullLabel = label + " (" + std::to_string(pair.second) + ")";
            if (ImGui::Checkbox(fullLabel.c_str(), &checked)) {
                checkedValues[pair.first] = checked;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(70, 0))) {
            std::vector<std::string> accepted;
            for (const auto& pair : checkedValues) {
                if (pair.second) accepted.push_back(pair.first);
            }
            sheet.autoFilter.addFilter(targetCi, "in", accepted);
            sheet.resetAutoFilter();
            isOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(70, 0))) {
            isOpen = false;
        }
    }
    ImGui::End();
}

} // namespace xspreadsheet
