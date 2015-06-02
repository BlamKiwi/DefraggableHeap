#pragma once

#include <cstdint>

#include "HeapCommon.h"

/**
*	Defines the header for a defraggable heap block.
*
*	We assume an alignment and size of 16 bytes so we can be globbed by an aligned SIMD load.
*/

_declspec(align(16)) struct ListHeader
{
	/**
	*	Constructs an empty, allocated block header.
	*/
	ListHeader();

	/**
	*	Constructs a block header from the given block data.
	*
	*	@param prev the index of the previous block in the heap
	*	@param next the index of the next block in the heap
	*	@param next_free the index of the next free block in the heap
	*	@param num_chunks the number of chunks on the block we represent
	*	@param alloc the allocation state of the block
	*/
	ListHeader(IndexType prev, IndexType next, IndexType next_free, IndexType num_chunks, AllocationState alloc);

	/**< Index of the previous block for this header. */
	IndexType _prev;

	/**< Index of the next block for this header. */
	IndexType _next;

	/**< Index of the next free block. */
	IndexType _next_free;

	/**< Block allocation metadata. */
	BlockMetadata _block_metadata;
};

static_assert(sizeof(ListHeader) == 16, "The block header needs to be 16 bytes in size.");