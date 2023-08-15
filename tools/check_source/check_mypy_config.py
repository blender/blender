# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from typing import (
    Any,
    Tuple,
    Dict,
)

PATHS: Tuple[Tuple[str, Tuple[Any, ...], Dict[str, str]], ...] = (
    ("build_files/cmake/", (), {'MYPYPATH': "modules"}),
    ("build_files/utils/", (), {'MYPYPATH': "modules"}),
    ("doc/manpage/blender.1.py", (), {}),
    ("tools/check_blender_release/", (), {}),
    ("tools/check_source/", (), {'MYPYPATH': "modules"}),
    ("tools/check_wiki/", (), {}),
    ("tools/utils/", (), {}),
    ("tools/utils_api/", (), {}),
    ("tools/utils_build/", (), {}),
    ("tools/utils_doc/", (), {}),
    ("tools/utils_ide/", (), {}),
    ("tools/utils_maintenance/", (), {'MYPYPATH': "modules"}),
)

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", ".."))))

PATHS_EXCLUDE = set(
    os.path.join(SOURCE_DIR, p.replace("/", os.sep))
    for p in
    (
        "build_files/cmake/clang_array_check.py",
        "build_files/cmake/cmake_netbeans_project.py",
        "build_files/cmake/cmake_qtcreator_project.py",
        "build_files/cmake/cmake_static_check_smatch.py",
        "build_files/cmake/cmake_static_check_sparse.py",
        "build_files/cmake/cmake_static_check_splint.py",
        "tools/check_blender_release/check_module_enabled.py",
        "tools/check_blender_release/check_module_numpy.py",
        "tools/check_blender_release/check_module_requests.py",
        "tools/check_blender_release/check_release.py",
        "tools/check_blender_release/check_static_binaries.py",
        "tools/check_blender_release/check_utils.py",
        "tools/check_blender_release/scripts/modules_enabled.py",
        "tools/check_blender_release/scripts/requests_basic_access.py",
        "tools/check_blender_release/scripts/requests_import.py",
        "tools/check_source/check_descriptions.py",
        "tools/check_source/check_header_duplicate.py",
        "tools/check_source/check_unused_defines.py",
        "tools/utils/blend2json.py",
        "tools/utils/blender_keyconfig_export_permutations.py",
        "tools/utils/blender_merge_format_changes.py",
        "tools/utils/blender_theme_as_c.py",
        "tools/utils/cycles_commits_sync.py",
        "tools/utils/cycles_timeit.py",
        "tools/utils/gdb_struct_repr_c99.py",
        "tools/utils/git_log_review_commits.py",
        "tools/utils/git_log_review_commits_advanced.py",
        "tools/utils/gitea_inactive_developers.py",
        "tools/utils/make_cursor_gui.py",
        "tools/utils/make_gl_stipple_from_xpm.py",
        "tools/utils/make_shape_2d_from_blend.py",
        "tools/utils/weekly_report.py",
        "tools/utils_api/bpy_introspect_ui.py",  # Uses `bpy`.
        "tools/utils_doc/rna_manual_reference_updater.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_assembler_preview.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_blender_diffusion.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_cpp_to_c_comments.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_doxy_file.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_project_update.py",
        "tools/utils_ide/qtcreator/externaltools/qtc_sort_paths.py",
        "tools/utils_maintenance/blender_menu_search_coverage.py",  # Uses `bpy`.
        "tools/utils_maintenance/blender_update_themes.py",  # Uses `bpy`.
        "tools/utils_maintenance/trailing_space_clean.py",
        "tools/utils_maintenance/trailing_space_clean_config.py",
    )
)

PATHS = tuple(
    (os.path.join(SOURCE_DIR, p_items[0].replace("/", os.sep)), *p_items[1:])
    for p_items in PATHS
)
