#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>


#define Tag_Tuple  0
#define Tag_Fun 1
#define Tag_Arg 2

uint64_t make_tag_size_d(uint32_t tag, uint32_t size, uint8_t d) {
  return (((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1);
}

uint32_t get_tag(uintptr_t tag_size_d) {
  return tag_size_d >> 32;
}

uint32_t get_size(uintptr_t tag_size_d) {
  return ((uint32_t) tag_size_d) >> 1;
}

uint8_t get_d(uintptr_t tag_size_d) {
  return tag_size_d & 1;
}

uintptr_t uint_to_v(uint64_t x) {
  return (uintptr_t) ((x << 1) | 1);
}

uint64_t v_to_uint(uintptr_t x) {
  return ((uint64_t) x) >> 1;
}

//TODO:  int_to_v, uint_to_v
#define debug_stream stderr
void print_debug(uintptr_t x) {
  if (x & 1) {
    //immediate
    x >>= 1;
    fprintf(debug_stream, "[%lu]", x);
  } else {
    //block

    uintptr_t *v = (uintptr_t *) x;
    {
      fputs("{rc:", debug_stream);
      uintptr_t refcount = v[0];
      if (refcount & 1) {
        refcount >>= 1;
        fprintf(debug_stream, "%lu", refcount);
      } else {
        fputs(refcount ? "abstr" : "static", debug_stream);
      }
    }
    uint32_t tag = get_tag(v[1]);
    uint32_t size = get_size(v[1]);
    uint8_t d = get_d(v[1]);
    fprintf(debug_stream, ", tag:%u, size:%u | ", tag, size);
    v += 2;
    if (tag == Tag_Fun) {
      --size;
      v += 1;
      fputs("[text_ptr]", debug_stream);
    }
    while (size--) {
      print_debug(*v);
      ++v;
    }
    if (d) {
      fputs("[destructor]", debug_stream);
    }
    fputs("}", debug_stream);
  }
}
void println_debug(uintptr_t x) {
  print_debug(x);
  putc(10, debug_stream);
}

typedef uintptr_t (*text_ptr)(uintptr_t);

uintptr_t apply_fn(uintptr_t f, uintptr_t x) {
  const uintptr_t *fb = (uintptr_t *) f;
  uintptr_t *n = (uintptr_t *) malloc(8 * 5);
  n[0] = uint_to_v(1);
  n[1] = (uintptr_t) make_tag_size_d(Tag_Arg, 3, 0);
  n[2] = f;
  n[3] = uint_to_v(v_to_uint(fb[3]) - 1);
  n[4] = x;
  if (v_to_uint(fb[3]) != 1)return (uintptr_t) n;
  while (get_tag(fb[1]) != Tag_Fun) {
    fb = (uintptr_t *) fb[2];
  }
  return ((text_ptr) fb[2])((uintptr_t) n);

}

void decrement_value(uintptr_t x);//partially_inlined
void decrement_trivial(uintptr_t x) {}//inlined
void decrement_boxed(uintptr_t x);//partially inlined
void decrement_nonglobal(uintptr_t x);//partially inlined
void decrement_unboxed(uintptr_t x) {}//inlined
void decrement_global(uintptr_t x) {}//inlined
void decrement_nontrivial(uintptr_t x);//partially inlined
void destroy_nontrivial(uintptr_t x);

void decrement_value(uintptr_t x) { //partially inlined
  if (x & 1)return; // rule out unboxed
  decrement_boxed(x);
}

void decrement_boxed(uintptr_t x) { //partially inlined
  assert((x & 0) == 0);
  uintptr_t *xb = (uintptr_t *) x;
  if (*xb == 0)return; // rule out global
  decrement_nontrivial(x);
}

void decrement_nonglobal(uintptr_t x) { //partially inlined
  if (x & 1)return; // rule out unboxed
  assert(*(uintptr_t *) x);
  decrement_nontrivial(x);
}

void decrement_nontrivial(uintptr_t x) {
  assert((x & 0) == 0);
  uintptr_t *xb = (uintptr_t *) x;
  assert(*xb);
  (*xb) -= 2;
#ifdef DEBUG_LOG
  printf("decrement block 0x%016" PRIxPTR " to %lu\n", x, v_to_uint(*xb)); // why printf is fine while fprintf is not?
#endif
  if (*xb != 1)return;
  destroy_nontrivial(x);
}

uintptr_t increment_value(uintptr_t x) {
  if (x & 1)return x;
  uintptr_t *xb = (uintptr_t *) x;
  if (*xb == 0)return x;
  (*xb) += 2;
#ifdef DEBUG_LOG
  printf("increment block 0x%016" PRIxPTR " to %lu\n", x, v_to_uint(*xb)); // why printf is fine while fprintf is not?
  fprintf(stderr,"increment block 0x%016" PRIxPTR " to %lu\n", x, v_to_uint(*xb)); // why printf is fine while fprintf is not?
#endif
  return x;
}

void destroy_nontrivial(uintptr_t x_v) {
  uintptr_t *x = (uintptr_t *) x_v;
  //get size, tag, d.
  //assert(x[0] == 1);
  uint32_t tag = get_tag(x[1]);
  uint32_t size = get_size(x[1]);
  uint8_t d = get_d(x[1]);
  if (d) {
    x[0] = 3; //refcount:=1
    x[1] ^= 1; // d:=0
    uintptr_t f = x[size + 2];
    uintptr_t y = apply_fn(f, (uintptr_t) x);
    decrement_value(y);
    return;
  } else {
#ifdef DEBUG_LOG
    printf("destroying block of size %u at 0x%016" PRIxPTR "\n", size, x_v);
    //println_debug(x_v);
#endif
    const uintptr_t *xloop = x;
    xloop += 2;
    if (tag == Tag_Fun) {
      ++xloop;
      --size;
    }
    while (size--) {
      decrement_value(*xloop);
      ++xloop;
    }
    free(x);
  }
}

uintptr_t sum_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  uint64_t b = v_to_uint(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  uint64_t a = v_to_uint(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return uint_to_v(a + b);
}

uintptr_t println_int(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  uint64_t b = v_to_uint(argv_b[4]);
  printf("%ld\n",b);
  decrement_boxed(argv);
  return uint_to_v(0);
}

uintptr_t println_int_err(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  uint64_t b = v_to_uint(argv_b[4]);
  fprintf(stderr,"%ld\n",b);
  decrement_boxed(argv);
  return uint_to_v(0);
}

uintptr_t println_int_err_skim(uintptr_t x) {
  fprintf(stderr,"%lu\n",x);
  return x;
}

#define Make_Tag_Size_d(tag, size, d)  ((((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1))
#define Uint_to_v(x) ((uintptr_t)((x<<1)|1))
#define V_to_uint(x) ((uint64_t)(x>>1))
//const uintptr_t sum_block[4] = {0, Make_Tag_Size_d(Tag_Fun, 2, 0), (uintptr_t) &sum_fun, Uint_to_v(2)};

/*TEST*/
/*
int main() {

  println_debug(25 * 2 + 1);
  putchar(10);
  uintptr_t *tuple = (uintptr_t *) malloc(8 * 7);
  tuple[0] = 3;
  tuple[1] = make_tag_size_d(Tag_Tuple, 5,0);
  tuple[2] = uint_to_v(1);
  tuple[3] = uint_to_v(2);
  tuple[4] = uint_to_v(3);
  tuple[5] = uint_to_v(4);
  tuple[6] = uint_to_v(5);
  println_debug((uintptr_t) tuple);
  fputs("decrementing tuple\n",stderr);
  decrement((uintptr_t)tuple);

  uintptr_t *fun_blk = (uintptr_t *) malloc(8 * 4);
  fun_blk[0] = 3;
  fun_blk[1] = make_tag_size_d(Tag_Fun, 2,0);
  fun_blk[2] = (uintptr_t) &sum_fun;
  fun_blk[3] = uint_to_v(2);


  println_debug((uintptr_t) fun_blk);

  uintptr_t part_app = apply_fn(fun_blk, uint_to_v(42));//fun_blk moved out

  println_debug(part_app);

  increment_value(part_app);
  uintptr_t r1 = apply_fn(part_app, uint_to_v(1729));

  println_debug(r1);

  uintptr_t r2 = apply_fn(part_app, uint_to_v(1000));

  println_debug(r2);

}
*/