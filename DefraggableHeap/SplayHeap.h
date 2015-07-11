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

#include "DefraggablePointerList.h"
#include "HeapCommon.h"

struct SplayHeader;

/**
*	A defraggable heap implemented as a splay tree.
*/
class SplayHeap final
{

public:

	/**
	*	Constructs a splay heap.
	*
	*	@param size the size of the heap in bytes.
	*/
	SplayHeap(size_t size);

	/**
	*	Destroys a splay heap.
	*/
	~SplayHeap();

	/**
	*	Allocates from the splay heap. Always 16 byte aligned.
	*
	*	@param num_bytes the number of bytes to allocated
	*	@returns the pointer to allocated memory
	*/
	DefraggablePointerControlBlock Allocate(size_t num_bytes);

	/**
	*	Frees the given heap data. Invalidates all defraggable pointers
	*	pointing into the free block.
	*
	*	@param ptr pointer into block in heap to free
	*/
	void Free(DefraggablePointerControlBlock &ptr);

	/**
	*	Fully Defragments the heap.
	*/
	void FullDefrag();

	/**
	*	Iterates the defragmentation process on the heap.
	*	Heap is still valid for use after a call to this method. 
	*
	*	@returns true if the heap is now fully defragmented
	*/
	bool IterateHeap();

	/**
	*	Gets the fragmentation ratio of the heap.
	*
	*	@returns 0 if no fragmentation, 1 if fully fragmented
	*/
	float FragmentationRatio() const;

	/**
	*	Gets if the heap is fully defragmented.
	*
	*	@returns true if fully defragmented, false if there is fragmentation
	*/
	bool IsFullyDefragmented() const;

protected:

	/**
	*	Finds a free heap block of desired size.
	*
	*	@param t the node to start the search at
	*	@param num_chunks the minimum number of chunks required in the free block
	*	@returns the free block
	*/
	IndexType FindFreeBlock( IndexType t, IndexType num_chunks ) const;

	/**
	*	Splays the given value to the root of the tree.
	*
	*	@param value the node to splay
	*	@param t the node to start the splay from
	*/
	IndexType Splay(IndexType value, IndexType t);

	/**
	*	Updates the node statistics of a given node.
	*
	*	@param node the node to update the statistics for
	*/
	void UpdateNodeStatistics( SplayHeader &node );

	/**
	*	Rotates the given tree node with its left child.
	*
	*	@param k2 the node to rotate
	*	@returns the index of the new parent
	*/
	IndexType RotateWithLeftChild(IndexType k2);

	/**
	*	Rotates the given tree node with its right child.
	*
	*	@param k2 the node to rotate
	*	@returns the index of the new parent
	*/
	IndexType RotateWithRightChild(IndexType k1);

	/**
	*	Asserts invariants over the heap.
	*/
	void AssertHeapInvariants() const;

	/**< The data heap we manage. */
	SplayHeader* _heap;

	/**< The number of chunks in the heap. */
	IndexType _num_chunks;

	/**< The root of the splay tree. */
	IndexType _root_index;

	/**< The total number of free chunks in the heap. */
	IndexType _free_chunks;

	/**< The list of defraggable pointers for this heap. */
	DefraggablePointerList _pointer_list;

	/**< The offset of the null sentinel node into the heap. */
	static const IndexType NULL_INDEX = 0;

	/**< The offset of the splay header node into the heap. */
	static const IndexType SPLAY_HEADER_INDEX = 1;
};