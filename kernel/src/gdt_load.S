.section .text

.global gdt_load

gdt_load:
    lgdt (%rdi)

    pushq $0x08
    leaq .Lreload_cs(%rip), %rax
    pushq %rax
    lretq

.Lreload_cs:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movw $0x18, %ax
    ltr %ax

    // La TSS queda cargada en el registro TR.

    ret


.section .note.GNU-stack,"",@progbits