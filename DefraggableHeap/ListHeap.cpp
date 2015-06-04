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
	assert(!block._block_metadata._is_allocated);
	
	/* Split the free block into two, one allocated block and one free block */

	// Calculate the new raw free block size
	const auto raw_free_chunks = block._block_metadata._num_chunks
		- required_chunks;

	// Remove the found block from the freelist
	const auto prev_free = RemoveFreeBlock(found_block);

	// Set the found block so that it represents a now allocated block
	block._block_metadata = { ALLOCATED, required_chunks };

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
	_free_chunks -= required_chunks;

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