/* Exact int64 fast path; the original int128 expression remains the fallback.
 * Valid original shifts are 0..127. No signed overflow/left shift is introduced. */
static inline int32_t p3_projection(int64_t base16,int64_t v,int32_t mant,int shift,int64_t z,int negate){
 int64_t product,num,q,r;
 if(__builtin_mul_overflow(v,(int64_t)mant,&product)||product<INT64_MIN/16||product>INT64_MAX/16)goto wide;
 num=(int64_t)((uint64_t)product<<4);
 if((unsigned)shift<64)num>>=shift;
 else if((unsigned)shift<128)num=num<0?-1:0;
 else goto wide; /* Outside original defined domain; do not extend semantics. */
 if(z==-1&&num==INT64_MIN)goto wide;
 q=z?num/z:0;
 if(negate){if(__builtin_sub_overflow(base16,q,&r))goto wide;}
 else if(__builtin_add_overflow(base16,q,&r))goto wide;
 if(r>INT32_MAX)return INT32_MAX;if(r<INT32_MIN)return INT32_MIN;return (int32_t)r;
wide:;
 __int128 n=(__int128)((unsigned __int128)((__int128)v*(__int128)mant)<<4);n>>=shift;
 __int128 w=z?n/(__int128)z:0,res=(__int128)base16+(negate?-w:w);
 if(res>INT32_MAX)return INT32_MAX;if(res<INT32_MIN)return INT32_MIN;return (int32_t)res;
}
