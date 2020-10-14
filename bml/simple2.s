;yasm -g dwarf2 -f elf64 simple2.s -l simple2.lst; gcc -no-pie simple2.o -o simple2;
section .data
answer dq 0
another_answer dq 0
int_format db "%lld",10,0

section .text
global main
extern printf
main:
    mov rax, 85
    mov qword [answer], rax
    mov rax, qword [another_answer]

.LC19
    jmp .LC19
    mov qword [another_answer], rax

mov     edi, int_format
mov     rsi, 43
call    printf

xor eax, eax
ret
