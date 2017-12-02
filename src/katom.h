#ifndef KATOM_H
#define KATOM_H
#include "global.h"
#ifdef ENABLE_ATOM
#include "forwin32.h"
#ifdef WIN32
#if  _MSC_VER < 1600
typedef char                    int8_t;
typedef short                   int16_t;
typedef int                     int32_t;
typedef __int64					int64_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;
typedef unsigned __int64    uint64_t;

#else
#include <stdint.h>
#endif
#else
#include <stdint.h>
#endif
//����
inline uint64_t katom_inc64(void * var) 
{
#ifdef WIN32
  return InterlockedIncrement64((LONGLONG *)(var)); // NOLINT
#else
  return __sync_add_and_fetch((uint64_t *)(var), 1); // NOLINT
#endif
}

//����
inline uint32_t katom_inc(void * var) 
{
#ifdef WIN32
  return InterlockedIncrement((long *)(var)); // NOLINT
#else
  return __sync_add_and_fetch((uint32_t *)(var), 1); // NOLINT
#endif
}

//�Լ�
inline uint32_t katom_dec(void * var) 
{
#ifdef WIN32
  return InterlockedDecrement((long *)(var)); // NOLINT
#else
  return __sync_add_and_fetch((uint32_t *)(var), -1); // NOLINT
#endif
}

//��һ��ֵ
inline uint32_t katom_add(void * var, const uint32_t value) 
{
#ifdef WIN32
  return InterlockedExchangeAdd((long *)(var), value); // NOLINT
#else
  return __sync_fetch_and_add((uint32_t *)(var), value);  // NOLINT
#endif
}

//��һ��ֵ
inline uint32_t katom_sub(void * var, int32_t value) 
{
	value = value * -1;
#ifdef WIN32
	return InterlockedExchangeAdd((long *)(var), value); // NOLINT
#else
	return __sync_fetch_and_add((uint32_t *)(var), value);  // NOLINT
#endif
}

//��ֵ,windows�·�����ֵ��linux�·��ؾ�ֵ
inline uint32_t katom_set(void * var, const uint32_t value) 
{
#ifdef WIN32
	::InterlockedExchange((long *)(var), (long)(value)); // NOLINT
#else
	__sync_lock_test_and_set((uint32_t *)(var), value);  // NOLINT
#endif
	return value;
}
//ȡֵ
inline uint32_t katom_get(void * var) 
{
#ifdef WIN32
  return InterlockedExchangeAdd((long *)(var), 0); // NOLINT
#else
  return __sync_fetch_and_add((uint32_t *)(var), 0);  // NOLINT
#endif
}
//ȡֵ
inline uint64_t katom_get64(void * var) 
{
#ifdef WIN32
  return InterlockedExchangeAdd64((LONGLONG *)(var), 0); // NOLINT
#else
  return __sync_fetch_and_add((uint64_t *)(var), 0);  // NOLINT
#endif
}
inline bool katom_cas(void *var,int32_t compare,int32_t value)
{
#ifdef WIN32
	return compare == InterlockedCompareExchange((long *)var,value,compare);
#else
	return __sync_bool_compare_and_swap((int32_t *)var,compare,value);
#endif
}
#endif
#endif
