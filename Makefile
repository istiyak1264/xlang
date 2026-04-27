CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -Ilib -D_GNU_SOURCE
SRCS = src/main.c src/lexer.c src/parser.c src/ast.c src/codegen.c src/error.c lib/runtime.c
OBJS = $(SRCS:%.c=obj/%.o)
TARGET = xlangc

.PHONY: all clean install test debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ -lm

obj/%.o: %.c include/*.h lib/*.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/lib/%.o: lib/%.c lib/*.h
	@mkdir -p obj/lib
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -Wextra -g -O0 -Iinclude -Ilib -DXLANG_DEBUG_MEMORY"

clean:
	rm -rf obj $(TARGET) a.out

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo mkdir -p /usr/local/include/xlang
	sudo cp lib/runtime.h /usr/local/include/xlang/

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -rf /usr/local/include/xlang

test: $(TARGET)
	@echo "Testing XLang compiler..."
	./$(TARGET) examples/hello.x
	./a.out
	@echo "Test complete!"

test-all: $(TARGET)
	@for test in examples/*.x; do \
		echo "Running $$test..."; \
		./$(TARGET) "$$test" -o test_output; \
		if [ -f test_output ]; then \
			./test_output; \
			rm test_output; \
		fi \
	done