BUILD_DIR := build

.PHONY: all build test clean

all: build

build:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>&1 | tail -5
	@cmake --build $(BUILD_DIR) --parallel

test: build
	@cmake --build $(BUILD_DIR) --target izi_tests 2>/dev/null || true
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	@rm -rf $(BUILD_DIR)

# Run hello.izi end-to-end
run-example: build
	./$(BUILD_DIR)/izi build examples/hello.izi -o /tmp/hello_izi
	/tmp/hello_izi
