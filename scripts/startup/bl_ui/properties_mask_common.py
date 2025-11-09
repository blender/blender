# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Panels get sub-classed (not registered directly).
# Menus are referenced as-is.

from bpy.types import Menu, UIList
from bpy.app.translations import contexts as i18n_contexts
from bl_ui import anim


# Use by both image & clip context menus.
def draw_mask_context_menu(layout, _context):
    layout.operator_menu_enum("mask.handle_type_set", "type")
    layout.operator("mask.switch_direction")
    layout.operator("mask.cyclic_toggle")

    layout.separator()
    layout.operator("mask.copy_splines", icon='COPYDOWN')
    layout.operator("mask.paste_splines", icon='PASTEDOWN')

    layout.separator()

    layout.operator("mask.shape_key_rekey", text="Re-Key Shape Points")
    layout.operator("mask.feather_weight_clear")
    layout.operator("mask.shape_key_feather_reset", text="Reset Feather Animation")

    layout.separator()

    layout.operator("mask.parent_set")
    layout.operator("mask.parent_clear")

    layout.separator()

    layout.operator("mask.delete")


class MASK_UL_layers(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.MaskLayer)
        mask = item
        layout.prop(mask, "name", text="", emboss=False, icon_value=icon)
        row = layout.row(align=True)
        row.prop(mask, "hide", text="", emboss=False)
        row.prop(mask, "hide_select", text="", emboss=False)
        row.prop(mask, "hide_render", text="", emboss=False)


class MASK_PT_mask:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'UI'
    bl_label = "Mask Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        mask = sc.mask

        col = layout.column(align=True)
        col.prop(mask, "frame_start")
        col.prop(mask, "frame_end")


class MASK_PT_layers:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'UI'
    bl_label = "Mask Layers"

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        mask = sc.mask
        active_layer = mask.layers.active

        rows = 4 if active_layer else 1

        row = layout.row()
        row.template_list(
            "MASK_UL_layers", "", mask, "layers",
            mask, "active_layer_index", rows=rows,
        )

        sub = row.column(align=True)

        sub.operator("mask.layer_new", icon='ADD', text="")
        sub.operator("mask.layer_remove", icon='REMOVE', text="")

        if active_layer:
            sub.separator()

            sub.operator("mask.layer_move", icon='TRIA_UP', text="").direction = 'UP'
            sub.operator("mask.layer_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            # blending
            row = layout.row(align=True)
            row.prop(active_layer, "alpha")
            row.prop(active_layer, "invert", text="", icon='IMAGE_ALPHA')

            layout.prop(active_layer, "blend")
            layout.prop(active_layer, "falloff")

            col = layout.column()
            col.prop(active_layer, "use_fill_overlap", text="Overlap")
            col.prop(active_layer, "use_fill_holes", text="Holes")


class MASK_PT_spline:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'UI'
    bl_label = "Active Spline"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        mask = sc.mask

        if mask and sc.mode == 'MASK':
            return mask.layers.active and mask.layers.active.splines.active

        return False

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        mask = sc.mask
        spline = mask.layers.active.splines.active

        col = layout.column()
        col.prop(spline, "offset_mode")
        col.prop(spline, "weight_interpolation", text="Interpolation")

        col.prop(spline, "use_cyclic")
        col.prop(spline, "use_fill")

        col.prop(spline, "use_self_intersection_check")


class MASK_PT_point:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'UI'
    bl_label = "Active Point"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        mask = sc.mask

        if mask and sc.mode == 'MASK':
            mask_layer_active = mask.layers.active
            return (
                mask_layer_active and
                mask_layer_active.splines.active_point
            )

        return False

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        mask = sc.mask
        point = mask.layers.active.splines.active_point
        parent = point.parent

        col = layout.column()
        # Currently only parenting the movie-clip is allowed,
        # so do not over-complicate things for now by using single template_ID
        # col.template_any_ID(parent, "id", "id_type", text="")

        col.label(text="Parent:")
        col.prop(parent, "id", text="")

        if parent.id_type == 'MOVIECLIP' and parent.id:
            clip = parent.id
            tracking = clip.tracking

            row = col.row()
            row.prop(parent, "type", expand=True)

            col.prop_search(parent, "parent", tracking, "objects", icon='OBJECT_DATA', text="Object")

            tracks_list = "tracks" if parent.type == 'POINT_TRACK' else "plane_tracks"

            if parent.parent in tracking.objects:
                ob = tracking.objects[parent.parent]
                col.prop_search(
                    parent, "sub_parent", ob,
                    tracks_list, icon='ANIM_DATA', text="Track", text_ctxt=i18n_contexts.id_movieclip,
                )
            else:
                col.prop_search(
                    parent, "sub_parent", tracking,
                    tracks_list, icon='ANIM_DATA', text="Track", text_ctxt=i18n_contexts.id_movieclip,
                )


class MASK_PT_animation:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'UI'
    bl_label = "Animation"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        # poll() ensures this is not None.
        sc = context.space_data
        mask = sc.mask

        col = layout.column(align=True)
        anim.draw_action_and_slot_selector_for_id(col, mask)


class MASK_PT_display:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'HEADER'
    bl_label = "Mask Display"

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, context):
        layout = self.layout

        space_data = context.space_data

        row = layout.row(align=True)
        row.prop(space_data, "show_mask_spline", text="Spline")
        sub = row.row()
        sub.active = space_data.show_mask_spline
        sub.prop(space_data, "mask_display_type", text="")
        row = layout.row(align=True)
        row.prop(space_data, "show_mask_overlay", text="Overlay")
        sub = row.row()
        sub.active = space_data.show_mask_overlay
        sub.prop(space_data, "mask_overlay_mode", text="")
        row = layout.row()
        row.active = space_data.show_mask_overlay and (space_data.mask_overlay_mode == 'COMBINED')
        row.prop(space_data, "blend_factor", text="Blending Factor")


class MASK_PT_transforms:
    # subclasses must define...
    # ~ bl_space_type = 'CLIP_EDITOR'
    # ~ bl_region_type = 'TOOLS'
    bl_label = "Transforms"
    bl_category = "Mask"

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, _context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.operator("transform.transform", text="Scale Feather").mode = 'MASK_SHRINKFATTEN'


class MASK_PT_tools:
    bl_label = "Mask Tools"
    bl_category = "Mask"

    @classmethod
    def poll(cls, context):
        space_data = context.space_data
        return space_data.mask and space_data.mode == 'MASK'

    def draw(self, _context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Spline:")
        col.operator("mask.delete")
        col.operator("mask.cyclic_toggle")
        col.operator("mask.switch_direction")
        col.operator("mask.handle_type_set").type = 'VECTOR'
        col.operator("mask.feather_weight_clear")

        col = layout.column(align=True)
        col.label(text="Parenting:")
        row = col.row(align=True)
        row.operator("mask.parent_set", text="Parent")
        row.operator("mask.parent_clear", text="Clear")

        col = layout.column(align=True)
        col.label(text="Animation:")
        row = col.row(align=True)
        row.operator("mask.shape_key_insert", text="Insert Key")
        row.operator("mask.shape_key_clear", text="Clear Key")
        col.operator("mask.shape_key_feather_reset", text="Reset Feather Animation")
        col.operator("mask.shape_key_rekey", text="Re-Key Shape Points")


class MASK_MT_mask(Menu):
    bl_label = "Mask"

    def draw(self, _context):
        layout = self.layout

        layout.menu("MASK_MT_transform")
        layout.operator("mask.feather_weight_clear")

        layout.separator()
        layout.operator("mask.cyclic_toggle")
        layout.operator("mask.handle_type_set")
        layout.operator("mask.normals_make_consistent")
        layout.operator("mask.switch_direction")

        layout.separator()
        layout.operator("mask.copy_splines")
        layout.operator("mask.paste_splines")

        layout.separator()
        layout.operator("mask.parent_clear")
        layout.operator("mask.parent_set")

        layout.separator()
        layout.menu("MASK_MT_animation")

        layout.separator()
        layout.menu("MASK_MT_visibility")
        layout.operator("mask.delete")


class MASK_MT_add(Menu):
    bl_idname = "MASK_MT_add"
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mask.primitive_circle_add", text="Circle", icon='MESH_CIRCLE')
        layout.operator("mask.primitive_square_add", text="Square", icon='MESH_PLANE')


class MASK_MT_visibility(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mask.hide_view_clear")
        layout.operator("mask.hide_view_set", text="Hide Selected").unselected = False
        layout.operator("mask.hide_view_set", text="Hide Unselected").unselected = True


class MASK_MT_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")

        layout.separator()
        layout.operator("transform.tosphere")
        layout.operator("transform.shear")
        layout.operator("transform.push_pull")

        layout.separator()
        layout.operator("transform.transform", text="Scale Feather").mode = 'MASK_SHRINKFATTEN'


class MASK_MT_animation(Menu):
    bl_label = "Animation"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mask.shape_key_clear")
        layout.operator("mask.shape_key_insert")
        layout.operator("mask.shape_key_feather_reset")
        layout.operator("mask.shape_key_rekey")


class MASK_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mask.select_all", text="All").action = 'SELECT'
        layout.operator("mask.select_all", text="None").action = 'DESELECT'
        layout.operator("mask.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("mask.select_box")
        layout.operator("mask.select_circle")
        layout.operator_menu_enum("mask.select_lasso", "mode")

        layout.separator()

        layout.operator("mask.select_more")
        layout.operator("mask.select_less")

        layout.separator()

        layout.operator("mask.select_linked", text="Select Linked")


classes = (
    MASK_UL_layers,
    MASK_MT_mask,
    MASK_MT_add,
    MASK_MT_visibility,
    MASK_MT_transform,
    MASK_MT_animation,
    MASK_MT_select,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
