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

set(SRC
  node.cpp
  node_type.cpp
  node_xml.cpp
)

set(SRC_HEADERS
  node.h
  node_enum.h
  node_type.h
  node_xml.h
)

set(LIB
  cycles_util
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_graph "${LIB}" ${SRC} ${SRC_HEADERS})
