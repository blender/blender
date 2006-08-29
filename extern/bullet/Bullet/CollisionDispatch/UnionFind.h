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

#ifndef UNION_FIND_H
#define UNION_FIND_H

///UnionFind calculates connected subsets
// Implements weighted Quick Union with path compression
// optimization: could use short ints instead of ints (halving memory, would limit the number of rigid bodies to 64k, sounds reasonable)
class UnionFind
  {
    private:
		int*	m_id;
		int*	m_sz;
		int m_N;

    public:
		int find(int x);
	  
		UnionFind();
		~UnionFind();

	  void	reset(int N);

	  inline int	getNumElements() const
	  {
		  return m_N;
	  }
	  inline bool  isRoot(int x) const
	  {
		  return (x == m_id[x]);
	  }

      int find(int p, int q);
      void unite(int p, int q);

	  void	Allocate(int N);
	  void	Free();

  };


#endif //UNION_FIND_H
