# Library Build Orchestrator
CC = emcc
CFLAGS = -O3

# Directory Configurations
SRC_C_DIR = src/c
SRC_JS_DIR = src/js
DIST_DIR = dist
OBJ_DIR = obj

# Targets Layout
WASM_OUTPUT = $(DIST_DIR)/waveform.js

# Recursive C Engine Sources Discovery
SRC_C = $(shell find $(SRC_C_DIR) -name '*.c')
OBJ_C = $(patsubst $(SRC_C_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_C))

# Emscripten Library Optimization Directives
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_set_echo_parameters", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_init_wasm_synth_core", "_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_set_echo_parameters", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_init_wasm_synth_core", "_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_set_echo_parameters", "_set_custom_harmonics_api", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
#EMSCRIPTEN_FLAGS = \
#	-s STANDALONE_WASM=1 \
#	--no-entry \
#	-s EXPORTED_FUNCTIONS='["_init_wasm_synth_core", "_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_set_echo_parameters", "_set_custom_harmonics", "_malloc", "_free"]' \
#	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
EMSCRIPTEN_FLAGS = \
	-s STANDALONE_WASM=1 \
	--no-entry \
	-s EXPORTED_FUNCTIONS='["_init_wasm_synth_core", "_note_on", "_note_off", "_generate_wave", "_stream_mix_polyphonic", "_set_adsr_parameters", "_set_tremolo_parameters", "_set_echo_parameters", "_set_distortion_parameters_api", "_set_chorus_parameters_api", "_set_custom_harmonics", "_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["HEAPF32"]'
# Core operational build rule
all: compile_wasm deploy_js

# Compiles target binary elements
compile_wasm: $(OBJ_C)
	@mkdir -p $(DIST_DIR)
	@echo "🔗 Linking C object modules into target directory library..."
	$(CC) $(OBJ_C) -o $(WASM_OUTPUT) $(CFLAGS) $(EMSCRIPTEN_FLAGS)
	@echo "✓ WebAssembly compilation successful."

# Object tracking blueprint rule
$(OBJ_DIR)/%.o: $(SRC_C_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "📦 Building Component: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# JavaScript Library Assets Deployment Rule
deploy_js:
	@mkdir -p $(DIST_DIR)
	@cp -f $(SRC_JS_DIR)/index.js $(DIST_DIR)/synth-engine.js
	@cp -f $(SRC_JS_DIR)/processor.js $(DIST_DIR)/stream-processor.js
	@echo "✓ JavaScript Wrapper classes deployed to $(DIST_DIR)/"

# Workspace scrubbing sequence
clean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(DIST_DIR)
	@echo "✓ Library production workspace cleared."

test_html: all
	@echo "📡 Launching Vanilla Library Test Environment at http://localhost:8000/test/html/"
	python3 -m http.server 8000


.PHONY: all compile_wasm deploy_js clean

