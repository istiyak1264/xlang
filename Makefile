# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lm

# Directories
SRC_DIR = .
BUILD_DIR = build
BIN_DIR = bin
INSTALL_DIR = /usr/local/bin

# Source files
SOURCES = main.c lexer.c parser.c ast.c codegen.c error.c runtime.c
OBJECTS = $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))
TARGET = $(BIN_DIR)/xlangc

# Default target
all: $(TARGET)

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install
install: $(TARGET)
	install -d $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)/
	@echo "XLang compiler installed successfully to $(INSTALL_DIR)/xlangc"

# Uninstall
uninstall:
	rm -f $(INSTALL_DIR)/xlangc
	@echo "XLang compiler uninstalled"

# Test
test: $(TARGET)
	@echo "Running tests..."
	./$(TARGET) --version

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Build the compiler (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install compiler to system"
	@echo "  uninstall- Remove compiler from system"
	@echo "  test     - Run basic tests"
	@echo "  help     - Show this help message"

.PHONY: all clean install uninstall test help