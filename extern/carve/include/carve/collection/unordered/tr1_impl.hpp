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

#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <tr1/functional>

namespace std {
  namespace tr1 {
    template <typename A, typename B>
    struct hash<std::pair<A, B> > : public std::unary_function<std::pair<A, B>, size_t> {
      size_t operator()(const std::pair<A, B> &v) const {
        std::size_t seed = 0;

        seed ^= hash<A>()(v.first);
        seed ^= hash<B>()(v.second) + (seed<<6) + (seed>>2);

        return seed;
      }
    };
  }



  template <typename Key, typename T,
            typename Hash = tr1::hash<Key>,
            typename Pred = std::equal_to<Key> >
  class unordered_map : public std::tr1::unordered_map<Key, T, Hash, Pred> {
  public:
    typedef T data_type;
  };

  template <typename Value,
            typename Hash = tr1::hash<Value>,
            typename Pred = std::equal_to<Value> >
  class unordered_set : public std::tr1::unordered_set<Value, Hash, Pred> {
  public:
  };

}

#undef UNORDERED_COLLECTIONS_SUPPORT_RESIZE
