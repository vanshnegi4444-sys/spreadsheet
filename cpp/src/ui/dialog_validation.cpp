#include "dialog_validation.h"
#include "../locale/locale.h"
#include <imgui.h>
#include <cstring>

namespace xspreadsheet {

void DialogValidation::open(SheetData& sheet) {
    isOpen = true;
    rangeStr = sheet.selector.toString();

    const ValidationRule* rule = sheet.validations.get(sheet.select_ri, sheet.select_ci);
    if (rule) {
        type = rule->validator.type;
        op = rule->validator.op;
        required = rule->validator.required;
        strncpy(val1Buf, rule->validator.value.c_str(), sizeof(val1Buf) - 1);
        val1Buf[sizeof(val1Buf) - 1] = '\0';
        strncpy(val2Buf, rule->validator.value2.c_str(), sizeof(val2Buf) - 1);
        val2Buf[sizeof(val2Buf) - 1] = '\0';
    } else {
        type = "list";
        op = "be";
        required = false;
        val1Buf[0] = '\0';
        val2Buf[0] = '\0';
    }
}

void DialogValidation::render(SheetData& sheet) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Data Validation##Dialog", &isOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Range: %s", rangeStr.c_str());
        ImGui::Separator();

        // Criteria Type
        const char* types[] = { "list", "number", "date", "phone", "email" };
        const char* typeLabels[] = { "List", "Number", "Date", "Phone", "Email" };
        int currentTypeIdx = 0;
        for (int i = 0; i < 5; ++i) {
            if (type == types[i]) { currentTypeIdx = i; break; }
        }

        if (ImGui::Combo("Criteria", &currentTypeIdx, typeLabels, 5)) {
            type = types[currentTypeIdx];
        }

        // Operator
        if (type == "number" || type == "date") {
            const char* ops[] = { "be", "nbe", "eq", "neq", "lt", "lte", "gt", "gte" };
            const char* opLabels[] = { "Between", "Not Between", "Equal", "Not Equal", "Less Than", "Less Than or Equal", "Greater Than", "Greater Than or Equal" };
            int currentOpIdx = 0;
            for (int i = 0; i < 8; ++i) {
                if (op == ops[i]) { currentOpIdx = i; break; }
            }
            if (ImGui::Combo("Operator", &currentOpIdx, opLabels, 8)) {
                op = ops[currentOpIdx];
            }

            if (op == "be" || op == "nbe") {
                ImGui::InputText("Min Value", val1Buf, sizeof(val1Buf));
                ImGui::InputText("Max Value", val2Buf, sizeof(val2Buf));
            } else {
                ImGui::InputText("Value", val1Buf, sizeof(val1Buf));
            }
        } else if (type == "list") {
            ImGui::InputText("Items (comma-separated)", val1Buf, sizeof(val1Buf));
        }

        ImGui::Checkbox("Required", &required);

        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(80, 0))) {
            Validator v;
            v.type = type;
            v.op = op;
            v.required = required;
            v.value = val1Buf;
            v.value2 = val2Buf;
            sheet.validations.add("cell", rangeStr, v);
            isOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove", ImVec2(80, 0))) {
            sheet.validations.remove(CellRange::valueOf(rangeStr));
            isOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            isOpen = false;
        }
    }
    ImGui::End();
}

} // namespace xspreadsheet
