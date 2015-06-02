#pragma once

/**
*	The index type for defraggable heap blocks.
*/
typedef uint32_t IndexType;

/**
*	The allocation states for defraggable heap blocks.
*/
enum AllocationState : IndexType
{
	FREE = 0, ALLOCATED = 1
};

/**
*	Defines general metadata bitfield for block headers to use. 
*/
struct BlockMetadata
{
	/**
	*	The number of chunks gets cannibalized for allocation metadata as we access
	*	it less compared to max contiguous free chunks in a splay operation.
	*/

	/**< Is the block allocated or unallocated. */
	IndexType _is_allocated : 1;

	/**< The number of chunks in the block we represent (including the header). */
	IndexType _num_chunks : 31;

};

static_assert(sizeof(BlockMetadata) == sizeof(IndexType), "The metadata field must be the size of the index type.");


/**< The pattern initial blocks should be set to */
const int INIT_PATTERN = 0x12345678;

/**< The pattern allocated blocks should be set to */
const int ALLOC_PATTERN = 0xACACACAC;

/**< The pattern merged blocks should be set to */
const int MERGE_PATTERN = 0xDDDDDDDD;

/**< The pattern freed blocks should be set to */
const int FREED_PATTERN = 0xFEEFEEFE;

/**< The pattern moved freed blocks should be set to */
const int MOVE_PATTERN = 0xDEADB0B1;

/**< The pattern free blocks from an allocation split should be set to. */
const int SPLIT_PATTERN = 0x51775177;