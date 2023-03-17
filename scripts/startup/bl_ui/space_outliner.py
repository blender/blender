# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Header, Menu, Panel

from bpy.app.translations import (
    contexts as i18n_contexts,
)


class OUTLINER_HT_header(Header):
    bl_space_type = "OUTLINER"

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        display_mode = space.display_mode
        scene = context.scene
        ks = context.scene.keying_sets.active

        layout.template_header()

        layout.prop(space, "display_mode", icon_only=True)

        if display_mode == "DATA_API":
            OUTLINER_MT_editor_menus.draw_collapsible(context, layout)
        if display_mode == "LIBRARY_OVERRIDES":
            layout.prop(space, "lib_override_view_mode", text="")

        layout.separator_spacer()

        filter_text_supported = True
        # No text filtering for library override hierarchies. The tree is lazy built to avoid
        # performance issues in complex files.
        if (
            display_mode == "LIBRARY_OVERRIDES"
            and space.lib_override_view_mode == "HIERARCHIES"
        ):
            filter_text_supported = False

        if filter_text_supported:
            row = layout.row(align=True)
            row.prop(space, "filter_text", icon="VIEWZOOM", text="")

        layout.separator_spacer()

        if display_mode == "SEQUENCE":
            row = layout.row(align=True)
            row.prop(space, "use_sync_select", icon="UV_SYNC_SELECT", text="")

        row = layout.row(align=True)
        if display_mode in {"SCENES", "VIEW_LAYER", "LIBRARY_OVERRIDES"}:
            row.popover(
                panel="OUTLINER_PT_filter",
                text="",
                icon="FILTER",
            )

        if display_mode in {"LIBRARIES", "ORPHAN_DATA"}:
            row.prop(space, "use_filter_id_type", text="", icon="FILTER")
            sub = row.row(align=True)
            sub.active = space.use_filter_id_type
            sub.prop(space, "filter_id_type", text="", icon_only=True)

        if display_mode == "VIEW_LAYER":
            layout.operator(
                "outliner.collection_new", text="", icon="COLLECTION_NEW"
            ).nested = True

        elif display_mode == "ORPHAN_DATA":
            layout.operator("outliner.orphans_purge", text="Purge").do_recursive = True

        elif space.display_mode == "DATA_API":
            layout.separator()

            row = layout.row(align=True)
            row.operator("outliner.keyingset_add_selected", icon="ADD", text="")
            row.operator("outliner.keyingset_remove_selected", icon="REMOVE", text="")

            if ks:
                row = layout.row()
                row.prop_search(
                    scene.keying_sets, "active", scene, "keying_sets", text=""
                )

                row = layout.row(align=True)
                row.operator("anim.keyframe_insert", text="", icon="KEY_HLT")
                row.operator("anim.keyframe_delete", text="", icon="KEY_DEHLT")
            else:
                row = layout.row()
                row.label(text="No Keying Set Active")


class OUTLINER_MT_editor_menus(Menu):
    bl_idname = "OUTLINER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.display_mode == "DATA_API":
            layout.menu("OUTLINER_MT_edit_datablocks")


class OUTLINER_MT_context_menu(Menu):
    bl_label = "Outliner Context Menu"

    @staticmethod
    def draw_common_operators(layout):
        layout.menu_contents("OUTLINER_MT_asset")

        layout.separator()

        layout.menu("OUTLINER_MT_liboverride")

        layout.separator()

        layout.menu("OUTLINER_MT_context_menu_view")

        layout.separator()

        layout.menu("INFO_MT_area")

    def draw(self, context):
        space = context.space_data

        layout = self.layout

        if space.display_mode == "VIEW_LAYER":
            OUTLINER_MT_collection_new.draw_without_context_menu(context, layout)
            layout.separator()

        OUTLINER_MT_context_menu.draw_common_operators(layout)


class OUTLINER_MT_context_menu_view(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        layout.operator("outliner.show_active")

        layout.separator()

        layout.operator("outliner.show_hierarchy")
        layout.operator("outliner.show_one_level", text="Show One Level")
        layout.operator("outliner.show_one_level", text="Hide One Level").open = False


class OUTLINER_MT_view_pie(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("outliner.show_hierarchy")
        pie.operator("outliner.show_active", icon="ZOOM_SELECTED")


class OUTLINER_MT_edit_datablocks(Menu):
    bl_label = "Edit"

    def draw(self, _context):
        layout = self.layout

        layout.operator("outliner.keyingset_add_selected")
        layout.operator("outliner.keyingset_remove_selected")

        layout.separator()

        layout.operator("outliner.drivers_add_selected")
        layout.operator("outliner.drivers_delete_selected")


class OUTLINER_MT_collection_view_layer(Menu):
    bl_label = "View Layer"

    def draw(self, context):
        layout = self.layout

        layout.operator("outliner.collection_exclude_set")
        layout.operator("outliner.collection_exclude_clear")

        layout.operator("outliner.collection_holdout_set")
        layout.operator("outliner.collection_holdout_clear")

        if context.engine == "CYCLES":
            layout.operator("outliner.collection_indirect_only_set")
            layout.operator("outliner.collection_indirect_only_clear")


class OUTLINER_MT_collection_visibility(Menu):
    bl_label = "Visibility"

    def draw(self, _context):
        layout = self.layout

        layout.operator("outliner.collection_isolate", text="Isolate")

        layout.separator()

        layout.operator("outliner.collection_show", text="Show", icon="HIDE_OFF")
        layout.operator("outliner.collection_show_inside", text="Show All Inside")
        layout.operator("outliner.collection_hide", text="Hide", icon="HIDE_ON")
        layout.operator("outliner.collection_hide_inside", text="Hide All Inside")

        layout.separator()

        layout.operator(
            "outliner.collection_enable",
            text="Enable in Viewports",
            icon="RESTRICT_VIEW_OFF",
        )
        layout.operator("outliner.collection_disable", text="Disable in Viewports")

        layout.separator()

        layout.operator(
            "outliner.collection_enable_render",
            text="Enable in Render",
            icon="RESTRICT_RENDER_OFF",
        )
        layout.operator("outliner.collection_disable_render", text="Disable in Render")


class OUTLINER_MT_collection(Menu):
    bl_label = "Collection"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.operator(
            "outliner.collection_new", text="New", text_ctxt=i18n_contexts.id_collection
        ).nested = True
        layout.operator("outliner.collection_duplicate", text="Duplicate Collection")
        layout.operator("outliner.collection_duplicate_linked", text="Duplicate Linked")
        layout.operator("outliner.id_copy", text="Copy", icon="COPYDOWN")
        layout.operator("outliner.id_paste", text="Paste", icon="PASTEDOWN")

        layout.separator()

        layout.operator("outliner.delete", text="Delete", icon="X")
        layout.operator("outliner.delete", text="Delete Hierarchy").hierarchy = True

        layout.separator()

        layout.operator(
            "outliner.collection_objects_select",
            text="Select Objects",
            icon="RESTRICT_SELECT_OFF",
        )
        layout.operator("outliner.collection_objects_deselect", text="Deselect Objects")

        layout.separator()

        layout.operator("outliner.collection_instance", text="Instance to Scene")

        if space.display_mode != "VIEW_LAYER":
            layout.operator("outliner.collection_link", text="Link to Scene")
        layout.operator("outliner.id_operation", text="Unlink").type = "UNLINK"

        layout.separator()

        layout.menu("OUTLINER_MT_collection_visibility")

        if space.display_mode == "VIEW_LAYER":
            layout.separator()
            layout.menu("OUTLINER_MT_collection_view_layer", icon="RENDERLAYERS")

        layout.separator()
        row = layout.row(align=True)
        row.operator_enum("outliner.collection_color_tag_set", "color", icon_only=True)

        layout.separator()

        layout.operator_menu_enum("outliner.id_operation", "type", text="ID Data")

        layout.separator()

        OUTLINER_MT_context_menu.draw_common_operators(layout)


class OUTLINER_MT_collection_new(Menu):
    bl_label = "Collection"

    @staticmethod
    def draw_without_context_menu(_context, layout):
        layout.operator("outliner.collection_new", text="New Collection").nested = True
        layout.operator("outliner.id_paste", text="Paste Data-Blocks", icon="PASTEDOWN")

    def draw(self, context):
        layout = self.layout

        self.draw_without_context_menu(context, layout)

        layout.separator()

        OUTLINER_MT_context_menu.draw_common_operators(layout)


class OUTLINER_MT_object(Menu):
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.operator("outliner.id_copy", text="Copy", icon="COPYDOWN")
        layout.operator("outliner.id_paste", text="Paste", icon="PASTEDOWN")

        layout.separator()

        layout.operator("outliner.delete", text="Delete", icon="X")
        layout.operator("outliner.delete", text="Delete Hierarchy").hierarchy = True

        layout.separator()

        layout.operator(
            "outliner.object_operation", text="Select", icon="RESTRICT_SELECT_OFF"
        ).type = "SELECT"
        layout.operator(
            "outliner.object_operation", text="Select Hierarchy"
        ).type = "SELECT_HIERARCHY"
        layout.operator("outliner.object_operation", text="Deselect").type = "DESELECT"

        layout.separator()

        if not (space.display_mode == "VIEW_LAYER" and not space.use_filter_collection):
            layout.operator("outliner.id_operation", text="Unlink").type = "UNLINK"
            layout.separator()

        layout.operator("outliner.collection_new", text="New Collection").nested = True

        layout.separator()

        layout.operator_menu_enum("outliner.id_operation", "type", text="ID Data")

        layout.separator()

        OUTLINER_MT_context_menu.draw_common_operators(layout)


def has_selected_ids_in_context(context):
    if hasattr(context, "id"):
        return True
    if len(context.selected_ids) > 0:
        return True

    return False


class OUTLINER_MT_asset(Menu):
    bl_label = "Assets"

    @classmethod
    def poll(cls, context):
        return has_selected_ids_in_context(context)

    def draw(self, _context):
        layout = self.layout

        layout.operator("asset.mark")
        layout.operator("asset.clear", text="Clear Asset").set_fake_user = False
        layout.operator(
            "asset.clear", text="Clear Asset (Set Fake User)"
        ).set_fake_user = True


class OUTLINER_MT_liboverride(Menu):
    bl_label = "Library Override"

    @classmethod
    def poll(cls, context):
        return has_selected_ids_in_context(context)

    def draw(self, _context):
        layout = self.layout

        layout.operator_menu_enum(
            "outliner.liboverride_operation", "selection_set", text="Make"
        ).type = "OVERRIDE_LIBRARY_CREATE_HIERARCHY"
        layout.operator_menu_enum(
            "outliner.liboverride_operation", "selection_set", text="Reset"
        ).type = "OVERRIDE_LIBRARY_RESET"
        layout.operator_menu_enum(
            "outliner.liboverride_operation", "selection_set", text="Clear"
        ).type = "OVERRIDE_LIBRARY_CLEAR_SINGLE"

        layout.separator()

        layout.operator_menu_enum(
            "outliner.liboverride_troubleshoot_operation", "type", text="Troubleshoot"
        ).selection_set = "SELECTED"


class OUTLINER_PT_filter(Panel):
    bl_space_type = "OUTLINER"
    bl_region_type = "HEADER"
    bl_label = "Filter"

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        display_mode = space.display_mode

        if display_mode == "VIEW_LAYER":
            layout.label(text="Restriction Toggles")
            row = layout.row(align=True)
            row.prop(space, "show_restrict_column_enable", text="")
            row.prop(space, "show_restrict_column_select", text="")
            row.prop(space, "show_restrict_column_hide", text="")
            row.prop(space, "show_restrict_column_viewport", text="")
            row.prop(space, "show_restrict_column_render", text="")
            row.prop(space, "show_restrict_column_holdout", text="")
            row.prop(space, "show_restrict_column_indirect_only", text="")
            layout.separator()
        elif display_mode == "SCENES":
            layout.label(text="Restriction Toggles")
            row = layout.row(align=True)
            row.prop(space, "show_restrict_column_select", text="")
            row.prop(space, "show_restrict_column_hide", text="")
            row.prop(space, "show_restrict_column_viewport", text="")
            row.prop(space, "show_restrict_column_render", text="")
            layout.separator()

        if display_mode != "DATA_API":
            col = layout.column(align=True)
            col.prop(space, "use_sort_alpha")

        if display_mode != "LIBRARY_OVERRIDES":
            row = layout.row(align=True)
            row.prop(space, "use_sync_select", text="Sync Selection")

            row = layout.row(align=True)
            row.prop(space, "show_mode_column", text="Show Mode Column")
            layout.separator()

        filter_text_supported = True
        # Same exception for library overrides as in OUTLINER_HT_header.
        if (
            display_mode == "LIBRARY_OVERRIDES"
            and space.lib_override_view_mode == "HIERARCHIES"
        ):
            filter_text_supported = False

        if filter_text_supported:
            col = layout.column(align=True)
            col.label(text="Search")
            col.prop(space, "use_filter_complete", text="Exact Match")
            col.prop(space, "use_filter_case_sensitive", text="Case Sensitive")

        if (
            display_mode == "LIBRARY_OVERRIDES"
            and space.lib_override_view_mode == "PROPERTIES"
            and bpy.data.libraries
        ):
            row = layout.row()
            row.label(icon="LIBRARY_DATA_OVERRIDE")
            row.prop(space, "use_filter_lib_override_system", text="System Overrides")

        if display_mode != "VIEW_LAYER":
            return

        layout.separator()

        layout.label(text="Filter")

        col = layout.column(align=True)

        row = col.row()
        row.label(icon="RENDERLAYERS")
        row.prop(space, "use_filter_view_layers", text="All View Layers")

        row = col.row()
        row.label(icon="OUTLINER_COLLECTION")
        row.prop(space, "use_filter_collection", text="Collections")

        row = col.row()
        row.label(icon="OBJECT_DATAMODE")
        row.prop(space, "use_filter_object", text="Objects")
        row = col.row(align=True)
        row.label(icon="BLANK1")
        row.prop(space, "filter_state", text="")
        sub = row.row(align=True)
        sub.enabled = space.filter_state != "ALL"
        sub.prop(space, "filter_invert", text="", icon="ARROW_LEFTRIGHT")

        sub = col.column(align=True)
        sub.active = space.use_filter_object

        row = sub.row()
        row.label(icon="BLANK1")
        row.prop(space, "use_filter_object_content", text="Object Contents")
        row = sub.row()
        row.label(icon="BLANK1")
        row.prop(space, "use_filter_children", text="Object Children")

        if bpy.data.meshes:
            row = sub.row()
            row.label(icon="MESH_DATA")
            row.prop(space, "use_filter_object_mesh", text="Meshes")
        if bpy.data.armatures:
            row = sub.row()
            row.label(icon="ARMATURE_DATA")
            row.prop(space, "use_filter_object_armature", text="Armatures")
        if bpy.data.lights:
            row = sub.row()
            row.label(icon="LIGHT_DATA")
            row.prop(space, "use_filter_object_light", text="Lights")
        if bpy.data.cameras:
            row = sub.row()
            row.label(icon="CAMERA_DATA")
            row.prop(space, "use_filter_object_camera", text="Cameras")
        if bpy.data.grease_pencils:
            row = sub.row()
            row.label(icon="STROKE")
            row.prop(space, "use_filter_object_grease_pencil", text="Grease Pencil")
        row = sub.row()
        row.label(icon="EMPTY_DATA")
        row.prop(space, "use_filter_object_empty", text="Empties")

        if (
            bpy.data.curves
            or bpy.data.metaballs
            or (hasattr(bpy.data, "hair_curves") and bpy.data.hair_curves)
            or (hasattr(bpy.data, "pointclouds") and bpy.data.pointclouds)
            or bpy.data.volumes
            or bpy.data.lightprobes
            or bpy.data.lattices
            or bpy.data.fonts
            or bpy.data.speakers
        ):
            row = sub.row()
            row.label(icon="BLANK1")
            row.prop(space, "use_filter_object_others", text="Others")


classes = (
    OUTLINER_HT_header,
    OUTLINER_MT_editor_menus,
    OUTLINER_MT_edit_datablocks,
    OUTLINER_MT_collection,
    OUTLINER_MT_collection_new,
    OUTLINER_MT_collection_visibility,
    OUTLINER_MT_collection_view_layer,
    OUTLINER_MT_object,
    OUTLINER_MT_asset,
    OUTLINER_MT_liboverride,
    OUTLINER_MT_context_menu,
    OUTLINER_MT_context_menu_view,
    OUTLINER_MT_view_pie,
    OUTLINER_PT_filter,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
