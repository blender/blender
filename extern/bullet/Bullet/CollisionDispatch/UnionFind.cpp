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

#include "UnionFind.h"
#include <assert.h>


int UnionFind::find(int x)
{ 
	assert(x < m_N);
	assert(x >= 0);

	while (x != m_id[x]) 
	{
		x = m_id[x];
		assert(x < m_N);
		assert(x >= 0);

	}
	return x; 
}

UnionFind::~UnionFind()
{
	Free();

}

UnionFind::UnionFind()
:m_id(0),
m_sz(0),
m_N(0)
{ 

}

void	UnionFind::Allocate(int N)
{
	if (m_N < N)
	{
		Free();

		m_N = N;
		m_id = new int[N]; 
		m_sz = new int[N];
	}
}
void	UnionFind::Free()
{
	if (m_N)
	{
		m_N=0;
		delete m_id;
		delete m_sz;
	}
}


void	UnionFind::reset(int N)
{
	Allocate(N);

	for (int i = 0; i < m_N; i++) 
	{ 
		m_id[i] = i; m_sz[i] = 1; 
	} 
}


int UnionFind ::find(int p, int q)
{ 
	return (find(p) == find(q)); 
}

void UnionFind ::unite(int p, int q)
{
	int i = find(p), j = find(q);
	if (i == j) 
		return;
	if (m_sz[i] < m_sz[j])
	{ 
		m_id[i] = j; m_sz[j] += m_sz[i]; 
	}
	else 
	{ 
		m_id[j] = i; m_sz[i] += m_sz[j]; 
	}
}
