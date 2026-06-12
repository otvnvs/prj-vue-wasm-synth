# Compiler configuration
CC = emcc
CFLAGS = -O3

# Directory layout
SRC_DIR = src
OBJ_DIR = obj
TARGET_DIR = www

# Output targets
JS_OUTPUT = $(TARGET_DIR)/waveform.js

# 1. Recursive Source Discovery
# Automatically search all directories under ./src/ for .c files
SRC = $(shell find $(SRC_DIR) -name '*.c')

# 2. Object File Mapping
# Convert paths: src/folder/file.c -> obj/folder/file.o
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# Web resources definitions
RESOURCES = res/index.html res/main.js res/stream-processor.js
PORT = 8000

# Emscripten compilation flags
EMSCRIPTEN_FLAGS = \
	-s STANDALONE_WASM=1 \
	--no-entry \
	-s EXPORTED_FUNCTIONS='["_generate_wave", "_stream_wave", "_reset_stream_phase", "_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'

# Default action rule
all: $(JS_OUTPUT) copy_resources

# Final Linker Step: Combines all discrete object files into the target output
$(JS_OUTPUT): $(OBJ)
	@mkdir -p $(TARGET_DIR)
	@echo "🔗 Linking object modules into final WebAssembly target..."
	$(CC) $(OBJ) -o $(JS_OUTPUT) $(CFLAGS) $(EMSCRIPTEN_FLAGS)
	@echo "✓ Modular WebAssembly build complete."

# 3. Individual Compilation Rule with Directory Shadowing
# This compiles each individual .c file to its corresponding .o file.
# The '@mkdir -p' command mirrors the source subfolder structure inside /obj.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "📦 Compiling: $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

# Resource Deployment Step
copy_resources: $(RESOURCES)
	@mkdir -p $(TARGET_DIR)
	@cp -f res/index.html $(TARGET_DIR)/
	@cp -f res/main.js $(TARGET_DIR)/
	@cp -f res/stream-processor.js $(TARGET_DIR)/
	@echo "✓ Web resources copied to $(TARGET_DIR)/"

# Local Server Target
serve: all
	@echo "📡 Server running at http://localhost:$(PORT)"
	@cd $(TARGET_DIR) && python3 -m http.server $(PORT)

# Clean target to clear object trees and build folders safely
clean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	@echo "✓ Build workspace and object trees cleaned."

.PHONY: all copy_resources clean serve

