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

#include <carve/carve.hpp>

namespace carve {

  class tagable {
  private:
    static int s_count;

  protected:
    mutable int __tag;

  public:
    tagable(const tagable &) : __tag(s_count - 1) { }
    tagable &operator=(const tagable &) { return *this; }

    tagable() : __tag(s_count - 1) { }

    void tag() const { __tag = s_count; }
    void untag() const { __tag = s_count - 1; }
    bool is_tagged() const { return __tag == s_count; }
    bool tag_once() const { if (__tag == s_count) return false; __tag = s_count; return true; }

    static void tag_begin() { s_count++; }
  };
}
