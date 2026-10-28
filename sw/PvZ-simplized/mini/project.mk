# Independent artifacts; the main Makefile dispatches nvboard PROGRAM=pvz here.
PVZ_DIR := sw/PvZ-simplized
PVZ_MINI := $(PVZ_DIR)/mini
PVZ_ASSETS := $(wildcard $(PVZ_DIR)/assets/images/* $(PVZ_DIR)/assets/reanim/* $(PVZ_DIR)/assets/fonts/*) $(PVZ_DIR)/assets/source-manifest.json
PVZ_HEADERS := $(PVZ_MINI)/game.h $(PVZ_MINI)/demo.h $(PVZ_MINI)/pointer.h $(PVZ_DIR)/generated/assets.h
PVZ_FRAMES ?= 1500
PVZ_CAMPAIGN_FRAMES ?= 3200
.PHONY: pvz-assets pvz-native pvz-rom pvz-nvboard-build pvz-nvboard pvz-unit pvz-test pvz-ps2-test
pvz-assets: $(PVZ_DIR)/generated/assets.h
$(PVZ_DIR)/generated/assets.h: $(PVZ_MINI)/prepare_assets.py $(PVZ_ASSETS)
	python3 $<
build/pvz-simplized-native/pvz: $(PVZ_MINI)/native.cpp $(PVZ_MINI)/game.cpp $(PVZ_MINI)/render.h $(PVZ_HEADERS)
	@mkdir -p build/pvz-simplized-native
	$(CXX) -std=c++17 -O2 -fno-strict-aliasing $(shell sdl2-config --cflags) $(PVZ_MINI)/native.cpp $(PVZ_MINI)/game.cpp $(shell sdl2-config --libs) -o $@
pvz-native: build/pvz-simplized-native/pvz
	./build/pvz-simplized-native/pvz $(NVBOARD_ARGS)
build/pvz-simplized-native/unit: $(PVZ_MINI)/unit.cpp $(PVZ_MINI)/game.cpp $(PVZ_HEADERS)
	@mkdir -p build/pvz-simplized-native
	$(CXX) -std=c++17 -O2 -fno-strict-aliasing $(PVZ_MINI)/unit.cpp $(PVZ_MINI)/game.cpp -o $@
pvz-unit: build/pvz-simplized-native/unit
	./build/pvz-simplized-native/unit
build/pvz-rv32i/pvz.elf: $(PVZ_MINI)/main.cpp $(PVZ_MINI)/game.cpp $(PVZ_HEADERS) sw/mario/runtime.cpp sw/start.S sw/link.ld
	@mkdir -p build/pvz-rv32i
	$(CROSS_COMPILE)g++ $(CFLAGS_RV) -O2 -std=c++17 -fno-strict-aliasing -nostdinc++ -Isw/mario/include \
	  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -ffunction-sections -fdata-sections \
	  $(LDFLAGS_RV) -Wl,--gc-sections sw/start.S $(PVZ_MINI)/main.cpp $(PVZ_MINI)/game.cpp sw/mario/runtime.cpp -o $@
	$(CROSS_COMPILE)size $@
	$(OBJDUMP_RV) -d $@ > build/pvz-rv32i/pvz.dis
build/pvz-rv32i/pvz.hex: build/pvz-rv32i/pvz.elf
	$(OBJCOPY_RV) -O verilog --verilog-data-width=1 $< $@
pvz-rom: build/pvz-rv32i/pvz.hex
build/pvz-nvboard/pins.cpp: constr/nvboard_top.nxdc
	@mkdir -p build/pvz-nvboard
	python3 '$(NVBOARD_HOME)/scripts/auto_pin_bind.py' $< $@
build/pvz-nvboard/Vnvboard_top: $(SOC_RTL) rtl/nvboard_top.v sim/pvz_main.cpp sim/nvboard_mouse.h $(PVZ_HEADERS) build/pvz-nvboard/pins.cpp $(NVBOARD_LIB) $(NVBOARD_DISPLAY_LIB) $(PVZ_MINI)/project.mk
	$(VERILATOR) --cc --exe --build -j 2 --threads 1 -O3 --top-module nvboard_top -GPVZ_MODE=1 \
	  $(BOARD_LINT_FLAGS) -I$(CORE_DIR) --Mdir build/pvz-nvboard -MAKEFLAGS 'OPT_FAST=-O3 -W $(abspath sim/pvz_main.cpp)' \
	  -CFLAGS '-std=c++17 -O3 -flto $(NVBOARD_FLAGS)' -LDFLAGS '-flto $(NVBOARD_LIBS)' \
	  $(SOC_RTL) rtl/nvboard_top.v $(abspath sim/pvz_main.cpp) $(abspath build/pvz-nvboard/pins.cpp)
pvz-nvboard-build: pvz-rom build/pvz-nvboard/Vnvboard_top
pvz-nvboard: pvz-nvboard-build
	NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER='$(NVBOARD_RENDERER)' ./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex $(NVBOARD_ARGS)
pvz-test: pvz-unit pvz-nvboard-build build/pvz-simplized-native/pvz
	./build/pvz-simplized-native/pvz --headless --demo --frames $(PVZ_FRAMES) --log build/pvz-reference.state --screenshot build/pvz-reference.ppm
	./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --headless --demo --fps 0 --verify --frames $(PVZ_FRAMES) --log build/pvz-rtl.state --screenshot build/pvz-rtl.ppm
	python3 sim/verify_pvz.py build/pvz-reference.state build/pvz-rtl.state
	cmp build/pvz-reference.ppm build/pvz-rtl.ppm
pvz-ps2-test: pvz-test
	SDL_VIDEODRIVER=dummy NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER=software ./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --ps2-demo --fps 0 --verify --frames $(PVZ_FRAMES) --log build/pvz-ps2.state --screenshot build/pvz-ps2.ppm
	python3 sim/verify_pvz.py build/pvz-reference.state build/pvz-ps2.state
	cmp build/pvz-reference.ppm build/pvz-ps2.ppm

.PHONY: mouse-test pvz-mouse-test pvz-mouse-sdl-test pvz-ui-test
pvz-ui-test: pvz-unit mouse-test pvz-nvboard-build build/pvz-simplized-native/pvz
	NVBOARD_HOME='$(NVBOARD_HOME)' python3 sim/verify_pvz_ui.py
build/mouse_tb: rtl/soc/periph/mouse_mmio.v sim/unit/mouse_tb.sv
	@mkdir -p build
	$(IVERILOG) -g2012 -s mouse_tb -o $@ $^
mouse-test: build/mouse_tb nvboard-events-test
	$(VVP) build/mouse_tb
pvz-mouse-test: pvz-unit mouse-test pvz-nvboard-build build/pvz-simplized-native/pvz
	./build/pvz-simplized-native/pvz --headless --mouse-demo --frames $(PVZ_FRAMES) --log build/pvz-mouse-reference.state --screenshot build/pvz-mouse-reference.ppm
	./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --headless --mouse-demo --fps 0 --verify --frames $(PVZ_FRAMES) --log build/pvz-mouse-rtl.state --screenshot build/pvz-mouse-rtl.ppm
	python3 sim/verify_pvz.py build/pvz-mouse-reference.state build/pvz-mouse-rtl.state
	cmp build/pvz-mouse-reference.ppm build/pvz-mouse-rtl.ppm
pvz-mouse-sdl-test: pvz-mouse-test
	SDL_VIDEODRIVER=dummy NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER=software ./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --mouse-sdl-demo --fps 0 --verify --frames $(PVZ_FRAMES) --log build/pvz-mouse-sdl.state --screenshot build/pvz-mouse-sdl.ppm
	python3 sim/verify_pvz.py build/pvz-mouse-reference.state build/pvz-mouse-sdl.state
	cmp build/pvz-mouse-reference.ppm build/pvz-mouse-sdl.ppm

.PHONY: pvz-campaign-reference pvz-campaign-test pvz-campaign-sdl-test
.PHONY: pvz-video-test
build/pvz-video/Vpvz_video: rtl/soc/periph/vga/pvz_video.v rtl/soc/periph/vga/vga_timing.v rtl/soc/periph/vga/mouse_cursor.v sim/pvz_video_test.cpp $(PVZ_MINI)/game.cpp $(PVZ_MINI)/render.h $(PVZ_HEADERS)
	$(VERILATOR) --cc --exe --build -j 2 --threads 1 -O3 --top-module pvz_video $(BOARD_LINT_FLAGS) --Mdir build/pvz-video \
	  -CFLAGS '-std=c++17 -O2 -fno-strict-aliasing' rtl/soc/periph/vga/pvz_video.v rtl/soc/periph/vga/vga_timing.v rtl/soc/periph/vga/mouse_cursor.v \
	  $(abspath sim/pvz_video_test.cpp) $(abspath $(PVZ_MINI)/game.cpp)
pvz-video-test: build/pvz-video/Vpvz_video
	@mkdir -p build/pvz-video-tests
	./build/pvz-video/Vpvz_video
pvz-campaign-reference: pvz-unit mouse-test pvz-nvboard-build build/pvz-simplized-native/pvz
	./build/pvz-simplized-native/pvz --headless --campaign-demo --frames $(PVZ_CAMPAIGN_FRAMES) --log build/pvz-campaign-reference.state --screenshot build/pvz-campaign-reference.ppm
pvz-campaign-test: pvz-campaign-reference
	./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --headless --campaign-demo --fps 0 --verify --frames $(PVZ_CAMPAIGN_FRAMES) --log build/pvz-campaign-rtl.state --screenshot build/pvz-campaign-rtl.ppm --perf-log build/pvz-campaign-rtl.csv
	python3 sim/verify_pvz.py build/pvz-campaign-reference.state build/pvz-campaign-rtl.state --campaign
	cmp build/pvz-campaign-reference.ppm build/pvz-campaign-rtl.ppm
pvz-campaign-sdl-test: pvz-campaign-reference
	SDL_VIDEODRIVER=dummy NVBOARD_HOME='$(NVBOARD_HOME)' NVBOARD_RENDERER=software ./build/pvz-nvboard/Vnvboard_top +rom=build/pvz-rv32i/pvz.hex --mouse-sdl-demo --campaign-demo --fps 0 --verify --frames $(PVZ_CAMPAIGN_FRAMES) --log build/pvz-campaign-sdl.state --screenshot build/pvz-campaign-sdl.ppm --perf-log build/pvz-campaign-sdl.csv
	python3 sim/verify_pvz.py build/pvz-campaign-reference.state build/pvz-campaign-sdl.state --campaign
	cmp build/pvz-campaign-reference.ppm build/pvz-campaign-sdl.ppm
