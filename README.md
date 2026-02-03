# Console Calculator

A small console calculator written in C++.

Features
- Supports operators: +, -, *, /, ^
- Supports parentheses for grouping
- Supports functions: sqrt(), log() (alias ln()), sin(), cos(), tan()
- Saves evaluated expressions (one per line) to ~/.calchistory when possible
- Error handling for division by zero and mismatched parentheses

Building
- Using CMake:
  - cmake -S . -B build
  - cmake --build build
- Or compile directly with g++:
  - g++ src/calc.cpp -o calc -std=c++17 -lm

Usage
- Inline single (default): runs a single prompt then exits
  - ./calc
- Inline multiple: repeat prompts until exit
  - ./calc -m
- Argument mode: evaluate expression provided on the command line
  - ./calc -a "2 + 2 * (3 - 1)"
- If no flags are provided but arguments exist, all argv elements are concatenated and evaluated (e.g. ./calc 2 + 2)

Examples
- Evaluate a literal expression:
  - ./calc "3 + 4 * 2"
- Use functions:
  - ./calc "sqrt(16) + sin(0)"

Notes
- Only one mode flag may be provided at a time (-s, -m, or -a).
- log() uses the natural logarithm (base e).