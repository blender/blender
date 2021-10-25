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

#include BOOST_INCLUDE(unordered_set.hpp)
#include BOOST_INCLUDE(unordered_map.hpp)

#include <functional>

namespace std {
  template <typename Key, typename T, typename Hash = boost::hash<Key>,
            typename Pred = std::equal_to<Key> >
  class unordered_map : public boost::unordered_map<Key, T, Hash, Pred> {

  public:
    typedef T data_type;
  };

  template <typename Key, typename T, typename Hash = boost::hash<Key>,
            typename Pred = std::equal_to<Key> >
  class unordered_multimap : public boost::unordered_multimap<Key, T, Hash, Pred> {
  };

  template <typename Value, typename Hash = boost::hash<Value>,
            typename Pred = std::equal_to<Value> >
  class unordered_set : public boost::unordered_set<Value, Hash, Pred> {
  };
}

#undef UNORDERED_COLLECTIONS_SUPPORT_RESIZE
