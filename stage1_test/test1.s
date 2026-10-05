	.text
main:
	li s0, 0x20000000
	li s1, 0x7FFFFF
	li s2, 8388608
	li t0, 0
	li t2, -1
loop:
	and t1, t0, s1
	add t1, t1, s0
	sw t2, 0(t1)
	addi t0, t0, 4
	blt t0, s2, loop
	li a7, 10
	ecall
