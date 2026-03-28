# Specify the minimum required version of CMake
cmake_minimum_required(VERSION 3.14)

# Specify the C++ standard to use
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)

# Define the project name and enable C++
project(ConsoleCalculatorCPP LANGUAGES CXX)

# Add an executable target named "calc" from the source file "calc.cpp"
add_executable(calc src/calc.cpp)