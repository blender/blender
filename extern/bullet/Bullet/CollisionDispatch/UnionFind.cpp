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
