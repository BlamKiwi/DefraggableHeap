#include "stdafx.h"

#include "ListHeader.h"

ListHeader::ListHeader()
	: _prev(0)
	, _prev_free(0)
	, _next_free(0)
	, _block_metadata({ AllocationState::ALLOCATED, 0 })
{

}

ListHeader::ListHeader(IndexType prev, IndexType prev_free, IndexType next_free, IndexType num_chunks, AllocationState alloc)
	: _prev(prev)
	, _prev_free(prev_free)
	, _next_free(next_free)
	, _block_metadata({ alloc, num_chunks })

{

}

