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

