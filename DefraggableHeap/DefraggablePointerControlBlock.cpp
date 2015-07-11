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