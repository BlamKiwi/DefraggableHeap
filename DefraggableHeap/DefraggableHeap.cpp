// AATreePlayground.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <algorithm>

#include <iostream>

#include "SplayHeap.h"
#include "ListHeap.h"

int _tmain(int argc, _TCHAR* argv[])
{

	ListHeap heap(127);

	auto val = heap.Allocate(16);
	//auto val2 = heap.Allocate(32);
	heap.Free(val);
	//heap.Free(val2);

	int* x = reinterpret_cast<int*>(val.Get());

	*x = 6666;

	//heap.IterateHeap();

	//x = reinterpret_cast<int*>(val2.Get());

	//std::cout << *x << std::endl;

	DefraggablePointerControlBlock ptr;
	ptr = nullptr;

	/*DefraggableHeap n( 500 ); // 1GB


	auto p = n.Allocate( 4 );
	auto p1 = n.Allocate( 8 );
	auto p2 = n.Allocate( 16 );
	auto p3 = n.Allocate( 2 );
	auto p4 = n.Allocate( 4 );
	auto p5 = n.Allocate( 8 );
	auto p6 = n.Allocate( 16 );
	auto p7 = n.Allocate( 2 );
	auto p8 = n.Allocate( 4 );
	auto p9 = n.Allocate( 8 );
	auto p10 = n.Allocate( 16 );
	auto p11 = n.Allocate( 2 );
	auto p12 = n.Allocate( 4 );
	auto p13 = n.Allocate( 8 );
	auto p14 = n.Allocate( 16 );
	auto p15 = n.Allocate( 2 );

	std::memset( p, 0xABABABAB, 4 );
	std::memset( p1, 0xCDCDCDCD, 8 );
	std::memset( p2, 0xEFEFEFEF, 16 );
	std::memset( p3, 0x12121212, 2 );

	n.Free( p );
	n.Free( p15 );

	n.Defrag( );*/

	return 0;
}

