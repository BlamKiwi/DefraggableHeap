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