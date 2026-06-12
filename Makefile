CC = emcc
CFLAGS = -O3
DIST_DIR = dist
OBJ_DIR = obj
INCLUDES = -Isrc/c/engine
PLUGIN_FLAGS = \
	-s STANDALONE_WASM=1 \
	--no-entry \
	-s ERROR_ON_UNDEFINED_SYMBOLS=0 \
	-Wno-undefined \
	-s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_trigger_note_on", "_trigger_note_off", "_process", "_destroy_instance", "_get_plugin_manifest", "_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
ENGINE_SRC = $(shell find src/c/engine -name '*.c')
ENGINE_OBJ = $(patsubst src/c/%.c, $(OBJ_DIR)/%.o, $(ENGINE_SRC))
PLUGIN_DIRS = $(wildcard src/c/plugins/*)
PLUGIN_TARGETS = $(patsubst src/c/plugins/%, $(DIST_DIR)/%.wasm, $(PLUGIN_DIRS))
VUE_ASSETS_DIR = test/vue/src/assets/wasm
all: clean compile_engine build_plugins deploy_js
compile_engine: $(ENGINE_OBJ)
$(OBJ_DIR)/%.o: src/c/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
build_plugins: $(PLUGIN_TARGETS)
$(DIST_DIR)/%.wasm: src/c/plugins/%
	@mkdir -p $(DIST_DIR)
	@mkdir -p $(OBJ_DIR)/plugins/$*
	$(eval CURRENT_SRC := $(shell find $< -name '*.c'))
	$(CC) $(CURRENT_SRC) $(ENGINE_OBJ) -o $@ $(CFLAGS) $(INCLUDES) $(PLUGIN_FLAGS) \
		-s EXPORTED_FUNCTIONS='["_create_instance", "_set_parameter", "_trigger_note_on", "_trigger_note_off", "_process", "_destroy_instance", "_malloc", "_free"]'
deploy_js:
	@mkdir -p $(DIST_DIR)
	@cp -f src/js/index.js $(DIST_DIR)/index.js
	@cp -f src/js/processor.js $(DIST_DIR)/stream-processor.js
	@mkdir -p $(VUE_ASSETS_DIR)
	@cp -f $(DIST_DIR)/*.wasm $(VUE_ASSETS_DIR)/
	@cp -f $(DIST_DIR)/*.js $(VUE_ASSETS_DIR)/
run: all
	@cd test/vue && npm install && npm run dev
clean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(DIST_DIR)
.PHONY: all compile_engine build_plugins deploy_js clean run
