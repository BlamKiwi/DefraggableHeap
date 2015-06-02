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

ListHeap::~ListHeap()
{
	_pointer_list.RemoveAll();

	// Delete the system heap
	AlignedDelete(_heap);
}

float ListHeap::FragmentationRatio() const
{
	// Heap is not fragmented if we are at full load
	// This doesn't take into account free blocks of 0 size that might exist
	if (!_free_chunks)
		return 0.0f;

	// Get free chunks statistics
	const auto free = static_cast<float>(_free_chunks);
	const auto free_max = static_cast<float>(_max_contiguous_free_chunks);

	// Calculate free chunks ratio to determine fragmentation
	return (free - free_max) / free;
}


bool ListHeap::IsFullyDefragmented() const
{
	return _max_contiguous_free_chunks == _free_chunks;
}