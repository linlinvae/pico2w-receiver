# Windows convenience wrapper. tools/build.ps1 is the canonical build.
CHIP ?= CH582
BOARD ?= dev
LOG_LEVEL ?= 3
PS ?= powershell

.PHONY: all test setup info
all:
	$(PS) -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Chip $(CHIP) -Board $(BOARD) -LogLevel $(LOG_LEVEL)
test:
	$(PS) -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1 -Chip $(CHIP)
setup:
	$(PS) -NoProfile -ExecutionPolicy Bypass -File tools/setup.ps1 -Chip $(CHIP)
info:
	@echo CHIP=$(CHIP) BOARD=$(BOARD) LOG_LEVEL=$(LOG_LEVEL)
