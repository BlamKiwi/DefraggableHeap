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
	const auto offset = ( 16 - ( size & mask ) ) & mask;
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
	new (&_heap[NULL_INDEX]) BlockHeader(NULL_INDEX, NULL_INDEX, 1, ALLOCATED);
	UpdateNodeStatistics(_heap[NULL_INDEX]);

	// Setup the root node
	_root_index = NULL_INDEX + 1;
	_free_chunks = _num_chunks - 1;
	new (&_heap[_root_index]) BlockHeader(NULL_INDEX, NULL_INDEX, _free_chunks, FREE);
	UpdateNodeStatistics(_heap[_root_index]);
}

void SplayHeap::UpdateNodeStatistics(BlockHeader &node)
{
	// The maximum free contiguous block for the current node
	// is a 3 way maximum of of children and max of self if we are a free block
	node._max_contiguous_free_chunks = std::max(
		_heap[node._left]._max_contiguous_free_chunks, 
		_heap[node._right]._max_contiguous_free_chunks);

	if (!node._block_metadata._is_allocated)
		node._max_contiguous_free_chunks = std::max(
			node._max_contiguous_free_chunks, 
			node._block_metadata._num_chunks);
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

IndexType SplayHeap::RotateWithLeftChild( IndexType k2 )
{
	// Get left subtree
	auto k1 = _heap[k2]._left;

	// Bind nodes
	auto &n1 = _heap [ k1 ];
	auto &n2 = _heap [ k2 ];

	// Move right subtree of left child
	n2._left = n1._right;

	// Lower original node
	n1._right = k2;

	// Update new child node statistics
	UpdateNodeStatistics( n2 );

	// Update new parent node statistics
	UpdateNodeStatistics( n1 );

	return k1;
}


IndexType SplayHeap::RotateWithRightChild( IndexType k1 )
{
	// Promote right subtree
	auto k2 = _heap[ k1 ]._right;

	// Bind nodes
	auto &n1 = _heap [ k1 ];
	auto &n2 = _heap [ k2 ];

	// Move left subtree of right child
	n1._right = n2._left;

	// Lower original node
	n2._left = k1;

	// Update new child node statistics
	UpdateNodeStatistics( n1 );

	// Update new parent node statistics
	UpdateNodeStatistics( n2 );

	return k2;
}

IndexType SplayHeap::FindFreeBlock( IndexType t, size_t num_chunks )
{
	// We assume the tree already has a free block large enough
	assert( _heap [ t ]._max_contiguous_free_chunks >= num_chunks );

	// Traverse down tree until we find a free block large enough
	while ( t != NULL_INDEX )
	{
		auto &n = _heap [ t ];

		// Does the left subtree contain a free block large enough
		if ( _heap [ n._left ]._max_contiguous_free_chunks >= num_chunks )
			t = n._left;
		// Is the current block free and big enough?
		else if ( !n._block_metadata._is_allocated && n._block_metadata._num_chunks >= num_chunks )
			break;
		// The right subtree must contain the free block
		else
			t = n._right;
	}
	
	// Return found index
	return t;
}