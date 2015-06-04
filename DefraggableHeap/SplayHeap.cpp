#include "stdafx.h"

#include "SplayHeap.h"
#include "AlignedAllocator.h"

#include "SIMDMem.h"

#include <cassert>
#include <new>
#include <algorithm>


#include "SplayHeader.h"

SplayHeap::SplayHeap(size_t size)
{
	// Make sure heap size is multiples of 16 bytes
	static const size_t mask = 16 - 1;
	const auto offset = ( 16 - ( size & mask ) ) & mask;
	const auto total_size = size + offset;
	assert(total_size % 16 == 0);

	// A heap of <64 bytes is undefined
	assert(total_size >= 64);

	// Get the total number of chunks we need
	_num_chunks = total_size / 16;

	// Can the total number of chunks be indexed bu a 31 bit unsigned integer.
	assert(_num_chunks <= (IndexType(-1) >> 1));

	// Allocate the system heap
	_heap = static_cast<SplayHeader*>(AlignedNew(total_size, 16));

	// Setup the null sentinel node
	new (&_heap[NULL_INDEX]) SplayHeader(NULL_INDEX, NULL_INDEX, 0, ALLOCATED);
	UpdateNodeStatistics(_heap[NULL_INDEX]);

	/* Splay header can be left unallocated */

	// Setup the root node
	_root_index = 2;
	_free_chunks = _num_chunks - 2; // Null, Splay, therefore -2
	new (&_heap[_root_index]) SplayHeader(NULL_INDEX, NULL_INDEX, _free_chunks, FREE);
	UpdateNodeStatistics(_heap[_root_index]);

	// Debug set free chunks in the heap
#ifdef _DEBUG
		SIMDMemSet(&_heap[_root_index + 1], INIT_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

}

void SplayHeap::UpdateNodeStatistics(SplayHeader &node)
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
	_pointer_list.RemoveAll();

	// Delete the system heap
	AlignedDelete(_heap);
}

float SplayHeap::FragmentationRatio() const
{
	// Heap is not fragmented if we are at full load
	// This doesn't take into account free blocks of 0 size that might exist
	if (!_free_chunks)
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

IndexType SplayHeap::FindFreeBlock( IndexType t, IndexType num_chunks ) const
{
	// Is there even enough space in the tree to make an allocation
	if (_heap[t]._max_contiguous_free_chunks < num_chunks)
		return NULL_INDEX;

	// Traverse down tree until we find a free block large enough
	while ( t )
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
	new (&_heap[SPLAY_HEADER_INDEX]) SplayHeader(NULL_INDEX, NULL_INDEX, 0, ALLOCATED);
	IndexType left_tree_max = SPLAY_HEADER_INDEX, right_tree_min = SPLAY_HEADER_INDEX;

	// Simulate a null node with a reassignable value
	IndexType lut[] = { value, NULL_INDEX };
	auto lookup = [&](IndexType v) { 
		lut[1] = v;
		return lut[bool(v)]; 
	};

	// Continually rotate the tree until we splay the desired value
	while (true)
	{
		// Is the desired value in the left subtree
		if (value < lookup(t))
		{
			// If the desired value is in the left subtree of the left child
			if (value < lookup(_heap[t]._left))
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
		else if (value > lookup(t))
		{
			// If the desired value is in the right subtree of the right child
			if (value > lookup(_heap[t]._right))
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

DefraggablePointerControlBlock SplayHeap::Allocate(size_t num_bytes)
{
	// An allocation of 0 bytes is redundant
	if (!num_bytes)
		return nullptr;

	// Calculate the number of chunks required to fulfil the request
	const size_t mask = 16 - 1;
	const auto offset = (16 - (num_bytes & mask)) & mask;
	const auto required_chunks = ((num_bytes + offset) / 16) + 1;
	assert(required_chunks);

	// Do we have enough contiguous space for the allocation
	if (_heap[_root_index]._max_contiguous_free_chunks < required_chunks)
		return nullptr;

	// Splay the found free block to the root
	const auto free_block = FindFreeBlock(_root_index, required_chunks);
	assert(free_block);
	_root_index = Splay(free_block, _root_index);

	/* Split the root free block into two, one allocated block and one free block */

	// Calculate the new raw free block size
	const auto raw_free_chunks = _heap[_root_index]._block_metadata._num_chunks
		- required_chunks;

	// Set the root so that it represents a now allocated block
	const auto old_index = _root_index;
	_heap[old_index]._block_metadata = { ALLOCATED, required_chunks };
	_free_chunks -= required_chunks;

#ifdef _DEBUG
		SIMDMemSet(&_heap[_root_index + 1], ALLOC_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

	// Is there a new free block to add to the tree
	if (raw_free_chunks)
	{
		auto &old_root = _heap[_root_index];

		// Calculate the new free block offset
		const auto new_free_index = _root_index + required_chunks;

		// Create the header for the new free block
		// New address value will always be bigger since 
		// we allocate to the lower adresses
		// Right subtree is untouched
		new (&_heap[new_free_index]) SplayHeader(
				_root_index, 
				old_root._right,
				raw_free_chunks, 
				FREE
			);

		// Clear right subtree of old root
		old_root._right = NULL_INDEX;

		// Update old root node statistics
		UpdateNodeStatistics(old_root);

		// Set new root
		_root_index = new_free_index;

#ifdef _DEBUG
			SIMDMemSet(&_heap[_root_index + 1], SPLIT_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

	}

	// Update root node statistics
	UpdateNodeStatistics(_heap[_root_index]);

	// Possible strict aliasing problem?
	return _pointer_list.Create(&_heap[old_index + 1]);
}

void SplayHeap::Free(DefraggablePointerControlBlock& ptr)
{
	void* data = ptr.Get();

	// We cannot free the null pointer
	if (!data)
		return;

	// Get the offset of the pointer into the heap
	const auto block_addr = static_cast<SplayHeader*>(data);
	const std::ptrdiff_t offset = block_addr - _heap;

	// Is the offset in a valid range
	if (offset < 0 || offset >= _num_chunks)
		return;

	// Is the data pointer of expected alignment
	if (block_addr != &_heap[offset])
		return;

	// Splay the block to free to the root of the tree
	_root_index = Splay(offset - 1, _root_index);

	// Mark the root as being free
	_heap[_root_index]._block_metadata._is_allocated = FREE;
	_free_chunks += _heap[_root_index]._block_metadata._num_chunks;

	// Invalidate defraggable pointers that point into the root block before we invalidate data in the heap
//	RemovePointersInRange(_root_index, _root_index + _heap[_root_index]._block_metadata._num_chunks);
	_pointer_list.RemovePointersInRange(&_heap[_root_index], &_heap[_root_index + _heap[_root_index]._block_metadata._num_chunks]);

#ifdef _DEBUG
		SIMDMemSet(&_heap[_root_index + 1], FREED_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

	// We may have invalidated our invariant of having no two free adjacent blocks
	// Collapse adjacent free blocks in the heap to restore the invariant
	// Does the left subtree contain any potential free blocks
	if (_heap[_heap[_root_index]._left]._max_contiguous_free_chunks)
	{
		// Splay the previous heap block to the root of the tree
		auto left = Splay(_root_index, _heap[_root_index]._left);

		// Is the root of the left subtree a free block
		if (!_heap[left]._block_metadata._is_allocated)
		{
			// Copy down block metadata and right subtree
			_heap[left]._right = _heap[_root_index]._right;
			_heap[left]._block_metadata._num_chunks += 
				_heap[_root_index]._block_metadata._num_chunks;
			
			// Make the left root the new tree root
			_root_index = left;

#ifdef _DEBUG
				SIMDMemSet(&_heap[_root_index + 1], MERGE_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif
		}
		// Previous block is allocated, fix up pointers
		else
			_heap[_root_index]._left = left;
	}

	// Does the right subtree contain any potential free blocks
	if (_heap[_heap[_root_index]._right]._max_contiguous_free_chunks)
	{
		// Splay the minimum value up from the right subtree
		auto right = Splay(_root_index, _heap[_root_index]._right);

		// Is the root of the right subtree a free block
		if (!_heap[right]._block_metadata._is_allocated)
		{
			// Copy up block metadata and new right subtree
			_heap[_root_index]._right = _heap[right]._right;
			_heap[_root_index]._block_metadata._num_chunks += 
				_heap[right]._block_metadata._num_chunks;

#ifdef _DEBUG
				SIMDMemSet(&_heap[_root_index + 1], MERGE_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif
		}
		// Next block is allocated, fix up pointers
		else
			_heap[_root_index]._right = right;
	}

	// Update root node statistics
	UpdateNodeStatistics(_heap[_root_index]);
}

void SplayHeap::FullDefrag()
{
	while (!IterateHeap())
		;
}

bool SplayHeap::IsFullyDefragmented() const
{
	return _heap[_root_index]._max_contiguous_free_chunks == _free_chunks;
}

bool SplayHeap::IterateHeap()
{
	// Do we actually need to defrag the heap
	if (IsFullyDefragmented())
		return true;

	// Splay the first free block in the heap to the root
	// This will put the fully defragmented subheap in the left subtree
	const auto free_block = FindFreeBlock(_root_index, 1);
	_root_index = Splay(free_block, _root_index);
	
	// Splay the next block in the heap up from the right subtree
	auto right = Splay(_root_index + 1, _heap[_root_index]._right);
	assert(!_heap[right]._left);

	// Our heap invariant means the next block must be allocated
	assert(_heap[right]._block_metadata._is_allocated);

	auto &root = _heap[_root_index];
	auto &n = _heap[right];

	// Update defraggable pointers before invalidating the heap
	//OffsetPointersInRange(right, right + n._block_metadata._num_chunks, _root_index - right);
	_pointer_list.OffsetPointersInRange(&_heap[right], &_heap[right + n._block_metadata._num_chunks], (_root_index - right) * 16);

	// Create new free block header
	SplayHeader new_free(NULL_INDEX, n._right, root._block_metadata._num_chunks, FREE);
	const auto new_free_offset = _root_index + n._block_metadata._num_chunks;

	// Create new allocated block header
	SplayHeader new_allocated(root._left, new_free_offset, n._block_metadata._num_chunks, ALLOCATED);

	/* CONSIDER THE HEAP INVALID FROM HERE */

	// Copy new allocated block header and move the data
	SIMDMemCopy(&_heap[_root_index], &new_allocated, 1);
	SIMDMemCopy(&_heap[_root_index + 1], &_heap[right + 1], 
		new_allocated._block_metadata._num_chunks - 1);

	// Copy new free block header
	SIMDMemCopy(&_heap[new_free_offset], &new_free, 1);

	/* HEAP IS NOW VALID */

	// Update new node statistics
	UpdateNodeStatistics(_heap[new_free_offset]);
	UpdateNodeStatistics(_heap[_root_index]);

	// Rotate right child up to root
	_root_index = RotateWithRightChild(_root_index);

#ifdef _DEBUG
		SIMDMemSet(&_heap[_root_index + 1], MOVE_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

	// We possibly invalidated our heap invariant
	// Does the right subtree contain any potential free blocks
	if (_heap[_heap[_root_index]._right]._max_contiguous_free_chunks)
	{
		// Splay the minimum value up from the right subtree
		auto right = Splay(_root_index, _heap[_root_index]._right);

		// Is the root of the right subtree a free block
		if (!_heap[right]._block_metadata._is_allocated)
		{
			// Copy up block metadata and new right subtree
			_heap[_root_index]._right = _heap[right]._right;
			_heap[_root_index]._block_metadata._num_chunks +=
				_heap[right]._block_metadata._num_chunks;

			// Update root node statistics
			UpdateNodeStatistics(_heap[_root_index]);

#ifdef _DEBUG
				SIMDMemSet(&_heap[_root_index + 1], MERGE_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif
		}
		// Next block is allocated, fix up pointers
		else
			_heap[_root_index]._right = right;
	}

	return IsFullyDefragmented( );
}