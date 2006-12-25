/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#ifndef BT_OBJECT_ARRAY__
#define BT_OBJECT_ARRAY__

#include "btScalar.h" // has definitions like SIMD_FORCE_INLINE
#include "btAlignedAllocator.h"

///btAlignedObjectArray uses a subset of the stl::vector interface for its methods
///It is developed to replace stl::vector to avoid STL alignment issues to add SIMD/SSE data
template <typename T> 
//template <class T> 
class btAlignedObjectArray
{
	int					m_size;
	int					m_capacity;
	T*					m_data;

	btAlignedAllocator<T , 16>	m_allocator;

	protected:
		SIMD_FORCE_INLINE	int	allocSize(int size)
		{
			return (size ? size*2 : 1);
		}
		SIMD_FORCE_INLINE	void	copy(int start,int end, T* dest)
		{
			int i;
			for (i=0;i<m_size;++i)
				dest[i] = m_data[i];
		}

		SIMD_FORCE_INLINE	void	init()
		{
			m_data = 0;
			m_size = 0;
			m_capacity = 0;
		}
		SIMD_FORCE_INLINE	void	destroy(int first,int last)
		{
			int i;
			for (i=0; i<m_size;i++)
			{
				m_data[i].~T();
			}
		}

		SIMD_FORCE_INLINE	void* allocate(int size)
		{
			if (size)
				return m_allocator.allocate(size);
			return 0;
		}

		SIMD_FORCE_INLINE	void	deallocate()
		{
			if(m_data)	{
				m_allocator.deallocate(m_data);
				m_data = 0;
			}
		}


	public:
		
		btAlignedObjectArray()
		{
			init();
		}

		~btAlignedObjectArray()
		{
			clear();
		}

		SIMD_FORCE_INLINE	int capacity() const
		{	// return current length of allocated storage
			return m_capacity;
		}
		
		SIMD_FORCE_INLINE	int size() const
		{	// return length of sequence
			return m_size;
		}
		
		SIMD_FORCE_INLINE const T& operator[](int n) const
		{
			return m_data[n];
		}

		SIMD_FORCE_INLINE T& operator[](int n)
		{
			return m_data[n];
		}
		

		SIMD_FORCE_INLINE	void	clear()
		{
			destroy(0,size());
			
			deallocate();
			
			init();
		}

		SIMD_FORCE_INLINE	void	pop_back()
		{
			m_size--;
			m_data[m_size].~T();
		}

		SIMD_FORCE_INLINE	void	resize(int newsize)
		{
			if (newsize > size())
			{
				reserve(newsize);
			}

			m_size = newsize;
		}
	


		SIMD_FORCE_INLINE	void push_back(const T& _Val)
		{	
			int sz = size();
			if( sz == capacity() )
			{
				reserve( allocSize(size()) );
			}
			
			m_data[size()] = _Val;
			//::new ( m_data[m_size] ) T(_Val);
			m_size++;
		}

	
		
		SIMD_FORCE_INLINE	void reserve(int _Count)
		{	// determine new minimum length of allocated storage
			if (capacity() < _Count)
			{	// not enough room, reallocate
				if (capacity() < _Count)
				{
					T*	s = (T*)allocate(_Count);

					copy(0, size(), s);

					destroy(0,size());

					deallocate();

					m_data = s;
					
					m_capacity = _Count;

				}
			}
		}

};

#endif //BT_OBJECT_ARRAY__