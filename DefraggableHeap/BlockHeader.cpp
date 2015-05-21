#include "stdafx.h"

#include "BlockHeader.h"

BlockHeader::BlockHeader()
	: _left(0)
	, _right(0)
	, _block_metadata({ AllocationState::ALLOCATED, 0 })
	, _max_contiguous_free_chunks(0)
{

}

BlockHeader::BlockHeader(IndexType left, IndexType right, IndexType num_chunks, AllocationState alloc)
	: _left(left)
	, _right(right)
	, _block_metadata({ alloc, num_chunks })
	, _max_contiguous_free_chunks(0)
{

}

