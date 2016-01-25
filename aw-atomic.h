
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

#include "aw-thread.h"

#if _MSC_VER
# include <intrin.h>
#endif

#if !_MSC_VER || _MSC_VER >= 1800
# include <stdbool.h>
#endif

#ifndef _atomic_assert
# include <assert.h>
# define _atomic_assert assert
#endif

#ifndef _atomic_memcpy
# include <string.h>
# define _atomic_memcpy memcpy
#endif

#if __GNUC__
# define _atomic_alwaysinline __attribute__((always_inline))
# define _atomic_restrict __restrict__
#elif _MSC_VER
# define _atomic_alwaysinline __forceinline
# define _atomic_restrict __restrict
#endif

#ifdef RL_TEST
# define _atomic_var(type) rl::atomic<type>
# define _atomic_load(var) var.load(rl::memory_order_relaxed)
# define _atomic_store(var,val) var.store(val, rl::memory_order_relaxed)
# define _atomic_rbarrier() rl::atomic_thread_fence(rl::memory_order_acquire)
# define _atomic_wbarrier() rl::atomic_thread_fence(rl::memory_order_release)
#else
# define _atomic_var(type) type
# define _atomic_load(var) (var)
# define _atomic_store(var,val) (var = (val))
# define _atomic_rbarrier() _rbarrier()
# define _atomic_wbarrier() _wbarrier()
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if __GNUC__
# define _compareandswap32(ptr,cmp,val) (__sync_val_compare_and_swap((ptr), (cmp), (val)))
# define _compareandswap64(ptr,cmp,val) (__sync_val_compare_and_swap((ptr), (cmp), (val)))
# define _barrier() do { asm volatile ("" : : : "memory"); } while (0)
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
# define _compareandswap32(ptr,cmp,val) (_InterlockedCompareExchange((ptr), (val), (cmp)))
# define _compareandswap64(ptr,cmp,val) (_InterlockedCompareExchange64((ptr), (val), (cmp)))
# define _barrier() do { _ReadWriteBarrier(); }
# if _M_IX86 || _M_X64
#  define _rbarrier() do { _mm_lfence(); } while (0)
#  define _wbarrier() do { _mm_sfence(); } while (0)
#  define _rwbarrier() do { _mm_mfence(); } while (0)
# endif
#endif

/*
   Once, e.g. safely lazy-create singletons shared by multiple threads.
 */

typedef int once_t;

_atomic_alwaysinline bool once_init(once_t *once) {
	switch (_compareandswap32(once, 0, 1)) {
	case 1:
		for (;; thread_yield())
			for (int i = 0; i < 1024; ++i)
				if (*(volatile once_t *) once == 2) {
					_rbarrier();
					return false;
				}
	case 2:
		_rbarrier();
		return false;
	}

	return true;
}

_atomic_alwaysinline void once_end(once_t *once) {
	_wbarrier();
	*once = 2;
}

/*
   Spinlock implementation.
 */

typedef int spin_t;

_atomic_alwaysinline bool spin_trylock(spin_t *lock) {
	return _compareandswap32(lock, 0, 1) == 0;
}

_atomic_alwaysinline void spin_lock(spin_t *lock) {
	for (;; thread_yield())
		for (int i = 0; i < 1024; ++i)
			if (spin_trylock(lock))
				return;
}

_atomic_alwaysinline void spin_unlock(spin_t *lock) {
	_barrier();
	*lock = 0;
}

/*
   Single-producer, single-consumer lockless ring buffer. Wrap the
   read and write functions with one or two spin locks for 1-to-N
   or N-to-M respectively.
 */

struct atomic_ring {
	void *base;
	size_t size;
	_atomic_var(size_t) read;
	_atomic_var(size_t) write;
};

_atomic_alwaysinline
void atomic_ring_init(struct atomic_ring *ring, void *base, size_t size) {
	_atomic_assert((size & (size - 1)) == 0);
	ring->base = base;
	ring->size = size;
	_atomic_store(ring->read, 0);
	_atomic_store(ring->write, 0);
}

_atomic_alwaysinline size_t _atomic_min(size_t a, size_t b) { return a < b ? a : b; }

_atomic_alwaysinline
void _atomic_read(struct atomic_ring *_atomic_restrict ring, size_t r, void *p, size_t n) {
	const size_t l = _atomic_min(n, ring->size - r);
	if (l > 0) _atomic_memcpy(p, (const char *) ring->base + r, l);
	if (n - l > 0) _atomic_memcpy((char *) p + l, ring->base, n - l);
	_atomic_store(ring->read, (r + n) & (ring->size - 1));
}

_atomic_alwaysinline
void _atomic_write(struct atomic_ring *_atomic_restrict ring, size_t w, const void *p, size_t n) {
	const size_t l = _atomic_min(n, ring->size - w);
	if (l > 0) _atomic_memcpy((char *) ring->base + w, p, l);
	if (n - l > 0) _atomic_memcpy(ring->base, (const char *) p + l, n - l);
	_atomic_wbarrier();
	_atomic_store(ring->write, (w + n) & (ring->size - 1));
}

_atomic_alwaysinline
bool atomic_dequeue(struct atomic_ring *_atomic_restrict ring, void *p, size_t n) {
	_atomic_rbarrier();
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = (w < r ? w + ring->size : w);
	return (n <= x - r) ? _atomic_read(ring, r, p, n), true : false;
}

_atomic_alwaysinline
bool atomic_enqueue(struct atomic_ring *_atomic_restrict ring, const void *p, size_t n) {
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = (r <= w ? r + ring->size : r);
	return (n <= x - (w + 1)) ? _atomic_write(ring, w, p, n), true : false;
}

_atomic_alwaysinline
size_t atomic_read(struct atomic_ring *_atomic_restrict ring, void *p, size_t n) {
	_atomic_rbarrier();
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = (w < r ? w + ring->size : w);
	const size_t k = _atomic_min(n, x - r);
	return _atomic_read(ring, r, p, k), k;
}

_atomic_alwaysinline
size_t atomic_write(struct atomic_ring *_atomic_restrict ring, const void *p, size_t n) {
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = (r <= w ? r + ring->size : r);
	const size_t k = _atomic_min(n, x - (w + 1));
	return _atomic_write(ring, w, p, k), k;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_ATOMIC_H */

