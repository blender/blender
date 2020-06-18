# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  include
)

set(INC_SYS
  ../gtest/include
)

set(SRC
  # src/gmock-all.cc
  # src/gmock_main.cc

  src/gmock.cc
  src/gmock-cardinalities.cc
  src/gmock-internal-utils.cc
  src/gmock-matchers.cc
  src/gmock-spec-builders.cc
)

set(SRC_HEADERS
  include/gmock/gmock.h
  include/gmock/gmock-actions.h
  include/gmock/gmock-cardinalities.h
  include/gmock/gmock-function-mocker.h
  include/gmock/gmock-generated-actions.h
  include/gmock/gmock-generated-function-mockers.h
  include/gmock/gmock-generated-matchers.h
  include/gmock/gmock-matchers.h
  include/gmock/gmock-more-actions.h
  include/gmock/gmock-more-matchers.h
  include/gmock/gmock-nice-strict.h
  include/gmock/gmock-spec-builders.h
  include/gmock/internal/custom/gmock-generated-actions.h
  include/gmock/internal/custom/gmock-matchers.h
  include/gmock/internal/custom/gmock-port.h
  include/gmock/internal/gmock-internal-utils.h
  include/gmock/internal/gmock-port.h
  include/gmock/internal/gmock-pp.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})
add_library(extern_gmock ${SRC} ${SRC_HEADERS})
