#pragma once

/**
*	Copies regions on memory using SIMD operations. 
*	All addresses must be 16-byte aligned.
*
*	If the target region overlaps with the source region,
*	the source region will be overwritten. The target region
*	will contain valid data, the source region will be undefined.
*
*	@param target the address that we want to copy memory to
*	@param source the address that we want to copy memory from
*	@param num_chunks the number of SIMD chunks to copy
*/
void SIMDMemCopy(void* target, void* source, size_t num_chunks);

/**
*	Sets a region in memory to a pattern using SIMD operations.
*	All addresses must be 16-byte aligned.
*
*	@param target the address that we want to copy memory to
*	@param pattern the pattern to set memory to
*	@param num_chunks the number of SIMD chunks to set
*/
void SIMDMemSet(void* target, int pattern, size_t num_chunks);