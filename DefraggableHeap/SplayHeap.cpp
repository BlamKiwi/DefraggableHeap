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
	// We allocate an extra chunk as we need a valid value for a splay header
	_num_chunks = (total_size / 16) + 1;

	// Can the total number of chunks be indexed bu a 31 bit unsigned integer.
	assert(_num_chunks <= (IndexType(-1) >> 1));

	// Allocate the system heap
	_heap = static_cast<BlockHeader*>(AlignedNew(_num_chunks * 16, 16));

	// Setup the null sentinel node
	new (&_heap[NULL_INDEX]) BlockHeader(NULL_INDEX, NULL_INDEX, 0, ALLOCATED);
	UpdateNodeStatistics(_heap[NULL_INDEX]);

	/* Splay header can be left unallocated */

	// Setup the root node
	_root_index = 2;
	_free_chunks = _num_chunks - 3; // Null, Splay and Root Header, therefore -3
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

IndexType SplayHeap::Splay(IndexType value, IndexType t)
{
	// Setup splay tracking state
	new (&_heap[SPLAY_HEADER_INDEX]) BlockHeader(NULL_INDEX, NULL_INDEX, 0, ALLOCATED);
	IndexType left_tree_max = SPLAY_HEADER_INDEX, right_tree_min = SPLAY_HEADER_INDEX;

	// Simulate a null node with a reassignable value
	IndexType lut[] = { value, NULL_INDEX };

	// Continually rotate the tree until we splay the desired value
	while (true)
	{
		// Update lut value to new t
		lut[1] = t;

		// Is the desired value in the left subtree
		if (value < lut[t / 1])
		{
			// If the desired value is in the left subtree of the left child
			if (value < lut[_heap[t]._left / 1])
				// Rotate left subtree up
				t = RotateWithLeftChild(t);

			auto &n = _heap[t];
			auto &r = _heap[right_tree_min];

			// If the desired value is now at the root, stop splay
			if (n._left == NULL_INDEX)
				break;

			// t is now a minimum value
			// Link right state tree
			r._left = t;
			UpdateNodeStatistics(r);

			// Advance splay tracking offsets
			right_tree_min = t;
			t = n._left;
		}
		// Is the desired value in the right subtree
		else if (value > lut[t / 1])
		{
			// If the desired value is in the right subtree of the right child
			if (value > lut[_heap[t]._right / 1])
				// Rotate right subtree up
				t = RotateWithRightChild(t);

			auto &n = _heap[t];
			auto &l = _heap[left_tree_max];

			// If the desired value is now at the root, stop splay
			if (n._right == NULL_INDEX)
				break;

			// t is now a maximum value
			// Link left state tree
			l._right = t;
			UpdateNodeStatistics(l);

			// Advance splay tracking offsets
			left_tree_max = t;
			t = n._right;
		}
		// Is the desired value at the root
		else
			break;
	}
	
	auto &n = _heap[t];
	auto &l = _heap[left_tree_max];
	auto &r = _heap[right_tree_min];

	// Rebuild tree from left and right sub trees
	l._right = n._left;
	UpdateNodeStatistics(l);
	r._left = n._right;
	UpdateNodeStatistics(r);

	// Rebuild tree root
	n._left = _heap[SPLAY_HEADER_INDEX]._right;
	n._right = _heap[SPLAY_HEADER_INDEX]._left;
	UpdateNodeStatistics(n);

	return t;
}