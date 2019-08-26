#ifndef COMPARE_KEY_H_
#define COMPARE_KEY_H_

#include <iostream>
#include <map>

namespace qflow {

struct Key2i
{
	Key2i(int x, int y)
		: key(std::make_pair(x, y))
	{}
	bool operator==(const Key2i& other) const
	{
		return key == other.key;
	}
	bool operator<(const Key2i& other) const
	{
		return key < other.key;
	}
	std::pair<int, int> key;
};

struct Key3i
{
	Key3i(int x, int y, int z)
		: key(std::make_pair(x, std::make_pair(y, z)))
	{}
	bool operator==(const Key3i& other) const
	{
		return key == other.key;
	}
	bool operator<(const Key3i& other) const
	{
		return key < other.key;
	}
	std::pair<int, std::pair<int, int> > key;
};

struct Key3f
{
	Key3f(double x, double y, double z, double threshold)
		: key(std::make_pair(x / threshold, std::make_pair(y / threshold, z / threshold)))
	{}
	bool operator==(const Key3f& other) const
	{
		return key == other.key;
	}
	bool operator<(const Key3f& other) const
	{
		return key < other.key;
	}
	std::pair<int, std::pair<int, int> > key;
};

struct KeySorted2i
{
	KeySorted2i(int x, int y)
		: key(std::make_pair(x, y))
	{
		if (x > y)
			std::swap(key.first, key.second);
	}
	bool operator==(const KeySorted2i& other) const
	{
		return key == other.key;
	}
	bool operator<(const KeySorted2i& other) const
	{
		return key < other.key;
	}
	std::pair<int, int> key;
};

struct KeySorted3i
{
	KeySorted3i(int x, int y, int z)
		: key(std::make_pair(x, std::make_pair(y, z)))
	{
		if (key.first > key.second.first)
			std::swap(key.first, key.second.first);
		if (key.first > key.second.second)
			std::swap(key.first, key.second.second);
		if (key.second.first > key.second.second)
			std::swap(key.second.first, key.second.second);
	}
	bool operator==(const Key3i& other) const
	{
		return key == other.key;
	}
	bool operator<(const Key3i& other) const
	{
		return key < other.key;
	}
	std::pair<int, std::pair<int, int> > key;
};


} // namespace qflow

#endif
