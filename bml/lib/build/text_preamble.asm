section .text
global main
extern printf, malloc, exit

fail_match:
        mov rax, 1 ; system code for write
        mov rsi, mf_format ; addr
        mov rdi, 2 ; stderr
        mov rdx, 13; count to write
        syscall ; system call
        mov     edi, 1
        call    exit

apply_fn:
        push    rbp
        mov     rbp, rdi
        mov     edi, 32
        push    rbx
        mov     rbx, rsi
        sub     rsp, 8
        call    malloc
        mov     rcx, qword [rbp+8]
        mov     qword [rax], 5
        lea     rdx, [rcx-1]
        mov     qword [rax+16], rbp
        mov     qword [rax+8], rdx
        mov     qword [rax+24], rbx
        test    rdx, rdx
        je      .L18
        add     rsp, 8
        pop     rbx
        pop     rbp
        ret
.L7:
        mov     rbp, qword [rbp+16]
.L18:
        mov     rdx, qword [rbp+0]
        cmp     rdx, 5
        je      .L7
        mov     rcx, qword [rbp+16]
        cmp     rdx, 1
        je      .L19
        add     rsp, 8
        mov     rsi, rbp
        mov     rdi, rax
        pop     rbx
        pop     rbp
        jmp     rcx
.L19:
        add     rsp, 8
        mov     rdi, rax
        pop     rbx
        pop     rbp
        jmp     rcx
int_sum_fn:
        mov     rax, qword [rdi+16]
        mov     rdx, qword [rdi+24]
        mov     rax, qword [rax+24]
        sar     rdx, 1
        sar     rax, 1
        add     rax, rdx
        lea     rax, [rax+1+rax]
        ret

int_sub_fn:
        mov     rax, qword [rdi+16]
        mov     rdx, qword [rdi+24]
        mov     rax, qword [rax+24]
        sar     rdx, 1
        sar     rax, 1
        sub     rax, rdx
        lea     rax, [rax+1+rax]
        ret

int_eq_fn:
        mov     rax, qword [rdi+16]
        mov     rdx, qword [rdi+24]
        mov     rax, qword [rax+24]
        sar     rdx, 1
        sar     rax, 1
        cmp     rax, rdx
        jnz .unequal
        mov rax, 3
        ret
.unequal:
        mov     rax, 1
        ret

int_print_fn:
        sub     rsp, 8
        mov     rsi, qword [rdi+24]
        xor     eax, eax
        mov     edi, int_format
        sar     rsi, 1
        call    printf
        xor     eax, eax
        add     rsp, 8
        ret
