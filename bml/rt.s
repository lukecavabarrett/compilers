; nasm -felf64 rt.s -o rt.o; gcc -no-pie rt.o -o rt; ./rt
global main
extern malloc, calloc, printf

int_sum_fn:
        mov     rax, qword [rdi+24]
        mov     rdx, qword [rdi+16]
        sar     rax, 1
        sar     rdx, 1
        add     rax, rdx
        lea     rax, [rax+1+rax]
        ret

int_print_fn:
        sub     rsp, 8
        mov     rsi, qword [rdi+16]
        xor     eax, eax
        mov     edi, int_format
        sar     rsi, 1
        call    printf
        xor     eax, eax
        add     rsp, 8
        ret
apply_fn:
        push    r13
        push    r12
        mov     r12, rsi
        push    rbp
        mov     rbp, rdi
        push    rbx
        sub     rsp, 8
        cmp     qword [rdi+8], 1
        jne     .L4
        mov     r13, qword [rdi]
        mov     rbx, rdi
        cmp     r13, 5
        jne     .L5
.L6:
        mov     rbp, qword [rbp+16]
        cmp     qword [rbp+0], 5
        je      .L6
        mov     r13, qword [rbp+8]
        mov     esi, 1
        lea     rdi, [16+r13*8]
        call    calloc
        mov     rdi, rax
        mov     qword [rax], 2
        mov     qword [rax+8], r13
        mov     rax, qword [rbp+8]
        add     rax, 1
        mov     qword [rdi+rax*8], r12
        lea     rdx, [rdi-8+rax*8]
.L8:
        mov     rcx, qword [rbx+24]
        mov     rbx, qword [rbx+16]
        sub     rdx, 8
        mov     qword [rdx+8], rcx
        cmp     qword [rbx], 5
        je      .L8
        mov     r13, qword [rbp+0]
.L10:
        mov     rax, qword [rbp+16]
        cmp     r13, 1
        je      .L16
        add     rsp, 8
        mov     rsi, rbp
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        jmp     rax
.L4:
        mov     edi, 32
        sub     r12, 1
        call    malloc
        mov     qword [rax], 5
        mov     qword [rax+8], r12
        mov     qword [rax+16], rbp
        mov     qword [rax+24], r13
        add     rsp, 8
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        ret
.L5:
        mov     edi, 24
        mov     esi, 1
        call    calloc
        mov     qword [rax], 2
        mov     rdi, rax
        mov     qword [rax+8], 1
        mov     qword [rax+16], r12
        jmp     .L10
.L16:
        add     rsp, 8
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        jmp     rax
main:
        sub     rsp, 8
        mov     esi, 7
        mov     edi, int_sum
        call    apply_fn
        mov     esi, 11
        mov     rdi, rax
        call    apply_fn


        ;mov     rsi, rax
        ;mov     edi, int_print
        ;call    apply_fn




        xor     eax, eax
        add     rsp, 8
        ret

section   .data
int_format:  db  "%lld", 10      ; note the 0 at the end
int_sum: dq 1,2,int_sum_fn
int_print: dq 1,1,int_print_fn

