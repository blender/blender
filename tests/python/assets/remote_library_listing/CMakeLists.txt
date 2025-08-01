# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_blender_test(
  assets_remote_library_listing_downloader_test
    --python ${CMAKE_CURRENT_LIST_DIR}/listing_downloader_test.py --
    --output-dir ${TEST_OUT_DIR}/assets/
)
