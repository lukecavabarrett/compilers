;yasm -g dwarf2 -f elf64 simple2.s -l simple2.lst; gcc -no-pie simple2.o -o simple2;
section .data

section .text
global main
extern _Z14print_no_cyclem, malloc



main:
    mov rdi, 5*8
    call malloc
    mov qword [rax], 3
    mov rdi, 0x0000000100000003
    mov qword [rax+8], rdi
    mov qword [rax+16], 1*2+1
    mov qword [rax+24], 2*2+1
    mov qword [rax+32], 3*2+1
    mov rdi, rax
    call _Z14print_no_cyclem
    xor eax, eax
    ret