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

	static_assert(sizeof(_block_metadata) == sizeof(IndexType), "The metadata field must be the size of the index type.");
};

static_assert(sizeof(SplayHeader) == 16, "The block header needs to be 16 bytes in size.");