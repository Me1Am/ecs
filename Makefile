OS := $(shell uname -s)

# Compiler and Flags
CC := gcc
C_FLAGS := -Wall -Wextra -std=c11 -ggdb -I$(INC) -I$(EXT_INC)

# Directories
SRC := src
INC := include
BUILD := build
BIN := bin
EXT_INC := klib khashl

# Target Executable
TARGET := $(BIN)/main

# Source and Object Files
SRC_FILES := $(wildcard $(SRC)/*.c)
EXT_SRC_FILES := $(wildcard $(EXT_INC)/*.c)
OBJ_FILES := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRC_FILES))

# Determine which klib sources to compile (if a .h file is included)
KLIB_OBJS := $(foreach src,$(EXT_SRC_FILES),$(if $(shell grep -l $(basename $(notdir $(src))).h $(SRC_FILES)), $(BUILD)/$(notdir $(src:.c=.o)),))

# All object files to compile
ALL_OBJS := $(OBJ_FILES) $(KLIB_OBJS)

# OS-Specific Adjustments
ifeq ($(OS),Windows_NT)
    TARGET := $(BIN)/project.exe
    RM := del /Q
    MKDIR := if not exist
else
    RM := rm -rf
    MKDIR := mkdir -p
endif

# Default target
all: build

# Build target
build: $(TARGET)

# Compile source files
$(BUILD)/%.o: $(SRC)/%.c
	$(MKDIR) $(BUILD)
	$(CC) $(C_FLAGS) -c $< -o $@

# Compile klib files if needed
$(BUILD)/%.o: $(EXT_INC)/%.c
	$(MKDIR) $(BUILD)
	$(CC) $(C_FLAGS) -c $< -o $@

# Link executable
$(TARGET): $(ALL_OBJS)
	$(MKDIR) $(BIN)
	$(CC) $(C_FLAGS) $^ -o $@

# Clean target
clean:
	$(RM) $(BUILD)/* $(TARGET)

# Run target
run: build
	$(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  build - Compile the project"
	@echo "  clean - Remove built files"
	@echo "  run   - Build and run the project"
	@echo "  help  - Show this help message"
