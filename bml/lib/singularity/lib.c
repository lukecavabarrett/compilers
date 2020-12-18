#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>


uintptr_t println_int_err_skim(uintptr_t x) {
  fprintf(stderr,"%lu\n",x);
  return x;
}