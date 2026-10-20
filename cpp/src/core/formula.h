#pragma once
#include <string>
#include <vector>
#include <functional>
#include <set>

namespace xspreadsheet {

struct FormulaInfo {
    std::string key;
    std::string titleKey;
};

class FormulaEngine {
public:
    using CellLookupFn = std::function<std::string(int x, int y)>;

    static std::string eval(const std::string& formula,
                            CellLookupFn lookup,
                            std::set<std::pair<int, int>>* visited = nullptr);

    static const std::vector<FormulaInfo>& getBaseFormulas();

private:
    struct Token {
        enum Type {
            Number,
            String,
            CellRef,
            Operator,
            Function,
            Range,
            Comma,
            LParen,
            RParen
        } type;
        std::string text;
        int argCount = 0; // for function calls
    };

    static std::vector<Token> tokenize(const std::string& expr);
    static std::vector<Token> toRPN(const std::vector<Token>& tokens);
    static std::string evaluateRPN(const std::vector<Token>& rpn,
                                   CellLookupFn lookup,
                                   std::set<std::pair<int, int>>& visited);
};

} // namespace xspreadsheet
