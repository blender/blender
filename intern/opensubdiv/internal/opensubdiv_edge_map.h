// Copyright 2019 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_EDGE_MAP_H_
#define OPENSUBDIV_EDGE_MAP_H_

#include "internal/opensubdiv_util.h"

namespace opensubdiv_capi {

// Helper class to ease dealing with edge indexing.
// Simply takes care of ensuring order of vertices is strictly defined.
class EdgeKey {
 public:
  inline EdgeKey();
  inline EdgeKey(int v1, int v2);

  inline size_t hash() const;
  inline bool operator==(const EdgeKey &other) const;

  // These indices are guaranteed to be so v1 < v2.
  int v1;
  int v2;
};

// Map from an edge defined by its vertices index to a custom tag value.
template<typename T> class EdgeTagMap {
 public:
  typedef EdgeKey key_type;
  typedef T value_type;

  inline EdgeTagMap();

  // Modifiers.
  inline void clear();
  inline void insert(const key_type &key, const value_type &value);
  inline void insert(int v1, int v2, const value_type &value);

  // Lookup.
  value_type &at(const key_type &key);
  value_type &at(key_type &&key);
  value_type &at(int v1, int v2);

  value_type &operator[](const key_type &key);
  value_type &operator[](key_type &&key);

 protected:
  unordered_map<key_type, value_type> edge_tags_;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation.

// EdgeKey.

EdgeKey::EdgeKey() : v1(-1), v2(-1)
{
}

EdgeKey::EdgeKey(int v1, int v2)
{
  assert(v1 >= 0);
  assert(v2 >= 0);
  assert(v1 != v2);
  if (v1 < v2) {
    this->v1 = v1;
    this->v2 = v2;
  }
  else {
    this->v1 = v2;
    this->v2 = v1;
  }
}

size_t EdgeKey::hash() const
{
  return (static_cast<uint64_t>(v1) << 32) | v2;
}

bool EdgeKey::operator==(const EdgeKey &other) const
{
  return v1 == other.v1 && v2 == other.v2;
}

// EdgeTagMap.

template<typename T> EdgeTagMap<T>::EdgeTagMap()
{
}

template<typename T> void EdgeTagMap<T>::clear()
{
  edge_tags_.clear();
}

template<typename T> void EdgeTagMap<T>::insert(const key_type &key, const value_type &value)
{
  edge_tags_.insert(make_pair(key, value));
}

template<typename T> void EdgeTagMap<T>::insert(int v1, int v2, const value_type &value)
{
  insert(EdgeKey(v1, v2), value);
}

template<typename T> typename EdgeTagMap<T>::value_type &EdgeTagMap<T>::at(const key_type &key)
{
  return edge_tags_.at[key];
}

template<typename T> typename EdgeTagMap<T>::value_type &EdgeTagMap<T>::at(key_type &&key)
{
  return edge_tags_.at[key];
}

template<typename T> typename EdgeTagMap<T>::value_type &EdgeTagMap<T>::at(int v1, int v2)
{
  return edge_tags_.at(EdgeKey(v1, v2));
}

template<typename T>
typename EdgeTagMap<T>::value_type &EdgeTagMap<T>::operator[](const key_type &key)
{
  return edge_tags_[key];
}

template<typename T> typename EdgeTagMap<T>::value_type &EdgeTagMap<T>::operator[](key_type &&key)
{
  return edge_tags_[key];
}

}  // namespace opensubdiv_capi

namespace std {

template<> struct hash<opensubdiv_capi::EdgeKey> {
  std::size_t operator()(const opensubdiv_capi::EdgeKey &key) const
  {
    return key.hash();
  }
};

}  // namespace std

#endif  // OPENSUBDIV_EDGE_MAP_H_
