CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
          -O2 -std=c11 -D_GNU_SOURCE
TARGET  = xlangc

# ── Directory layout ────────────────────────────────────────────
SRCDIR  = src
INCDIR  = include
LIBDIR  = lib
OBJDIR  = obj

# ── Install paths ───────────────────────────────────────────────
PREFIX      = /usr/local
BINDIR      = $(PREFIX)/bin
RUNTIMEDIR  = $(PREFIX)/lib/xlang

# ── Sources & objects ───────────────────────────────────────────
# compiler sources live in src/
COMPILER_SRCS = $(SRCDIR)/ast.c    \
                $(SRCDIR)/codegen.c \
                $(SRCDIR)/error.c   \
                $(SRCDIR)/lexer.c   \
                $(SRCDIR)/main.c    \
                $(SRCDIR)/parser.c

# runtime source lives in lib/
RUNTIME_SRC   = $(LIBDIR)/runtime.c

ALL_SRCS = $(COMPILER_SRCS) $(RUNTIME_SRC)

# Object files go into obj/
COMPILER_OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(COMPILER_SRCS))
RUNTIME_OBJ   = $(OBJDIR)/runtime.o
ALL_OBJS      = $(COMPILER_OBJS) $(RUNTIME_OBJ)

# ── Add include path + runtime dir define ───────────────────────
CFLAGS += -I$(INCDIR)
CFLAGS += -DXLANG_RUNTIME_DIR=\"$(RUNTIMEDIR)\"

all: $(OBJDIR) $(TARGET)
	@echo ""
	@echo "  ✓  Built: ./$(TARGET)"
	@echo "  →  Now run:  sudo make install"
	@echo "  →  Then:     xlangc --version"
	@echo ""

# Link all objects into the final binary
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Create obj/ directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile src/*.c → obj/*.o
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile lib/runtime.c → obj/runtime.o
$(OBJDIR)/runtime.o: $(LIBDIR)/runtime.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(LIBDIR) -c -o $@ $<

install: $(TARGET)
	@echo "── Installing xlangc ──────────────────────────────────"
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "  ✓  $(BINDIR)/$(TARGET)"
	@echo "── Installing runtime ─────────────────────────────────"
	install -d $(RUNTIMEDIR)
	install -m 644 $(LIBDIR)/runtime.c  $(RUNTIMEDIR)/runtime.c
	install -m 644 $(LIBDIR)/runtime.h  $(RUNTIMEDIR)/runtime.h
	install -m 644 $(INCDIR)/ast.h      $(RUNTIMEDIR)/ast.h
	@echo "  ✓  $(RUNTIMEDIR)/"
	@echo ""
	@echo "  ✓  Installation complete!"
	@echo "  →  Test it:  xlangc --version"
	@echo ""

uninstall:
	@echo "Removing xlangc..."
	rm -f  $(BINDIR)/$(TARGET)
	rm -rf $(RUNTIMEDIR)
	@echo "  ✓  Uninstalled."

clean:
	rm -rf $(OBJDIR) $(TARGET) a.out
	@echo "  ✓  Cleaned."

TEST_SRC = /tmp/xlang_test.x
TEST_BIN = /tmp/xlang_test_out

test: $(TARGET)
	@echo "── Writing test program ───────────────────────────────"
	@printf 'function main():\n    output("XLang test OK!")\n    int x := 6\n    int y := 7\n    output(x * y)\n    return 0\n' > $(TEST_SRC)
	@echo "── Compiling ──────────────────────────────────────────"
	./$(TARGET) $(TEST_SRC) -o $(TEST_BIN)
	@echo "── Running ────────────────────────────────────────────"
	@$(TEST_BIN)
	@rm -f $(TEST_SRC) $(TEST_BIN)
	@echo "── Test passed ✓ ──────────────────────────────────────"

help:
	@echo ""
	@echo "XLang Compiler — Makefile targets"
	@echo "──────────────────────────────────────────────────"
	@echo "  make              Build xlangc (output: ./xlangc)"
	@echo "  make install      Install to $(BINDIR)  [needs sudo]"
	@echo "  make uninstall    Remove installed xlangc"
	@echo "  make clean        Delete obj/ and xlangc"
	@echo "  make test         Build + run a quick test program"
	@echo "  make help         Show this message"
	@echo ""
	@echo "Custom install prefix (no sudo needed):"
	@echo "  make PREFIX=\$$HOME/.local install"
	@echo "  export PATH=\$$HOME/.local/bin:\$$PATH"
	@echo ""

.PHONY: all install uninstall clean test help