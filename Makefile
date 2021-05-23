BUILD = build
SRC = src

K210-SERIALPORT = /dev/ttyUSB0
K210-BURNER = platform/k210/kflash.py
BOOTLOADER = platform/k210/rustsbi-k210.bin
K210_BOOTLOADER_SIZE = 131072

KERNEL_BIN = k210.bin
KERNEL_O = $(BUILD)/kernel.o

GCC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy

SRC_ALL = $(wildcard src/asm/*.s src/lib/*.h src/lib/*.c)
SRC_DRIVER = $(wildcard src/driver/*.h src/driver/*.c)

dst = /mnt/sd
sd = /dev/sda

# 在当前目录生成k210.bin
all:
	# gen build/
	@test -d $(BUILD) || mkdir -p $(BUILD)
	$(GCC) -o $(KERNEL_O) -w -g -mcmodel=medany -T src/linker.ld -O2 -ffreestanding -nostdlib\
                                    $(SRC_ALL) \
                                    $(SRC_DRIVER) \
                                    src/main.c
	$(OBJCOPY) $(KERNEL_O) --strip-all -O binary $(KERNEL_BIN)
	@cp $(BOOTLOADER) $(BOOTLOADER).copy
	@dd if=$(KERNEL_BIN) of=$(BOOTLOADER).copy bs=$(K210_BOOTLOADER_SIZE) seek=1
	@mv $(BOOTLOADER).copy $(KERNEL_BIN)

# 编译运行
run: all up see

# 烧录到板子
up:
	@sudo chmod 777 $(K210-SERIALPORT)
	python3 $(K210-BURNER) -p $(K210-SERIALPORT) -b 1500000 $(KERNEL_BIN)

# 通过串口查看
see:
	python3 -m serial.tools.miniterm --eol LF --dtr 0 --rts 0 --filter direct $(K210-SERIALPORT) 115200


sd:
	@if [ ! -f "fs.img" ]; then \
		echo "making fs image..."; \
		dd if=/dev/zero of=fs.img bs=512k count=512; \
		mkfs.vfat -F 32 fs.img; fi
	@sudo mount fs.img $(dst)
	@sudo test -d $(dst) || mkdir -p $(dst)
	@sudo cp -r test_suites/* $(dst)
	@sudo umount $(dst)

flash:
	@sudo dd if=fs.img of=$(sd);
