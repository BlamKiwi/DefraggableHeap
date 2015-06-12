#include "stdafx.h"

#include "SplayHeap.h"
#include "AlignedAllocator.h"

#include "SIMDMem.h"

#include <cassert>
#include <new>
#include <algorithm>

#include <deque>

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
	new (&_heap[NULL_INDEX]) SplayHeader(NULL_INDEX, NULL_INDEX, 1, ALLOCATED);

	// Setup the splay header to a known initial state
	new (&_heap[SPLAY_HEADER_INDEX]) SplayHeader(NULL_INDEX, NULL_INDEX, 1, ALLOCATED);

	// Setup the root node
	_root_index = 2;
	_free_chunks = _num_chunks - 2; // Null, Splay, therefore -2
	new (&_heap[_root_index]) SplayHeader(NULL_INDEX, NULL_INDEX, _free_chunks, FREE);
	UpdateNodeStatistics(_heap[_root_index]);

	// Debug set free chunks in the heap
#ifdef _DEBUG
		SIMDMemSet(&_heap[_root_index + 1], INIT_PATTERN, _heap[_root_index]._block_metadata._num_chunks - 1);
#endif

		AssertHeapInvariants();
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
	AssertHeapInvariants();

	_pointer_list.RemoveAll();

	// Delete the system heap
	AlignedDelete(_heap);
}

float SplayHeap::FragmentationRatio() const
{
	AssertHeapInvariants();

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
	AssertHeapInvariants();

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
	new (&_heap[SPLAY_HEADER_INDEX]) SplayHeader(NULL_INDEX, NULL_INDEX, 1, ALLOCATED);
	IndexType left_tree_max = SPLAY_HEADER_INDEX, right_tree_min = SPLAY_HEADER_INDEX;
	IndexType last_splayed_node = SPLAY_HEADER_INDEX;

	// Simulate a null node with a reassignable value
	IndexType lut[] = { value, NULL_INDEX };
	auto lookup = [&](IndexType v) { 
		lut[1] = v;
		return lut[v > 0]; 
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

			// Add node to change list
			_heap [ t ]._max_contiguous_free_chunks = last_splayed_node;
			last_splayed_node = t;

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

			// Add node to change list
			_heap [ t ]._max_contiguous_free_chunks = last_splayed_node;
			last_splayed_node = t;

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

	// Rebuild left and right subtrees
	l._right = n._left;
	r._left = n._right;

	// Update statistics for nodes in changelist
	while ( last_splayed_node != SPLAY_HEADER_INDEX )
	{
		// Remember next item in list
		auto next = _heap [ last_splayed_node ]._max_contiguous_free_chunks;

		// Update node statistics
		UpdateNodeStatistics( _heap [ last_splayed_node ] );

		// Go to next item
		last_splayed_node = next;
	}

	// Rebuild tree root
	n._left = _heap[SPLAY_HEADER_INDEX]._right;
	n._right = _heap[SPLAY_HEADER_INDEX]._left;
	UpdateNodeStatistics(n);

	return t;
}

DefraggablePointerControlBlock SplayHeap::Allocate(size_t num_bytes)
{
	AssertHeapInvariants();
	// An allocation of 0 bytes is redundant
	if (!num_bytes)
		return nullptr;

	// Calculate the number of chunks required to fulfil the request
	const size_t mask = 16 - 1;
	const auto offset = (16 - (num_bytes & mask)) & mask;
	const IndexType required_chunks = ((num_bytes + offset) / 16) + 1;
	assert(required_chunks);

	// Do we have enough contiguous space for the allocation
	if (_heap[_root_index]._max_contiguous_free_chunks < required_chunks)
		return nullptr;

	// Splay the found free block to the root
	const auto free_block = FindFreeBlock(_root_index, required_chunks);
	assert(free_block);
	_root_index = Splay(free_block, _root_index);
	AssertHeapInvariants();

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

	AssertHeapInvariants();

	// Possible strict aliasing problem?
	return _pointer_list.Create(&_heap[old_index + 1]);
}

void SplayHeap::Free(DefraggablePointerControlBlock& ptr)
{
	AssertHeapInvariants();
	void* data = ptr.Get();

	// We cannot free the null pointer
	if (!data)
		return;

	// Get the offset of the pointer into the heap
	const auto block_addr = static_cast<SplayHeader*>(data);
	const std::ptrdiff_t offset = block_addr - _heap;

	// Is the offset in a valid range
	if ( offset < 0 || offset >= ptrdiff_t( _num_chunks ) )
		return;

	// Is the data pointer of expected alignment
	if (block_addr != &_heap[offset])
		return;

	// Splay the block to free to the root of the tree
	_root_index = Splay(offset - 1, _root_index);
	AssertHeapInvariants();

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

	AssertHeapInvariants();
}

void SplayHeap::FullDefrag()
{
	AssertHeapInvariants();
	while (!IterateHeap())
		;
	AssertHeapInvariants();
}

bool SplayHeap::IsFullyDefragmented() const
{
	AssertHeapInvariants();
	return _heap[_root_index]._max_contiguous_free_chunks == _free_chunks;
}

bool SplayHeap::IterateHeap()
{
	AssertHeapInvariants();
	// Do we actually need to defrag the heap
	if (IsFullyDefragmented())
		return true;

	// Splay the first free block in the heap to the root
	// This will put the fully defragmented subheap in the left subtree
	const auto free_block = FindFreeBlock(_root_index, 1);
	_root_index = Splay(free_block, _root_index);
	AssertHeapInvariants();
	
	// Splay the next block in the heap up from the right subtree
	auto right = Splay(_root_index + 1, _heap[_root_index]._right);
	assert(!_heap[right]._left);

	// Our heap invariant means the next block must be allocated
	assert(_heap[right]._block_metadata._is_allocated);

	auto &root = _heap[_root_index];
	auto &n = _heap[right];

	// Update defraggable pointers before invalidating the heap
	//OffsetPointersInRange(right, right + n._block_metadata._num_chunks, _root_index - right);
	_pointer_list.OffsetPointersInRange(&_heap[right], &_heap[right + n._block_metadata._num_chunks], (ptrdiff_t(_root_index) - ptrdiff_t(right)) * 16);

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

	AssertHeapInvariants();

	return IsFullyDefragmented( );
}
void SplayHeap::AssertHeapInvariants() const
{
#ifdef NDEBUG
	// We don't want to call this in release code
	return;
#endif

	/**
	*	Splay heap uses a null sentinel node to simplify some heap operations.
	*/
	{
		// Assert that the first block is the null node, is allocated and is one chunk large
		assert(_heap[NULL_INDEX]._left == NULL_INDEX);
		assert(_heap[NULL_INDEX]._right == NULL_INDEX);
		assert(_heap[NULL_INDEX]._block_metadata._is_allocated);
		assert(_heap[NULL_INDEX]._block_metadata._num_chunks == 1);
		assert(_heap[NULL_INDEX]._max_contiguous_free_chunks == 0);
	}

	/**
	*	Splay heap uses a splay header to perform top down splays. 
	*/
	{
		// Assert that the second block is the header, is allocated and is one chunk large
		assert(_heap[SPLAY_HEADER_INDEX]._block_metadata._is_allocated);
		assert(_heap[SPLAY_HEADER_INDEX]._block_metadata._num_chunks == 1);

		/* The children of the splay header may be arbitrary values. */
	}

	/**
	*	Splay heap uses block sizes to track position in the heap. The sum of all the metadata block sizes should be the raw size of the heap.
	*/
	{
		IndexType size = 0;
		while (size < _num_chunks)
		{
			// Add block size to sum
			size += _heap[size]._block_metadata._num_chunks;
		}

		// Assert size invariant
		assert(size == _num_chunks);
	}

	/**
	*	Splay heap follows the defraggable heap property that there are no two contiguous blocks free blocks.
	*/
	{
		bool starting_alloc = !_heap[2]._block_metadata._is_allocated;
		IndexType prev = 0; // Start at the null and header nodes, these should both follow these invariant
		IndexType current = 1;
		while (current < _num_chunks)
		{
			// Asert invariant
			if (!_heap[current]._block_metadata._is_allocated)
			{
				assert(_heap[prev]._block_metadata._is_allocated);
			}


			// Go to next block
			prev = current;
			current += _heap[current]._block_metadata._num_chunks;
		}
	}

	/**
	*	Splay heap tracks the total number of free chunks in the heap. This should be the sum of all the free block sizes.
	*/
	{
		IndexType size = 0;
		IndexType index = 0;
		while (index < _num_chunks)
		{
			// Add block size to sum
			if (!_heap[index]._block_metadata._is_allocated)
				size += _heap[index]._block_metadata._num_chunks;

			index += _heap[index]._block_metadata._num_chunks;
		}

		// Assert size invariant
		assert(size == _free_chunks);
	}

	/**
	*	Splay heap uses a tree structure to manage blocks. An inorder traversal should match the heap structure exactly.
	*/
	{
		// Flatten the tree
		std::deque<IndexType> tree;
		IndexType node = _root_index;
		IndexType current = 2;

		// Perform inorder traversal 
		while (true)
		{
			// Have we reached the null node
			if (node)
			{
				// Push current node on the stack to visit later
				tree.push_back(node);
				node = _heap[node]._left;
			}
			else
			{
				// We found a null node
				// Do we have more nodes to visit
				if (!tree.empty())
				{
					// We should be still inside the heap
					assert(current < _num_chunks);

					// Visit last node on stack
					node = tree.back();
					tree.pop_back();
					
					// Assert that the current positions match in the heap
					assert(current == node);

					// Advance down right subtree
					node = _heap[node]._right;

					// Advance raw heap position
					current += _heap[current]._block_metadata._num_chunks;
				}
				else
				{
					break;
				}
			}
		}

		// We should be at the end of the heap
		assert(current == _num_chunks);
	}

	/**
	*	Splay heap uses a max contiguous free chunk tree statistic to speed up searching. This maximum must be valid at all tree levels. 
	*/
	{
		// Push root node onto the tracking stack
		std::deque<IndexType> tree;
		std::deque<IndexType> max;
		tree.push_back(_root_index);
		IndexType prev = NULL_INDEX;

		// Perform pre order traversal and calculate heirachical max
		while (!tree.empty())
		{
			// Get the current node
			IndexType curr = tree.back();

			// Have we found an inner node
			if (!prev || _heap[prev]._left == curr || _heap[prev]._right == curr)
			{
				// Do we have a left subtree to traverse down
				if (_heap[curr]._left)
					tree.push_back(_heap[curr]._left);
				// Or do we have a right subtree to traverse down
				else if (_heap[curr]._right)
					tree.push_back(_heap[curr]._right);
			}
			// Have we found a previously visisted node
			else if (_heap[curr]._left == prev)
			{
				// Do we have a right subtree to visist
				if (_heap[curr]._right)
					tree.push_back(_heap[curr]._right);
			}
			else
			{
				// Visit the current node

				// Calculate the current max contiguous chunks at this node
				IndexType max_contiguous_chunks = 0;

				// Update max for current node
				if (!_heap[curr]._block_metadata._is_allocated)
					max_contiguous_chunks = _heap[curr]._block_metadata._num_chunks;

				// Do we have a right subtree
				if (_heap[curr]._right)
				{
					assert(!max.empty());

					// Update the current max based on previous calculated max
					max_contiguous_chunks = std::max(max_contiguous_chunks, max.back());

					// Consume the max value
					max.pop_back();
				}

				// Do we have a left subtree
				if (_heap[curr]._left)
				{
					assert(!max.empty());

					// Update the current max based on previous calculated max
					max_contiguous_chunks = std::max(max_contiguous_chunks, max.back());

					// Consume the max value
					max.pop_back();
				}

				// Assert that the cached and calculated maxes are the same
				assert(max_contiguous_chunks == _heap[curr]._max_contiguous_free_chunks);
				//if (max_contiguous_chunks != _heap[curr]._max_contiguous_free_chunks)
					//throw std::runtime_error("exec");

				// Push result to max stack
				max.push_back(max_contiguous_chunks);

				// Node visisted, remove from tree stack
				tree.pop_back();
			}

			// Note which node we visisted last
			prev = curr;
		}

		// The value at the top of the max stack should be the same as the cached root max
		assert(tree.empty());
		assert(!max.empty());
		assert(_heap[_root_index]._max_contiguous_free_chunks == max.back());
	}
}
