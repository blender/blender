# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(DLSS_VERSION   310.9.1)
set(OPTIX_VERSION  8.0.0)

set(CUDA12_VERSION 12.9.1)
set(CUDA13_VERSION 13.4.1)

# CUDA_TOOLKIT_ROOT_DIR and CUDA_NVCC_EXECUTABLE will be initialized to the CUDA toolkit
# with this major version.
set(DEFAULT_CUDA_MAJOR_VERSION 13)

if(WIN32)
  set(HIP_VERSION 7.1.51803)
  set(OCLOC_VERSION 101.8424)
elseif(UNIX AND NOT (APPLE OR HAIKU))
  set(HIP_VERSION 7.2.1)
endif()
