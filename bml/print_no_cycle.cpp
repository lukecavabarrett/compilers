#include <cstdlib>
#include <cstdint>
#include <cstdio>
static constexpr uint32_t FUN_TAG = 1;
void print_no_cycle(uintptr_t x){
  static_assert(sizeof(uintptr_t)==8);
  if(x&1){
    //immediate
    x>>=1;
    printf("[%llu]",x);
  } else {
    //block

    uintptr_t* v = reinterpret_cast<uintptr_t*>(x);
    {
      fputs("{rc:",stdout);
      uintptr_t refcount = v[0];
      if(refcount&1){
        refcount>>=1;
        printf("%llu",refcount);
      } else {
        fputs("abstr",stdout);
      }
    }
    uint64_t tag_len = v[1];
    uint32_t tag = tag_len>>32;
    uint32_t len = tag_len;
    printf(", tag:%llu, size:%llu | ",tag,len);
    v+=2;
    if(tag==FUN_TAG){
      --len;
      v+=1;
      fputs("[text_ptr]",stdout);
    }
    while(len--){
      print_no_cycle(*v);
      ++v;
    }
    fputs("}",stdout);
  }
}