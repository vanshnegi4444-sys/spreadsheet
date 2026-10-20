#include "formula.h"
#include "alphabet.h"
#include "cell_range.h"
#include <cctype>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <stack>
#include <iostream>

namespace xspreadsheet {

static bool isNumeric(const std::string& str, double& outVal) {
    if (str.empty()) return false;
    char* end = nullptr;
    outVal = std::strtod(str.c_str(), &end);
    return end != str.c_str() && *end == '\0';
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<FormulaEngine::Token> FormulaEngine::tokenize(const std::string& expr) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = expr.length();

    while (i < n) {
        char c = expr[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (c == '"') {
            // String literal
            std::string str;
            ++i;
            while (i < n && expr[i] != '"') {
                str += expr[i++];
            }
            if (i < n && expr[i] == '"') ++i;
            tokens.push_back({ Token::String, str });
            continue;
        }

        if (c == '(') {
            tokens.push_back({ Token::LParen, "(" });
            ++i;
            continue;
        }
        if (c == ')') {
            tokens.push_back({ Token::RParen, ")" });
            ++i;
            continue;
        }
        if (c == ',') {
            tokens.push_back({ Token::Comma, "," });
            ++i;
            continue;
        }

        // Comparison operators: <=, >=, <>, <, >, =
        if (c == '<' || c == '>' || c == '=') {
            std::string op(1, c);
            if (i + 1 < n) {
                char nextC = expr[i + 1];
                if ((c == '<' && (nextC == '=' || nextC == '>')) ||
                    (c == '>' && nextC == '=') ||
                    (c == '=' && nextC == '=')) {
                    op += nextC;
                    ++i;
                }
            }
            tokens.push_back({ Token::Operator, op });
            ++i;
            continue;
        }

        // Arithmetic operators: +, -, *, /
        if (c == '+' || c == '*' || c == '/') {
            tokens.push_back({ Token::Operator, std::string(1, c) });
            ++i;
            continue;
        }

        // Minus sign could be unary or binary
        if (c == '-') {
            bool isUnary = tokens.empty() ||
                           tokens.back().type == Token::LParen ||
                           tokens.back().type == Token::Comma ||
                           tokens.back().type == Token::Operator;
            if (isUnary) {
                // Check if directly preceding a number
                if (i + 1 < n && (std::isdigit(static_cast<unsigned char>(expr[i + 1])) || expr[i + 1] == '.')) {
                    std::string num = "-";
                    ++i;
                    while (i < n && (std::isdigit(static_cast<unsigned char>(expr[i])) || expr[i] == '.')) {
                        num += expr[i++];
                    }
                    tokens.push_back({ Token::Number, num });
                    continue;
                }
                tokens.push_back({ Token::Operator, "u-" });
                ++i;
                continue;
            } else {
                tokens.push_back({ Token::Operator, "-" });
                ++i;
                continue;
            }
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(expr[i + 1])))) {
            std::string num;
            while (i < n && (std::isdigit(static_cast<unsigned char>(expr[i])) || expr[i] == '.')) {
                num += expr[i++];
            }
            tokens.push_back({ Token::Number, num });
            continue;
        }

        // Words: Identifier (Function or CellRef or Range)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '$') {
            std::string word;
            while (i < n && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_' || expr[i] == '$')) {
                word += expr[i++];
            }
            // Check if followed by colon ':' for range e.g. A1:B10
            if (i < n && expr[i] == ':') {
                std::string rangeStr = word + ":";
                ++i;
                while (i < n && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '$')) {
                    rangeStr += expr[i++];
                }
                tokens.push_back({ Token::Range, rangeStr });
                continue;
            }

            // If followed by '(', it's a function call
            size_t peek = i;
            while (peek < n && std::isspace(static_cast<unsigned char>(expr[peek]))) ++peek;
            if (peek < n && expr[peek] == '(') {
                std::string upperWord = word;
                std::transform(upperWord.begin(), upperWord.end(), upperWord.begin(), ::toupper);
                tokens.push_back({ Token::Function, upperWord });
            } else {
                // Check if valid cell reference
                int cx = 0, cy = 0;
                if (expr2xy(word, cx, cy)) {
                    std::string upperWord = word;
                    std::transform(upperWord.begin(), upperWord.end(), upperWord.begin(), ::toupper);
                    tokens.push_back({ Token::CellRef, upperWord });
                } else {
                    // Constant or keyword
                    tokens.push_back({ Token::String, word });
                }
            }
            continue;
        }

        // Unknown character
        tokens.push_back({ Token::Operator, std::string(1, c) });
        ++i;
    }

    return tokens;
}

static int getPrecedence(const std::string& op) {
    if (op == "u-") return 5;
    if (op == "*" || op == "/") return 4;
    if (op == "+" || op == "-") return 3;
    if (op == "=" || op == "<" || op == ">" || op == "<=" || op == ">=" || op == "<>") return 2;
    return 0;
}

std::vector<FormulaEngine::Token> FormulaEngine::toRPN(const std::vector<Token>& tokens) {
    std::vector<Token> output;
    std::stack<Token> opStack;
    std::stack<int> argCountStack;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];

        if (t.type == Token::Number || t.type == Token::String || t.type == Token::CellRef) {
            output.push_back(t);
        } else if (t.type == Token::Range) {
            // Expand range into individual cell refs!
            auto colonPos = t.text.find(':');
            if (colonPos != std::string::npos) {
                std::string s1 = t.text.substr(0, colonPos);
                std::string s2 = t.text.substr(colonPos + 1);
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (expr2xy(s1, x1, y1) && expr2xy(s2, x2, y2)) {
                    int minX = std::min(x1, x2), maxX = std::max(x1, x2);
                    int minY = std::min(y1, y2), maxY = std::max(y1, y2);
                    for (int y = minY; y <= maxY; ++y) {
                        for (int x = minX; x <= maxX; ++x) {
                            output.push_back({ Token::CellRef, xy2expr(x, y) });
                            if (!argCountStack.empty()) {
                                argCountStack.top()++;
                            }
                        }
                    }
                    if (!argCountStack.empty()) {
                        // Subtract 1 since the comma or function entry will account for 1
                        argCountStack.top()--;
                    }
                    continue;
                }
            }
            output.push_back(t);
        } else if (t.type == Token::Function) {
            opStack.push(t);
            argCountStack.push(0);
        } else if (t.type == Token::Comma) {
            while (!opStack.empty() && opStack.top().type != Token::LParen) {
                output.push_back(opStack.top());
                opStack.pop();
            }
            if (!argCountStack.empty()) {
                argCountStack.top()++;
            }
        } else if (t.type == Token::Operator) {
            int prec = getPrecedence(t.text);
            while (!opStack.empty() && opStack.top().type == Token::Operator &&
                   getPrecedence(opStack.top().text) >= prec) {
                output.push_back(opStack.top());
                opStack.pop();
            }
            opStack.push(t);
        } else if (t.type == Token::LParen) {
            opStack.push(t);
        } else if (t.type == Token::RParen) {
            while (!opStack.empty() && opStack.top().type != Token::LParen) {
                output.push_back(opStack.top());
                opStack.pop();
            }
            if (!opStack.empty() && opStack.top().type == Token::LParen) {
                opStack.pop();
            }
            if (!opStack.empty() && opStack.top().type == Token::Function) {
                Token fnToken = opStack.top();
                opStack.pop();
                int args = 0;
                if (!argCountStack.empty()) {
                    args = argCountStack.top();
                    argCountStack.pop();
                    // If there was at least one item before RParen (and not empty parens like NOW())
                    if (i > 0 && tokens[i - 1].type != Token::LParen) {
                        args += 1;
                    }
                }
                fnToken.argCount = args;
                output.push_back(fnToken);
            }
        }
    }

    while (!opStack.empty()) {
        output.push_back(opStack.top());
        opStack.pop();
    }

    return output;
}

static std::string formatDouble(double res) {
    if (std::isnan(res)) return "#NUM!";
    if (std::isinf(res)) return "#NUM!";
    char buf[64];
    if (std::fmod(res, 1.0) == 0.0 && std::abs(res) < 1e15) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(res));
    } else {
        std::snprintf(buf, sizeof(buf), "%.12g", res);
    }
    return std::string(buf);
}

std::string FormulaEngine::evaluateRPN(const std::vector<Token>& rpn,
                                       CellLookupFn lookup,
                                       std::set<std::pair<int, int>>& visited) {
    std::vector<std::string> stack;

    auto resolveVal = [&](const std::string& item) -> std::string {
        int x = 0, y = 0;
        if (expr2xy(item, x, y)) {
            if (visited.count({x, y}) || visited.size() > 64) return "#REF!";
            visited.insert({x, y});
            std::string cellRaw = lookup(x, y);
            std::string evaluated = eval(cellRaw, lookup, &visited);
            visited.erase({x, y});
            return evaluated;
        }
        return item;
    };

    for (const auto& token : rpn) {
        if (token.type == Token::Number || token.type == Token::String) {
            stack.push_back(token.text);
        } else if (token.type == Token::CellRef) {
            stack.push_back(resolveVal(token.text));
        } else if (token.type == Token::Operator) {
            if (token.text == "u-") {
                if (!stack.empty()) {
                    double v = 0;
                    isNumeric(stack.back(), v);
                    stack.pop_back();
                    stack.push_back(formatDouble(-v));
                }
            } else {
                if (stack.size() < 2) continue;
                std::string rightStr = stack.back(); stack.pop_back();
                std::string leftStr = stack.back(); stack.pop_back();

                double leftNum = 0, rightNum = 0;
                bool leftIsNum = isNumeric(leftStr, leftNum);
                bool rightIsNum = isNumeric(rightStr, rightNum);

                if (token.text == "+") {
                    if (leftIsNum && rightIsNum) {
                        stack.push_back(formatDouble(leftNum + rightNum));
                    } else {
                        stack.push_back(leftStr + rightStr);
                    }
                } else if (token.text == "-") {
                    if (leftIsNum && rightIsNum) {
                        stack.push_back(formatDouble(leftNum - rightNum));
                    } else {
                        stack.push_back("#VALUE!");
                    }
                } else if (token.text == "*") {
                    if (leftIsNum && rightIsNum) {
                        stack.push_back(formatDouble(leftNum * rightNum));
                    } else {
                        stack.push_back("#VALUE!");
                    }
                } else if (token.text == "/") {
                    if (rightNum == 0) {
                        stack.push_back("#DIV/0!");
                    } else if (leftIsNum && rightIsNum) {
                        stack.push_back(formatDouble(leftNum / rightNum));
                    } else {
                        stack.push_back("#VALUE!");
                    }
                } else if (token.text == "=" || token.text == "==") {
                    bool eq = (leftIsNum && rightIsNum) ? (leftNum == rightNum) : (leftStr == rightStr);
                    stack.push_back(eq ? "TRUE" : "FALSE");
                } else if (token.text == ">") {
                    bool res = (leftIsNum && rightIsNum) ? (leftNum > rightNum) : (leftStr > rightStr);
                    stack.push_back(res ? "TRUE" : "FALSE");
                } else if (token.text == ">=") {
                    bool res = (leftIsNum && rightIsNum) ? (leftNum >= rightNum) : (leftStr >= rightStr);
                    stack.push_back(res ? "TRUE" : "FALSE");
                } else if (token.text == "<") {
                    bool res = (leftIsNum && rightIsNum) ? (leftNum < rightNum) : (leftStr < rightStr);
                    stack.push_back(res ? "TRUE" : "FALSE");
                } else if (token.text == "<=") {
                    bool res = (leftIsNum && rightIsNum) ? (leftNum <= rightNum) : (leftStr <= rightStr);
                    stack.push_back(res ? "TRUE" : "FALSE");
                } else if (token.text == "<>") {
                    bool eq = (leftIsNum && rightIsNum) ? (leftNum == rightNum) : (leftStr == rightStr);
                    stack.push_back(!eq ? "TRUE" : "FALSE");
                }
            }
        } else if (token.type == Token::Function) {
            int argCount = token.argCount;
            std::vector<std::string> args;
            for (int a = 0; a < argCount && !stack.empty(); ++a) {
                args.push_back(stack.back());
                stack.pop_back();
            }
            std::reverse(args.begin(), args.end());

            const std::string& fn = token.text;

            if (fn == "SUM") {
                double sum = 0;
                for (const auto& a : args) {
                    double v = 0;
                    if (isNumeric(a, v)) sum += v;
                }
                stack.push_back(formatDouble(sum));
            } else if (fn == "AVERAGE") {
                double sum = 0;
                int count = 0;
                for (const auto& a : args) {
                    double v = 0;
                    if (isNumeric(a, v)) {
                        sum += v;
                        ++count;
                    }
                }
                if (count == 0) {
                    stack.push_back("0");
                } else {
                    stack.push_back(formatDouble(sum / count));
                }
            } else if (fn == "MAX") {
                double maxVal = -1e30;
                bool found = false;
                for (const auto& a : args) {
                    double v = 0;
                    if (isNumeric(a, v)) {
                        if (!found || v > maxVal) { maxVal = v; found = true; }
                    }
                }
                stack.push_back(found ? formatDouble(maxVal) : "0");
            } else if (fn == "MIN") {
                double minVal = 1e30;
                bool found = false;
                for (const auto& a : args) {
                    double v = 0;
                    if (isNumeric(a, v)) {
                        if (!found || v < minVal) { minVal = v; found = true; }
                    }
                }
                stack.push_back(found ? formatDouble(minVal) : "0");
            } else if (fn == "IF") {
                if (args.size() >= 2) {
                    bool cond = (args[0] == "TRUE" || args[0] == "1");
                    if (args.size() == 2) {
                        stack.push_back(cond ? args[1] : "FALSE");
                    } else {
                        stack.push_back(cond ? args[1] : args[2]);
                    }
                } else {
                    stack.push_back("");
                }
            } else if (fn == "AND") {
                bool allTrue = true;
                for (const auto& a : args) {
                    if (a != "TRUE" && a != "1") { allTrue = false; break; }
                }
                stack.push_back(allTrue ? "TRUE" : "FALSE");
            } else if (fn == "OR") {
                bool anyTrue = false;
                for (const auto& a : args) {
                    if (a == "TRUE" || a == "1") { anyTrue = true; break; }
                }
                stack.push_back(anyTrue ? "TRUE" : "FALSE");
            } else if (fn == "CONCAT") {
                std::string res;
                for (const auto& a : args) res += a;
                stack.push_back(res);
            } else {
                stack.push_back("#NAME?");
            }
        }
    }

    return stack.empty() ? "" : stack.back();
}

std::string FormulaEngine::eval(const std::string& formula,
                                CellLookupFn lookup,
                                std::set<std::pair<int, int>>* visitedPtr) {
    if (formula.empty()) return "";
    if (formula[0] != '=') return formula;

    std::set<std::pair<int, int>> localVisited;
    std::set<std::pair<int, int>>& visited = visitedPtr ? *visitedPtr : localVisited;

    std::string expr = trim(formula.substr(1));
    if (expr.empty()) return "";

    auto tokens = tokenize(expr);
    if (tokens.empty()) return formula;

    auto rpn = toRPN(tokens);
    return evaluateRPN(rpn, lookup, visited);
}

const std::vector<FormulaInfo>& FormulaEngine::getBaseFormulas() {
    static const std::vector<FormulaInfo> list = {
        { "SUM", "formula.sum" },
        { "AVERAGE", "formula.average" },
        { "MAX", "formula.max" },
        { "MIN", "formula.min" },
        { "IF", "formula._if" },
        { "AND", "formula.and" },
        { "OR", "formula.or" },
        { "CONCAT", "formula.concat" }
    };
    return list;
}

} // namespace xspreadsheet
