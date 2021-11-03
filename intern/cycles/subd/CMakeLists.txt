# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(INC
  ..
)

set(INC_SYS

)

set(SRC
  dice.cpp
  patch.cpp
  split.cpp
  patch_table.cpp
)

set(SRC_HEADERS
  dice.h
  patch.h
  patch_table.h
  split.h
  subpatch.h
)

set(LIB

)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_subd "${LIB}" ${SRC} ${SRC_HEADERS})
