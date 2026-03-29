#include <iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <csignal>
#include <vector>
#include <stack>
#include <cmath>
#include <map>
#include <limits>
#include <stdexcept>
#include <fstream>
#include <cstdlib>

class History {
private:
    std::string path;
    std::ostringstream dataCached;

    std::string getHistoryPath(){
        const char* home = std::getenv("HOME");
        if (!home) return "\0";
        return std::string(home) + "/.calchistory";
    }

public:
    History() : path(this->getHistoryPath()) {}

    // Append expression (only) to ~/.calchistory. If HOME is not available or
    // the file cannot be opened, this is a no-op.
    void write(const std::string& expr) {
        if (expr.empty()) return;
        dataCached << expr << '\n';
    }

    void flush(){
        std::ofstream ofs(path);
        ofs << dataCached.str();
        ofs.close();
    }
};

// Global pointer to History for signal handler access
History* g_history = nullptr;

// Calculator run modes
enum class Mode {
    MODE_ARGUMENT,
    MODE_INTERACTIVE
};

void signalHandler(int sig) {
    if (g_history) {
        g_history->flush();
    }
    exit(sig);
}

// Apply binary operators: +, -, *, /, ^
double applyOperation(double a, double b, char op) {
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/':
        if (b == 0.0) throw std::runtime_error("Error: Division by zero!");
        return a / b;
    case '^': return std::pow(a, b);
    default: throw std::runtime_error(std::string("Error: Unknown operator '") + op + "'!");
    }
}

// Apply unary functions: sqrt, log, sin, cos, tan
double applyFunction(const std::string& fn, double x) {
    if (fn == "sqrt") return std::sqrt(x);
    if (fn == "log" || fn == "ln") {
        if (x <= 0.0) throw std::runtime_error("Error: log domain error!");
        return std::log(x);
    }
    if (fn == "sin") return std::sin(x);
    if (fn == "cos") return std::cos(x);
    if (fn == "tan") return std::tan(x);
    throw std::runtime_error("Error: Unknown function '" + fn + "'!");
}

// Operator precedence
std::map<char, int> precedence = {
    {'+', 1}, {'-', 1},
    {'*', 2}, {'/', 2},
    {'^', 3}
};

// Check balanced parentheses
bool checkParentheses(const std::string& s) {
    int bal = 0;
    for (char ch : s) {
        if (ch == '(') ++bal;
        else if (ch == ')' && --bal < 0) return false;
    }
    return bal == 0;
}

// Split the input into tokens: numbers, operators, parentheses, names
std::vector<std::string> tokenize(const std::string& eq) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
        };
    for (size_t i = 0; i < eq.size(); ++i) {
        char ch = eq[i];
        if (std::isspace(ch)) continue;

        // Identifier (function name)
        if (std::isalpha(ch)) {
            flush();
            std::string name;
            while (i < eq.size() && std::isalpha(eq[i])) {
                name += eq[i++];
            }
            tokens.push_back(name);
            --i;  // back up one char
        }
        // Part of a number (digit, dot, or leading minus)
        else if (std::isdigit(ch) || ch == '.' ||
            (ch == '-' && (
                i == 0 ||
                eq[i - 1] == '(' ||
                precedence.count(eq[i - 1]) > 0
                ))) {
            cur += ch;
        }
        else {
            flush();
            tokens.push_back(std::string(1, ch));
        }
    }
    flush();
    return tokens;
}

// Evaluate expression with recursion for parentheses and functions
double evaluateExpression(
    std::vector<std::string>::const_iterator& it,
    const std::vector<std::string>::const_iterator& end)
{
    std::stack<double> nums;
    std::stack<char> ops;

    while (it != end && *it != ")") {
        const std::string& tok = *it;

        // Function call
        if (std::isalpha(tok[0])) {
            std::string fn = tok;
            ++it;  // skip function name
            if (it == end || *it != "(")
                throw std::runtime_error("Error: Expected '(' after " + fn);
            ++it;  // skip '('
            double arg = evaluateExpression(it, end);
            nums.push(applyFunction(fn, arg));
            continue;
        }

        // Sub-expression
        if (tok == "(") {
            ++it;
            nums.push(evaluateExpression(it, end));
            continue;
        }

        // Number literal
        if (std::isdigit(tok[0]) || (tok[0] == '-' && tok.size() > 1)) {
            nums.push(std::stod(tok));
            ++it;
            continue;
        }

        // Operator
        if (tok.size() == 1 && precedence.count(tok[0])) {
            char op = tok[0];
            while (!ops.empty()) {
                char top = ops.top();
                int p1 = precedence[op];
                int p2 = precedence[top];
                if ((op == '^' && p1 < p2) ||
                    (op != '^' && p1 <= p2)) {
                    ops.pop();
                    double b = nums.top(); nums.pop();
                    double a = nums.top(); nums.pop();
                    nums.push(applyOperation(a, b, top));
                }
                else break;
            }
            ops.push(op);
            ++it;
            continue;
        }

        throw std::runtime_error("Error: Invalid token '" + tok + "'");
    }

    // Consume ')'
    if (it != end && *it == ")") ++it;

    // Flush remaining ops
    while (!ops.empty()) {
        char op = ops.top(); ops.pop();
        double b = nums.top(); nums.pop();
        double a = nums.top(); nums.pop();
        nums.push(applyOperation(a, b, op));
    }

    if (nums.empty())
        throw std::runtime_error("Error: Empty expression!");
    return nums.top();
}

// Wrapper to evaluate full token list
double evaluate(const std::vector<std::string>& tokens) {
    auto it = tokens.begin();
    return evaluateExpression(it, tokens.end());
}

void interactiveMode(History& history){
    for(;;) {
        std::string line;
        std::getline(std::cin, line);
        // store expression in history
        history.write(line);

        try {
            if (!checkParentheses(line))
                throw std::runtime_error("Error: Mismatched parentheses.");

            auto tokens = tokenize(line);
            double result = evaluate(tokens);
            std::cout << result << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }
}

void argumentMode(History& history, const std::string& arg){
    // store expression in history
    history.write(arg);
    try {
        if (!checkParentheses(arg))
            throw std::runtime_error("Error: Mismatched parentheses.");

        auto tokens = tokenize(arg);
        double result = evaluate(tokens);
        std::cout << result << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    History history;
    g_history = &history;

    // Register signal handlers to flush history on interrupt
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Default to inline-single mode when no flags are provided
    Mode mode = Mode::MODE_INTERACTIVE;

    // If there are arguments but no known modifier flags, treat all argv
    // elements (1..argc-1) as a single expression and evaluate it.
    std::ostringstream oss;
    if (argc > 1) {
        mode = Mode::MODE_ARGUMENT;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) oss << ' ';
            oss << argv[i];
        }
    }

    // Dispatch based on selected mode
    switch (mode) {
    case Mode::MODE_INTERACTIVE:
        interactiveMode(history);
        break;
    case Mode::MODE_ARGUMENT:
        argumentMode(history, oss.str());
        break;
    }

    return 0;
}
