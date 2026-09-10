# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# Find the DLSS SDK. This modules defines
#  DLSS_INCLUDE_DIR, where to find the NGX headers for DLSS
#  DLSS_FOUND, if the DLSS SDK is found.

find_path(DLSS_INCLUDE_DIR
    NAMES
        "nvsdk_ngx.h"
    PATHS
        "${DLSS_SDK_ROOT}/include"
        "$ENV{DLSS_SDK_ROOT}/include"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(DLSS REQUIRED_VARS DLSS_INCLUDE_DIR)
