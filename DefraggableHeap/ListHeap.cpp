#include "stdafx.h"

#include "ListHeap.h"
#include "ListHeader.h"
#include "AlignedAllocator.h"
#include "SIMDMem.h"

#include <cassert>
#include <new>
#include <algorithm>

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
	_free_chunks = _max_contiguous_free_chunks = free;
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

IndexType ListHeap::FindFreeBlock( IndexType num_chunks) const
{
	// Is there even enough space in the heap to make an allocation
	if (_max_contiguous_free_chunks < num_chunks)
		return NULL_INDEX;

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
	// An allocation of 0 bytes is redundant
	if (!num_bytes)
		return nullptr;

	// Calculate the number of chunks required to fulfil the request
	const size_t mask = 16 - 1;
	const auto offset = (16 - (num_bytes & mask)) & mask;
	const auto required_chunks = ((num_bytes + offset) / 16) + 1;
	assert(required_chunks);

	// Do we have enough contiguous space for the allocation
	if (_max_contiguous_free_chunks < required_chunks)
		return nullptr;

	// Find a suitable free block
	const auto found_block = FindFreeBlock(required_chunks);
	auto &block = _heap[found_block];
	assert(found_block != NULL_INDEX);
	
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

#ifdef _DEBUG
			SIMDMemSet(&_heap[new_free_index + 1], SPLIT_PATTERN, _heap[new_free_index]._block_metadata._num_chunks - 1);
#endif
	}

	// Update heap statistics
	UpdateMaxFreeContiguousChunks();

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

void ListHeap::UpdateMaxFreeContiguousChunks()
{
	// Start searching at the first non null node
	IndexType block = _heap[NULL_INDEX]._next_free;

	// Reset max to minimum valid value
	_max_contiguous_free_chunks = 0;

	// Iterate through the freelist 
	while (block != NULL_INDEX)
	{
		assert(!_heap[block]._block_metadata._is_allocated);

		// Get the local max contiguous free block size
		_max_contiguous_free_chunks = std::max(_max_contiguous_free_chunks, 
			_heap[block]._block_metadata._num_chunks);

		// Advance the list index
		block = _heap[block]._next_free;
	}
}

void ListHeap::Free(DefraggablePointerControlBlock &ptr)
{
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

#ifdef _DEBUG
		SIMDMemSet(&prev + 1, MERGE_PATTERN, prev._block_metadata._num_chunks - 1);
#endif
	}

	// Update heap statistics
	UpdateMaxFreeContiguousChunks();
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
	while (!IterateHeap(false))
		;

	// Update heap statistics
	UpdateMaxFreeContiguousChunks();
}

bool ListHeap::IterateHeap()
{
	return IterateHeap(true);
}

bool ListHeap::IterateHeap(const bool update_stats)
{
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

#ifdef _DEBUG
		SIMDMemSet(&block + 1, MERGE_PATTERN, block._block_metadata._num_chunks - 1);
#endif
	}

	// Update heap statistics
	if (update_stats)
		UpdateMaxFreeContiguousChunks();

	return IsFullyDefragmented();
}