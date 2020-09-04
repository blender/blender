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
  subd_dice.cpp
  subd_patch.cpp
  subd_split.cpp
  subd_patch_table.cpp
)

set(SRC_HEADERS
  subd_dice.h
  subd_patch.h
  subd_patch_table.h
  subd_split.h
  subd_subpatch.h
)

set(LIB

)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_subd "${LIB}" ${SRC} ${SRC_HEADERS})
