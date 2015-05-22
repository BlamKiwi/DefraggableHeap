#include "stdafx.h"

#include "AlignedAllocator.h"

#include <cstdint>
#include <cassert>

/**
*	Based off the alligned allocator in Jason Gregory's book.
*/

void* AlignedNew(size_t bytes, size_t alignment)
{
	// An alignment of 0 is undefined
	assert(alignment);

	// We do not support alignments of over 128 bytes
	assert(alignment <= 128);

	// We only support power of two alignments
	assert((alignment & (alignment - 1)) == 0);

	// What is the total amount of bytes to allocate including padding
	const size_t total_bytes = bytes + alignment;

	// Perform the actual allocation
	const uintptr_t raw_address = uintptr_t(::operator new(total_bytes));

	// Calculate the misalignment of the data given to us by ::new
	const uintptr_t mask = alignment - 1;
	const uintptr_t misalignment = raw_address & mask;
	
	// Calculate the offset to fix the alignment
	const ptrdiff_t offset = alignment - misalignment;

	// Offset should never be more than what we can store in a byte
	assert(offset < 256); 

	// Calculate the aligned address by applying the offset
	const uintptr_t aligned_address = raw_address + offset;

	// Store offset in byte preceding the aligned address
	uint8_t* const aligned_data = reinterpret_cast<uint8_t*>(aligned_address);
	aligned_data[-1] = static_cast<uint8_t>(offset);

	// Return the aligned data pointer
	return aligned_data;
}

void AlignedDelete(void* const addr)
{
	// Retrieve the alignment offset from memory
	const uint8_t* const aligned_data = static_cast<uint8_t*>(addr);
	const uintptr_t offset = aligned_data[-1];

	// Unapply the offset to get the raw address from the system allocation
	const uintptr_t aligned_address = uintptr_t(addr);
	const uintptr_t raw_address = aligned_address - offset;
	
	// Get the raw pointer 
	void* const raw = reinterpret_cast<void*>(raw_address);

	// Delete the memory
	::operator delete(raw);
}