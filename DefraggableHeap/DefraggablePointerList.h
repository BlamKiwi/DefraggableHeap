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