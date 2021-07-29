//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef DETOURNODE_H
#define DETOURNODE_H

enum dtNodeFlags
{
	DT_NODE_OPEN = 0x01,
	DT_NODE_CLOSED = 0x02,
};

struct dtNode
{
	float cost;
	float total;
	unsigned int id;
	unsigned int pidx : 30;
	unsigned int flags : 2;
};

class dtNodePool
{
public:
	dtNodePool(int maxNodes, int hashSize);
	~dtNodePool();
	inline void operator=(const dtNodePool&) {}
	void clear();
	dtNode* getNode(unsigned int id);
	const dtNode* findNode(unsigned int id) const;

	inline unsigned int getNodeIdx(const dtNode* node) const
	{
		if (!node) return 0;
		return (unsigned int)(node - m_nodes)+1;
	}

	inline dtNode* getNodeAtIdx(unsigned int idx)
	{
		if (!idx) return 0;
		return &m_nodes[idx-1];
	}
	
	inline int getMemUsed() const
	{
		return sizeof(*this) +
		sizeof(dtNode)*m_maxNodes +
		sizeof(unsigned short)*m_maxNodes +
		sizeof(unsigned short)*m_hashSize;
	}
	
private:
	inline unsigned int hashint(unsigned int a) const
	{
		a += ~(a<<15);
		a ^=  (a>>10);
		a +=  (a<<3);
		a ^=  (a>>6);
		a += ~(a<<11);
		a ^=  (a>>16);
		return a;
	}
	
	dtNode* m_nodes;
	unsigned short* m_first;
	unsigned short* m_next;
	const int m_maxNodes;
	const int m_hashSize;
	int m_nodeCount;
};

class dtNodeQueue
{
public:
	dtNodeQueue(int n);
	~dtNodeQueue();
	inline void operator=(dtNodeQueue&) {}
	
	inline void clear()
	{
		m_size = 0;
	}
	
	inline dtNode* top()
	{
		return m_heap[0];
	}
	
	inline dtNode* pop()
	{
		dtNode* result = m_heap[0];
		m_size--;
		trickleDown(0, m_heap[m_size]);
		return result;
	}
	
	inline void push(dtNode* node)
	{
		m_size++;
		bubbleUp(m_size-1, node);
	}
	
	inline void modify(dtNode* node)
	{
		for (int i = 0; i < m_size; ++i)
		{
			if (m_heap[i] == node)
			{
				bubbleUp(i, node);
				return;
			}
		}
	}
	
	inline bool empty() const { return m_size == 0; }
	
	inline int getMemUsed() const
	{
		return sizeof(*this) +
		sizeof(dtNode*)*(m_capacity+1);
	}
	
	
private:
	void bubbleUp(int i, dtNode* node);
	void trickleDown(int i, dtNode* node);
	
	dtNode** m_heap;
	const int m_capacity;
	int m_size;
};		


#endif // DETOURNODE_H