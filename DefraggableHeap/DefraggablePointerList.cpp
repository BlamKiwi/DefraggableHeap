/*
Copyright (c) 2015, Missing Box Studio
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "stdafx.h"

#include "DefraggablePointerList.h"

DefraggablePointerList::DefraggablePointerList()
	: _pointer_root(nullptr, &_pointer_root, &_pointer_root)
{

}

DefraggablePointerList::~DefraggablePointerList()
{
	RemoveAll();
}

void DefraggablePointerList::RemoveAll()
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