CC       := clang
CXX      := clang++
AR       := ar

INCLUDES := -I. \
            -Ilib/lvgl \
            -DLV_CONF_INCLUDE_SIMPLE \
            -I/opt/homebrew/include \
            -I/opt/homebrew/include/SDL2 \
            -I/opt/homebrew/opt/freetype/include/freetype2

CFLAGS   := -O2 -w $(INCLUDES)
CXXFLAGS := -std=c++17 -O2 -w $(INCLUDES)
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 \
            -L/opt/homebrew/opt/freetype/lib -lfreetype \
            -lcurl -lpthread

LVGL_SRCS := $(shell find lib/lvgl/src -name '*.c')
LVGL_OBJS := $(patsubst lib/lvgl/src/%.c,build/lvgl/%.o,$(LVGL_SRCS))
LVGL_LIB  := build/liblvgl.a

TARGET    := lvgl_sim
MINI_TARGET := lvgl_sim_320
FW_TARGET := firmware_sim
FW_480_TARGET := firmware_sim_480
HISTORY_V1_TEST := build/daily_history_store_v1_smoke
WEEKLY_STORE_TEST := build/weekly_history_store_smoke
GATEWAY_CONFIG_STATUS_TEST := build/gateway_config_status_smoke
SIMULATOR_GATEWAY_CLI_TEST := build/simulator_gateway_cli_smoke
DAILY_TREND_STORE_TEST := build/daily_trend_store_smoke

FW_LVGL_DIR  := firmware/.pio/libdeps/waveshare-esp32p4-720x720/lvgl
FW_AJ_DIR    := firmware/.pio/libdeps/waveshare-esp32p4-720x720/ArduinoJson/src
FW_INCLUDES  := -Ihost_shim \
                -Ifirmware/src \
                -I$(FW_LVGL_DIR) \
                -I$(FW_AJ_DIR) \
                -DLV_CONF_INCLUDE_SIMPLE \
                -I/opt/homebrew/include \
                -I/opt/homebrew/include/SDL2
FW_CFLAGS    := -O2 -w $(FW_INCLUDES)
FW_CXXFLAGS  := -std=c++17 -O2 -Wformat -Werror=format $(FW_INCLUDES)
FW_LDFLAGS   := -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread

FW_LVGL_SRCS := $(shell find $(FW_LVGL_DIR)/src -name '*.c')
FW_LVGL_OBJS := $(patsubst $(FW_LVGL_DIR)/src/%.c,build/fw_lvgl/%.o,$(FW_LVGL_SRCS))
FW_LVGL_LIB  := build/libfw_lvgl.a

.PHONY: all clean run run-fw

all: $(TARGET) $(FW_TARGET)

$(LVGL_LIB): $(LVGL_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

lib/lvgl/lvgl.h:
	@mkdir -p lib
	git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git lib/lvgl

build/lvgl/%.o: lib/lvgl/src/%.c lv_conf.h | lib/lvgl/lvgl.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): sim_main.cpp firmware/src/usable_eta.hpp $(LVGL_LIB)
	$(CXX) $(CXXFLAGS) sim_main.cpp $(LVGL_LIB) $(LDFLAGS) -o $(TARGET)

$(MINI_TARGET): sim_main.cpp sim_320.hpp firmware/src/usable_eta.hpp $(LVGL_LIB)
	$(CXX) $(CXXFLAGS) -DPOWERWALL_SIM_320 sim_main.cpp $(LVGL_LIB) $(LDFLAGS) -o $(MINI_TARGET)

$(FW_LVGL_LIB): $(FW_LVGL_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

build/fw_lvgl/font/lv_font_montserrat_48.o: $(FW_LVGL_DIR)/src/font/lv_font_montserrat_48.c firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(FW_CFLAGS) -DLV_FONT_MONTSERRAT_48=1 -MMD -MP -c $< -o $@

build/fw_lvgl/%.o: $(FW_LVGL_DIR)/src/%.c firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(FW_CFLAGS) -MMD -MP -c $< -o $@

build/cjk_fonts.o: firmware/src/cjk_fonts.cpp firmware/src/cjk_fonts.hpp firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

build/dashboard_cjk_fallback.o: firmware/src/dashboard_cjk_fallback.cpp firmware/src/cjk_fonts.hpp firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

build/universal_sans_fonts.o: firmware/src/universal_sans_fonts.cpp firmware/src/cjk_fonts.hpp firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

build/stage_3d_assets.o: firmware/src/stage_3d_assets.cpp firmware/src/stage_3d_assets.hpp firmware/src/stage_phase_effects.inc
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

build/firmware_sim.o: firmware_sim.cpp powerflow_checks.hpp weekly_history_sim.hpp weekly_history_checks.hpp $(wildcard host_shim/*.h) $(wildcard firmware/src/*.hpp) firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

$(FW_TARGET): build/firmware_sim.o build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_home_cjk.o build/universal_sans_fonts.o build/stage_3d_assets.o $(FW_LVGL_LIB)
	$(CXX) $^ $(FW_LDFLAGS) -o $@

-include build/firmware_sim.d build/cjk_fonts.d build/dashboard_cjk_fallback.d build/dashboard_home_cjk.d build/universal_sans_fonts.d build/stage_3d_assets.d $(FW_LVGL_OBJS:.o=.d)

$(HISTORY_V1_TEST): firmware/tests/daily_history_store_v1_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/src/daily_history_store.hpp firmware/src/powerwall_client.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -I firmware/tests/stubs -Ihost_shim -Ifirmware/src -I$(FW_AJ_DIR) $< -lcurl -o $@

.PHONY: firmware-history-v1-smoke
firmware-history-v1-smoke: $(HISTORY_V1_TEST)
	python3 firmware/tests/run_daily_history_store_v1_smoke.py $(abspath $(HISTORY_V1_TEST))

$(WEEKLY_STORE_TEST): firmware/tests/weekly_history_store_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/tests/weekly_stubs/HTTPClient.h firmware/src/weekly_history_store.hpp firmware/src/weekly_history_recorder.hpp firmware/src/weekly_history.hpp firmware/src/daily_history_store.hpp firmware/src/powerwall_client.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I firmware/tests/weekly_stubs -I firmware/tests/stubs -Ihost_shim -Ifirmware/src -I$(FW_AJ_DIR) $< -o $@

.PHONY: firmware-weekly-history-smoke
firmware-weekly-history-smoke: $(WEEKLY_STORE_TEST)
	python3 firmware/tests/run_daily_history_store_v1_smoke.py $(abspath $(WEEKLY_STORE_TEST))

$(DAILY_TREND_STORE_TEST): firmware/tests/daily_trend_store_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/src/daily_trend_history.hpp firmware/src/daily_trend_store.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I firmware/tests/stubs -Ifirmware/src $< -o $@

.PHONY: firmware-daily-trend-store-smoke
firmware-daily-trend-store-smoke: $(DAILY_TREND_STORE_TEST)
	./$(DAILY_TREND_STORE_TEST)

$(GATEWAY_CONFIG_STATUS_TEST): firmware/tests/gateway_config_status_smoke.cpp firmware/src/gateway_config_status.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ifirmware/src $< -o $@

.PHONY: firmware-gateway-config-smoke
firmware-gateway-config-smoke: $(GATEWAY_CONFIG_STATUS_TEST)
	./$(GATEWAY_CONFIG_STATUS_TEST)

$(SIMULATOR_GATEWAY_CLI_TEST): firmware/tests/simulator_gateway_cli_smoke.cpp firmware/src/simulator_gateway_cli.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ifirmware/src $< -o $@

.PHONY: firmware-simulator-gateway-cli-smoke
firmware-simulator-gateway-cli-smoke: $(SIMULATOR_GATEWAY_CLI_TEST)
	./$(SIMULATOR_GATEWAY_CLI_TEST)

run-fw: $(FW_TARGET)
	./$(FW_TARGET)

build/console_parity_smoke: firmware/tests/console_parity_smoke.cpp firmware/src/gateway_console.hpp firmware/src/console_preview.hpp firmware/src/history_save_console.hpp firmware/src/powerwall_client.hpp firmware/src/weather_client.hpp firmware/tests/weekly_stubs/HTTPClient.h
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I firmware/tests/weekly_stubs -Ihost_shim -Ifirmware/src -I$(FW_AJ_DIR) $< -o $@

.PHONY: firmware-console-smoke
firmware-console-smoke: build/console_parity_smoke
	./build/console_parity_smoke

.PHONY: firmware-history-console-smoke
firmware-history-console-smoke: $(FW_TARGET) $(FW_480_TARGET)
	python3 firmware/tests/run_history_console_smoke.py

clean:
	rm -rf build $(TARGET) $(MINI_TARGET) $(FW_TARGET) $(FW_480_TARGET) firmware_sim_native480 firmware_sim_native480_en

# Same firmware UI and controls, presented in a 480px host window.
build/firmware_sim_480.o: firmware_sim.cpp powerflow_checks.hpp home_480_checks.hpp weekly_history_sim.hpp weekly_history_checks.hpp $(wildcard host_shim/*.h) $(wildcard firmware/src/*.hpp) $(wildcard firmware/src/*.inc) firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -DPOWERWALL_SIM_WINDOW_SIZE=480 -DPOWERWALL_S3_LATIN_FONT=1 -MMD -MP -c $< -o $@

build/dashboard_cjk_48.o: firmware/src/dashboard_cjk_48.cpp firmware/src/cjk_fonts.hpp firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@

build/s3_latin_48.o: firmware/src/s3_latin_48.cpp firmware/src/cjk_fonts.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -DPOWERWALL_S3_LATIN_FONT=1 -c $< -o $@

$(FW_480_TARGET): build/s3_latin_48.o build/firmware_sim_480.o build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_cjk_48.o build/dashboard_home_cjk.o build/universal_sans_fonts.o build/stage_3d_assets.o $(FW_LVGL_LIB)
	$(CXX) $^ $(FW_LDFLAGS) -o $@

build/dashboard_home_cjk.o: firmware/src/dashboard_home_cjk.cpp firmware/src/cjk_fonts.hpp firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_CXXFLAGS) -MMD -MP -c $< -o $@


-include build/firmware_sim_480.d build/dashboard_home_cjk.d

.PHONY: run-fw-480
run-fw-480: $(FW_480_TARGET)
	./$(FW_480_TARGET)

build/display_animation_smoke: firmware/tests/display_animation_smoke.cpp firmware/src/scaled_panel_flush.hpp firmware/src/powerflow_animation.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -Ifirmware/src $< -o $@

.PHONY: firmware-display-animation-smoke
firmware-display-animation-smoke: build/display_animation_smoke
	./build/display_animation_smoke

# Canonical source fonts -> versioned runtime asset pack (480/S3).
build/export_font_assets: firmware/tools/export_font_assets.cpp firmware/src/font_asset_format.hpp firmware/src/font_asset_names.hpp build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_cjk_48.o build/dashboard_home_cjk.o build/universal_sans_fonts.o $(FW_LVGL_LIB)
	$(CXX) $(FW_CXXFLAGS) $< build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_cjk_48.o build/dashboard_home_cjk.o build/universal_sans_fonts.o $(FW_LVGL_LIB) -o $@

firmware/assets/fonts.pwf: build/export_font_assets
	@mkdir -p $(dir $@)
	./build/export_font_assets $@

.PHONY: firmware-font-assets
firmware-font-assets: firmware/assets/fonts.pwf

$(FW_480_TARGET): | firmware/assets/fonts.pwf

build/font_assets_smoke: firmware/tests/font_assets_smoke.cpp firmware/src/font_assets.hpp firmware/src/font_asset_format.hpp firmware/src/font_asset_names.hpp build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_cjk_48.o build/dashboard_home_cjk.o build/universal_sans_fonts.o $(FW_LVGL_LIB)
	$(CXX) $(FW_CXXFLAGS) $< build/cjk_fonts.o build/dashboard_cjk_fallback.o build/dashboard_cjk_48.o build/dashboard_home_cjk.o build/universal_sans_fonts.o $(FW_LVGL_LIB) -o $@

.PHONY: firmware-font-assets-smoke
firmware-font-assets-smoke: firmware/assets/fonts.pwf build/font_assets_smoke
	./build/font_assets_smoke

# ---------------------------------------------------------------------------
# Native 480x480 firmware simulator (firmware/sim480 + firmware/src/{board_480,ui_480}).
FW_NATIVE480_TARGET := firmware_sim_native480
FW_NATIVE480_EN_TARGET := firmware_sim_native480_en
NATIVE480_LANG ?= zh
ifeq ($(NATIVE480_LANG),zh)
FW_NATIVE480_RUN_TARGET := $(FW_NATIVE480_TARGET)
else ifeq ($(NATIVE480_LANG),en)
FW_NATIVE480_RUN_TARGET := $(FW_NATIVE480_EN_TARGET)
else
$(error NATIVE480_LANG must be zh or en)
endif
FW_NATIVE480_LVGL_DIR := firmware/.pio/libdeps/waveshare-esp32s3-touch-lcd-4b/lvgl
FW_NATIVE480_AJ_DIR := firmware/.pio/libdeps/waveshare-esp32s3-touch-lcd-4b/ArduinoJson/src
FW_NATIVE480_FLAGS := -DPOWERWALL_NATIVE480=1 -Ifirmware/sim480
FW_NATIVE480_INCLUDES := -Ihost_shim -Ifirmware/src -Ifirmware/src/ui_480 \
                         -I$(FW_NATIVE480_LVGL_DIR) -I$(FW_NATIVE480_AJ_DIR) \
                         -DLV_CONF_INCLUDE_SIMPLE -I/opt/homebrew/include \
                         -I/opt/homebrew/include/SDL2 $(FW_NATIVE480_FLAGS)
FW_NATIVE480_CFLAGS := -O2 -w $(FW_NATIVE480_INCLUDES)
FW_NATIVE480_CXXFLAGS := -std=c++17 -O2 -Wformat -Werror=format $(FW_NATIVE480_INCLUDES)
FW_NATIVE480_LDFLAGS := -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread
FW_NATIVE480_LVGL_SRCS := $(shell find $(FW_NATIVE480_LVGL_DIR)/src -name '*.c')
FW_NATIVE480_LVGL_OBJS := $(patsubst $(FW_NATIVE480_LVGL_DIR)/src/%.c,build/fw_lvgl_native480/%.o,$(FW_NATIVE480_LVGL_SRCS))
FW_NATIVE480_LVGL_LIB := build/libfw_lvgl_native480.a

$(FW_NATIVE480_LVGL_LIB): $(FW_NATIVE480_LVGL_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

build/fw_lvgl_native480/%.o: $(FW_NATIVE480_LVGL_DIR)/src/%.c firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(FW_NATIVE480_CFLAGS) -MMD -MP -c $< -o $@

build/firmware_sim_native480.o: firmware/sim480/firmware_sim_native480.cpp Makefile $(wildcard firmware/sim480/checks/*.hpp) $(wildcard firmware/src/board_480/*.hpp) $(wildcard firmware/src/ui_480/*) $(wildcard host_shim/*.h) $(wildcard firmware/src/*.hpp) firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_NATIVE480_CXXFLAGS) -MMD -MP -c $< -o $@

build/firmware_sim_native480_en.o: firmware/sim480/firmware_sim_native480.cpp Makefile $(wildcard firmware/sim480/checks/*.hpp) $(wildcard firmware/src/board_480/*.hpp) $(wildcard firmware/src/ui_480/*) $(wildcard host_shim/*.h) $(wildcard firmware/src/*.hpp) firmware/src/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(FW_NATIVE480_CXXFLAGS) -DPOWERWALL_NATIVE480_LANG_EN=1 -MMD -MP -c $< -o $@

# Native canvas links the 469x293 stage assets.
FW_NATIVE480_STAGE := build/stage_assets_469x293.o
FW_NATIVE480_FONT_ASSETS := build/font_assets_native480.o

build/font_assets_native480.o: firmware/src/font_assets.cpp Makefile firmware/src/font_assets.hpp firmware/src/native480_font_names.hpp firmware/src/native480_fonts.hpp firmware/src/font_asset_format.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(FW_NATIVE480_CXXFLAGS) -MMD -MP -c $< -o $@

build/stage_assets_469x293.o: firmware/src/ui_480/stage_assets_469x293.cpp Makefile firmware/src/ui_480/stage_phase_effects_469x293.inc firmware/src/stage_3d_assets.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(FW_NATIVE480_CXXFLAGS) -c $< -o $@

$(FW_NATIVE480_TARGET): build/firmware_sim_native480.o $(FW_NATIVE480_FONT_ASSETS) $(FW_NATIVE480_STAGE) $(FW_NATIVE480_LVGL_LIB) | firmware/assets/native480/fonts.pwf
	$(CXX) $(filter %.o %.a,$^) $(FW_NATIVE480_LDFLAGS) -o $@

$(FW_NATIVE480_EN_TARGET): build/firmware_sim_native480_en.o $(FW_NATIVE480_FONT_ASSETS) $(FW_NATIVE480_STAGE) $(FW_NATIVE480_LVGL_LIB) | firmware/assets/native480/fonts.pwf
	$(CXX) $(filter %.o %.a,$^) $(FW_NATIVE480_LDFLAGS) -o $@

-include build/firmware_sim_native480.d build/firmware_sim_native480_en.d build/font_assets_native480.d $(FW_NATIVE480_LVGL_OBJS:.o=.d)

.PHONY: native480 run-fw-native480 package-native480 package-native480-zh package-native480-en
native480: $(FW_NATIVE480_RUN_TARGET)
run-fw-native480: $(FW_NATIVE480_RUN_TARGET)
	./$(FW_NATIVE480_RUN_TARGET)

package-native480: package-native480-$(NATIVE480_LANG)
package-native480-zh:
	python3 firmware/tools/package_s3_assets.py --lang zh
package-native480-en:
	python3 firmware/tools/package_s3_assets.py --lang en
