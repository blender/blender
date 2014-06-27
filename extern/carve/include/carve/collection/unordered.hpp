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

#if defined(HAVE_STD_UNORDERED_COLLECTIONS)

#  include <carve/collection/unordered/std_impl.hpp>

#elif defined(HAVE_TR1_UNORDERED_COLLECTIONS)

#  include <carve/collection/unordered/tr1_impl.hpp>

#elif defined(HAVE_BOOST_UNORDERED_COLLECTIONS)

#  include <carve/collection/unordered/boost_impl.hpp>

#elif defined(HAVE_LIBSTDCPP_UNORDERED_COLLECTIONS)

#  include <carve/collection/unordered/libstdcpp_impl.hpp>

#elif defined(_MSC_VER) && _MSC_VER >= 1300

#  include <carve/collection/unordered/vcpp_impl.hpp>

#else

#  include <carve/collection/unordered/fallback_impl.hpp>

#endif
