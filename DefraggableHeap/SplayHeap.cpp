#include "stdafx.h"

#include "SplayHeap.h"
#include "AlignedAllocator.h"

#include <cassert>
#include <new>
#include <algorithm>

SplayHeap::SplayHeap(size_t size)
{
	// Make sure heap size is multiples of 16 bytes
	const size_t mask = 16 - 1;
	const auto offset = 16 - (size & mask);
	const auto total_size = size + offset;
	assert(total_size % 16 == 0);

	// A heap of <64 bytes is undefined
	assert(size >= 64);

	// Get the total number of chunks we need
	_num_chunks = total_size / 16;

	// Can the total number of chunks be indexed bu a 31 bit unsigned integer.
	assert(_num_chunks <= (IndexType(-1) >> 1));

	// Allocate the system heap
	_heap = static_cast<BlockHeader*>(AlignedNew(total_size, 16));

	// Setup the null sentinel node
	new (&_heap[NULL_INDEX]) BlockHeader(NULL_INDEX, NULL_INDEX, 0, ALLOCATED);
	UpdateNodeStatistics(NULL_INDEX);

	// Setup the root node
	_root_index = NULL_INDEX + 1;
	_free_chunks = _num_chunks - 1;
	new (&_heap[_root_index]) BlockHeader(NULL_INDEX, NULL_INDEX, _free_chunks, FREE);
	UpdateNodeStatistics(_root_index);
}

void SplayHeap::UpdateNodeStatistics(IndexType node)
{
	auto &n = _heap[node];

	// The maximum free contiguous block for the current node
	// is a 3 way maximum of of children and max of self if we are a free block
	n._max_contiguous_free_chunks = std::max(_heap[n._left]._max_contiguous_free_chunks, _heap[n._right]._max_contiguous_free_chunks);

	if (!n._block_metadata._is_allocated)
		n._max_contiguous_free_chunks = std::max(n._max_contiguous_free_chunks, n._block_metadata._num_chunks);
}

SplayHeap::~SplayHeap()
{
	// Delete the system heap
	AlignedDelete(_heap);
}

float SplayHeap::FragmentationRatio() const
{
	// Heap is not fragmented if we are at full load
	if (_free_chunks == 0)
		return 0.0f;

	// Get free chunks statistics
	const auto free = static_cast<float>(_free_chunks);
	const auto free_max = static_cast<float>(_heap[_root_index]._max_contiguous_free_chunks);
	
	// Calculate free chunks ratio to determine fragmentation
	return (free - free_max) / free;
}