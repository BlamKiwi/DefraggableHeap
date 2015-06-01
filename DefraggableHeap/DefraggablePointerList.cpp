#include "stdafx.h"

#include "DefraggablePointerList.h"

DefraggablePointerList::DefraggablePointerList()
	: _pointer_root(nullptr, &_pointer_root, &_pointer_root)
{

}

DefraggablePointerList::~DefraggablePointerList()
{
	DefraggablePointerControlBlock *node = _pointer_root._next;

	// Null out all pointers we manage
	while (node != &_pointer_root)
	{
		// Cache the current next pointer
		auto *next = node->_next;

		node->Remove();

		// Go to the next pointer
		node = next;
	}
}

DefraggablePointerControlBlock DefraggablePointerList::Create(void* data)
{
	DefraggablePointerControlBlock new_ptr(_pointer_root);
	new_ptr.Set(data);
	return new_ptr;
}

void DefraggablePointerList::RemovePointersInRange(void* lower_bound, void* upper_bound)
{
	// Get bounds as raw address values
	intptr_t lower = intptr_t(lower_bound);
	intptr_t upper = intptr_t(upper_bound);

	DefraggablePointerControlBlock *n = &_pointer_root;

	// While we have not wrapped around to the beginning in the management list
	// Remove pointers that point into the range specified
	while (n->_next != &_pointer_root)
	{
		auto next = n->_next;
		intptr_t addr = intptr_t(next->_data);

		// Does the address lie in the range to remove
		if (addr >= lower && addr < upper)
		{
			// Remove the node from the list
			next->Remove();
		}
		else
			// Go to the next node
			n = next;
	}
}

void DefraggablePointerList::OffsetPointersInRange(void* lower_bound, void* upper_bound, intptr_t offset)
{
	// Get bounds as raw address values
	intptr_t lower = intptr_t(lower_bound);
	intptr_t upper = intptr_t(upper_bound);

	DefraggablePointerControlBlock *n = &_pointer_root;

	// While we have not wrapped around to the beginning in the management list
	// Offset pointers that point into the range specified
	do
	{
		// Cache the next pointer before modifying the current list item
		auto next = n->_next;

		// Does the data address lie in the range to offset
		intptr_t addr = intptr_t(n->_data);
		if (addr >= lower && addr < upper)
			n->_data = reinterpret_cast<void*>(addr + offset);

		// Does the next address lie in the range to offset
		addr = intptr_t(n->_next);
		if (addr >= lower && addr < upper)
			n->_next = reinterpret_cast<DefraggablePointerControlBlock*>(addr + offset);

		// Does the previous address lie in the range to offset
		addr = intptr_t(n->_prev);
		if (addr >= lower && addr < upper)
			n->_prev = reinterpret_cast<DefraggablePointerControlBlock*>(addr + offset);

		// Go to the next node
		n = next;
	} while (n != &_pointer_root);
}