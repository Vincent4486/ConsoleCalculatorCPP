
PROGRAM_DIR=$(abspath .)
PROGRAM_FILES=$(wildcard $(PROGRAM_DIR)/*.cpp)
EXE_NAME=calc

CXX=clang++
CXXFLAGS=-std=c++17 -o $(EXE_NAME) 

all: $(PROGRAM_DIR)/$(EXE_NAME)

$(PROGRAM_DIR)/$(EXE_NAME): $(PROGRAM_FILES)
	@echo "Building $(EXE_NAME)..."
	@$(CXX) $(CXXFLAGS) $(PROGRAM_FILES)

clean:
	@echo "Cleaning up..."
	@rm -f $(PROGRAM_DIR)/$(EXE_NAME)

install: all
	@echo "Installing $(EXE_NAME)..."
	@sudo cp $(PROGRAM_DIR)/$(EXE_NAME) /usr/local/bin/$(EXE_NAME)
	@echo "$(EXE_NAME) installed to /usr/local/bin/$(EXE_NAME)"
