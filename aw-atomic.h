
/*
   Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

#ifndef AW_ATOMIC_H
#define AW_ATOMIC_H

#if __GNUC__
# define _compareandswap(ptr,cmp,val) (__sync_val_compare_and_swap((ptr), (cmp), (val)))
# if __x86_64__ || __i386__
#  define _rbarrier() do { asm volatile ("lfence" : : : "memory"); } while (0)
#  define _wbarrier() do { asm volatile ("sfence" : : : "memory"); } while (0)
#  define _rwbarrier() do { asm volatile ("mfence" : : : "memory"); } while (0)
# elif __PPU__ || __ppc64__
#  define _rbarrier() do { asm volatile ("sync 1" : : : "memory"); } while (0)
#  define _wbarrier() do { asm volatile ("eieio" : : : "memory"); } while (0)
#  define _rwbarrier() do { asm volatile ("sync" : : : "memory"); } while (0)
# elif __arm__
#  define _rbarrier() do { asm volatile ("dmb ish" : : : "memory"); } while (0)
#  define _wbarrier() do { asm volatile ("dmb ishst" : : : "memory"); } while (0)
#  define _rwbarrier() do { asm volatile ("dmb ish" : : : "memory"); } while (0)
# endif
#elif _MSC_VER
# define _compareandswap(ptr,cmp,val) (_InterlockedCompareExchange((ptr), (val), (cmp)))
# if _M_IX86 || _M_X64
#  define _rbarrier() do { _mm_lfence(); } while (0)
#  define _wbarrier() do { _mm_sfence(); } while (0)
#  define _rwbarrier() do { _mm_mfence(); } while (0)
# endif
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_ATOMIC_H */
