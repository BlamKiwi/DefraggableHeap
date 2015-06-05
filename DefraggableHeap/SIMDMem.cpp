#include "stdafx.h"

#include "SIMDMem.h"

#include <cassert>
#include <emmintrin.h>
#include <smmintrin.h>

void SIMDMemCopy(void* target, void* source, size_t num_chunks)
{
	// A copy of zero chunks is a no-op
	if (!num_chunks)
		return;

	// Check that the given addresses are 16 byte aligned
	assert(!((reinterpret_cast<intptr_t>(target) & 15)));
	assert(!((reinterpret_cast<intptr_t>(source) & 15)));

	// Interpret the addresses as a bunch of bytes
	auto t = static_cast<__m128i*>(target);
	auto s = static_cast<__m128i*>(source);

	// Start copying the source to the target addresses
	for (size_t i = 0; i < num_chunks; i++, t++, s++)
	{
		// Load chunk from source address
		const __m128i chunk = _mm_load_si128(s);
		//__m128i chunk = _mm_stream_load_si128(s);

		// Store chunk into target address
		_mm_store_si128(t, chunk);
		//_mm_stream_si128(t, chunk);
	}
}

void SIMDMemSet(void* target, int pattern, size_t num_chunks)
{
	// A set of zero chunks is a no-op
	if (!num_chunks)
		return;

	// Check that the given addresses are 16 byte aligned
	assert(!((reinterpret_cast<intptr_t>(target)& 15)));

	// Interpret the addresses as a bunch of bytes
	auto t = static_cast<__m128i*>(target);

	// Create a SIMD register with the desired pattern
	_declspec(align(16)) const int p[] = { pattern, pattern, pattern, pattern };
	const __m128i chunk = _mm_load_si128(reinterpret_cast<const __m128i*>(&p));

	// Start setting the target region in memory to the pattern register
	for (size_t i = 0; i < num_chunks; i++, t++)
		_mm_store_si128(t, chunk);
}