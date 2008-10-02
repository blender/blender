#ifndef BT_HASH_MAP_H
#define BT_HASH_MAP_H

#include "btAlignedObjectArray.h"

const int BT_HASH_NULL=0xffffffff;

template <class Value>
class btHashKey
{
	int	m_uid;
public:

	btHashKey(int uid)
		:m_uid(uid)
	{
	}

	int	getUid() const
	{
		return m_uid;
	}

	//to our success
	SIMD_FORCE_INLINE	unsigned int getHash()const
	{
		int key = m_uid;
		// Thomas Wang's hash
		key += ~(key << 15);
		key ^=  (key >> 10);
		key +=  (key << 3);
		key ^=  (key >> 6);
		key += ~(key << 11);
		key ^=  (key >> 16);
		return key;
	}

	btHashKey	getKey(const Value& value) const
	{
		return btHashKey(value.getUid());
	}
};


template <class Value>
class btHashKeyPtr
{
	int	m_uid;
public:

	btHashKeyPtr(int uid)
		:m_uid(uid)
	{
	}

	int	getUid() const
	{
		return m_uid;
	}

	//to our success
	SIMD_FORCE_INLINE	unsigned int getHash()const
	{
		int key = m_uid;
		// Thomas Wang's hash
		key += ~(key << 15);
		key ^=  (key >> 10);
		key +=  (key << 3);
		key ^=  (key >> 6);
		key += ~(key << 11);
		key ^=  (key >> 16);
		return key;
	}

	btHashKeyPtr	getKey(const Value& value) const
	{
		return btHashKeyPtr(value->getUid());
	}
};

///The btHashMap template class implements a generic and lightweight hashmap.
///A basic sample of how to use btHashMap is located in Demos\BasicDemo\main.cpp
template <class Key, class Value>
class btHashMap
{

	btAlignedObjectArray<int>		m_hashTable;
	btAlignedObjectArray<int>		m_next;
	btAlignedObjectArray<Value>		m_valueArray;



	void	growTables(const Key& key)
	{
		int newCapacity = m_valueArray.capacity();

		if (m_hashTable.size() < newCapacity)
		{
			//grow hashtable and next table
			int curHashtableSize = m_hashTable.size();

			m_hashTable.resize(newCapacity);
			m_next.resize(newCapacity);

			int i;

			for (i= 0; i < newCapacity; ++i)
			{
				m_hashTable[i] = BT_HASH_NULL;
			}
			for (i = 0; i < newCapacity; ++i)
			{
				m_next[i] = BT_HASH_NULL;
			}

			for(i=0;i<curHashtableSize;i++)
			{
				const Value& value = m_valueArray[i];

				int	hashValue = key.getKey(value).getHash() & (m_valueArray.capacity()-1);	// New hash value with new mask
				m_next[i] = m_hashTable[hashValue];
				m_hashTable[hashValue] = i;
			}


		}
	}

	public:

	void insert(const Key& key, const Value& value) {
		int hash = key.getHash() & (m_valueArray.capacity()-1);
		//don't add it if it is already there
		if (find(key))
		{
			return;
		}
		int count = m_valueArray.size();
		int oldCapacity = m_valueArray.capacity();
		m_valueArray.push_back(value);
		int newCapacity = m_valueArray.capacity();
		if (oldCapacity < newCapacity)
		{
			growTables(key);
			//hash with new capacity
			hash = key.getHash() & (m_valueArray.capacity()-1);
		}
		m_next[count] = m_hashTable[hash];
		m_hashTable[hash] = count;
	}

	void remove(const Key& key) {

		int hash = key.getHash() & (m_valueArray.capacity()-1);

		int pairIndex = findIndex(key);
		
		if (pairIndex ==BT_HASH_NULL)
		{
			return;
		}

		// Remove the pair from the hash table.
		int index = m_hashTable[hash];
		btAssert(index != BT_HASH_NULL);

		int previous = BT_HASH_NULL;
		while (index != pairIndex)
		{
			previous = index;
			index = m_next[index];
		}

		if (previous != BT_HASH_NULL)
		{
			btAssert(m_next[previous] == pairIndex);
			m_next[previous] = m_next[pairIndex];
		}
		else
		{
			m_hashTable[hash] = m_next[pairIndex];
		}

		// We now move the last pair into spot of the
		// pair being removed. We need to fix the hash
		// table indices to support the move.

		int lastPairIndex = m_valueArray.size() - 1;

		// If the removed pair is the last pair, we are done.
		if (lastPairIndex == pairIndex)
		{
			m_valueArray.pop_back();
			return;
		}

		// Remove the last pair from the hash table.
		const Value* lastValue = &m_valueArray[lastPairIndex];
		int lastHash = key.getKey(*lastValue).getHash() & (m_valueArray.capacity()-1);

		index = m_hashTable[lastHash];
		btAssert(index != BT_HASH_NULL);

		previous = BT_HASH_NULL;
		while (index != lastPairIndex)
		{
			previous = index;
			index = m_next[index];
		}

		if (previous != BT_HASH_NULL)
		{
			btAssert(m_next[previous] == lastPairIndex);
			m_next[previous] = m_next[lastPairIndex];
		}
		else
		{
			m_hashTable[lastHash] = m_next[lastPairIndex];
		}

		// Copy the last pair into the remove pair's spot.
		m_valueArray[pairIndex] = m_valueArray[lastPairIndex];

		// Insert the last pair into the hash table
		m_next[pairIndex] = m_hashTable[lastHash];
		m_hashTable[lastHash] = pairIndex;

		m_valueArray.pop_back();

	}


	int size() const
	{
		return m_valueArray.size();
	}

	const Value* getAtIndex(int index) const
	{
		btAssert(index < m_valueArray.size());

		return &m_valueArray[index];
	}

	Value* getAtIndex(int index)
	{
		btAssert(index < m_valueArray.size());

		return &m_valueArray[index];
	}

	Value* operator[](const Key& key) {
		return find(key);
	}

	const Value*	find(const Key& key) const
	{
		int index = findIndex(key);
		if (index == BT_HASH_NULL)
		{
			return NULL;
		}
		return &m_valueArray[index];
	}

	Value*	find(const Key& key)
	{
		int index = findIndex(key);
		if (index == BT_HASH_NULL)
		{
			return NULL;
		}
		return &m_valueArray[index];
	}


	int	findIndex(const Key& key) const
	{
		int hash = key.getHash() & (m_valueArray.capacity()-1);

		if (hash >= m_hashTable.size())
		{
			return BT_HASH_NULL;
		}

		int index = m_hashTable[hash];
		while ((index != BT_HASH_NULL) && (key.getUid() == key.getKey(m_valueArray[index]).getUid()) == false)
		{
			index = m_next[index];
		}
		return index;
	}

	void	clear()
	{
		m_hashTable.clear();
		m_next.clear();
		m_valueArray.clear();
	}

};

#endif //BT_HASH_MAP_H
