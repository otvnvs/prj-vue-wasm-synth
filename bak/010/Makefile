# Compiler & Toolchain Optimization Flags
CC = emcc
CFLAGS = -O3
DIST_DIR = dist
OBJ_DIR = obj

# Include Paths (Allows plugins to easily locate engine/plugin.h via #include "plugin.h")
INCLUDES = -Isrc/c/engine

# Linker Optimization Flags for Standalone Modules
#PLUGIN_FLAGS = -s STANDALONE_WASM=1 --no-entry -s ERROR_ON_UNDEFINED_SYMBOLS=0
#PLUGIN_FLAGS = -s STANDALONE_WASM=1 --no-entry -s ERROR_ON_UNDEFINED_SYMBOLS=0 -Wno-undefined
PLUGIN_FLAGS = \
	-s STANDALONE_WASM=1 \
	--no-entry \
	-s ERROR_ON_UNDEFINED_SYMBOLS=0 \
	-Wno-undefined \
	-s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_trigger_note_on", "_trigger_note_off", "_process", "_destroy_instance", "_get_plugin_manifest", "_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'



# 1. Recursive Core Engine Discovery
# Gathers any shared calculations or mathematical engines sitting under engine/
ENGINE_SRC = $(shell find src/c/engine -name '*.c')
ENGINE_OBJ = $(patsubst src/c/%.c, $(OBJ_DIR)/%.o, $(ENGINE_SRC))

# 2. Dynamic Plugin Subfolder Discovery
# Locates every unique folder inside src/c/plugins/
PLUGIN_DIRS = $(wildcard src/c/plugins/*)

# Maps folder names to desired output WASM files:
# e.g., src/c/plugins/plugin_synth -> dist/plugin_synth.wasm
PLUGIN_TARGETS = $(patsubst src/c/plugins/%, $(DIST_DIR)/%.wasm, $(PLUGIN_DIRS))

# --- Target Routing Lists ---
all: clean compile_engine build_plugins deploy_js

# Compiles any helper object models present inside the engine subdirectory
compile_engine: $(ENGINE_OBJ)

$(OBJ_DIR)/%.o: src/c/%.c
	@mkdir -p $(dir $@)
	@echo "📦 Compiling Engine Shared Module: $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 3. Dynamic Automated Rule: Compiles each plugin folder dynamically
# Uses the exact method bindings required by our node contracts
build_plugins: $(PLUGIN_TARGETS)

$(DIST_DIR)/%.wasm: src/c/plugins/%
	@mkdir -p $(DIST_DIR)
	@mkdir -p $(OBJ_DIR)/plugins/$*
	@echo "🔥 Building Standalone Plugin Target: $*"
	
	$(eval CURRENT_SRC := $(shell find $< -name '*.c'))
	
	$(CC) $(CURRENT_SRC) $(ENGINE_OBJ) -o $@ $(CFLAGS) $(INCLUDES) $(PLUGIN_FLAGS) \
		-s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_trigger_note_on", "_trigger_note_off", "_process", "_destroy_instance", "_malloc", "_free"]'
	@echo "✓ Dynamic compilation successful: $@"

# Resource Deployment Rule
deploy_js:
	@mkdir -p $(DIST_DIR)
	@cp -f src/js/index.js $(DIST_DIR)/index.js
	@cp -f src/js/processor.js $(DIST_DIR)/stream-processor.js
	@echo "✓ JavaScript assets copied to distribution."

# Local Server Target (Vue test harness launcher shortcut)
test_vue: all
	@echo "📡 Initializing local Vue development workspace server..."
	@cd test/vue && npm install && npm run dev

# Deep Clean workspace sequence
clean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(DIST_DIR)
	@echo "✓ Compiled objects and asset tree cleared."

.PHONY: all compile_engine build_plugins deploy_js clean test_vue

