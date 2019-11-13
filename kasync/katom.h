#ifndef KATOM_H_99
#define KATOM_H_99
#include "kforwin32.h"
KBEGIN_DECLS
//自增
INLINE uint64_t katom_inc64(void * var)
{
#ifdef _WIN32
	return InterlockedIncrement64((LONGLONG *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((uint64_t *)(var), 1); // NOLINT
#endif
}

//自增
INLINE uint32_t katom_inc(void * var)
{
#ifdef _WIN32
	return InterlockedIncrement((long *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((uint32_t *)(var), 1); // NOLINT
#endif
}
#ifdef ENABLE_KATOM_16
//自增，返回之后的值
INLINE int16_t katom_inc16(void * var)
{
#ifdef _WIN32
	return InterlockedIncrement16((short *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((int16_t *)(var), 1); // NOLINT
#endif
}
//自减，返回之后的值
INLINE int16_t katom_dec16(void * var)
{
#ifdef _WIN32
	return InterlockedDecrement16((short *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((int16_t *)(var), -1); // NOLINT
#endif
}
#endif
//自减
INLINE uint32_t katom_dec(void * var)
{
#ifdef _WIN32
	return InterlockedDecrement((long *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((uint32_t *)(var), -1); // NOLINT
#endif
}
//自减，返回之后的值
INLINE int64_t katom_dec64(void * var)
{
#ifdef WIN32
	return InterlockedDecrement64((LONGLONG *)(var)); // NOLINT
#else
	return __sync_add_and_fetch((int64_t *)(var), -1); // NOLINT
#endif
}
//加一个值
INLINE uint32_t katom_add(void * var, const uint32_t value)
{
#ifdef _WIN32
	return InterlockedExchangeAdd((long *)(var), value); // NOLINT
#else
	return __sync_fetch_and_add((uint32_t *)(var), value);  // NOLINT
#endif
}
//加一个值，返回之前的值
INLINE int64_t katom_add64(void * var, const int64_t value)
{
#ifdef WIN32
	return InterlockedExchangeAdd64((LONGLONG *)(var), value); // NOLINT
#else
	return __sync_fetch_and_add((int64_t *)(var), value);  // NOLINT
#endif
}

//减一个值
INLINE uint32_t katom_sub(void * var, int32_t value)
{
	value = value * -1;
#ifdef _WIN32
	return InterlockedExchangeAdd((long *)(var), value); // NOLINT
#else
	return __sync_fetch_and_add((uint32_t *)(var), value);  // NOLINT
#endif
}
//返回之前的值
INLINE int64_t katom_sub64(void * var, const int64_t value)
{
#ifdef WIN32
	return InterlockedExchangeAdd64((LONGLONG *)(var), -value); // NOLINT
#else
	return __sync_fetch_and_sub((int64_t *)(var), value);  // NOLINT
#endif
}
//返回之前的值
INLINE int64_t katom_set64(void * var, const uint64_t value)
{
#ifdef WIN32
	return InterlockedExchange64((LONGLONG *)(var), (LONGLONG)(value));
#else
	return __sync_lock_test_and_set((uint64_t *)(var), value);
#endif
}
//赋值,windows下返回新值，linux下返回旧值
INLINE uint32_t katom_set(void * var, const uint32_t value)
{
#ifdef _WIN32
	return InterlockedExchange((long *)(var), (long)(value)); // NOLINT
#else
	return __sync_lock_test_and_set((uint32_t *)(var), value);  // NOLINT
#endif
}
//取值
INLINE uint32_t katom_get(void * var)
{
#ifdef _WIN32
	return InterlockedExchangeAdd((long *)(var), 0); // NOLINT
#else
	return __sync_fetch_and_add((uint32_t *)(var), 0);  // NOLINT
#endif
}
//取值
INLINE uint64_t katom_get64(void * var)
{
#ifdef _WIN32
	return InterlockedExchangeAdd64((LONGLONG *)(var), 0); // NOLINT
#else
	return __sync_fetch_and_add((uint64_t *)(var), 0);  // NOLINT
#endif
}
INLINE bool katom_cas(void *var, int32_t compare, int32_t value)
{
#ifdef _WIN32
	return compare == InterlockedCompareExchange((long *)var, value, compare);
#else
	return __sync_bool_compare_and_swap((int32_t *)var, compare, value);
#endif
}
INLINE bool katom_cas16(void *var, int16_t compare, int16_t value)
{
#ifdef WIN32
	return compare == InterlockedCompareExchange16((SHORT *)var, value, compare);
#else
	return __sync_bool_compare_and_swap((int16_t *)var, compare, value);
#endif
}
INLINE bool katom_cas64(void *var, int64_t compare, int64_t value)
{
#ifdef WIN32
	return compare == InterlockedCompareExchange64((LONGLONG *)var, value, compare);
#else
	return __sync_bool_compare_and_swap((int64_t *)var, compare, value);
#endif
}
KEND_DECLS
#endif
