#include "UnionFind.h"
#include <assert.h>


int UnionFind::find(int x)
{ 
	assert(x < m_N);
	assert(x >= 0);

	while (x != id[x]) 
	{
		x = id[x];
		assert(x < m_N);
		assert(x >= 0);

	}
	return x; 
}

UnionFind::UnionFind(int N)
:m_N(N)
{ 
	id = new int[N]; sz = new int[N];
	reset();
}

void	UnionFind::reset()
{
	for (int i = 0; i < m_N; i++) 
	{ 
		id[i] = i; sz[i] = 1; 
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
	if (sz[i] < sz[j])
	{ 
		id[i] = j; sz[j] += sz[i]; 
	}
	else 
	{ 
		id[j] = i; sz[i] += sz[j]; 
	}
}
