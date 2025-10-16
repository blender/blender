#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
this script updates XML themes once new settings are added

  ./blender.bin --background --python tools/check_source/check_descriptions.py
"""
__all__ = (
    "main",
)

import bpy

# These are known duplicates which do not warn.
DUPLICATE_ACCEPT = (
    # operators
    ('ACTION_OT_clean', 'GRAPH_OT_clean'),
    ('ACTION_OT_clickselect', 'GRAPH_OT_clickselect'),
    ('ACTION_OT_copy', 'GRAPH_OT_copy'),
    ('ACTION_OT_delete', 'GRAPH_OT_delete'),
    ('ACTION_OT_duplicate', 'GRAPH_OT_duplicate'),
    ('ACTION_OT_duplicate_move', 'GRAPH_OT_duplicate_move'),
    ('ACTION_OT_extrapolation_type', 'GRAPH_OT_extrapolation_type'),
    ('ACTION_OT_handle_type', 'GRAPH_OT_handle_type'),
    ('ACTION_OT_interpolation_type', 'GRAPH_OT_interpolation_type'),
    ('ACTION_OT_keyframe_insert', 'GRAPH_OT_keyframe_insert'),
    ('ACTION_OT_mirror', 'GRAPH_OT_mirror'),
    ('ACTION_OT_paste', 'GRAPH_OT_paste'),
    ('ACTION_OT_sample', 'GRAPH_OT_sample'),
    ('ACTION_OT_select_all', 'GRAPH_OT_select_all'),
    ('ACTION_OT_select_border', 'GRAPH_OT_select_border'),
    ('ACTION_OT_select_column', 'GRAPH_OT_select_column'),
    ('ACTION_OT_select_leftright', 'GRAPH_OT_select_leftright'),
    ('ACTION_OT_select_less', 'GRAPH_OT_select_less'),
    ('ACTION_OT_select_linked', 'GRAPH_OT_select_linked'),
    ('ACTION_OT_select_more', 'GRAPH_OT_select_more'),
    ('ACTION_OT_unlink', 'NLA_OT_action_unlink'),
    ('ACTION_OT_view_all', 'CLIP_OT_dopesheet_view_all', 'GRAPH_OT_view_all'),
    ('ACTION_OT_view_frame', 'GRAPH_OT_view_frame'),
    ('ANIM_OT_change_frame', 'CLIP_OT_change_frame', 'IMAGE_OT_change_frame'),
    ('ARMATURE_OT_autoside_names', 'POSE_OT_autoside_names'),
    ('ARMATURE_OT_bone_layers', 'POSE_OT_bone_layers'),
    ('ARMATURE_OT_extrude_forked', 'ARMATURE_OT_extrude_move'),
    ('ARMATURE_OT_flip_names', 'POSE_OT_flip_names'),
    ('ARMATURE_OT_select_all', 'POSE_OT_select_all'),
    ('ARMATURE_OT_select_hierarchy', 'POSE_OT_select_hierarchy'),
    ('ARMATURE_OT_select_linked', 'POSE_OT_select_linked'),
    ('ARMATURE_OT_select_mirror', 'POSE_OT_select_mirror'),
    ('CLIP_OT_cursor_set', 'UV_OT_cursor_set'),
    ('CLIP_OT_disable_markers', 'CLIP_OT_graph_disable_markers'),
    ('CLIP_OT_graph_select_border', 'MASK_OT_select_border'),
    ('CLIP_OT_view_ndof', 'IMAGE_OT_view_ndof', 'VIEW2D_OT_ndof'),
    ('CLIP_OT_view_pan', 'IMAGE_OT_view_pan', 'VIEW2D_OT_pan', 'VIEW3D_OT_view_pan'),
    ('CLIP_OT_view_zoom', 'VIEW2D_OT_zoom'),
    ('CLIP_OT_view_zoom_in', 'VIEW2D_OT_zoom_in'),
    ('CLIP_OT_view_zoom_out', 'VIEW2D_OT_zoom_out'),
    ('CONSOLE_OT_copy', 'FONT_OT_text_copy', 'TEXT_OT_copy'),
    ('CONSOLE_OT_delete', 'FONT_OT_delete', 'TEXT_OT_delete'),
    ('CONSOLE_OT_insert', 'FONT_OT_text_insert', 'TEXT_OT_insert'),
    ('CONSOLE_OT_paste', 'FONT_OT_text_paste', 'TEXT_OT_paste'),
    ('CURVE_OT_handle_type_set', 'MASK_OT_handle_type_set'),
    ('CURVE_OT_shortest_path_pick', 'MESH_OT_shortest_path_pick'),
    ('CURVE_OT_switch_direction', 'MASK_OT_switch_direction'),
    ('FONT_OT_line_break', 'TEXT_OT_line_break'),
    ('FONT_OT_move', 'TEXT_OT_move'),
    ('FONT_OT_move_select', 'TEXT_OT_move_select'),
    ('FONT_OT_select_all', 'TEXT_OT_select_all'),
    ('FONT_OT_text_cut', 'TEXT_OT_cut'),
    ('GRAPH_OT_previewrange_set', 'NLA_OT_previewrange_set'),
    ('GRAPH_OT_properties', 'IMAGE_OT_properties', 'LOGIC_OT_properties', 'NLA_OT_properties'),
    ('IMAGE_OT_clear_render_border', 'VIEW3D_OT_clear_render_border'),
    ('IMAGE_OT_render_border', 'VIEW3D_OT_render_border'),
    ('IMAGE_OT_toolshelf', 'NODE_OT_toolbar', 'VIEW3D_OT_toolshelf'),
    ('LATTICE_OT_select_ungrouped', 'MESH_OT_select_ungrouped', 'PAINT_OT_vert_select_ungrouped'),
    ('MESH_OT_extrude_region_move', 'MESH_OT_extrude_region_shrink_fatten'),
    ('NODE_OT_add_node', 'NODE_OT_add_search'),
    ('NODE_OT_move_detach_links', 'NODE_OT_move_detach_links_release'),
    ('NODE_OT_properties', 'VIEW3D_OT_properties'),
    ('OBJECT_OT_bake', 'OBJECT_OT_bake_image'),
    ('OBJECT_OT_duplicate_move', 'OBJECT_OT_duplicate_move_linked'),
    ('WM_OT_context_cycle_enum', 'WM_OT_context_toggle', 'WM_OT_context_toggle_enum'),
    ('WM_OT_context_set_boolean', 'WM_OT_context_set_enum', 'WM_OT_context_set_float',
     'WM_OT_context_set_int', 'WM_OT_context_set_string', 'WM_OT_context_set_value'),
)

DUPLICATE_IGNORE = {
    "",
}


def check_duplicates():
    import _rna_info as rna_info

    DUPLICATE_IGNORE_FOUND = set()
    DUPLICATE_ACCEPT_FOUND = set()

    structs, funcs, ops, props = rna_info.BuildRNAInfo()

    # This is mainly useful for operators,
    # other types have too many false positives

    # for t in (structs, funcs, ops, props):
    for t in (ops, ):
        description_dict = {}
        print("")
        for k, v in t.items():
            if v.description not in DUPLICATE_IGNORE:
                id_str = ".".join([s if isinstance(s, str) else s.identifier for s in k if s])
                description_dict.setdefault(v.description, []).append(id_str)
            else:
                DUPLICATE_IGNORE_FOUND.add(v.description)
        # sort for easier viewing
        sort_ls = [(tuple(sorted(v)), k) for k, v in description_dict.items()]
        sort_ls.sort()

        for v, k in sort_ls:
            if len(v) > 1:
                if v not in DUPLICATE_ACCEPT:
                    print("found %d: %r, \"%s\"" % (len(v), v, k))
                    # print("%r," % (v,))
                else:
                    DUPLICATE_ACCEPT_FOUND.add(v)

    test = (DUPLICATE_IGNORE - DUPLICATE_IGNORE_FOUND)
    if test:
        print("Invalid 'DUPLICATE_IGNORE': %r" % test)
    test = (set(DUPLICATE_ACCEPT) - DUPLICATE_ACCEPT_FOUND)
    if test:
        print("Invalid 'DUPLICATE_ACCEPT': %r" % test)


def main():
    check_duplicates()


if __name__ == "__main__":
    main()
