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

#include <carve/collection/unordered.hpp>

namespace carve {

  template<typename set_t>
  class set_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {

  protected:
    set_t *set;
  public:

    set_insert_iterator(set_t &s) : set(&s) {
    }

    set_insert_iterator &
    operator=(typename set_t::const_reference value) {
      set->insert(value);
      return *this;
    }

    set_insert_iterator &operator*() { return *this; }
    set_insert_iterator &operator++() { return *this; }
    set_insert_iterator &operator++(int) { return *this; }
  };

  template<typename set_t>
  inline set_insert_iterator<set_t>
  set_inserter(set_t &s) {
    return set_insert_iterator<set_t>(s);
  }

}
