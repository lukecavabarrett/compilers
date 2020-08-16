# Read one int and computes the fibonacci sequence
scan_int %rax
push_add %rip, $3
push %rax # push argument
jmp fibonacci
pop %rax
print_int %rax
exit %rax

fibonacci: # stack: ret | n |  __  <- %rsp
    #print stack depth
    mov %rax, %rsp
    sub %rax, $0x100000000
    print_int %rax

    pop %rbx
    jmple %rbx, $1, fib_end # if n<=1 return it

    sub %rbx, $1
    push %rbx # save n-1
    push_add %rip, $3 #load return
    push %rbx #load arg
    jmp fibonacci

    # now the stack looks like: ret | n-1 | f(n-1)

    pop %rax
    pop %rbx
    sub %rbx, $1
    push %rax
    push_add %rip, $3
    push %rbx
    jmp fibonacci

    # now it's: ret | f(n-1) | f(n-2)

    pop %rbx
    pop %rax
    add %rbx, %rax

    fib_end: # we have answer in rbx
    pop %rax # extract the source
    push %rbx # push the result
    jmp %rax # jump back to it

nop
nop
nop
nop
nop

exit $1