.section .text

.extern idt_exception_report
.extern irq_handler

// Excepción sin código de error.
.macro ISR_NOERR num
.global isr\num
.type isr\num, @function

isr\num:
    pushq $0
    pushq $\num
    jmp isr_common

.size isr\num, .-isr\num
.endm

// Excepción con código de error proporcionado por la CPU.
.macro ISR_ERR num
.global isr\num
.type isr\num, @function

isr\num:
    pushq $\num
    jmp isr_common

.size isr\num, .-isr\num
.endm

// Excepciones sin código de error.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7

// Excepciones con código de error.
ISR_ERR 8

// Excepciones sin código de error.
ISR_NOERR 9

// Excepciones con código de error.
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14

// Excepciones sin código de error.
ISR_NOERR 15
ISR_NOERR 16

// Excepciones con código de error.
ISR_ERR 17

// Excepciones sin código de error.
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20

// Control Protection.
ISR_ERR 21

// Excepciones sin código de error.
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28

// VMM Communication.
ISR_ERR 29

// Security Exception.
ISR_ERR 30

// Reservada.
ISR_NOERR 31


// Handler predeterminado para las demás entradas de la IDT.
.global idt_default_handler
.type idt_default_handler, @function

idt_default_handler:
    pushq $0
    pushq $255
    jmp isr_common

.size idt_default_handler, .-idt_default_handler


// Manejador común de excepciones.
.type isr_common, @function

isr_common:
    cli
    cld

    // Guardamos todos los registros generales.
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi

    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // Primer argumento de C: puntero al frame.
    movq %rsp, %rdi

    // Conservamos la dirección original de la pila.
    movq %rsp, %rbx

    // Alineamos la pila según la ABI de x86-64.
    andq $-16, %rsp

    // Llamamos al manejador escrito en C.
    call idt_exception_report

    // El manejador no debería retornar.
    movq %rbx, %rsp

1:
    cli
    hlt
    jmp 1b

.size isr_common, .-isr_common


// ===== Interrupciones externas (IRQ 0-15 -> vectores 32-47) =====

// Cada IRQ empuja un código de error falso (0) y su número de vector,
// para que el frame tenga la misma forma que el de las excepciones.
.macro IRQ_STUB num, vec
.global irq\num
.type irq\num, @function

irq\num:
    pushq $0
    pushq $\vec
    jmp irq_common

.size irq\num, .-irq\num
.endm

IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47


// Manejador comun de IRQ. A diferencia de isr_common, este SI vuelve
// al codigo interrumpido con iretq.
.type irq_common, @function

irq_common:
    cld

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi

    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // Primer argumento de C: puntero al frame.
    movq %rsp, %rdi

    // Conservamos la pila original y la alineamos a 16 bytes.
    movq %rsp, %rbx
    andq $-16, %rsp

    call irq_handler

    movq %rbx, %rsp

    // Restauramos los registros en orden inverso.
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8

    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    // Descartamos el vector y el codigo de error falso.
    addq $16, %rsp

    iretq

.size irq_common, .-irq_common

.section .note.GNU-stack,"",@progbits
