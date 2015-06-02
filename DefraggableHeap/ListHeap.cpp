#include "stdafx.h"

#include "ListHeap.h"
#include "ListHeader.h"
#include "AlignedAllocator.h"

#include <cassert>
#include <new>

ListHeap::ListHeap(size_t size)
{
	// Make sure heap size is multiples of 16 bytes
	static const size_t mask = 16 - 1;
	const auto offset = (16 - (size & mask)) & mask;
	const auto total_size = size + offset;
	assert(total_size % 16 == 0);

	// A heap of <64 bytes is undefined
	assert(total_size >= 64);

	// Get the total number of chunks we need
	_num_chunks = total_size / 16;

	// Can the total number of chunks be indexed bu a 31 bit unsigned integer.
	assert(_num_chunks <= (IndexType(-1) >> 1));

	// Setup the null sentinel node
	new (&_heap[NULL_INDEX]) ListHeader(1, 1, NULL_INDEX, 0, ALLOCATED);

	// Setup the first free block
	const auto free = _num_chunks - 1;
	new (&_heap[NULL_INDEX]) ListHeader(NULL_INDEX, NULL_INDEX, NULL_INDEX, free, FREE);

	// Setup heap tracking state
	_free_chunks = free;
	_first_free_block = 1;
	_max_contiguous_free_chunks = free;
}