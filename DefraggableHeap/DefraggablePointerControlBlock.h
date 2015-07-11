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

#include <cstdint>
#include <type_traits>

/**
*	A control block that allows smart pointers to point into defraggable heaps. 
*
*	Defraggable Pointer Control Blocks are defined as circular, relatively managed linked lists. 
*	This simplifies management operations while allowing us to well define copy and move operations.
*/
class DefraggablePointerControlBlock final
{
	/**< Allow control list to manipulate the list pointers directly. */
	friend class DefraggablePointerList;

public:

	/**
	*	Constructs a null defraggable pointer.
	*/
	DefraggablePointerControlBlock();

	/**
	*	Destroys a defraggable pointer.
	*/
	~DefraggablePointerControlBlock();

	/**
	*	Constructs a defraggable pointer with the given values.
	*/
	DefraggablePointerControlBlock(void* data, DefraggablePointerControlBlock *next, DefraggablePointerControlBlock *prev);

	/**
	*	Constructs a null defraggable pointer.
	*/
	DefraggablePointerControlBlock(nullptr_t);

	/**
	*	Copy constructor for defraggable pointers. 
	*
	*	@param other the pointer to copy.
	*/
	DefraggablePointerControlBlock(DefraggablePointerControlBlock &other);

	/**
	*	Move constructor for defraggable pointers.
	*
	*	@param other the pointer to move.
	*/
	DefraggablePointerControlBlock(DefraggablePointerControlBlock &&other);

	/**
	*	Copy assignment for defraggable pointers.
	*
	*	@param other the pointer to copy.
	*	@returns this object
	*/
	DefraggablePointerControlBlock& operator=(DefraggablePointerControlBlock &other);

	/**
	*	Move assignment for defraggable pointers.
	*
	*	@param other the pointer to move.
	*	@returns this object
	*/
	DefraggablePointerControlBlock& operator=(DefraggablePointerControlBlock &&other);

	/**
	*	Move assignment for defraggable pointers.
	*
	*	@param other the pointer to move.
	*	@returns this object
	*/
	DefraggablePointerControlBlock& operator=(nullptr_t);


	/**
	*	Gets the managed pointer.
	*
	*	@returns the managed pointer at its current value
	*/
	void* Get();

	/**
	*	Sets the managed pointer.
	*
	*	@param data the new managed pointer value
	*/
	void Set(void* data);

	/**
	*	Converts the defraggable pointer to a bool value.
	*
	*	@returns true if the defraggable pointer isn't equivalent to a null pointer.
	*/
	operator bool();

protected:

	/**
	*	Inserts this defraggable pointer into the management list.
	*
	*	@param reference object into the list
	*/
	void Insert(DefraggablePointerControlBlock &other);

	/**
	*	Removes the defraggable pointer from the management list and makes it null.
	*/
	void Remove();

	/**< The native pointer we manage. */
	void *_data;

	/**< The next defraggable pointer in the management list. */
	DefraggablePointerControlBlock *_next;

	/**< The previous defraggable pointer in the management list. */
	DefraggablePointerControlBlock *_prev;
};

namespace std
{
	template <>
	struct is_trivially_copyable < DefraggablePointerControlBlock >
	{
		static const bool value = false;
	};
}