#include "stdafx.h"

#include "DefraggablePointerControlBlock.h"

#include <cassert>

DefraggablePointerControlBlock::DefraggablePointerControlBlock()
	: _data(nullptr)
	, _next(nullptr)
	, _prev(nullptr)
{

}

DefraggablePointerControlBlock::~DefraggablePointerControlBlock()
{
	Remove();
}

DefraggablePointerControlBlock::DefraggablePointerControlBlock(void* data, DefraggablePointerControlBlock *next, DefraggablePointerControlBlock *prev)
	: _data(data)
	, _next(next)
	, _prev(prev)
{
	assert(_next);
	assert(_prev);
}

DefraggablePointerControlBlock::DefraggablePointerControlBlock(std::nullptr_t)
	: DefraggablePointerControlBlock()
{

}

DefraggablePointerControlBlock::DefraggablePointerControlBlock(DefraggablePointerControlBlock &other)
{
	Insert(other);
}

DefraggablePointerControlBlock::DefraggablePointerControlBlock(DefraggablePointerControlBlock &&other)
	: DefraggablePointerControlBlock(other)
{
	// Make the moved object null
	other = nullptr;
}

DefraggablePointerControlBlock& DefraggablePointerControlBlock::operator=(DefraggablePointerControlBlock &other)
{
	// Make sure we are copying a unqiue value
	if (this != &other)
	{
		Insert(other);
	}

	return *this;
}

void DefraggablePointerControlBlock::Insert(DefraggablePointerControlBlock &other)
{
	// Does the reference pointer actually belong to a list to insert into
	if (other._prev && other._next)
	{
		// Copy over new pointers
		_data = (other._data);
		_prev = (other._prev);
		_next = (&other);

		// Insert new pointer into management list
		_prev->_next = this;
		_next->_prev = this;
	}
	else
	{
		// Make us a null pointer
		_data = nullptr;
		_prev = nullptr;
		_next = nullptr;
	}
}

DefraggablePointerControlBlock& DefraggablePointerControlBlock::operator=(DefraggablePointerControlBlock &&other)
{
	// Make sure we are copying a unqiue value
	if (this != &other)
	{
		// Insert ourselves into the management list
		Insert(other);

		// Make other a null pointer
		other = nullptr;
	}

	return *this;
}

DefraggablePointerControlBlock& DefraggablePointerControlBlock::operator=(nullptr_t)
{
	Remove();

	return *this;
}

void DefraggablePointerControlBlock::Remove()
{
	// Do we actually belong to a list to remove from
	if (_prev && _next)
	{
		_next->_prev = _prev;
		_prev->_next = _next;
	}

	// Make us a null pointer
	_data = nullptr;
	_prev = nullptr;
	_next = nullptr;
}

void* DefraggablePointerControlBlock::Get()
{
	return _data;
}

void DefraggablePointerControlBlock::Set(void* data)
{
	_data = data;
}

DefraggablePointerControlBlock::operator bool()
{
	return _data && _prev && _next;
}