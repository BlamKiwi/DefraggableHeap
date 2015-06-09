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
#include <random>

#include "SplayHeap.h"
#include "ListHeap.h"

#include <windows.h>

double GetTiming()
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		std::terminate();

	return double(li.QuadPart) / 1000.0; // Scale to ms timing
}

/**< The timing scale of the computer */
static double TIMING_SCALE;

typedef __int64 Counter;

Counter SEED;

Counter SamplePerformanceCounter()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

double GetDuration(Counter c)
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - c) / TIMING_SCALE;
}

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

const char * const UNIT_STRING = "ms";

std::vector<uint32_t> EratosthenesSieve(uint32_t upper_bound) 
{
	// Our list of from numbers
	std::vector<uint32_t> res;

	// Initialize the composite memoization array to false
	std::vector<bool> is_composite;
	for (uint32_t i = 0; i <= upper_bound + 1; i++)
		is_composite.push_back(0);

	// Calculate sqr for sqr optimization
	uint32_t sqr = uint32_t(sqrt((double)upper_bound));

	// Run the sieve of eratosthenes up to the square root of the upper bound
	for (int m = 2; m <= sqr; m++) {
		// If we have not visited this number before, we are prime
		if (!is_composite[m]) {
			
			// Add prime number to output list
			res.push_back(m);

			// Mark all composites of this prime
			for (int k = m * m; k <= upper_bound; k += m)
				is_composite[k] = true;
		}
	}

	// We have covered all composite numbers, add remaining primes to the output list
	for (int m = sqr; m <= upper_bound; m++)
		if (!is_composite[m])
			res.push_back(m);

	return res;
}

template <typename PreBenchmark, typename Benchmark, typename PostBenchmark, typename Heap>
void RunBenchmark(PreBenchmark& pre_benchmark, Benchmark& benchmark, PostBenchmark &post_benchmark, Heap &heap, const char * const name )
{
	// Do two warmup runs of the benchmark
	for (auto i = 0U; i < WARMUP_RUNS; i++)
	{
		pre_benchmark();
		benchmark();
		post_benchmark();
		std::cout << "Warmup: " << i << std::endl;
	}

	// Run the actual benchmark
	std::vector<double> time_log;
	for (auto i = 0U; i < RUNS; i++)
	{
		pre_benchmark();

		auto start_time = SamplePerformanceCounter();

		benchmark();

		time_log.push_back(GetDuration(start_time));

		post_benchmark();
		std::cout << "Run: " << i << std::endl;
	}

	// Display results
	std::cout << "----- " << name << " -----" << std::endl << std::endl;;
	std::cout << std::endl << "Heap Type: " << GetTypeString(heap) << std::endl << std::endl;

	for (auto i = 0U; i < RUNS; i++)
		std::cout << "Run " << i << ": " << time_log[i] << UNIT_STRING << std::endl << std::endl;

	double sum = 0;
	for (auto i : time_log)
		sum += i;
	sum /= RUNS;

	std::cout << "Average : " << sum << UNIT_STRING << std::endl << std::endl;

	std::cout << "-------------------------------------" << std::endl << std::endl;
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

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Pure Allocation Benchmark");
}

template <typename T>
void PureFreeBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);

	auto pre_benchmark = [&](){
		// Allocate ALLOC_SIZES until we fail
		while (auto alloc = heap.Allocate(ALLOC_SIZE))
			blas.push_back(std::move(alloc)); 
	};

	auto benchmark = [&]()
	{
		// Return all allocated data to the heap
		for (auto &i : blas)
			heap.Free(i);
	};

	auto post_benchmark = [&]()
	{
		// Clear blas
		blas.clear();
	};

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Pure Free Benchmark");
}

template <typename T>
void PrimeStrideFreeBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);
	const auto primes = EratosthenesSieve(CHUNKS / 2);

	auto pre_benchmark = [&](){
		// Allocate ALLOC_SIZES until we fail
		while (auto alloc = heap.Allocate(ALLOC_SIZE))
			blas.push_back(std::move(alloc));
	};

	auto benchmark = [&]()
	{
		// Free the first 2 items (non prime numbers)
		heap.Free(blas[0]);
		heap.Free(blas[1]);

		// For every prime
		for (auto p : primes)
		{
			if (p < blas.size())
			{
				// Return all allocated data to the heap in the strie
				for (auto i = p; i < blas.size(); i += p)
					heap.Free(blas[i]);
			}
		}
	};

	auto post_benchmark = [&]()
	{
		// Clear blas
		blas.clear();
	};

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Prime Stride Free Benchmark");
}

template <typename T>
void StackBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);
	const auto primes = EratosthenesSieve(CHUNKS / 2);

	auto pre_benchmark = [&](){
		
	};

	auto benchmark = [&]()
	{
		// Allocate ALLOC_SIZES until we fail
		while (auto alloc = heap.Allocate(ALLOC_SIZE))
			blas.push_back(std::move(alloc));

		// Pop items off
		for (auto it = blas.rbegin(); it != blas.rend(); it++)
			heap.Free(*it);
	};

	auto post_benchmark = [&]()
	{
		// Clear blas
		blas.clear();
	};

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Stack Benchmark");
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

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Full Defrag Benchmark");
}

template <typename T>
void RandomBenchmark(T& heap)
{
	std::vector<DefraggablePointerControlBlock> blas;
	blas.reserve(CHUNKS / 2);
	
	std::mt19937 engine;
	std::uniform_int_distribution<int> dist(0, 6);
	static const size_t ITERATIONS = 2000;

	auto remove_random_item = [&]()
	{
		auto index = std::uniform_int_distribution<int>(0, blas.size() - 1)(engine);
		//std::cout << "free," << index << std::endl;
		auto it = blas.begin() + index;
		heap.Free(*it);
		blas.erase(it);
	};

	auto pre_benchmark = [&]()
	{
		// Reset the random generator
		new (&engine) std::mt19937();
		engine.seed(SEED); 
	};

	auto benchmark = [&]()
	{
		// Run iterations of the benchmark
		for (auto i = 0U; i < ITERATIONS; i++)
		{
			// Generate option to perform
			const auto res = dist(engine);

			switch (res)
			{
			case 0:
			case 1:
			case 2:
			{
				//std::cout << "allocate";
				// Allocate some data
				if (auto alloc = heap.Allocate(ALLOC_SIZE))
				{
					//std::cout << ",success";
					blas.push_back(std::move(alloc));
				}
				else
				{
					//std::cout << ",fail";
				}
			//	std::cout << std::endl;
				
			}
			break;
			case 3:
			case 4:
			case 5:
			{
				// Free some data
				if (!blas.empty())
				{
					remove_random_item();
				}
			}
			break;
			case 6:
				//std::cout << "iterate" << std::endl;
				// Do a little defragging
				heap.IterateHeap();
			break;
			}
		}
	};

	auto post_benchmark = [&]()
	{
		// Return all allocated data to the heap
		for (auto &i : blas)
			heap.Free(i);

		// Clear blas
		blas.clear();
	};

	RunBenchmark(pre_benchmark, benchmark, post_benchmark, heap, "Random Benchmark");
}

int _tmain(int argc, _TCHAR* argv[])
{
	TIMING_SCALE = GetTiming();
	SEED = SamplePerformanceCounter();
	std::cout << "Timing: " << TIMING_SCALE << ", Seed: " << SEED << std::endl << std::endl;

	ListHeap list(HEAP_SIZE);
	SplayHeap splay(HEAP_SIZE);

	/** 
		--- Pure Allocate Benchmark ---

		Benchmarks the performance of the Allocate function for the heaps.
	**/
	//PureAllocationBenchmark(list);
	//PureAllocationBenchmark(splay);

	/**
		--- Full Defragmentation Benchmark ---

		Benchmarks the performance of the Fully Defragment function for the heaps.
	**/
    //FullDefragBenchmark(list);
    //FullDefragBenchmark(splay);

	/**
		--- Pure Free Benchmark ---

		Benchmarks the performance of the Free function for the heaps.
	**/
	//PureFreeBenchmark(list);
	//PureFreeBenchmark(splay);

	/**
		--- Prime Stride Free Benchmark ---

		Benchmarks the performance of the Free function for the heaps using prime strides.
	**/
	//PrimeStrideFreeBenchmark(list);
	//PrimeStrideFreeBenchmark(splay);

	/**
		--- Stack Free Benchmark ---

		Benchmarks the performance of the allocator functions with a stack like access pattern.
	**/
	//StackBenchmark(list);
	//StackBenchmark(splay);

	/**
		--- Random Benchmark ---
	
		Applies random behaviour to the heaps.
	**/
	RandomBenchmark(list);
	//RandomBenchmark(splay);

	int x;
	std::cin >> x;

	return 0;
}