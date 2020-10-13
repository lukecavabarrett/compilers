; nasm -felf64 rt.s -o rt.o; gcc -no-pie rt.o -o rt; ./rt
global main
extern malloc, calloc, printf

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
int_sum:
        dq 1,2,int_sum_fn

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
int_format:
        db  "%lld",10, 0      ; note the 0 at the end
int_print:
        dq 1,1,int_print_fn

uint_print_fn:
        sub     rsp, 8
        mov     rsi, qword [rdi+24]
        xor     eax, eax
        mov     edi, uint_format
        shr     rsi, 1
        call    printf
        xor     eax, eax
        add     rsp, 8
        ret
uint_format:
        db  "%llu",10, 0      ; note the 0 at the end
uint_print:
        dq 1,1,uint_print_fn


main:
        sub     rsp, 8
        mov     esi, 15
        mov     edi, int_sum
        call    apply_fn
        mov     rsi, -69
        mov     rdi, rax
        call    apply_fn
        mov     edi, int_print
        mov     rsi, rax
        call    apply_fn
        xor     eax, eax
        add     rsp, 8
        ret