// AATreePlayground.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <algorithm>

#include "BlockHeader.h"

enum class AllocatorState : uint8_t
{
	ALLOCATED = 0, FREE = 1
};

struct SplayNode
{
	SplayNode( void* v, SplayNode *left, SplayNode *right, size_t block_size, AllocatorState state )
		: _value( v )
		, _left( left )
		, _right( right )
		, _block_size( block_size )
		, _state( state )
	{
		// We use the null object pattern
		assert( left );
		assert( right );

		UpdateNodeStatistics( );
	}

	SplayNode( SplayNode *left, SplayNode *right )
		: _value( nullptr )
		, _left( left )
		, _right( right )
		, _block_size( 0 )
		, _max_free_contiguous( 0 )
		, _state( AllocatorState::ALLOCATED )
	{
		// We use the null object pattern
		assert( left );
		assert( right );
	}

	void UpdateNodeStatistics( )
	{
		// The maximum free contiguous block for the current node
		// is a 3 way maximum of of children and max of self if we are a free block
		_max_free_contiguous = std::max( _left->_max_free_contiguous, _right->_max_free_contiguous );

		if ( _state == AllocatorState::FREE )
			_max_free_contiguous = std::max( _max_free_contiguous, _block_size );
	}

	void *_value;
	SplayNode *_left;
	SplayNode *_right;
	size_t _block_size;
	size_t _max_free_contiguous;
	AllocatorState _state;
};

class DefraggableHeap
{
public:
	DefraggableHeap( size_t size )
		: _null_node( Null( ), Null( ) )
		, _data_segment( ::operator new( size ) )
		, _size( size )
		, MAX_ADDRESS( ~0UL )
	{
		_root = new SplayNode( _data_segment, Null( ), Null( ), size, AllocatorState::FREE );
	}

	~DefraggableHeap( )
	{
		::operator delete( _data_segment );
	}

	void* Allocate( size_t num_bytes )
	{
		// An allocation of 0 bytes is a bad allocation
		if ( num_bytes == 0 )
			throw std::runtime_error( "Tried to make an allocation of 0 bytes. This is not allowed." );

		// Check that we have enough contiguous space for allocation
		if ( _root->_max_free_contiguous < num_bytes )
			throw std::runtime_error( "Tried to make an allocation but there wasn't enough space." ); 
			
		// Splay the found free block to the root
		const auto free_block = FindFreeBlock( _root, num_bytes );
		_root = Splay( free_block->_value, _root );

		/* Split the root free block into two, one allocated block and one free block */

		// Calculate the new free block size
		const auto new_free_block_size = _root->_block_size - num_bytes;

		// Set the allocator state for the root now that it points to an allocated block
		_root->_state = AllocatorState::ALLOCATED;
		_root->_block_size = num_bytes;
		const auto new_addr = _root->_value;

		// Is there a new free block to add back to the tree
		if ( new_free_block_size )
		{
			// Pun block address
			AddressPunner address;
			address.addr = _root->_value;

			// Calculate new free block address
			address.val += num_bytes;

			// New address value will always be bigger since we allocate to the lower adresses
			// Right subtree is untouched
			auto new_node = new SplayNode( address.addr, 
				_root, // Old root becomes new left subtree
				_root->_right, // Right subtree is untouched
				new_free_block_size, 
				AllocatorState::FREE ); 
		
			// Clear right subtree of old root
			_root->_right = Null( );

			// Update old root node statistics
			_root->UpdateNodeStatistics( );

			// Set new root
			_root = new_node;
		}

		// Update root node statistics
		_root->UpdateNodeStatistics( );

		return new_addr;
	}

	// Finds the next avaliable free block using inorder traversal
	SplayNode* FindFreeBlock( SplayNode* t, size_t num_bytes )
	{
		// We assume the tree already has a free block large enough
		assert( t->_max_free_contiguous >= num_bytes );

		// Traverse down tree until we find a free block large enough
		while ( t != Null( ) )
		{
			// Does the left subtree contain a free block large enough
			if ( t->_left->_max_free_contiguous >= num_bytes )
				t = t->_left;
			// Is the current block free and big enough?
			else if ( t->_state == AllocatorState::FREE && t->_block_size >= num_bytes )
				return t;
			// The right subtree must contain the free block
			else
				t = t->_right;
		}

		// This is a fatal error state
		// We shouldn't reach here
		std::exit(EXIT_FAILURE);

	}

	void Free( void* ptr )
	{
		// Splay the given value to the top of the tree
		_root = Splay( ptr, _root );

		// Is the given pointer a valid block value
		if ( _root->_value != ptr )
			throw std::runtime_error( "Pointer to free didn't belong to this heap or didn't have expected alignment into the heap." );

		// Mark the block as being free
		_root->_state = AllocatorState::FREE;

		// We may have invalidated our invariant of having no two free adjavent blocks
		// Collapse free blocks in the heap
		// Does the left subtree contain any potential free blocks
		if ( _root->_left->_max_free_contiguous )
		{
			// Splay the maximum value up from the left subtree
			auto left = Splay( MAX_ADDRESS.addr, _root->_left );

			// Is the root of the left subtree a free block
			if ( left->_state == AllocatorState::FREE )
			{
				// Copy up block metadata and new left subtree
				_root->_left = left->_left;
				_root->_value = left->_value;
				_root->_block_size += left->_block_size;

				// Delete the old left child
				delete left;
			}
			// If block is allocated fix up pointers
			else
				_root->_left = left;
		}

		// Does the right subtree contain any potential free blocks
		if ( _root->_right->_max_free_contiguous )
		{
			// Splay the minimum value up from the right subtree
			auto right = Splay( nullptr, _root->_right );

			// Is the root of the right subtree a free block
			if ( right->_state == AllocatorState::FREE )
			{
				// Copy up block metadata and new right subtree
				_root->_right = right->_right;
				_root->_block_size += right->_block_size;

				// Delete the old right child
				delete right;
			}
			// If block is allocated fix up pointers
			else
				_root->_right = right;
		}

		// Update root node statistics
		_root->UpdateNodeStatistics( );
	}

	// Defragments the heap
	void Defrag( )
	{
		// Do we actually need to defrag the heap or is it at capacity (unlikely)
		if ( _root->_max_free_contiguous )
		{
			// Splay the first free block in the heap to the root if required
			// This will put the fully defragmented subheap in the left subtree
			const auto free_block = FindFreeBlock( _root, 1 );
			_root = Splay( free_block->_value, _root );

			// While the root is not the only free block, keep defragging
			while ( _root->_right != Null( ) )
			{
				// Splay the minimum block up from the right sub tree
				auto right = Splay( nullptr, _root->_right );

				// Is the root of the right subtree a free block
				if ( right->_state == AllocatorState::FREE )
				{
					// Copy up block metadata and new right subtree
					_root->_right = right->_right;
					_root->_block_size += right->_block_size;

					// Delete the old right child
					delete right;

					// Update root node statistics
					_root->UpdateNodeStatistics( );
				}
				// We have found an allocated block and need to defragment
				else
				{
					// Calculate new offset into free block
					AddressPunner free_addr;
					free_addr.addr = _root->_value;
					free_addr.val += right->_block_size;

					// Construct new free block header
					auto old_alloc_block = right->_value;
					SplayNode new_free( free_addr.addr, Null( ), right->_right, _root->_block_size, AllocatorState::FREE );
					assert( right->_left == Null( ) );

					// Construct new allocated block header
					SplayNode new_alloc( _root->_value, _root->_left, right, right->_block_size, AllocatorState::ALLOCATED );

					/* CONSIDER THE HEAP INVALID FROM HERE, ONLY RELY ON VALUES FROM THE STACK */

					// Copy new allocated block header and allocated data
					*_root = new_alloc;
					std::memcpy( new_alloc._value, old_alloc_block, new_alloc._block_size );

					/* ROOT AND ITS DATA IS NOW VALID */

					// Copy new free block header
					*_root->_right = new_free;

					/* HEAP IS NOW VALID. UPDATE POINTERS TO NEW AREA IN HEAP */

					// Rotate right child up to root
					_root = RotateWithRightChild( _root );
				}
			}
		}
	}

	/*void Insert( void *value )
	{
		auto new_node = new SplayNode( value, Null( ), Null( ), 0, AllocatorState::UNINITIALIZED );

		if ( _root == Null( ) )
		{
			// Set children of new root to null
			new_node->_right = new_node->_left = Null( );

			// Set root to be the new node
			_root = new_node;
		}
		else
		{
			// Splay closest matching value to root
			_root = Splay( value, _root );

			// Is the new value less than the closest value
			if ( value < _root->_value )
			{
				// Left subtree is left untouched
				new_node->_left = _root->_left;
				_root->_left = Null( );

				// Old root becomes new right subtree
				new_node->_right = _root;

				// Set new root
				_root = new_node;
			}
			// Is the new value greater than the closest value
			else if ( value > _root->_value )
			{
				// Right subtree is untouched
				new_node->_right = _root->_right;
				_root->_right = Null( );

				// Old root becomes new left subtree
				new_node->_left = _root;

				// Set new root
				_root = new_node;
			}
			// Duplicate value found
			else
			{
				delete new_node;
				throw std::runtime_error( "Duplicate value in splay tree" );
			}
		}
	}

	void Remove( void *value )
	{
		SplayNode *new_root;

		// Splay closest matching value to root
		_root = Splay( value, _root );

		// Did we find our value?
		if ( _root->_value != value || _root == Null( ) )
			throw std::runtime_error( "Didn't find value in splay tree" );

		// If left subtree is empty, desired value is at root (ie: minimum)
		if ( _root->_left == Null( ) )
			// Promote right subtree as root
			new_root = _root->_right;
		else
		{
			// Find max value in left subtree
			new_root = _root->_left;
			new_root = Splay( value, new_root );
			new_root->_right = _root->_right;
		}

		// Delete existing root
		delete _root;

		// Update root
		_root = new_root;
	}*/

protected:

	SplayNode* Splay( void* value, SplayNode *t )
	{
		// Setup splay tracking state
		SplayNode splay_header( Null( ), Null( ) );
		SplayNode *left_tree_max = &splay_header, *right_tree_min = &splay_header;

		// Ensure we have a match with the null node
		_null_node._value = value;

		// Continually rotate the tree until we splay the desired value
		while ( true )
		{
			// Is the desired value in the left subtree
			if ( value < t->_value )
			{
				// If the desired value is in the left subtree of the left child
				if ( value < t->_left->_value )
					// Rotate left subtree up
					t = RotateWithLeftChild( t );

				// If the desired value is now at the root, stop splay
				if ( t->_left == Null( ) )
					break;

				// t is now a minimum value
				// Link right state tree
				right_tree_min->_left = t;
				right_tree_min->UpdateNodeStatistics( );

				// Advance splay tracking pointers
				right_tree_min = t;
				t = t->_left;
			}
			// Is the desired value in the right subtree
			else if ( value > t->_value )
			{
				// If the desired value is in the right subtree of the right child
				if ( value > t->_right->_value )
					// Rotate right subtree up
					t = RotateWithRightChild( t );

				// If the desired value is now at the root, stop splay
				if ( t->_right == Null( ) )
					break;

				// t is now a maximum value
				// Link left state tree
				left_tree_max->_right = t;
				left_tree_max->UpdateNodeStatistics( );

				// Advance splay tracking pointers
				left_tree_max = t;
				t = t->_right;
			}
			// Is the desired value at the root
			else
				break;
		}

		// Rebuild tree from left and right sub trees
		left_tree_max->_right = t->_left;
		left_tree_max->UpdateNodeStatistics( );
		right_tree_min->_left = t->_right;
		right_tree_min->UpdateNodeStatistics( );

		// Rebuild tree root
		t->_left = splay_header._right;
		t->_right = splay_header._left;
		t->UpdateNodeStatistics( );

		// Clear value in null node
		_null_node._value = nullptr;

		return t;
	}

	static SplayNode* RotateWithLeftChild( SplayNode* k2 )
	{
		// Promote left child
		auto k1 = k2->_left;

		// Move right subtree of left child
		k2->_left = k1->_right;

		// Lower original node
		k1->_right = k2;

		// Update new child node statistics
		k2->UpdateNodeStatistics( );

		// Update new parent node statistics
		k1->UpdateNodeStatistics( );

		return k1;
	}

	static SplayNode* RotateWithRightChild( SplayNode* k1 )
	{
		// Promote right child
		auto k2 = k1->_right;

		// Move left subtree of right child
		k1->_right = k2->_left;

		// Lower original node
		k2->_left = k1;

		// Update new child node statistics
		k1->UpdateNodeStatistics( );

		// Update new parent node statistics
		k2->UpdateNodeStatistics( );

		return k2;
	}

	inline SplayNode* Null( )
	{
		return &_null_node;
	}

	SplayNode _null_node; // The null sentinel node for the splay tree
	SplayNode *_root; // The root node of the splay tree
	void * const _data_segment; // The data segment in the regular runtime heap we manage
	const size_t _size; // The size (in bytes) of the data segment we manage

	union AddressPunner
	{
		SplayNode *node;
		void *addr;
		size_t val;

		AddressPunner( size_t v = 0UL )
			: val( v )
		{

		}
	};

	const AddressPunner MAX_ADDRESS;
};

int _tmain(int argc, _TCHAR* argv[])
{
	BlockHeader null;
	BlockHeader x(1, 2, 15, AllocationState::ALLOCATED);

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

