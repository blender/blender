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

set(LICENSES
  Apache_2.0.txt
  ILM.txt
  NVidia.txt
  OSL.txt
  Sobol.txt
  readme.txt
)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR}  "${LICENSES}" ${CYCLES_INSTALL_PATH}/license)
