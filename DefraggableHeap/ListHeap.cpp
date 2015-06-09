#include "stdafx.h"

#include "ListHeap.h"
#include "ListHeader.h"
#include "AlignedAllocator.h"
#include "SIMDMem.h"

#include <iostream>

#include <cassert>
#include <new>
#include <algorithm>

#include <iterator>
#include <set>
#include <vector>

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

	// Allocate the system heap
	_heap = static_cast<ListHeader*>(AlignedNew(total_size, 16));

	// Setup the null sentinel node
	new (&_heap[NULL_INDEX]) ListHeader(NULL_INDEX, 1, 1, 1, ALLOCATED);

	// Setup the first free block
	const auto free = _num_chunks - 1;
	new (&_heap[1]) ListHeader(NULL_INDEX, NULL_INDEX, NULL_INDEX, free, FREE);

	// Setup heap tracking state
	_free_chunks = free;

	AssertHeapInvariants();
}

ListHeap::~ListHeap()
{
	AssertHeapInvariants();

	_pointer_list.RemoveAll();

	// Delete the system heap
	AlignedDelete(_heap);
}

float ListHeap::FragmentationRatio() const
{
	AssertHeapInvariants();

	// Heap is not fragmented if we are at full load
	// This doesn't take into account free blocks of 0 size that might exist
	if (!_free_chunks)
		return 0.0f;

	// Get free chunks statistics
	IndexType max_contiguous_free_chunks = 0;
	IndexType t = _heap[NULL_INDEX]._next_free;

	while (t != NULL_INDEX)
	{
		assert(!_heap[t]._block_metadata._is_allocated);

		max_contiguous_free_chunks = std::max(max_contiguous_free_chunks,
			_heap[t]._block_metadata._num_chunks);

		t = _heap[t]._next_free;
	}

	const auto free_max = static_cast<float>(max_contiguous_free_chunks);
	const auto free = static_cast<float>(_free_chunks);

	// Calculate free chunks ratio to determine fragmentation
	return (free - free_max) / free;
}


bool ListHeap::IsFullyDefragmented() const
{
	AssertHeapInvariants();

	// The heap is fully defragmented if there is <2 free blocks
	return _heap[NULL_INDEX]._next_free == _heap[NULL_INDEX]._prev_free;
}

IndexType ListHeap::FindFreeBlock( IndexType num_chunks) const
{
	AssertHeapInvariants();

	// Start searching at the first non null node
	IndexType block = _heap[NULL_INDEX]._next_free;

	// Iterate through the freelist until we find a block big enough
	while (block != NULL_INDEX && 
		_heap[block]._block_metadata._num_chunks < num_chunks)
	{
		assert(!_heap[block]._block_metadata._is_allocated);

		// Advance the list index
		block = _heap[block]._next_free;
	}

	// Return the found block
	return block;
}

DefraggablePointerControlBlock ListHeap::Allocate(size_t num_bytes)
{
	AssertHeapInvariants();

	// An allocation of 0 bytes is redundant
	if (!num_bytes)
		return nullptr;

	// Calculate the number of chunks required to fulfil the request
	const IndexType mask = 16 - 1;
	const IndexType offset = (16 - (num_bytes & mask)) & mask;
	const IndexType required_chunks = ((num_bytes + offset) / 16) + 1;
	assert(required_chunks);

	// Try find a suitable free block
	const auto found_block = FindFreeBlock(required_chunks);
	auto &block = _heap[found_block];

	// Did we fail to find a suitable free block
	if (found_block == NULL_INDEX)
		return nullptr;
	
	/* Split the free block into two, one allocated block and one free block */

	// Calculate the new raw free block size
	const auto raw_free_chunks = block._block_metadata._num_chunks
		- required_chunks;

	// Remove the found block from the freelist
	const auto prev_free = RemoveFreeBlock(found_block);

	// Set the found block so that it represents a now allocated block
	block._block_metadata = { ALLOCATED, required_chunks };
	_free_chunks -= required_chunks;

#ifdef _DEBUG
		SIMDMemSet(&block + 1, ALLOC_PATTERN, block._block_metadata._num_chunks - 1);
#endif

	// Is there a new free block to add back to the list
	if (raw_free_chunks)
	{
		// Calculate the new free block offset
		const auto new_free_index = found_block + required_chunks;

		// Create the header for the new free block
		// New address value will always be bigger since 
		// we allocate to the lower adresses
		new (&_heap[new_free_index]) ListHeader(
			found_block,
			NULL_INDEX,
			NULL_INDEX,
			raw_free_chunks,
			FREE
			);

		// Insert new free block into the free list
		InsertFreeBlock(prev_free, new_free_index);

		// Restore previous cycle of heap
		IndexType next = new_free_index + _heap[new_free_index]._block_metadata._num_chunks;
		if (next < _num_chunks)
			_heap[next]._prev = new_free_index;

#ifdef _DEBUG
			SIMDMemSet(&_heap[new_free_index + 1], SPLIT_PATTERN, _heap[new_free_index]._block_metadata._num_chunks - 1);
#endif
	}

	AssertHeapInvariants();

	// Possible strict aliasing problem?
	return _pointer_list.Create(&block + 1);
}

IndexType ListHeap::RemoveFreeBlock(IndexType index)
{
	assert(!_heap[index]._block_metadata._is_allocated);

	// Get blocks and offsets before modification
	auto &block = _heap[index];
	const auto prev_free = block._prev_free;

	// Remove the block from the free list
	_heap[block._next_free]._prev_free = prev_free;
	_heap[block._prev_free]._next_free = block._next_free;

	return prev_free;
}

void ListHeap::InsertFreeBlock(IndexType root, IndexType index)
{
	assert(!_heap[root]._block_metadata._is_allocated || root == NULL_INDEX);
	assert(!_heap[index]._block_metadata._is_allocated);
	assert(root != index);

	// Bind free nodes to be modified
	auto &r = _heap[root];
	auto &n = _heap[r._next_free];
	auto &i = _heap[index];

	// Would this insert invalidate odering of the list
	assert(root < index && (index < r._next_free || r._next_free == NULL_INDEX));

	// Modify forward chain into list
	i._next_free = r._next_free;
	r._next_free = index;
	
	// Modify backwards chain in list
	i._prev_free = root;
	n._prev_free = index;
}

void ListHeap::Free(DefraggablePointerControlBlock &ptr)
{
	AssertHeapInvariants();

	void* data = ptr.Get();

	// We cannot free the null pointer
	if (!data)
		return;

	// Get the offset of the pointer into the heap
	const auto block_addr = static_cast<ListHeader*>(data);
	const std::ptrdiff_t offset = block_addr - _heap;

	// Is the offset in a valid range
	if (offset < 0 || offset >= _num_chunks)
		return;

	// Is the data pointer of expected alignment
	if (block_addr != &_heap[offset])
		return;

	// Mark the block as being free
	const auto new_offset = offset - 1;
	auto &block = _heap[new_offset];
	block._block_metadata._is_allocated = FREE;
	_free_chunks += block._block_metadata._num_chunks;

	// Insert now free block into the freelist
	const auto prev_free = FindNearestFreeBlock(new_offset);
	InsertFreeBlock(prev_free, new_offset);

	// Invalidate defraggable pointers that point into the root block before we invalidate data in the heap
	_pointer_list.RemovePointersInRange(&block, &block + block._block_metadata._num_chunks);

#ifdef _DEBUG
	SIMDMemSet(&block + 1, FREED_PATTERN, block._block_metadata._num_chunks - 1);
#endif

	// Track which nodes we modify last so we can restore the heap invariants
	IndexType last_modified_node = new_offset;

	// We possibly invalidated our heap invariant
	// Does the right heap contain any potential free blocks
	if (block._next_free == new_offset + block._block_metadata._num_chunks &&
		block._next_free != NULL_INDEX)
	{
		// Bind the next block
		auto &next = _heap[block._next_free];
		assert(!next._block_metadata._is_allocated);

		// Remove the next block from the free list
		RemoveFreeBlock(block._next_free);

		// Grow the current free block 
		block._block_metadata._num_chunks += next._block_metadata._num_chunks;

#ifdef _DEBUG
		SIMDMemSet(&block + 1, MERGE_PATTERN, block._block_metadata._num_chunks - 1);
#endif
	}

	// Does the left heap contain any potential free blocks
	if (block._prev_free == block._prev &&
		block._prev_free != NULL_INDEX)
	{
		// Bind the previous block
		auto &prev = _heap[block._prev_free];
		assert(!prev._block_metadata._is_allocated);

		// Remove the block from the free list
		RemoveFreeBlock(new_offset);

		// Remove block from the heap
		const auto next_offset = new_offset + block._block_metadata._num_chunks;
		if ( next_offset != _num_chunks)
			_heap[next_offset]._prev = block._prev;

		// Grow the previous free block 
		prev._block_metadata._num_chunks += block._block_metadata._num_chunks;

		// Update which node we modified last
		last_modified_node = block._prev_free;

#ifdef _DEBUG
		SIMDMemSet(&prev + 1, MERGE_PATTERN, prev._block_metadata._num_chunks - 1);
#endif
	}

	// Restore previous cycle of heap
	IndexType next = last_modified_node + _heap[last_modified_node]._block_metadata._num_chunks;
	if (next < _num_chunks)
		_heap[next]._prev = last_modified_node;

	AssertHeapInvariants();
}

IndexType ListHeap::FindNearestFreeBlock(IndexType index) const
{
	// Start searching at the first non null node
	IndexType block = _heap[NULL_INDEX]._next_free;

	// Iterate through the freelist until with find a block with a larger offset
	while (block != NULL_INDEX &&
		index > block )
	{
		assert(!_heap[block]._block_metadata._is_allocated);

		// Advance the list index
		block = _heap[block]._next_free;
	}

	// Return the previous of the found block
	return _heap[block]._prev_free;
}

void ListHeap::FullDefrag()
{
	AssertHeapInvariants();

	while (!IterateHeap())
		;

	AssertHeapInvariants();
}


bool ListHeap::IterateHeap()
{
	AssertHeapInvariants();

	// Do we actually need to defrag the heap
	if (IsFullyDefragmented())
		return true;

	// Get the first free block in the heap
	auto free_block = _heap[NULL_INDEX]._next_free;
	auto &f = _heap[free_block];
	assert(free_block != NULL_INDEX);
	assert(!f._block_metadata._is_allocated);

	// If the next block points out of the heap, we are fully defragmented
	auto alloc_block = free_block + f._block_metadata._num_chunks;
	
	if (alloc_block == _num_chunks)
		return true;

	// Bind the next allocated block in the heap
	auto &a = _heap[alloc_block];
	assert(a._block_metadata._is_allocated);

	// Remove freeblock from the free list
	const auto prev_free = RemoveFreeBlock(free_block);

	// Update defraggable pointers before invalidating the heap
	_pointer_list.OffsetPointersInRange(&a, &a + a._block_metadata._num_chunks, (ptrdiff_t(free_block) - ptrdiff_t(alloc_block)) * 16);

	// Create new free block header
	ListHeader new_free(free_block, NULL_INDEX, NULL_INDEX, f._block_metadata._num_chunks, FREE);
	const auto new_free_offset = free_block + a._block_metadata._num_chunks;

	// Create new allocated block header
	ListHeader new_allocated(f._prev, NULL_INDEX, NULL_INDEX, a._block_metadata._num_chunks, ALLOCATED);

	/* CONSIDER THE HEAP INVALID FROM HERE */

	// Copy new allocated block header and move the data
	SIMDMemCopy(&f, &new_allocated, 1);
	SIMDMemCopy(&f + 1, &a + 1,
		new_allocated._block_metadata._num_chunks - 1);

	// Copy new free block header
	SIMDMemCopy(&_heap[new_free_offset], &new_free, 1);

	/* HEAP IS NOW VALID */

	// Add new free block to the free list
	InsertFreeBlock(prev_free, new_free_offset);

#ifdef _DEBUG
	SIMDMemSet(&_heap[new_free_offset + 1], MOVE_PATTERN, _heap[new_free_offset]._block_metadata._num_chunks - 1);
#endif

	// We possibly invalidated our heap invariant
	// Does the right heap contain any potential free blocks
	auto &block = _heap[new_free_offset];
	if (block._next_free == new_free_offset + block._block_metadata._num_chunks &&
		block._next_free != NULL_INDEX)
	{
		// Bind the next block
		auto &next = _heap[block._next_free];
		assert(!next._block_metadata._is_allocated);

		// Remove the next block from the free list
		RemoveFreeBlock(block._next_free);

		// Grow the current free block 
		block._block_metadata._num_chunks += next._block_metadata._num_chunks;

		// Restore previous cycle of heap
		IndexType node = new_free_offset + _heap[new_free_offset]._block_metadata._num_chunks;
		if (node < _num_chunks)
			_heap[node]._prev = new_free_offset;

#ifdef _DEBUG
		SIMDMemSet(&block + 1, MERGE_PATTERN, block._block_metadata._num_chunks - 1);
#endif
	}

	AssertHeapInvariants();

	return IsFullyDefragmented();
}

void ListHeap::AssertHeapInvariants() const
{
#ifdef _NDEBUG
	// We don't want to call this in release code
	return;
#endif

	/**
	*	List heap uses a null sentinel node to simplify some heap operations.
	*/
	{
		// Assert that the first block is the null node, is allocated and is one chunk large
		assert(_heap[NULL_INDEX]._prev == NULL_INDEX);
		assert(_heap[NULL_INDEX]._block_metadata._is_allocated);
		assert(_heap[NULL_INDEX]._block_metadata._num_chunks == 1);
	}

	/**
	*	List heap uses block sizes to track position in the heap. The sum of all the metadata block sizes should be the raw size of the heap.
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
	*	List heap keeps track of the previous block so we can navigate backwards.
	*/
	{
		IndexType prev = 0;
		IndexType current = 0;
		do
		{
			// Assert previous invariant
			assert(_heap[current]._prev == prev);

			prev = current;
			current += _heap[current]._block_metadata._num_chunks;
		} while (current < _num_chunks);
	}

	/**
	*	List heap follows the defraggable heap property that there are no two contiguous blocks free blocks (excluding any sentinel blocks).
	*/
	{
		bool starting_alloc = !_heap[1]._block_metadata._is_allocated;
		IndexType index = 1; // Start at not the null node
		while (index < _num_chunks)
		{
			// Asert invariant
			if (!_heap[index]._block_metadata._is_allocated)
			{
				assert(_heap[_heap[index]._prev]._block_metadata._is_allocated);
			}


			// Go to next block
			index += _heap[index]._block_metadata._num_chunks;
		}
	}

	/**
	*	List heap tracks the total number of free chunks in the heap. This should be the sum of all the free block sizes.
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
	*	List heap uses a free list to track the free blocks in the heap. Apart from the null node, only free blocks are in the list
	*/
	{
		IndexType node = _heap[NULL_INDEX]._next_free;
		while (node != NULL_INDEX)
		{
			assert(!_heap[node]._block_metadata._is_allocated);
			node = _heap[node]._next_free;
		}
	}

	/**
	*	List heap free list should contain ALL and ONLY the free list blocks in the heap
	*/
	{
		std::set<IndexType> free_list;
		std::set<IndexType> heap;

		// Collect free blocks in the heap
		IndexType index = 1;
		while (index < _num_chunks)
		{
			if (!_heap[index]._block_metadata._is_allocated)
				heap.insert(index);

			index += _heap[index]._block_metadata._num_chunks;
		}

		// Collect free blocks in the free list
		IndexType node = _heap[NULL_INDEX]._next_free;
		while (node != NULL_INDEX)
		{
			// Add block size to sum
			free_list.insert(node);

			node = _heap[node]._next_free;
		}

		// The difference between the freelist and raw free nodes in the heap should be the empty set
		std::vector<IndexType> out;
		std::set_symmetric_difference(free_list.begin(), free_list.end(), heap.begin(), heap.end(), std::insert_iterator<std::vector<IndexType>>(out, out.begin()));
		assert(out.empty());
	}

	/**
	*	List heap free list cycles should be symmetric
	*/
	{
		std::vector<IndexType> next;
		std::vector<IndexType> prev;

		IndexType node = _heap[NULL_INDEX]._next_free;
		while (node != NULL_INDEX)
		{
			next.push_back(node);

			node = _heap[node]._next_free;
		} 

		node = _heap[NULL_INDEX]._prev_free;
		do
		{
			prev.push_back(node);

			node = _heap[node]._prev_free;
		} while (node != NULL_INDEX);

		auto fit = next.begin();
		auto bit = prev.rbegin();
		for (; fit != next.end() && bit != prev.rend(); fit++, bit++)
			assert(*fit == *bit);
	}

	/**
	*	List heap free list next cycle should be in increasing order
	*/
	{
		IndexType node = NULL_INDEX;
		IndexType next = _heap[NULL_INDEX]._next_free;
		while (next != NULL_INDEX)
		{
			assert(node < next);
			node = next;
			next = _heap[next]._next_free;
		}
	}

	/**
	*	List heap free list prev cycle should be in decreasing order
	*/
	{
		IndexType node = _heap[NULL_INDEX]._prev_free;
		IndexType prev = _heap[node]._prev_free;
		while (prev != NULL_INDEX)
		{
			assert(prev < node);
			node = prev;
			prev = _heap[prev]._prev_free;
		}
	}
}