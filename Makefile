CORE_DIR := rtl/soc/core
CORE_RTL := $(sort $(wildcard $(CORE_DIR)/*.v))
SOC_RTL := $(CORE_RTL) rtl/soc/soc_top.v $(sort $(wildcard rtl/soc/memory/*.v rtl/soc/periph/*.v rtl/soc/periph/vga/*.v))
VERILATOR ?= verilator
IVERILOG ?= iverilog
VVP ?= vvp
CROSS_COMPILE ?= riscv64-linux-gnu-
# Game selection defaults to Mario without changing the C-test default (smoke).
# Capture the caller's PROGRAM before applying that separate default below.
NVBOARD_PROGRAM := $(if $(filter undefined,$(origin PROGRAM)),mario,$(PROGRAM))
PROGRAM ?= smoke
EXPECTED_smoke := 0
EXPECTED_sum := 55
EXPECTED_fibonacci := 144
EXPECTED_gcd := 6
EXPECTED_sort := 12
EXPECTED_memory := 42
EXPECTED ?= $(EXPECTED_$(PROGRAM))
NEMU_SRC ?= /home/vero/ysyx2407/ysyx-workbench/nemu
NEMU_BIN := build/nemu/build/riscv32-nemu-interpreter
CC_RV := $(CROSS_COMPILE)gcc
OBJCOPY_RV := $(CROSS_COMPILE)objcopy
OBJDUMP_RV := $(CROSS_COMPILE)objdump
CFLAGS_RV := -march=rv32i -mabi=ilp32 -O1 -ffreestanding -fno-builtin \
             -fno-pic -fno-pie -fno-stack-protector -msmall-data-limit=0 \
             -mno-relax -Wall -Wextra
LDFLAGS_RV := -nostdlib -nostartfiles -static -no-pie \
              -Wl,--build-id=none,--no-relax,-T,sw/link.ld

.PHONY: all test core-test memory-test c-test examples nemu-build nemu-run nemu-debug compare lint synth clean
all: test
test: core-test memory-test c-test

build/core/Vrv32i_core: $(CORE_RTL) $(CORE_DIR)/rv32i_defs.vh sim/core_test.cpp
	@mkdir -p build/core
	$(VERILATOR) --cc --exe --build -j 2 --trace --top-module rv32i_core -GWITH_CLOCK_ENABLE=1 \
	  -I$(CORE_DIR) --Mdir build/core -CFLAGS '-std=c++17 -O2' \
	  $(CORE_RTL) $(abspath sim/core_test.cpp)
core-test: build/core/Vrv32i_core
	./build/core/Vrv32i_core

build/memory_tb: rtl/soc/memory/imem.v rtl/soc/memory/dmem.v sim/unit/memory_tb.sv
	@mkdir -p build
	$(IVERILOG) -g2012 -s memory_tb -o $@ $^
memory-test: build/memory_tb
	$(VVP) build/memory_tb

build/mario-rv32i/%.elf: sw/start.S sw/link.ld sw/tests/%.c Makefile
	@mkdir -p build/mario-rv32i
	$(CC_RV) $(CFLAGS_RV) $(LDFLAGS_RV) sw/start.S sw/tests/$*.c -o $@
	$(OBJDUMP_RV) -d $@ > build/mario-rv32i/$*.dis
build/mario-rv32i/%.hex: build/mario-rv32i/%.elf
	$(OBJCOPY_RV) -O verilog --verilog-data-width=1 $< $@
build/mario-rv32i/%.bin: build/mario-rv32i/%.elf
	$(OBJCOPY_RV) -O binary $< $@
# 保留ELF和镜像，便于手动反汇编、查符号以及在NEMU中运行。
.SECONDARY:
build/soc_tb: $(SOC_RTL) $(CORE_DIR)/rv32i_defs.vh sim/system/soc_tb.sv
	@mkdir -p build
	$(IVERILOG) -g2012 -I$(CORE_DIR) -s soc_tb -o $@ $(SOC_RTL) sim/system/soc_tb.sv
c-test: build/soc_tb build/mario-rv32i/$(PROGRAM).hex
	@test -n '$(EXPECTED)' || { echo '请为自定义程序指定EXPECTED=<main预期返回值>'; exit 1; }
	$(VVP) build/soc_tb +rom=build/mario-rv32i/$(PROGRAM).hex +expected=$(EXPECTED)
examples:
	@for program in sum fibonacci gcd sort smoke; do \
	  $(MAKE) --no-print-directory c-test PROGRAM=$$program || exit $$?; \
	done

nemu-build:
	bash sim/nemu/build.sh '$(NEMU_SRC)'
nemu-run: nemu-build build/mario-rv32i/$(PROGRAM).bin
	$(NEMU_BIN) -b build/mario-rv32i/$(PROGRAM).bin
nemu-debug: nemu-build build/mario-rv32i/$(PROGRAM).bin
	$(NEMU_BIN) build/mario-rv32i/$(PROGRAM).bin
# 两次执行读取同一个ELF生成的hex/bin；最后比较全部GPR和停机PC。
compare: nemu-build build/soc_tb build/mario-rv32i/$(PROGRAM).hex build/mario-rv32i/$(PROGRAM).bin
	@test -n '$(EXPECTED)' || { echo '请指定EXPECTED=<main预期返回值>'; exit 1; }
	python3 sim/compare.py --program '$(PROGRAM)' --expected '$(EXPECTED)' --nemu '$(NEMU_BIN)'

lint:
	$(VERILATOR) --lint-only -Wall --top-module rv32i_core -I$(CORE_DIR) $(CORE_RTL)
synth:
	@mkdir -p build
	yosys -Q -p 'read_verilog -I$(CORE_DIR) $(CORE_RTL); synth -top rv32i_core; check; stat' > build/synth.log
	@tail -n 28 build/synth.log
clean:
	python3 sim/clean.py

.PHONY: cursor-test cursor-integration-test
build/mouse_cursor_tb: rtl/soc/periph/vga/mouse_cursor.v sim/unit/mouse_cursor_tb.sv
	@mkdir -p build
	$(IVERILOG) -g2012 -s mouse_cursor_tb -o $@ $^
cursor-test: build/mouse_cursor_tb nvboard-events-test
	$(VVP) build/mouse_cursor_tb
cursor-integration-test: cursor-test pvz-nvboard-build
	python3 sim/verify_mouse_cursor.py

SMB_SRC := sw/mario/vendor/SuperMarioBros-C/source
MARIO_SRC := $(SMB_SRC)/SMB/SMB.cpp $(SMB_SRC)/SMB/SMBData.cpp $(SMB_SRC)/SMB/SMBEngine.cpp \
             $(SMB_SRC)/Emulation/MemoryAccess.cpp $(SMB_SRC)/Emulation/Controller.cpp $(SMB_SRC)/Emulation/PPU.cpp
MARIO_HEADERS := $(wildcard $(SMB_SRC)/SMB/*.hpp $(SMB_SRC)/Emulation/*.hpp sw/mario/include/* sw/mario/include/boost/utility/*.hpp) sw/mario/platform.h sw/mario/timing.h
MARIO_FLAGS := $(CFLAGS_RV) -O2 -std=gnu++17 -nostdinc++ -fno-exceptions -fno-rtti \
               -fno-threadsafe-statics -fno-use-cxa-atexit -DSMB_NO_AUDIO -DSMB_RV32I \
               -Isw/mario/include -Isw/mario -Isw/mario/generated -I$(SMB_SRC) -w
.PHONY: mario-assets mario
mario-assets:
	python3 sw/mario/make_chr.py $(if $(MARIO_ROM),--rom '$(MARIO_ROM)',$(if $(MARIO_ROM_C),--rom-c '$(MARIO_ROM_C)'))
sw/mario/generated/chr_data.h: sw/mario/make_chr.py $(wildcard sw/mario/assets/smb-litenes.chr)
	python3 sw/mario/make_chr.py
build/mario-rv32i/mario.elf: $(MARIO_SRC) $(MARIO_HEADERS) sw/mario/main.cpp sw/mario/runtime.cpp sw/mario/generated/chr_data.h sw/start.S sw/link.ld Makefile
	@mkdir -p build/mario-rv32i
	$(CROSS_COMPILE)g++ $(MARIO_FLAGS) $(LDFLAGS_RV) sw/start.S sw/mario/main.cpp sw/mario/runtime.cpp $(MARIO_SRC) -o $@
	$(CROSS_COMPILE)size $@
	$(OBJDUMP_RV) -d $@ > build/mario-rv32i/mario.dis
mario: build/mario-rv32i/mario.hex build/mario-rv32i/mario.bin

NATIVE_SRC := $(MARIO_SRC) $(SMB_SRC)/Main.cpp $(SMB_SRC)/Configuration.cpp \
              $(SMB_SRC)/Emulation/APU.cpp $(SMB_SRC)/Util/Video.cpp
build/mario-native/mario: $(NATIVE_SRC) $(MARIO_HEADERS) $(wildcard $(SMB_SRC)/*.hpp) Makefile
	@mkdir -p build/mario-native
	$(CXX) -std=c++17 -O2 -Wno-narrowing $(NATIVE_SRC) $(shell sdl2-config --cflags --libs) -o $@
.PHONY: mario-native-build mario-native
mario-native-build: build/mario-native/mario sw/mario/generated/chr_data.h
	python3 sw/mario/prepare_native.py
mario-native: mario-native-build
	cd build/mario-native && ./mario

build/mario_art_test: sim/mario_art_test.cpp $(MARIO_SRC) $(MARIO_HEADERS)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wno-narrowing -DSMB_NO_AUDIO -I$(SMB_SRC) sim/mario_art_test.cpp $(MARIO_SRC) -o $@
.PHONY: mario-art-test
mario-art-test: build/mario_art_test sw/mario/generated/chr_data.h
	python3 sim/art_test.py
	./build/mario_art_test sw/mario/generated/chr.bin

NVBOARD_HOME ?= /home/vero/ysyx2407/ysyx-workbench/nvboard
NVBOARD_LIB := $(NVBOARD_HOME)/build/nvboard.a
NVBOARD_FLAGS := $(shell sdl2-config --cflags) -I$(NVBOARD_HOME)/usr/include
NVBOARD_DISPLAY_LIB := build/nvboard-display/renderer.a
NVBOARD_LIBS := $(abspath $(NVBOARD_DISPLAY_LIB)) $(NVBOARD_LIB) $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf
# GPU is available on request; measurements did not justify changing the default.
NVBOARD_RENDERER ?= software
NVBOARD_PRIVATE_HEADERS := $(wildcard $(NVBOARD_HOME)/include/*.h $(NVBOARD_HOME)/usr/include/*.h)
# Override renderer creation and the input dispatcher in a project-owned archive.
# Link it BEFORE the external library; all other NVBoard objects stay upstream.
build/nvboard-display/nvboard.o: $(NVBOARD_HOME)/src/nvboard.cpp $(NVBOARD_PRIVATE_HEADERS) Makefile
	@mkdir -p build/nvboard-display
	$(CXX) -std=c++17 -O3 -I$(NVBOARD_HOME)/include $(NVBOARD_FLAGS) \
	  -DSDL_CreateRenderer=rv32i_create_renderer -c $< -o $@
build/nvboard-display/renderer.o: sim/nvboard_renderer.cpp Makefile
	@mkdir -p build/nvboard-display
	$(CXX) -std=c++17 -O3 -Wall -Wextra $(shell sdl2-config --cflags) -c $< -o $@
NVBOARD_EVENT_FLAGS := -I$(NVBOARD_HOME)/include $(NVBOARD_FLAGS) -DNVBOARD_EVENT_SOURCE='"$(NVBOARD_HOME)/src/event.cpp"'
build/nvboard-display/events.o: sim/nvboard_events.cpp sim/nvboard_mouse.h sw/PvZ-simplized/mini/pointer.h $(NVBOARD_HOME)/src/event.cpp $(NVBOARD_PRIVATE_HEADERS) Makefile
	@mkdir -p build/nvboard-display
	$(CXX) -std=c++17 -O3 $(NVBOARD_EVENT_FLAGS) -c $< -o $@
$(NVBOARD_DISPLAY_LIB): build/nvboard-display/nvboard.o build/nvboard-display/renderer.o build/nvboard-display/events.o
	$(AR) rcs $@ $^
build/nvboard_events_test: sim/nvboard_events_test.cpp sim/nvboard_events.cpp sim/nvboard_mouse.h sw/PvZ-simplized/mini/pointer.h $(NVBOARD_HOME)/src/event.cpp $(NVBOARD_PRIVATE_HEADERS) Makefile
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Wno-implicit-fallthrough $(NVBOARD_EVENT_FLAGS) $< $(shell sdl2-config --libs) -o $@
.PHONY: nvboard-events-test
nvboard-events-test: build/nvboard_events_test
	./build/nvboard_events_test
# These optimize the host simulator, not the RV32I software or simulated clock.
# No -march=native: the normal build should not require this host's instruction set.
NVBOARD_VERILATOR_OPT ?= -O3
NVBOARD_CXX_OPT ?= -O3 -flto
NVBOARD_LINK_OPT ?= -flto
GAME_FRAMES ?= 300
NVBOARD_ARGS ?=
build/nvboard/pins.cpp: constr/nvboard_top.nxdc
	@mkdir -p build/nvboard
	python3 '$(NVBOARD_HOME)/scripts/auto_pin_bind.py' $< $@
BOARD_LINT_FLAGS := -Wall -Wno-PINCONNECTEMPTY -Wno-BLKSEQ -Wno-UNUSEDSIGNAL
# Verilator's generated link rule does not track libraries passed in -LDFLAGS.
# When this outer rule is triggered (including adapter archive changes), force
# rebuilding the small driver forces a link while still checking/rebuilding
# changed RTL model objects normally (never pretend the model archive is new).
build/nvboard/Vnvboard_top: $(SOC_RTL) rtl/nvboard_top.v sim/nvboard_main.cpp sim/game_input.h sw/mario/timing.h build/nvboard/pins.cpp Makefile $(NVBOARD_LIB) $(NVBOARD_DISPLAY_LIB)
	$(VERILATOR) --cc --exe --build -j 2 --threads 1 $(NVBOARD_VERILATOR_OPT) --top-module nvboard_top $(BOARD_LINT_FLAGS) \
	  -I$(CORE_DIR) --Mdir build/nvboard -MAKEFLAGS 'OPT_FAST=-O3 -W $(abspath sim/nvboard_main.cpp)' -CFLAGS '-std=c++17 $(NVBOARD_CXX_OPT) $(NVBOARD_FLAGS)' \
	  -LDFLAGS '$(NVBOARD_LINK_OPT) $(NVBOARD_LIBS)' $(SOC_RTL) rtl/nvboard_top.v \
	  $(abspath sim/nvboard_main.cpp) $(abspath build/nvboard/pins.cpp)
build/mario_reference: sim/mario_reference.cpp sim/game_input.h $(MARIO_SRC) $(MARIO_HEADERS)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wno-narrowing -DSMB_NO_AUDIO -I$(SMB_SRC) sim/mario_reference.cpp $(MARIO_SRC) -o $@
.PHONY: mario-blink-test
build/mario_blink_test: sim/mario_blink_test.cpp sim/game_input.h $(MARIO_SRC) $(MARIO_HEADERS)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wno-narrowing -DSMB_NO_AUDIO -I$(SMB_SRC) sim/mario_blink_test.cpp $(MARIO_SRC) -o $@
mario-blink-test: build/mario_blink_test
	./build/mario_blink_test
.PHONY: nvboard nvboard-build nvboard-base mario-nvboard mario-nvboard-build mario-test
mario-nvboard-build: mario build/nvboard/Vnvboard_top
nvboard-base: mario-nvboard-build
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER='$(NVBOARD_RENDERER)' ./build/nvboard/Vnvboard_top +rom=build/mario-rv32i/mario.hex $(NVBOARD_ARGS)
.PHONY: nvboard-pgo-build
nvboard-pgo-build: mario-nvboard-build build/mario_reference
	NVBOARD_HOME='$(NVBOARD_HOME)' python3 sim/build_nvboard_pgo.py --if-needed
mario-nvboard: nvboard-pgo-build
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER='$(NVBOARD_RENDERER)' ./build/nvboard-pgo/Vnvboard_top +rom=build/mario-rv32i/mario.hex $(NVBOARD_ARGS)

# Keep each game's ROM, model parameters and simulation driver together.
ifeq ($(NVBOARD_PROGRAM),mario)
nvboard: mario-nvboard
nvboard-build: mario-nvboard-build
else ifeq ($(NVBOARD_PROGRAM),pvz)
nvboard: pvz-nvboard
nvboard-build: pvz-nvboard-build
else ifeq ($(NVBOARD_PROGRAM),tetris)
nvboard: tetris-nvboard
nvboard-build: tetris-nvboard-build
else
nvboard nvboard-build:
	@echo 'Unsupported PROGRAM=$(NVBOARD_PROGRAM); choose mario, pvz or tetris.' >&2
	@exit 2
endif

mario-test: mario-nvboard-build build/mario_reference
	./build/nvboard/Vnvboard_top +rom=build/mario-rv32i/mario.hex --headless --demo --verify \
	  --frames $(GAME_FRAMES) --log build/mario-rtl.log --screenshot build/mario.ppm
	./build/mario_reference sw/mario/generated/chr.bin $(GAME_FRAMES) build/mario-reference.log
	python3 sim/compare_mario.py build/mario-reference.log build/mario-rtl.log

.PHONY: board-lint nvboard-test
board-lint:
	$(VERILATOR) --lint-only --top-module nvboard_top $(BOARD_LINT_FLAGS) -I$(CORE_DIR) $(SOC_RTL) rtl/nvboard_top.v
nvboard-test: mario-nvboard-build
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER=software SDL_VIDEODRIVER=dummy ./build/nvboard/Vnvboard_top \
	  +rom=build/mario-rv32i/mario.hex --ps2-demo --verify --frames 330 --log build/mario-ps2.log --screenshot build/mario-ps2.ppm

.PHONY: nvboard-input-test
nvboard-input-test: nvboard-events-test nvboard-test build/mario_reference
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER=software SDL_VIDEODRIVER=dummy ./build/nvboard/Vnvboard_top \
	  +rom=build/mario-rv32i/mario.hex --ps2-repeat-demo --verify --frames 330 \
	  --log build/mario-ps2-repeat.log --screenshot build/mario-ps2-repeat.ppm
	cmp build/mario-ps2.log build/mario-ps2-repeat.log
	cmp build/mario-ps2.ppm build/mario-ps2-repeat.ppm
	./build/mario_reference sw/mario/generated/chr.bin 330 build/mario-repeat-reference.log
	python3 sim/compare_mario.py build/mario-repeat-reference.log build/mario-ps2-repeat.log

.PHONY: peripheral-test controls-test palette-test
build/palette/Vpalette_tb: rtl/soc/periph/vga/mario_video.v rtl/soc/periph/vga/vga_timing.v sim/unit/palette_tb.sv
	@mkdir -p build/palette
	$(VERILATOR) --binary --timing --build -j 2 --top-module palette_tb --Mdir build/palette $^
palette-test: build/palette/Vpalette_tb
	./build/palette/Vpalette_tb
build/native_controls_test: sim/native_controls_test.cpp $(SMB_SRC)/HostControls.hpp
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -I$(SMB_SRC) $< -o $@
controls-test: build/native_controls_test build/keyboard_tb
	./build/native_controls_test
	$(VVP) build/keyboard_tb
build/keyboard_tb: rtl/soc/periph/ps2_keyboard.v sim/unit/keyboard_tb.sv
	@mkdir -p build
	$(IVERILOG) -g2012 -s keyboard_tb -o $@ $^
build/video/Vmario_video: rtl/soc/periph/vga/mario_video.v rtl/soc/periph/vga/vga_timing.v sim/video_test.cpp
	@mkdir -p build/video
	$(VERILATOR) --cc --exe --build -j 2 --top-module mario_video --Mdir build/video \
	  -MAKEFLAGS 'OPT_FAST=-O3' rtl/soc/periph/vga/mario_video.v rtl/soc/periph/vga/vga_timing.v $(abspath sim/video_test.cpp)
peripheral-test: palette-test build/keyboard_tb build/video/Vmario_video build/mario_reference sw/mario/generated/chr_data.h
	$(VVP) build/keyboard_tb
	@for frame in 30 210 240 300 420; do \
	  ./build/mario_reference sw/mario/generated/chr.bin $$frame build/video-reference.log build/video-fixture.bin && \
	  ./build/video/Vmario_video build/video-fixture.bin || exit $$?; \
	done

include sw/PvZ-simplized/mini/project.mk
include sw/tetris/project.mk
