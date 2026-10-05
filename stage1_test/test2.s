    .text
main:
    li s0, 0x20000000
    li t1, 0
    li t2, 20000000
loop:
    sw t2, 0(s0)
    addi t1, t1, 1
    bne t1, t2, loop
    li a7, 10
    ecall