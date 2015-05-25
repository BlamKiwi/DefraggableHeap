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
	// This doesn't take into account free blocks of 0 size that might exist
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
		if (value < lut[bool(t)])
		{
			// If the desired value is in the left subtree of the left child
			if (value < lut[bool(_heap[t]._left)])
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
		else if (value > lut[bool(t)])
		{
			// If the desired value is in the right subtree of the right child
			if (value > lut[bool(_heap[t]._right)])
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

void* SplayHeap::Allocate(size_t num_bytes)
{
	// An allocation of 0 bytes is redundant
	if (!num_bytes)
		return nullptr;

	// Calculate the number of chunks required to fulfull the request
	const size_t mask = 16 - 1;
	const auto offset = (16 - (num_bytes & mask)) & mask;
	const auto required_chunks = (num_bytes + offset) / 16;
	assert(required_chunks);

	// Do we have enough contiguous space for the allocation
	if (_heap[_root_index]._max_contiguous_free_chunks < required_chunks)
		return nullptr;

	// Splay the found free block to the root
	const auto free_block = FindFreeBlock(_root_index, required_chunks);
	_root_index = Splay(free_block, _root_index);

	/* Split the root free block into two, one allocated block and one free block */

	// Calculate the new raw free block size
	const auto raw_free_chunks = _heap[_root_index]._block_metadata._num_chunks
		- required_chunks;

	// Set the root so that it represents a now allocated block
	_heap[_root_index]._block_metadata = { ALLOCATED, required_chunks };
	_free_chunks -= required_chunks;

	// Is there a new free block to add to the tree
	if (raw_free_chunks)
	{
		auto &old_root = _heap[_root_index];

		// Calculate the new free block offset
		const auto new_free_index = _root_index + required_chunks + 1;

		// Create the header for the new free block
		// New address value will always be bigger since 
		// we allocate to the lower adresses
		// Right subtree is untouched
		new (&_heap[new_free_index]) BlockHeader(
				_root_index, 
				old_root._right,
				raw_free_chunks - 1, 
				FREE
			);
		_free_chunks--;

		// Clear right subtree of old root
		old_root._right = NULL_INDEX;

		// Update old root node statistics
		UpdateNodeStatistics(old_root);

		// Set new root
		_root_index = new_free_index;
	}

	// Update root node statistics
	UpdateNodeStatistics(_heap[_root_index]);

	// Possible strict aliasing problem?
	return &_heap[_root_index + 1];
}