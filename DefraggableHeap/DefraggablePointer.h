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
	*	Const copying defraggable pointers is disallowed. 
	*	Inserting a new pointer requires modifying the list members. 
	*/
	DefraggablePointerControlBlock(const DefraggablePointerControlBlock &) = delete;

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
	*	Const copying defraggable pointers is disallowed.
	*	Inserting a new pointer requires modifying the list members.
	*/
	DefraggablePointerControlBlock& operator=(const DefraggablePointerControlBlock &) = delete;

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