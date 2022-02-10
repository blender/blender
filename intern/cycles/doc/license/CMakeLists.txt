# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2020 Blender Foundation

set(LICENSES
  Apache_2.0.txt
  ILM.txt
  NVidia.txt
  OSL.txt
  Sobol.txt
  readme.txt
)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR}  "${LICENSES}" ${CYCLES_INSTALL_PATH}/license)
