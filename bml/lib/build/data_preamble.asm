section .data

int_sum dq 1,2,int_sum_fn
int_sub dq 1,2,int_sub_fn
int_format db  "%lld",10, 0
int_print dq 1,1,int_print_fn
int_eq dq 1,2,int_eq_fn
mf_format db "match failed",10,0
