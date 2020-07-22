# Read one int and computes the fibonacci sequence

scan_int %rax # number of iterations
mov %rbx, $0
mov %rcx, $1
loop:  # while true
jmpz %rax, end #if finished jump to end
    print_int %rcx
    add %rdx, %rcx, %rbx
    mov %rbx, %rcx
    mov %rcx, %rdx
    sub %rax, $1
    jmp loop
end:
exit $0


