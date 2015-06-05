// AATreePlayground.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <functional>

#include "SplayHeap.h"
#include "ListHeap.h"

static const size_t HEAP_SIZE = 1024 * 1024 * 32; // 32MB heap
static const size_t ALLOC_SIZE = 1024;
static const size_t CHUNKS = HEAP_SIZE / 16;

static const size_t RUNS = 5;
static const size_t WARMUP_RUNS = 2;

const char * const GetTypeString(const ListHeap&)
{
	return "ListHeap";
}

const char * const GetTypeString(const SplayHeap&)
{
	return "SplayHeap";
}


template <typename T>
void PureAllocationBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);

	auto pre_benchmark = [&](){};

	auto benchmark = [&]()
	{
		// Allocate ALLOC_SIZES until we fail
		while (auto alloc = heap.Allocate(ALLOC_SIZE))
			blas.push_back(std::move(alloc));

	};

	auto post_benchmark = [&]()
	{
		// Return all allocated data to the heap
		for (auto &i : blas)
			heap.Free(i);

		// Clear blas
		blas.clear();
	};

	// Do two warmup runs of the benchmark
	for (auto i = 0U; i < WARMUP_RUNS; i++)
	{
		pre_benchmark();
		benchmark();
		post_benchmark();
	}

	// Run the actual benchmark
	std::vector<size_t> time_log; 
	for (auto i = 0U; i < RUNS; i++)
	{
		pre_benchmark();

		auto start_time = std::chrono::high_resolution_clock::now();

		benchmark();

		auto end_time = std::chrono::high_resolution_clock::now();

		time_log.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count());

		post_benchmark();
	}

	// Display results
	std::cout << "----- Pure Allocation Benchmark -----" << std::endl << std::endl;;
	std::cout << std::endl << "Heap Type: " << GetTypeString(heap) << std::endl << std::endl;

	for (auto i = 0U; i < RUNS; i++)
		std::cout << "Run " << i << ": " << time_log[i] << "ns" << std::endl << std::endl;

	size_t sum = 0;
	for (auto i : time_log)
		sum += i;
	sum /= RUNS;

	std::cout << "Average : " << sum << "ns" << std::endl << std::endl;

	std::cout << "-------------------------------------" << std::endl << std::endl;
}

template <typename T>
void FullDefragBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);

	auto pre_benchmark = [&]()
	{
		// Allocate ALLOC_SIZES until we fail
		while (auto alloc = heap.Allocate(ALLOC_SIZE))
			blas.push_back(std::move(alloc));

		// Free every second block to maximize fragmentation
		size_t c = 0;
		for (auto& i : blas)
		{
			if ((c & 1) == 0)
			{
				heap.Free(i);
			}

			c++;
		}
	};

	auto benchmark = [&]()
	{
		heap.FullDefrag();
	};

	auto post_benchmark = [&]()
	{
		// Return all allocated data to the heap
		size_t c = 0;
		for (auto &i : blas)
		{
			if ((i & 1) == 1)
			{
				heap.Free(i);
			}

			c++;
		}

		// Clear blas
		blas.clear();
	};

	// Do two warmup runs of the benchmark
	for (auto i = 0U; i < WARMUP_RUNS; i++)
	{
		pre_benchmark();
		benchmark();
		post_benchmark();
	}

	// Run the actual benchmark
	std::vector<size_t> time_log;
	for (auto i = 0U; i < RUNS; i++)
	{
		pre_benchmark();

		auto start_time = std::chrono::high_resolution_clock::now();

		benchmark();

		auto end_time = std::chrono::high_resolution_clock::now();

		time_log.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count());

		post_benchmark();
	}

	// Display results
	std::cout << "----- Full Defragmentation Benchmark -----" << std::endl << std::endl;;
	std::cout << std::endl << "Heap Type: " << GetTypeString(heap) << std::endl << std::endl;

	for (auto i = 0U; i < RUNS; i++)
		std::cout << "Run " << i << ": " << time_log[i] << "ns" << std::endl << std::endl;

	size_t sum = 0;
	for (auto i : time_log)
		sum += i;
	sum /= RUNS;

	std::cout << "Average : " << sum << "ns" << std::endl << std::endl;

	std::cout << "-------------------------------------" << std::endl << std::endl;
}


int _tmain(int argc, _TCHAR* argv[])
{
	std::cout << "System ticks per second: " << std::chrono::high_resolution_clock::period::den << std::endl << std::endl;

	ListHeap list(HEAP_SIZE);
	SplayHeap splay(HEAP_SIZE);

	/** 
		--- Pure Allocate Benchmark ---

		Benchmarks the performance of the Allocate function for the heaps.
	**/
	PureAllocationBenchmark(list);
	PureAllocationBenchmark(splay);

	/**
		--- Full Defragmentation Benchmark ---

		Benchmarks the performance of the Fully Defragment function for the heaps.
	**/
	FullDefragBenchmark(list);
	FullDefragBenchmark(splay);

	return 0;
}

