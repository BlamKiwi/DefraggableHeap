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

#pragma once

#include <cstdint>

#include "HeapCommon.h"

/**
*	Defines the header for a defraggable heap block.
*
*	We assume an alignment and size of 16 bytes so we can be globbed by an aligned SIMD load.
*/

_declspec(align(16)) struct SplayHeader
{
	/**
	*	Constructs an empty, allocated block header.
	*	Does not calculate statistics for the heap.
	*/
	SplayHeader();

	/**
	*	Constructs a block header from the given block data.
	*	Does not calculate statistics for the heap.
	*
	*	@param left the index for our left heap
	*	@param right the index for our right heap
	*	@param num_chunks the number of chunks on the block we represent
	*	@param alloc the allocation state of the block
	*/
	SplayHeader(IndexType left, IndexType right, IndexType num_chunks, AllocationState alloc);

	/**< Index of the left heap for this header. */
	IndexType _left;

	/**< Index of the right heap for this header. */
	IndexType _right;

	/**< Block allocation metadata. */
	BlockMetadata _block_metadata;

	/**< The local maximum number of contiguous free chunks in the heap. */
	IndexType _max_contiguous_free_chunks;
};

static_assert(sizeof(SplayHeader) == 16, "The block header needs to be 16 bytes in size.");