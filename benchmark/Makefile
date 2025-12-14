# 1. Settings
CXX       = g++
CXXFLAGS  = -Wall -Wextra -std=c++17 -O3
BUILD_DIR = build
BEAR_CMD  = bear -- make all

# 2. File Discovery
# Finds all source files (*.cpp) in the current directory
SRCS      = $(wildcard *.cpp)

# 3. Target Executables
# Transforms source files (e.g., main.cpp) into build targets (e.g., build/main)
TARGETS   = $(SRCS:%.cpp=$(BUILD_DIR)/%)

# ----------------------------------------------------------------------
# 4. Rules
# ----------------------------------------------------------------------

# Default Rule: Builds all independent executables
all: $(TARGETS)

# Compilation and Linking Rule (Independent Executables)
# Compiles and links %.cpp directly into $(BUILD_DIR)/%
# $@ is the target executable (e.g., build/test01)
# $< is the dependency source file (e.g., test01.cpp)
$(BUILD_DIR)/%: %.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling and linking $< to $@"
	$(CXX) $(CXXFLAGS) -o $@ $<

# Clean Rule: Removes the entire build directory
clean:
	@echo "Removing $(BUILD_DIR) directory..."
	rm -rf $(BUILD_DIR)

# Rule for Bear/compile_commands.json
# Usage: 'make compile_db'
compile_db: clean all
	@echo "--- Generating compile_commands.json using Bear ---"
	@echo "This command will run: $(BEAR_CMD)"
	$(BEAR_CMD)
	@echo "----------------------------------------------------"

# Ensures 'all', 'clean', and 'compile_db' are treated as commands, not files
.PHONY: all clean compile_db
