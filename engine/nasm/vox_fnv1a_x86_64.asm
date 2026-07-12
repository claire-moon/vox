; SPDX-License-Identifier: GPL-3.0-or-later
; System V AMD64 leaf implementation of the VOX FNV-1a contract probe.

bits 64
default rel

section .text
align 16
global vox_fnv1a32_nasm

vox_fnv1a32_nasm:
    mov eax, 2166136261
    test esi, esi
    jz .done
    xor ecx, ecx

.loop:
    movzx edx, byte [rdi + rcx]
    xor eax, edx
    imul eax, eax, 16777619
    inc ecx
    cmp ecx, esi
    jb .loop

.done:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
