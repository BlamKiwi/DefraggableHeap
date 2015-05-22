#pragma once

/**
*	Performs an aligned allocation using ::new.
*
*	@param bytes the number of bytes to allocate
*	@param alignment the alignment requirement in bytes
*	@returns pointer to allocated memory. 
*/
void* AlignedNew(size_t bytes, size_t alignment);

/**
*	Frees memory allocated by a call to Aligned New. 
*
*	@param addr the pointer to allocated memory
*/
void AlignedDelete(void* addr);