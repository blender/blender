// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <hash_map>
#include <hash_set>

namespace std {

  namespace {

    template<class Value, class Hash> class hash_traits {
      Hash hash_value;
      std::less<Value> comp;
    public:
      enum {
        bucket_size = 4,
        min_buckets = 8
      };
      // hash _Keyval to size_t value
      size_t operator()(const Value& v) const {
        return ((size_t)hash_value(v));
      }
      // test if _Keyval1 ordered before _Keyval2
      bool operator()(const Value& v1, const Value& v2) const {
        return (comp(v1, v2));
      }
    };

  }

  template <typename Key, typename T, typename Hash = stdext::hash_compare<Key, less<Key> >, typename Pred = std::equal_to<Key> >
  class unordered_map 
    : public stdext::hash_map<Key, T, hash_traits<Key, Hash> > {
    typedef stdext::hash_map<Key, T, hash_traits<Key, Hash> > super;
  public:
    unordered_map() : super() {}
  };

  template <typename Value, typename Hash = stdext::hash_compare<Key, less<Key> >, typename Pred = std::equal_to<Value> >
  class unordered_set
    : public stdext::hash_set<Value, hash_traits<Value, Hash> > {
    typedef stdext::hash_set<Value, hash_traits<Value, Hash> > super;
  public:
    unordered_set() : super() {}
  };

}

#undef UNORDERED_COLLECTIONS_SUPPORT_RESIZE
