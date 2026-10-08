CC = riscv64-unknown-elf-gcc
CFLAGS = -O2 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin \
         -msmall-data-limit=0 -mno-relax
LDFLAGS = -nostdlib -nostartfiles -Wl,--no-relax -Wl,-Ttext=0x0 -Wl,-e,_start

ref.elf: ref.c tables.h
	$(CC) $(CFLAGS) $(LDFLAGS) ref.c -o ref.elf
	$(subst gcc,size,$(CC)) ref.elf

ref.s: ref.c tables.h
	$(CC) $(CFLAGS) -S ref.c -o ref.s