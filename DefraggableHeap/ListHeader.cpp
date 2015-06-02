#include "stdafx.h"

#include "ListHeader.h"

ListHeader::ListHeader()
	: _prev(0)
	, _next(0)
	, _block_metadata({ AllocationState::ALLOCATED, 0 })
	, _next_free(0)
{

}

ListHeader::ListHeader(IndexType prev, IndexType next, IndexType next_free, IndexType num_chunks, AllocationState alloc)
	: _prev(prev)
	, _next(next)
	, _block_metadata({ alloc, num_chunks })
	, _next_free(next_free)
	
{

}

