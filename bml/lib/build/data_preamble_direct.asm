section .data

int_sum dq 1,2,int_sum_fn
int_sub dq 1,2,int_sub_fn
int_format db  "%lld",32, 0
intln_format db  "%lld",10, 0
print_int dq 1,1,int_print_fn
println_int dq 1,1,int_println_fn
int_eq dq 1,2,int_eq_fn
int_le dq 1,2,int_le_fn
int_negate dq 1,1,int_negate_fn
mf_format db "match failed",10,0
