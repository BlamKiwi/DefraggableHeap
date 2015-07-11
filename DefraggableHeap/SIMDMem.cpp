/*
Copyright (c) 2015, Missing Box Studio
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
		// Check that the given addresses are 16 byte aligned
		assert(!((reinterpret_cast<intptr_t>(t)& 15)));
		assert(!((reinterpret_cast<intptr_t>(s)& 15)));

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
	{
		// Check that the given addresses are 16 byte aligned
		assert(!((reinterpret_cast<intptr_t>(target)& 15)));

		_mm_store_si128(t, chunk);
	}
}