#pragma once
#include <string>
#include <vector>
#include <utility>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

template <typename T = nlohmann::json>
class HistoryT {
public:
    std::vector<T> undoItems;
    std::vector<T> redoItems;
    size_t maxHistory = 50;

    void add(const T& state) {
        undoItems.push_back(state);
        if (undoItems.size() > maxHistory) {
            undoItems.erase(undoItems.begin());
        }
        redoItems.clear();
    }

    bool canUndo() const { return !undoItems.empty(); }
    bool canRedo() const { return !redoItems.empty(); }

    bool undo(const T& current, T& outPrevious) {
        if (!canUndo()) return false;
        redoItems.push_back(current);
        outPrevious = std::move(undoItems.back());
        undoItems.pop_back();
        return true;
    }

    bool redo(const T& current, T& outNext) {
        if (!canRedo()) return false;
        undoItems.push_back(current);
        outNext = std::move(redoItems.back());
        redoItems.pop_back();
        return true;
    }

    void clear() {
        undoItems.clear();
        redoItems.clear();
    }
};

using History = HistoryT<nlohmann::json>;

} // namespace xspreadsheet
