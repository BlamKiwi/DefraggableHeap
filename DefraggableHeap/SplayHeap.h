#pragma once

#include "DefraggablePointerList.h"
#include "HeapCommon.h"

class SplayHeader;

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