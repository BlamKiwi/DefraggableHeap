#include "DefraggablePointerList.h"
#include "HeapCommon.h"

#include <tuple>

class ListHeader;

/**
*	A defraggable heap implemented as a doubly linked list.
*/
class ListHeap final
{

public:

	/**
	*	Constructs a list heap.
	*
	*	@param size the size of the heap in bytes.
	*/
	ListHeap(size_t size);

	/**
	*	Destroys a list heap.
	*/
	~ListHeap();

	/**
	*	Allocates from the list heap. Always 16 byte aligned.
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
	*	@param num_chunks the minimum number of chunks required in the free block
	*	@returns the free block index
	*/
	IndexType FindFreeBlock(IndexType num_chunks) const;

	/**
	*	Removes the free block from the free list. 
	*
	*	@param index the index of the free block
	*	@returns the index of the previous free block in the heap
	*/
	IndexType RemoveFreeBlock(IndexType index);

	/**
	*	Inserts the free block from the free list after the given
	*	root node.
	*
	*	@param root the index of the root node
	*	@param index the index of the free block
	*/
	void InsertFreeBlock(IndexType root, IndexType index);

	/**
	*	Updates the max free contiguous chunks value. 
	*/
	void UpdateMaxFreeContiguousChunks();

	/**
	*	Finds the the nearest free block with a smaller offset in the freelist.
	*
	*	@param index the reference block index to find
	*	@returns the nearest free block 
	*/
	IndexType FindNearestFreeBlock(IndexType index) const;

	/**< The data heap we manage. */
	ListHeader* _heap;

	/**< The number of chunks in the heap. */
	IndexType _num_chunks;

	/**< The total number of free chunks in the heap. */
	IndexType _free_chunks;

	/**< The maximum number of contiguous chunks in the heap. */
	IndexType _max_contiguous_free_chunks;

	/**< The list of defraggable pointers for this heap. */
	DefraggablePointerList _pointer_list;

	/**< The offset of the null sentinel node into the heap. */
	static const IndexType NULL_INDEX = 0;
};