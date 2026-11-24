.syntax unified
.arch armv7-a
.text
.global _start
_start:
    /* sync() = syscall 36 */
    mov r7, #36
    svc #0

    /* argc in [sp], argv[0] in [sp,#4], argv[1] in [sp,#8] */
    ldr r0, [sp]
    cmp r0, #1
    ldrgt r3, [sp, #8]
    adrle r3, default_arg

    /* reboot(0xfee1dead, 672274793, 0xa1b2c3d4, arg) = syscall 88 */
    movw r0, #0xdead
    movt r0, #0xfee1
    movw r1, #0x1997
    movt r1, #0x2812
    movw r2, #0xc3d4
    movt r2, #0xa1b2
    mov r7, #88
    svc #0

    mov r0, #0
    mov r7, #1
    svc #0

.align 2
default_arg:
    .asciz "recovery"
