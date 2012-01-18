// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <set>
#include <map>

namespace std {

  template<typename K, typename T, typename H = int>
  class unordered_map : public std::map<K, T> {
    typedef std::map<K, T> super;
  public:
    typedef T data_type;
  };

  template<typename K, typename H = int>
  class unordered_set : public std::set<K> {
    typedef std::set<K> super;
  public:
  };

}

#undef UNORDERED_COLLECTIONS_SUPPORT_RESIZE
