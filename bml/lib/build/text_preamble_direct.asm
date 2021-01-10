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
        mov     rbp, rsi
        push    rbx
        mov     rbx, rdi
        mov     edi, 32
        sub     rsp, 8
        call    malloc
        mov     rcx, qword [rbx+8]
        mov     qword [rax], 5
        lea     rdx, [rcx-1]
        mov     qword [rax+16], rbx
        mov     qword [rax+8], rdx
        mov     qword [rax+24], rbp
        test    rdx, rdx
        je      .L13
        add     rsp, 8
        pop     rbx
        pop     rbp
        ret
.L9:
        mov     rbx, qword [rbx+16]
.L13:
        cmp     qword [rbx], 5
        je      .L9
        mov     rdx, qword [rbx+16]
        add     rsp, 8
        mov     rdi, rax
        pop     rbx
        pop     rbp
        jmp     rdx

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
        cmp     rax, rdx
        jnz .ret_false
        mov rax, 3
        ret
.ret_false:
        mov     rax, 1
        ret

int_le_fn:
        mov     rsi, qword [rdi+16]
        mov     rdi, qword [rdi+24]
        mov     rsi, qword [rsi+24]
        sar     rdi, 1
        sar     rsi, 1
        xor     eax,eax
        cmp    rsi, rdi
        setl    al
        lea     rax, qword [rax+1+rax]
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

int_println_fn:
        sub     rsp, 8
        mov     rsi, qword [rdi+24]
        xor     eax, eax
        mov     edi, intln_format
        sar     rsi, 1
        call    printf
        xor     eax, eax
        add     rsp, 8
        ret
