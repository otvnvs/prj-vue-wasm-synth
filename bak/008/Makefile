# Standalone Modular Plugin Builder (Updated for Filter)
CC = emcc
CFLAGS = -O3
DIST_DIR = dist

PLUGIN_FLAGS = -s STANDALONE_WASM=1 --no-entry -s ERROR_ON_UNDEFINED_SYMBOLS=0

all: clean build_plugins

build_plugins:
	@mkdir -p $(DIST_DIR)
	@echo "📦 Compiling Standalone Synth Generator Plugin..."
	$(CC) src/c/plugin_synth.c -o $(DIST_DIR)/plugin_synth.wasm $(CFLAGS) $(PLUGIN_FLAGS) -s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_trigger_note_on", "_trigger_note_off", "_process", "_destroy_instance", "_malloc", "_free"]'
	
	@echo "📦 Compiling Standalone Tape Echo Effect Plugin..."
	$(CC) src/c/plugin_echo.c -o $(DIST_DIR)/plugin_echo.wasm $(CFLAGS) $(PLUGIN_FLAGS) -s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_process", "_destroy_instance", "_malloc", "_free"]'
	
	@echo "📦 Compiling Standalone Resonant Filter Plugin..."
	$(CC) src/c/plugin_filter.c -o $(DIST_DIR)/plugin_filter.wasm $(CFLAGS) $(PLUGIN_FLAGS) -s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_process", "_destroy_instance", "_malloc", "_free"]'

	@cp -f src/js/index.js $(DIST_DIR)/index.js
	@cp -f src/js/processor.js $(DIST_DIR)/stream-processor.js
	@echo "✓ Modular Architecture Compilation Complete."

clean:
	@rm -rf $(DIST_DIR)
	@echo "✓ Workspace cleared."

.PHONY: all build_plugins clean

