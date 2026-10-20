#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

class History {
public:
    std::vector<nlohmann::json> undoItems;
    std::vector<nlohmann::json> redoItems;
    size_t maxHistory = 100;

    void add(const nlohmann::json& state) {
        undoItems.push_back(state);
        if (undoItems.size() > maxHistory) {
            undoItems.erase(undoItems.begin());
        }
        redoItems.clear();
    }

    bool canUndo() const { return !undoItems.empty(); }
    bool canRedo() const { return !redoItems.empty(); }

    bool undo(const nlohmann::json& current, nlohmann::json& outPrevious) {
        if (!canUndo()) return false;
        redoItems.push_back(current);
        outPrevious = undoItems.back();
        undoItems.pop_back();
        return true;
    }

    bool redo(const nlohmann::json& current, nlohmann::json& outNext) {
        if (!canRedo()) return false;
        undoItems.push_back(current);
        outNext = redoItems.back();
        redoItems.pop_back();
        return true;
    }

    void clear() {
        undoItems.clear();
        redoItems.clear();
    }
};

} // namespace xspreadsheet
