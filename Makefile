CC  := clang
CXX := clang++
AR  := ar

FW_NATIVE480_TARGET := firmware_sim_native480
FW_NATIVE480_EN_TARGET := firmware_sim_native480_en
NATIVE480_LANG ?= zh
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
FW_NATIVE480_LVGL_SRCS := $(shell if test -d "$(FW_NATIVE480_LVGL_DIR)/src"; then find "$(FW_NATIVE480_LVGL_DIR)/src" -name '*.c'; fi)
FW_NATIVE480_LVGL_OBJS := $(patsubst $(FW_NATIVE480_LVGL_DIR)/src/%.c,build/fw_lvgl_native480/%.o,$(FW_NATIVE480_LVGL_SRCS))
FW_NATIVE480_LVGL_LIB := build/libfw_lvgl_native480.a

ifeq ($(NATIVE480_LANG),zh)
FW_NATIVE480_RUN_TARGET := $(FW_NATIVE480_TARGET)
else ifeq ($(NATIVE480_LANG),en)
FW_NATIVE480_RUN_TARGET := $(FW_NATIVE480_EN_TARGET)
else
$(error NATIVE480_LANG must be zh or en)
endif

.PHONY: all help clean native480 run-fw-native480 package-native480 \
        package-native480-zh package-native480-en \
        firmware-native480-smoke firmware-button-navigation-smoke \
        firmware-history-v1-smoke firmware-weekly-history-smoke \
        firmware-daily-trend-store-smoke firmware-gateway-config-smoke \
        firmware-simulator-gateway-cli-smoke firmware-console-smoke
.DEFAULT_GOAL := all

all: help

help:
	@printf '%s\n' \
	  'Native 480 simulator build (macOS, Homebrew SDL2, PlatformIO Core, Python 3):' \
	  '  pio pkg install -d firmware -e waveshare-esp32s3-touch-lcd-4b' \
	  '  make firmware_sim_native480 firmware_sim_native480_en' \
	  '  ./run-zh.sh --demo    # Traditional Chinese' \
	  '  ./run-en.sh --demo    # English' \
	  '' \
	  'For simulator options, run ./run-zh.sh --help or ./run-en.sh --help.'

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

FW_NATIVE480_STAGE := build/stage_assets_469x293.o
FW_NATIVE480_FONT_ASSETS := build/font_assets_native480.o

build/font_assets_native480.o: firmware/src/font_assets.cpp Makefile firmware/src/font_assets.hpp firmware/src/native480_font_names.hpp firmware/src/font_asset_format.hpp
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

native480: $(FW_NATIVE480_RUN_TARGET)

run-fw-native480: $(FW_NATIVE480_RUN_TARGET)
	./$(FW_NATIVE480_RUN_TARGET)

package-native480: package-native480-$(NATIVE480_LANG)

package-native480-zh:
	python3 firmware/tools/package_s3_assets.py --lang zh

package-native480-en:
	python3 firmware/tools/package_s3_assets.py --lang en

firmware-native480-smoke: $(FW_NATIVE480_TARGET) $(FW_NATIVE480_EN_TARGET)
	./$(FW_NATIVE480_TARGET) --demo --smoke-test
	./$(FW_NATIVE480_EN_TARGET) --demo --smoke-test

build/button_navigation_smoke: firmware/tests/button_navigation_smoke.cpp firmware/src/button_navigation.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ifirmware/src $< -o $@

firmware-button-navigation-smoke: build/button_navigation_smoke
	./build/button_navigation_smoke

build/daily_history_store_v1_smoke: firmware/tests/daily_history_store_v1_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/src/daily_history_store.hpp firmware/src/powerwall_client.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -I firmware/tests/stubs -Ihost_shim -Ifirmware/src -I$(FW_NATIVE480_AJ_DIR) $< -lcurl -o $@

firmware-history-v1-smoke: build/daily_history_store_v1_smoke
	python3 firmware/tests/run_daily_history_store_v1_smoke.py $(abspath build/daily_history_store_v1_smoke)

build/weekly_history_store_smoke: firmware/tests/weekly_history_store_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/tests/weekly_stubs/HTTPClient.h firmware/src/weekly_history_store.hpp firmware/src/weekly_history_recorder.hpp firmware/src/weekly_history.hpp firmware/src/daily_history_store.hpp firmware/src/powerwall_client.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I firmware/tests/weekly_stubs -I firmware/tests/stubs -Ihost_shim -Ifirmware/src -I$(FW_NATIVE480_AJ_DIR) $< -o $@

firmware-weekly-history-smoke: build/weekly_history_store_smoke
	python3 firmware/tests/run_daily_history_store_v1_smoke.py $(abspath build/weekly_history_store_smoke)

build/daily_trend_store_smoke: firmware/tests/daily_trend_store_smoke.cpp firmware/tests/stubs/SPIFFS.h firmware/src/daily_trend_history.hpp firmware/src/daily_trend_store.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I firmware/tests/stubs -Ifirmware/src $< -o $@

firmware-daily-trend-store-smoke: build/daily_trend_store_smoke
	./build/daily_trend_store_smoke

build/gateway_config_status_smoke: firmware/tests/gateway_config_status_smoke.cpp firmware/src/gateway_config_status.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ifirmware/src $< -o $@

firmware-gateway-config-smoke: build/gateway_config_status_smoke
	./build/gateway_config_status_smoke

build/simulator_gateway_cli_smoke: firmware/tests/simulator_gateway_cli_smoke.cpp firmware/src/simulator_gateway_cli.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ifirmware/src $< -o $@

firmware-simulator-gateway-cli-smoke: build/simulator_gateway_cli_smoke
	./build/simulator_gateway_cli_smoke

build/console_parity_smoke: firmware/tests/console_parity_smoke.cpp firmware/src/gateway_console.hpp firmware/src/console_preview.hpp firmware/src/history_save_console.hpp firmware/src/powerwall_client.hpp firmware/src/weather_client.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Ihost_shim -Ifirmware/src -I$(FW_NATIVE480_AJ_DIR) $< -o $@

firmware-console-smoke: build/console_parity_smoke
	./build/console_parity_smoke

clean:
	rm -rf build $(FW_NATIVE480_TARGET) $(FW_NATIVE480_EN_TARGET)
