section .data

section .text
global main
extern println_int_err_skim

test_function:
sub rsp, 8
call println_int_err_skim
add rsp, 8
mov rdi, rax
jmp println_int_err_skim

main:
sub rsp, 8
call println_int_err_skim
add rsp, 8
;push rdi ; pushing rdi
push rsi
mov rdi, 3093
call test_function
pop rsi
;pop     rdi ; popping rdi
xor     eax, eax
ret
