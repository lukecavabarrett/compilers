//gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -g -O0
//gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt_fast.o -O3
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#define Tag_Tuple  0
#define Tag_Fun 1
#define Tag_Arg 2
#define Make_Tag_Size_D(tag, size, d) ((((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1))

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

uintptr_t int_to_v(int64_t x) {
  return (uintptr_t) ((x << 1) | 1);
}

uint64_t v_to_int(uintptr_t x) {
  return ((int64_t) x) >> 1;
}

#define debug_stream stderr
#define MAX_DEPTH 5
#define VISITED_MAX_SIZE 10000
uintptr_t visited[VISITED_MAX_SIZE];
void __json_debug(uintptr_t x, int depth) {
  if (x & 1) {
    //immediate
    x >>= 1;
    fprintf(debug_stream, "%lu", x);
  } else {
    //block
    const uintptr_t *v = (const uintptr_t *) x;
    for (size_t i = 0; i < depth; ++i)
      if (visited[i] == x) {
        fprintf(debug_stream, "\"<cycle> @ %p\"", v);
        return;
      }

    if (depth > MAX_DEPTH) {
      fprintf(debug_stream, "\"OBJECT_EXCEEDED_MAX_DEPTH @ %p\"", v);
      return;
    }
    visited[depth] = x;

    {
      fprintf(debug_stream, "{ \"address\" : \"%p\" , \"refcount\" : ", v);
      uintptr_t refcount = v[0];
      if (refcount & 1) {
        refcount >>= 1;
        fprintf(debug_stream, "%lu", refcount);
      } else {
        fputs(refcount ? "\"abstract\"" : "\"static\"", debug_stream);
      }
    }
    uint32_t tag = get_tag(v[1]);
    uint32_t size = get_size(v[1]);
    uint8_t d = get_d(v[1]);
    fprintf(debug_stream, ", \"tag\" : ");
    switch (tag) {
      case Tag_Fun: {
        fputs("\"Fun\"", debug_stream);
        assert(size >= 2);
        fprintf(debug_stream, ", \"text_ptr\" : \"%p\", \"n_args\" : %lu ", (const uintptr_t *) v[2], v_to_uint(v[3]));
        v += 4;
        size -= 2;
        if (size) {
          fprintf(debug_stream, ", \"captures\" : [");
          int comma = 0;
          while (size > 0) {
            --size;
            if (comma)fputs(", ", debug_stream);
            comma = 1;
            __json_debug(*v, depth + 1);
            ++v;
          }
          fputs("]", debug_stream);
        }
        assert(size == 0);
      };
        break;
      case Tag_Arg: {
        fputs("\"Arg\"", debug_stream);
        assert(size == 3);
        fprintf(debug_stream, ", \"n_args_left\" : %lu , \"x\" : ", v_to_uint(v[3]));
        __json_debug(v[4], depth + 1);
        fputs(", \"f\" : ", debug_stream);
        __json_debug(v[2], depth + 1);
        v += 5;
        size -= 3;
        assert(size == 0);
      };
        break;
      default: {
        switch (tag) {
          case Tag_Tuple:fputs("\"Tuple\"", debug_stream);
            break;
          default:fprintf(debug_stream, "%u", tag);
            break;
        }
        fprintf(debug_stream, ", \"size\" : %u, \"content\" : [", size);
        int comma = 0;
        v += 2;
        while (size > 0) {
          --size;
          if (comma)fputs(", ", debug_stream);
          comma = 1;
          __json_debug(*v, depth + 1);
          ++v;
        }
        fputs("]", debug_stream);
      }
        assert(size == 0);
    }
    assert(size == 0);
    if (d) {
      fputs(", \"destructor\" :", debug_stream);
      __json_debug(*v, depth + 1);
    }
    fputs("}", debug_stream);
    return;
  }
}
void json_debug(uintptr_t x) {
  return __json_debug(x, 0);
}
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
#ifdef DEBUG_JSON
  static int indent = 0;
  for(int i=0;i<indent;++i) fputs("  ", debug_stream);
  fputs("calling = ", debug_stream);
  json_debug((uintptr_t) n);
  fputs("\n", debug_stream);
  ++indent;
#endif
  uintptr_t result = ((text_ptr) fb[2])((uintptr_t) n);
#ifdef DEBUG_JSON
  --indent;
  for(int i=0;i<indent;++i) fputs("  ", debug_stream);
  fputs("returned ", debug_stream);
  json_debug(result);
  fputs("\n", debug_stream);
#endif
  return result;

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
#ifdef DEBUG_JSON
  //fprintf(debug_stream, "decrementing [%p] to %lu\n", xb, (*xb) >> 1);
#endif
#ifdef DEBUG_LOG
  fprintf(stderr,"decrement block 0x%016" PRIxPTR " to %lu\n", x, v_to_uint(*xb));
#endif
  if (*xb != 1)return;
  destroy_nontrivial(x);
}

uintptr_t increment_value(uintptr_t x) {
  if (x & 1)return x;
  uintptr_t *xb = (uintptr_t *) x;
  if (*xb == 0)return x;
  (*xb) += 2;
#ifdef DEBUG_JSON
  //fprintf(debug_stream, "incrementing [%p] to %lu\n", xb, (*xb) >> 1);
#endif
#ifdef DEBUG_LOG
  fprintf(stderr,"increment block 0x%016" PRIxPTR " to %lu\n", x, v_to_uint(*xb));
#endif
  return x;
}

void destroy_nontrivial(uintptr_t x_v) {
  uintptr_t *x = (uintptr_t *) x_v;
#ifdef DEBUG_JSON
  //fprintf(debug_stream, "destroying [%p] = ", x);
  //json_debug(x_v);
  //fprintf(debug_stream, "\n");
#endif

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
    fprintf(stderr,"destroying block of size %u at 0x%016" PRIxPTR "\n", size, x_v);
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

uintptr_t match_failed_fun(uintptr_t unit) {
  fputs("match failed\n", stderr);
  exit(1);
  return uint_to_v(0);
}

uintptr_t int_sum_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return int_to_v(a + b);
}

uintptr_t int_sub_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return int_to_v(a - b);
}

uintptr_t int_mul_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return int_to_v(a * b);
}

uintptr_t int_div_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return int_to_v(a * b);
}

uintptr_t int_eq_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  uint64_t b = v_to_uint(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  uint64_t a = v_to_uint(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return uint_to_v(a == b ? 1 : 0);
}

uintptr_t int_le_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return uint_to_v(a < b ? 1 : 0);
}

uintptr_t int_leq_fun(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  uintptr_t *argv_a = (uintptr_t *) argv_b[2];
  increment_value((uintptr_t) argv_a);
  decrement_value((uintptr_t) argv_b);
  int64_t a = v_to_int(argv_a[4]);
  decrement_value((uintptr_t) argv_a);
  return uint_to_v(a <= b ? 1 : 0);
}

uintptr_t int_negated(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  decrement_boxed(argv);
  return int_to_v(-b);
}

uintptr_t println_int(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  uint64_t b = v_to_uint(argv_b[4]);
  printf("%ld\n", b);
  decrement_boxed(argv);
  return uint_to_v(0);
}

uintptr_t print_int(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  printf("%ld ", b);
  fflush(stdout);
  decrement_boxed(argv);
  return uint_to_v(0);
}

uintptr_t scan_int(uintptr_t argv) {
  decrement_boxed(argv);
  int64_t x;
  scanf("%ld", &x);
  return int_to_v(x);
}

uintptr_t println_int_err(uintptr_t argv) {
  uintptr_t *argv_b = (uintptr_t *) argv;
  int64_t b = v_to_int(argv_b[4]);
  fprintf(stderr, "%ld\n", b);
  decrement_boxed(argv);
  return uint_to_v(0);
}

#define Make_Tag_Size_d(tag, size, d)  ((((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1))
#define Uint_to_v(x) ((uintptr_t)((x<<1)|1))
#define V_to_uint(x) ((uint64_t)(x>>1))
