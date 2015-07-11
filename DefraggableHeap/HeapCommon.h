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

/**
*	The index type for defraggable heap blocks.
*/
typedef uint32_t IndexType;

/**
*	The allocation states for defraggable heap blocks.
*/
enum AllocationState : IndexType
{
	FREE = 0, ALLOCATED = 1
};

/**
*	Defines general metadata bitfield for block headers to use. 
*/
struct BlockMetadata
{
	/**
	*	The number of chunks gets cannibalized for allocation metadata as we access
	*	it less compared to max contiguous free chunks in a splay operation.
	*/

	/**< Is the block allocated or unallocated. */
	IndexType _is_allocated : 1;

	/**< The number of chunks in the block we represent (including the header). */
	IndexType _num_chunks : 31;

};

static_assert(sizeof(BlockMetadata) == sizeof(IndexType), "The metadata field must be the size of the index type.");


/**< The pattern initial blocks should be set to */
const int INIT_PATTERN = 0x12345678;

/**< The pattern allocated blocks should be set to */
const int ALLOC_PATTERN = 0xACACACAC;

/**< The pattern merged blocks should be set to */
const int MERGE_PATTERN = 0xDDDDDDDD;

/**< The pattern freed blocks should be set to */
const int FREED_PATTERN = 0xFEEFEEFE;

/**< The pattern moved freed blocks should be set to */
const int MOVE_PATTERN = 0xDEADB0B1;

/**< The pattern free blocks from an allocation split should be set to. */
const int SPLIT_PATTERN = 0x51775177;