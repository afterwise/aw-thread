/* vim: set ts=4 sw=4 noet : */
/*
   Copyright (c) 2014-2024 Malte Hildingsson, malte (at) afterwi.se

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

#if _MSC_VER
# include <intrin.h>
#elif defined __i386__ || defined __x86_64__
# include <xmmintrin.h>
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
# define _atomic_alwaysinline inline __attribute__((always_inline))
#elif _MSC_VER
# define _atomic_alwaysinline __forceinline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if __GNUC__
# define _atomic_add32(ptr,val) (__sync_fetch_and_add((ptr), (val)))
# define _atomic_add64(ptr,val) (__sync_fetch_and_add((ptr), (val)))
# define _atomic_cas32(ptr,cmp,val) (__sync_val_compare_and_swap((ptr), (cmp), (val)))
# define _atomic_cas64(ptr,cmp,val) (__sync_val_compare_and_swap((ptr), (cmp), (val)))
# define _atomic_barrier() do { __asm__ volatile ("" : : : "memory"); } while (0)
# if __i386__ || __x86_64__
#  define _atomic_acquire() do { __asm__ volatile ("lfence" : : : "memory"); } while (0)
#  define _atomic_release() do { __asm__ volatile ("sfence" : : : "memory"); } while (0)
#  define _atomic_fence() do { __asm__ volatile ("mfence" : : : "memory"); } while (0)
#  define _atomic_yield() do { _mm_pause(); } while (0)
# elif __PPU__ || __ppc64__
#  define _atomic_acquire() do { __asm__ volatile ("sync 1" : : : "memory"); } while (0)
#  define _atomic_release() do { __asm__ volatile ("eieio" : : : "memory"); } while (0)
#  define _atomic_fence() do { __asm__ volatile ("sync" : : : "memory"); } while (0)
#  define _atomic_yield() do { __asm__ volatile ("or 27,27,27"); } while (0)
# elif __arm__ || __arm64__ || __aarch64__
#  define _atomic_acquire() do { __asm__ volatile ("dmb ish" : : : "memory"); } while (0)
#  define _atomic_release() do { __asm__ volatile ("dmb ishst" : : : "memory"); } while (0)
#  define _atomic_fence() do { __asm__ volatile ("dmb ish" : : : "memory"); } while (0)
#  define _atomic_yield() do { __asm__ volatile ("yield"); } while (0)
# endif
#elif _MSC_VER
# define _atomic_add32(ptr,val) (_InterlockedExchangeAdd((ptr), (val)))
# define _atomic_add64(ptr,val) (_InterlockedExchangeAdd64((ptr), (val)))
# define _atomic_cas32(ptr,cmp,val) (_InterlockedCompareExchange((ptr), (val), (cmp)))
# define _atomic_cas64(ptr,cmp,val) (_InterlockedCompareExchange64((ptr), (val), (cmp)))
# define _atomic_barrier() do { _ReadWriteBarrier(); }
# define _atomic_acquire() do { _mm_lfence(); } while (0)
# define _atomic_release() do { _mm_sfence(); } while (0)
# define _atomic_fence() do { _mm_mfence(); } while (0)
# define _atomic_yield() do { _mm_pause(); } while (0)
#endif

#ifdef RL_TEST
# define _atomic_var(type) rl::atomic<type>
# define _atomic_load(var) var.load(rl::memory_order_relaxed)
# define _atomic_store(var,val) var.store(val, rl::memory_order_relaxed)
# undef _atomic_acquire
# undef _atomic_release
# define _atomic_acquire() rl::atomic_thread_fence(rl::memory_order_acquire)
# define _atomic_release() rl::atomic_thread_fence(rl::memory_order_release)
#else
# define _atomic_var(type) type
# define _atomic_load(var) (var)
# define _atomic_store(var,val) (var = (val))
#endif

/*
   Once, e.g. safely lazy-create singletons shared by multiple threads.
 */

#if _MSC_VER
typedef long atomic_once_t;
#else
typedef int atomic_once_t;
#endif

_atomic_alwaysinline
static bool atomic_once_init(atomic_once_t *once) {
	switch (_atomic_cas32(once, 0, 1)) {
	case 1:
		do _atomic_yield();
		while (*(volatile atomic_once_t *) once != 2);
	case 2:
		_atomic_acquire();
		return false;
	}
	return true;
}

_atomic_alwaysinline
static void atomic_once_end(atomic_once_t *once) {
	_atomic_release();
	*once = 2;
}

/*
   Spinlock implementation.
 */

#if _MSC_VER
typedef long atomic_spin_t;
#else
typedef int atomic_spin_t;
#endif

_atomic_alwaysinline
static bool atomic_trylock(atomic_spin_t *spin) {
	if (_atomic_cas32(spin, 0, 1) == 0) {
		_atomic_acquire();
		return true;
	}
	return false;
}

_atomic_alwaysinline
static void atomic_lock(atomic_spin_t *spin) {
	while (!atomic_trylock(spin))
		_atomic_yield();
}

_atomic_alwaysinline
static void atomic_unlock(atomic_spin_t *spin) {
	_atomic_release();
	*spin = 0;
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
static void atomic_ring_init(struct atomic_ring *ring, void *base, size_t size) {
	_atomic_assert((size & (size - 1)) == 0);
	ring->base = base;
	ring->size = size;
	_atomic_store(ring->read, 0);
	_atomic_store(ring->write, 0);
}

_atomic_alwaysinline
static size_t _atomic_min(size_t a, size_t b) { return a < b ? a : b; }

_atomic_alwaysinline
static size_t _atomic_write_end(const struct atomic_ring *__restrict ring, size_t r, size_t w) {
	return (r <= w ? r + ring->size : r);
}

_atomic_alwaysinline
static size_t _atomic_read_end(const struct atomic_ring *__restrict ring, size_t r, size_t w) {
	return (w < r ? w + ring->size : w);
}

_atomic_alwaysinline
static bool _atomic_can_read(size_t r, size_t read_end, size_t n) {
	return (n <= read_end - r);
}

_atomic_alwaysinline
static bool _atomic_can_write(size_t w, size_t write_end, size_t n) {
	return (n <= write_end - (w + 1));
}

_atomic_alwaysinline
static void _atomic_read(struct atomic_ring *__restrict ring, size_t r, void *p, size_t n) {
	const size_t l = _atomic_min(n, ring->size - r);
	if (l > 0) _atomic_memcpy(p, (const char *) ring->base + r, l);
	if (n - l > 0) _atomic_memcpy((char *) p + l, ring->base, n - l);
	_atomic_store(ring->read, (r + n) & (ring->size - 1));
}

_atomic_alwaysinline
static void _atomic_write(struct atomic_ring *__restrict ring, size_t w, const void *p, size_t n) {
	const size_t l = _atomic_min(n, ring->size - w);
	if (l > 0) _atomic_memcpy((char *) ring->base + w, p, l);
	if (n - l > 0) _atomic_memcpy(ring->base, (const char *) p + l, n - l);
	_atomic_release();
	_atomic_store(ring->write, (w + n) & (ring->size - 1));
}

_atomic_alwaysinline
static bool atomic_dequeue(struct atomic_ring *__restrict ring, void *p, size_t n) {
	_atomic_acquire();
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = _atomic_read_end(ring, r, w);
	return _atomic_can_read(r, x, n) ? _atomic_read(ring, r, p, n), true : false;
}

_atomic_alwaysinline
static bool atomic_enqueue(struct atomic_ring *__restrict ring, const void *p, size_t n) {
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = _atomic_write_end(ring, r, w);
	return _atomic_can_write(w, x, n) ? _atomic_write(ring, w, p, n), true : false;
}

_atomic_alwaysinline
static size_t atomic_read(struct atomic_ring *__restrict ring, void *p, size_t n) {
	_atomic_acquire();
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = _atomic_read_end(ring, r, w);
	const size_t k = _atomic_min(n, x - r);
	return _atomic_read(ring, r, p, k), k;
}

_atomic_alwaysinline
static size_t atomic_write(struct atomic_ring *__restrict ring, const void *p, size_t n) {
	const size_t r = _atomic_load(ring->read), w = _atomic_load(ring->write);
	const size_t x = _atomic_write_end(ring, r, w);
	const size_t k = _atomic_min(n, x - (w + 1));
	return _atomic_write(ring, w, p, k), k;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_ATOMIC_H */

