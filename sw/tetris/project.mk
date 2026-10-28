TETRIS_DIR := sw/tetris
TETRIS_HEADERS := $(wildcard $(TETRIS_DIR)/*.h)
TETRIS_FRAMES ?= 600
.PHONY: tetris-rom tetris-native tetris-unit tetris-nvboard-build tetris-nvboard tetris-test tetris-ps2-test tetris-lint
build/tetris-rv32i/tetris.elf: $(TETRIS_DIR)/main.cpp $(TETRIS_DIR)/game.cpp $(TETRIS_HEADERS) sw/mario/runtime.cpp sw/start.S sw/link.ld $(TETRIS_DIR)/project.mk
	@mkdir -p build/tetris-rv32i
	$(CROSS_COMPILE)g++ $(CFLAGS_RV) -O2 -std=c++17 -nostdinc++ -Isw/mario/include \
	  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -ffunction-sections -fdata-sections \
	  $(LDFLAGS_RV) -Wl,--gc-sections sw/start.S $(TETRIS_DIR)/main.cpp $(TETRIS_DIR)/game.cpp sw/mario/runtime.cpp -o $@
	$(CROSS_COMPILE)size $@
	$(OBJDUMP_RV) -d $@ > build/tetris-rv32i/tetris.dis
build/tetris-rv32i/tetris.hex: build/tetris-rv32i/tetris.elf
	$(OBJCOPY_RV) -O verilog --verilog-data-width=1 $< $@
tetris-rom: build/tetris-rv32i/tetris.hex
build/tetris-native/tetris: $(TETRIS_DIR)/native.cpp $(TETRIS_DIR)/game.cpp $(TETRIS_HEADERS)
	@mkdir -p build/tetris-native
	$(CXX) -std=c++17 -O2 -Wall -Wextra $(shell sdl2-config --cflags) $(TETRIS_DIR)/native.cpp $(TETRIS_DIR)/game.cpp $(shell sdl2-config --libs) -o $@
tetris-native: build/tetris-native/tetris
	./build/tetris-native/tetris $(NVBOARD_ARGS)
build/tetris-native/unit: $(TETRIS_DIR)/unit.cpp $(TETRIS_DIR)/game.cpp $(TETRIS_HEADERS)
	@mkdir -p build/tetris-native
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -UNDEBUG $(TETRIS_DIR)/unit.cpp $(TETRIS_DIR)/game.cpp -o $@
tetris-unit: build/tetris-native/unit
	./build/tetris-native/unit
build/tetris-nvboard/pins.cpp: constr/nvboard_top.nxdc
	@mkdir -p build/tetris-nvboard
	python3 '$(NVBOARD_HOME)/scripts/auto_pin_bind.py' $< $@
build/tetris-nvboard/Vnvboard_top: $(SOC_RTL) rtl/nvboard_top.v sim/tetris_main.cpp $(TETRIS_DIR)/demo.h build/tetris-nvboard/pins.cpp $(NVBOARD_LIB) $(NVBOARD_DISPLAY_LIB) $(TETRIS_DIR)/project.mk Makefile
	$(VERILATOR) --cc --exe --build -j 2 --threads 1 -O3 --top-module nvboard_top -GTETRIS_MODE=1 \
	  $(BOARD_LINT_FLAGS) -I$(CORE_DIR) --Mdir build/tetris-nvboard -MAKEFLAGS 'OPT_FAST=-O3 -W $(abspath sim/tetris_main.cpp)' \
	  -CFLAGS '-std=c++17 -O3 -flto $(NVBOARD_FLAGS)' -LDFLAGS '-flto $(NVBOARD_LIBS)' \
	  $(SOC_RTL) rtl/nvboard_top.v $(abspath sim/tetris_main.cpp) $(abspath build/tetris-nvboard/pins.cpp)
tetris-nvboard-build: tetris-rom build/tetris-nvboard/Vnvboard_top
tetris-nvboard: tetris-nvboard-build
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER='$(NVBOARD_RENDERER)' ./build/tetris-nvboard/Vnvboard_top +rom=build/tetris-rv32i/tetris.hex $(NVBOARD_ARGS)
tetris-lint:
	$(VERILATOR) --lint-only --top-module nvboard_top -GTETRIS_MODE=1 $(BOARD_LINT_FLAGS) -I$(CORE_DIR) $(SOC_RTL) rtl/nvboard_top.v
tetris-test: tetris-unit tetris-nvboard-build build/tetris-native/tetris
	python3 sim/verify_tetris.py --frames $(TETRIS_FRAMES)
tetris-ps2-test: tetris-unit tetris-nvboard-build build/tetris-native/tetris
	NVBOARD_HOME='$(NVBOARD_HOME)' python3 sim/verify_tetris.py --frames $(TETRIS_FRAMES) --ps2
