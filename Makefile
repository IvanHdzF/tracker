.PHONY: build rebuild clean flash monitor run misra analyze

BOARD=esp_wrover_kit/esp32/procpu
BUILD_DIR=build

# Include external zephyr modules
ZEPHYR_EXTRA_MODULES?=$(shell realpath ../ble_mgr)


build:
	ZEPHYR_EXTRA_MODULES=$(ZEPHYR_EXTRA_MODULES) \
	west build -d $(BUILD_DIR) -b $(BOARD) .

rebuild:
	ZEPHYR_EXTRA_MODULES=$(ZEPHYR_EXTRA_MODULES) \
	west build -d $(BUILD_DIR) -p always -b $(BOARD)

clean:
	rm -rf $(BUILD_DIR)

flash:
	west flash -d $(BUILD_DIR) --esp-device /dev/ttyACM0

monitor:
	west espressif monitor -p /dev/ttyACM0

run: build flash monitor

analyze:
	west build -p always -b esp_wrover_kit/esp32/procpu .
	CodeChecker analyze build/compile_commands.json \
			--analyzers cppcheck \
			-o build/sca
	CodeChecker parse $(BUILD_DIR)/sca > $(BUILD_DIR)/analysis.txt

misra:
	cppcheck \
		--addon=misra.py \
		--std=c11 \
		--inline-suppr \
		--suppress=missingIncludeSystem \
		src \
		uart_irq \
		sim7000g \
		2> misra.txt
