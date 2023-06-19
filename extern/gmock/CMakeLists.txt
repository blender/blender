# SPDX-FileCopyrightText: 2014 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
