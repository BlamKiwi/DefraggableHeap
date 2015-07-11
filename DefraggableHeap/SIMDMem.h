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