@ Minimal GBA startup code: cartridge header, RAM setup, jump to main.
    .section .crt0, "ax"
    .arm
    .global _start
_start:
    b       rom_start

    @ Nintendo logo area (156 bytes). Emulators do not check it.
    .fill   156, 1, 0

    .ascii  "ROOTFLOWTOY "      @ game title, 12 bytes
    .ascii  "CRFT"              @ game code
    .ascii  "01"                @ maker code
    .byte   0x96                @ fixed value
    .byte   0x00                @ unit code
    .byte   0x00                @ device type
    .fill   7, 1, 0             @ reserved
    .byte   0x00                @ version
    .byte   0x00                @ header checksum (patched after build)
    .fill   2, 1, 0             @ reserved

rom_start:
    @ IRQ mode stack
    mov     r0, #0x12
    msr     cpsr_c, r0
    ldr     sp, =0x03007FA0
    @ System mode stack
    mov     r0, #0x1F
    msr     cpsr_c, r0
    ldr     sp, =0x03007F00

    @ Copy .data from ROM to IWRAM
    ldr     r0, =__data_lma
    ldr     r1, =__data_start
    ldr     r2, =__data_end
1:  cmp     r1, r2
    ldrlt   r3, [r0], #4
    strlt   r3, [r1], #4
    blt     1b

    @ Zero .bss
    ldr     r1, =__bss_start
    ldr     r2, =__bss_end
    mov     r3, #0
2:  cmp     r1, r2
    strlt   r3, [r1], #4
    blt     2b

    ldr     r0, =main
    bx      r0
3:  b       3b
    .pool
