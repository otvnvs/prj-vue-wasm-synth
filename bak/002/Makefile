# Compiler settings
CC = emcc
CFLAGS = -O3

# Output configurations
TARGET_DIR = www
JS_OUTPUT = $(TARGET_DIR)/waveform.js

# Source and Resource definitions
SRC = src/generator.c
RESOURCES = res/index.html res/main.js res/stream-processor.js

# Local Server Configuration
PORT = 8000

# Emscripten specific Linker flags
EMSCRIPTEN_FLAGS = \
	-s STANDALONE_WASM=1 \
	--no-entry \
	-s EXPORTED_FUNCTIONS='["_generate_sine_wave", "_generate_square_wave", "_stream_sine_wave", "_reset_stream_phase", "_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'

# Default 'all' target
all: $(JS_OUTPUT) copy_resources

# Compilation Step
$(JS_OUTPUT): $(SRC)
	@mkdir -p $(TARGET_DIR)
	$(CC) $(SRC) -o $(JS_OUTPUT) $(CFLAGS) $(EMSCRIPTEN_FLAGS)
	@echo "WebAssembly compilation complete."

# Resource Deployment Step
copy_resources: $(RESOURCES)
	@mkdir -p $(TARGET_DIR)
	@cp -f res/index.html $(TARGET_DIR)/
	@cp -f res/main.js $(TARGET_DIR)/
	@cp -f res/stream-processor.js $(TARGET_DIR)/
	@echo "Web resources copied to $(TARGET_DIR)/"

# Local Server Target (New)
# It changes directory into www first, then launches Python's built-in server.
serve: all
	@echo "Starting local development server at http://localhost:$(PORT)"
	@cd $(TARGET_DIR) && python3 -m http.server $(PORT)

# Clean target to clear build assets
clean:
	@rm -rf $(TARGET_DIR)
	@rm -f serve
	@echo "Build workspace cleaned."

# CRITICAL: Mark 'serve' as a phony target so make doesn't look for a file named 'serve'
.PHONY: all copy_resources clean serve
