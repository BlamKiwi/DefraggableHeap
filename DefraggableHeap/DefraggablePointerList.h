#pragma once

#include "DefraggablePointerControlBlock.h"

/**
*	Manages a defraggable pointer list on behalf of a defraggable heap. 
*/
class DefraggablePointerList
{
public:
	/**
	*	Constructs a defraggable pointer list.
	*/
	DefraggablePointerList();

	/**
	*	Destroys a defraggable pointer list. 
	*/
	~DefraggablePointerList();

	/**
	*	Copying is undefined.
	*/
	DefraggablePointerList(const DefraggablePointerList &) = delete;

	/**
	*	Copying is undefined.
	*/
	DefraggablePointerList& operator=(const DefraggablePointerList &) = delete;

	/**
	*	Removes defraggable pointers that point into the given range of addresses.
	*
	*	@param lower_bound the inclusive lower bound that we should remove
	*	@param upper_bound the exclusive upper bound that we should remove
	*/
	void RemovePointersInRange(void* lower_bound, void* upper_bound);

	/**
	*	Offsets defraggable pointers that point into the given range of addresses.
	*
	*	@param lower_bound the inclusive lower bound that we should offset
	*	@param upper_bound the exclusive upper bound that we should offset
	*	@param offset the offset in bytes to change pointers by
	*/
	void OffsetPointersInRange(void* lower_bound, void* upper_bound, ptrdiff_t offset);

	/**
	*	Removes and invalidates all pointers in the managed pointer list.
	*/
	void RemoveAll();

	/**
	*	Creates a new defraggable pointer with the given starting address.
	*
	*	@returns the defraggable pointer
	*/
	DefraggablePointerControlBlock Create(void *data);

protected:
	/**< The root of the defraggable pointer management list. */
	DefraggablePointerControlBlock _pointer_root;
};