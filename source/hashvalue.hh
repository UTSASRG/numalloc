#ifndef _HASHVALUE_H
#define _HASHVALUE_H

#include <utility>
#include <vector>

#include <stdint.h>

/* taken from TR1 and Boost hash.hpp */

inline size_t hash_value(void* v) { return (intptr_t)v; }
inline size_t hash_value(unsigned int v) { return v; }

template<typename T>
inline void hash_combine(size_t & seed, T const& v) {
	seed ^= hash_value(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

#ifdef ENABLE_TWO_LEVEL_KEY
inline size_t hash_value(int level, void** first, int second) {
  size_t seed = 0;
  for(int i=0; i<level; i++)
    hash_combine(seed, first[i]);
  hash_combine(seed, second);
  return seed;
}
#else
inline size_t hash_value(void* first, int second) {
  size_t seed = 0;
  hash_combine(seed, first);
  hash_combine(seed, second);
  return seed;
}
#endif

template<typename T>
inline size_t hash_range(T base, int start, int end) {
	size_t seed = 0;
	for (int i=start; i<end; i++)
		hash_combine(seed, base[i]);
	return seed;
}

template<typename T>
inline size_t hash_range(T first, T last) {
	size_t seed = 0;
	for (; first != last; ++first)
		hash_combine(seed, *first);
	return seed;
}

template<typename T, typename A>
inline size_t hash_value(std::vector<T, A> const &v) {
	return hash_range(v.begin(), v.end());
}

#endif

