#include <iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <vector>
#include <stack>
#include <cmath>
#include <map>
#include <limits>
#include <stdexcept>
#include <fstream>
#include <cstdlib>

int continue_ = 0;

// Calculator run modes
enum class Mode {
    MODE_ARGUMENT,
    MODE_INLINE_SINGLE,
    MODE_INLINE_MULTIPLE
};

enum class Modifiers {
    S,
    M,
    A
};

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

// Prompt to continue or exit
void askContinue() {
    char c;
    std::cout << "---------------------\nContinue? (y/n): ";
    std::cin >> c;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (c == 'n' || c == 'N') {
        continue_ = 1;
        std::cout << "Exiting...\n";
    }
}

// Append expression (only) to ~/.calchistory. If HOME is not available or
// the file cannot be opened, this is a no-op.
void writeHistory(const std::string& expr) {
    if (expr.empty()) return;
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.calchistory";
    std::ofstream ofs(path, std::ios::app);
    if (!ofs) return;
    ofs << expr << '\n';
}

void InlineMultipleMode(){
    while (continue_ == 0) {
        std::string line;
        std::cout << ">>> ";
        std::getline(std::cin, line);
        // store expression in history
        writeHistory(line);

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

void InlineSingleMode(){
    std::string line;
    std::cout << ">>> ";
    std::getline(std::cin, line);
    // store expression in history
    writeHistory(line);

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

void argumentMode(std::string arg){
    // store expression in history
    writeHistory(arg);
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
    std::string firstArg;
    if (argc > 1) {
        firstArg = argv[1];
    }
    // Default to inline-single mode when no flags are provided
    Mode mode = Mode::MODE_INLINE_SINGLE;

    bool sawSetVar = false;
    bool sawClearVar = false;
    int flagCount = 0; // count known modifier flags; only one allowed
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-s") {
            mode = Mode::MODE_INLINE_SINGLE;
            ++flagCount;
            continue;
        }
	else if (a == "-m") {
            mode = Mode::MODE_INLINE_MULTIPLE;
            ++flagCount;
            continue;
        }
	else if (a == "-a") {
            mode = Mode::MODE_ARGUMENT;
            ++flagCount;
            continue;
        }
	else if (a == "-h" || a == "--help") {
	    std::cout << "Usage: calc [options] [expression]\n"
		      << "Options:\n"
		      << "  -s          Inline single prompt mode (default)\n"
		      << "  -m          Inline multiple prompt mode\n"
		      << "  -a          Argument mode (evaluate expression from command line)\n"
		      << "  -h, --help  Show this help message\n";
	    return 0;
	}
	else if (a.rfind("-", 0) == 0) {
	    std::cerr << "Error: Unknown flag '" << a << "'.\n";
	    return 1;
	}
    }

    // Enforce only one known flag at a time
    if (flagCount > 1) {
        std::cerr << "Error: Only one flag may be provided at a time.\n";
        return 1;
    }
    // If there are arguments but no known modifier flags, treat all argv
    // elements (1..argc-1) as a single expression and evaluate it.
    if (flagCount == 0 && argc > 1) {
        std::ostringstream oss;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) oss << ' ';
            oss << argv[i];
        }
        argumentMode(oss.str());
        return 0;
    }

    // Dispatch based on selected mode
    switch (mode) {
    case Mode::MODE_INLINE_SINGLE:
        InlineSingleMode();
        break;
    case Mode::MODE_INLINE_MULTIPLE:
        InlineMultipleMode();
        break;
    case Mode::MODE_ARGUMENT: {
        // Build expression from non-flag argv elements (ignore known flags)
        std::ostringstream oss;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (!a.empty() && a[0] == '-') continue; // skip flags
            if (oss.tellp() > 0) oss << ' ';
            oss << a;
        }
        if (oss.tellp() > 0) {
            argumentMode(oss.str());
        }
        else if (!firstArg.empty()) {
            argumentMode(firstArg);
        }
        else {
            // No argument provided; fall back to inline single prompt
            InlineSingleMode();
        }
    } break;
    }

    return 0;
}
