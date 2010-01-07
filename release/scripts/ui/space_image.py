# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180


class IMAGE_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        # uv = sima.uv_editor
        settings = context.tool_settings

        show_uvedit = sima.show_uvedit

        layout.operator("image.properties", icon='MENU_PANEL')

        layout.separator()

        layout.prop(sima, "update_automatically")
        if show_uvedit:
            layout.prop(settings, "uv_local_view") # Numpad /

        layout.separator()

        layout.operator("image.view_zoom_in")
        layout.operator("image.view_zoom_out")

        layout.separator()

        ratios = [[1, 8], [1, 4], [1, 2], [1, 1], [2, 1], [4, 1], [8, 1]]

        for a, b in ratios:
            text = "Zoom %d:%d" % (a, b)
            layout.operator("image.view_zoom_ratio", text=text).ratio = a / b

        layout.separator()

        if show_uvedit:
            layout.operator("image.view_selected")

        layout.operator("image.view_all")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class IMAGE_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.select_border")
        layout.operator("uv.select_border").pinned = True

        layout.separator()

        layout.operator("uv.select_all")
        layout.operator("uv.select_inverse")
        layout.operator("uv.unlink_selection")

        layout.separator()

        layout.operator("uv.select_pinned")
        layout.operator("uv.select_linked")


class IMAGE_MT_image(bpy.types.Menu):
    bl_label = "Image"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        layout.operator("image.new")
        layout.operator("image.open")

        show_render = sima.show_render

        if ima:
            if not show_render:
                layout.operator("image.replace")
                layout.operator("image.reload")

            layout.operator("image.save")
            layout.operator("image.save_as")

            if ima.source == 'SEQUENCE':
                layout.operator("image.save_sequence")

            if not show_render:
                layout.separator()

                if ima.packed_file:
                    layout.operator("image.unpack")
                else:
                    layout.operator("image.pack")

                # only for dirty && specific image types, perhaps
                # this could be done in operator poll too
                if ima.dirty:
                    if ima.source in ('FILE', 'GENERATED') and ima.type != 'MULTILAYER':
                        layout.operator("image.pack", text="Pack As PNG").as_png = True

            layout.separator()

            layout.prop(sima, "image_painting")


class IMAGE_MT_uvs_showhide(bpy.types.Menu):
    bl_label = "Show/Hide Faces"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.reveal")
        layout.operator("uv.hide")
        layout.operator("uv.hide").unselected = True


class IMAGE_MT_uvs_transform(bpy.types.Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")


class IMAGE_MT_uvs_snap(bpy.types.Menu):
    bl_label = "Snap"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'

        layout.operator("uv.snap_selection", text="Selected to Pixels").target = 'PIXELS'
        layout.operator("uv.snap_selection", text="Selected to Cursor").target = 'CURSOR'
        layout.operator("uv.snap_selection", text="Selected to Adjacent Unselected").target = 'ADJACENT_UNSELECTED'

        layout.separator()

        layout.operator("uv.snap_cursor", text="Cursor to Pixels").target = 'PIXELS'
        layout.operator("uv.snap_cursor", text="Cursor to Selection").target = 'SELECTION'


class IMAGE_MT_uvs_mirror(bpy.types.Menu):
    bl_label = "Mirror"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'

        layout.operator("transform.mirror", text="X Axis").constraint_axis[0] = True
        layout.operator("transform.mirror", text="Y Axis").constraint_axis[1] = True


class IMAGE_MT_uvs_weldalign(bpy.types.Menu):
    bl_label = "Weld/Align"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.weld") # W, 1
        layout.operator_enums("uv.align", "axis") # W, 2/3/4


class IMAGE_MT_uvs(bpy.types.Menu):
    bl_label = "UVs"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        settings = context.tool_settings

        layout.prop(uv, "snap_to_pixels")
        layout.prop(uv, "constrain_to_image_bounds")

        layout.separator()

        layout.prop(uv, "live_unwrap")
        layout.operator("uv.unwrap")
        layout.operator("uv.pin", text="Unpin").clear = True
        layout.operator("uv.pin")

        layout.separator()

        layout.operator("uv.pack_islands")
        layout.operator("uv.average_islands_scale")
        layout.operator("uv.minimize_stretch")
        layout.operator("uv.stitch")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_transform")
        layout.menu("IMAGE_MT_uvs_mirror")
        layout.menu("IMAGE_MT_uvs_snap")
        layout.menu("IMAGE_MT_uvs_weldalign")

        layout.separator()

        layout.prop_menu_enum(settings, "proportional_editing")
        layout.prop_menu_enum(settings, "proportional_editing_falloff")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_showhide")


class IMAGE_HT_header(bpy.types.Header):
    bl_space_type = 'IMAGE_EDITOR'

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        iuser = sima.image_user
        settings = context.tool_settings

        # show_render = sima.show_render
        # show_paint = sima.show_paint
        show_uvedit = sima.show_uvedit

        row = layout.row(align=True)
        row.template_header()

        # menus
        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("IMAGE_MT_view")

            if show_uvedit:
                sub.menu("IMAGE_MT_select")

            if ima and ima.dirty:
                sub.menu("IMAGE_MT_image", text="Image*")
            else:
                sub.menu("IMAGE_MT_image", text="Image")

            if show_uvedit:
                sub.menu("IMAGE_MT_uvs")

        layout.template_ID(sima, "image", new="image.new")

        # uv editing
        if show_uvedit:
            uvedit = sima.uv_editor

            layout.prop(uvedit, "pivot", text="", icon_only=True)
            layout.prop(settings, "uv_sync_selection", text="")

            if settings.uv_sync_selection:
                layout.prop(settings, "mesh_selection_mode", text="", expand=True)
            else:
                layout.prop(settings, "uv_selection_mode", text="", expand=True)
                layout.prop(uvedit, "sticky_selection_mode", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(settings, "proportional_editing", text="", icon_only=True)
            if settings.proportional_editing != 'DISABLED':
                row.prop(settings, "proportional_editing_falloff", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(settings, "snap", text="")
            row.prop(settings, "snap_element", text="", icon_only=True)

            # mesh = context.edit_object.data
            # row.prop_object(mesh, "active_uv_layer", mesh, "uv_textures")

        if ima:
            # layers
            layout.template_image_layers(ima, iuser)

            # painting
            layout.prop(sima, "image_painting", text="")

            # draw options
            row = layout.row(align=True)
            row.prop(sima, "draw_channels", text="", expand=True)

            row = layout.row(align=True)
            if ima.type == 'COMPOSITE':
                row.operator("image.record_composite", icon='REC')
            if ima.type == 'COMPOSITE' and ima.source in ('MOVIE', 'SEQUENCE'):
                row.operator("image.play_composite", icon='PLAY')

        if show_uvedit or sima.image_painting:
            layout.prop(sima, "update_automatically", text="", icon_only=True, icon='LOCKED')


class IMAGE_PT_image_properties(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Image"

    def poll(self, context):
        sima = context.space_data
        return (sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        # ima = sima.image
        iuser = sima.image_user

        layout.template_image(sima, "image", iuser, compact=True)


class IMAGE_PT_game_properties(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Game Properties"

    def poll(self, context):
        rd = context.scene.render_data
        sima = context.space_data
        return (sima and sima.image) and (rd.engine == 'BLENDER_GAME')

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        sub.prop(ima, "animated")

        subsub = sub.column()
        subsub.active = ima.animated
        subsub.prop(ima, "animation_start", text="Start")
        subsub.prop(ima, "animation_end", text="End")
        subsub.prop(ima, "animation_speed", text="Speed")

        col.prop(ima, "tiles")
        sub = col.column(align=True)
        sub.active = ima.tiles or ima.animated
        sub.prop(ima, "tiles_x", text="X")
        sub.prop(ima, "tiles_y", text="Y")

        if wide_ui:
            col = split.column()
        col.label(text="Clamp:")
        col.prop(ima, "clamp_x", text="X")
        col.prop(ima, "clamp_y", text="Y")
        col.separator()
        col.prop(ima, "mapping", expand=True)


class IMAGE_PT_view_properties(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"

    def poll(self, context):
        sima = context.space_data
        return (sima and (sima.image or sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        show_uvedit = sima.show_uvedit
        uvedit = sima.uv_editor
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        if ima:
            col.prop(ima, "display_aspect", text="Aspect Ratio")

            if wide_ui:
                col = split.column()
            col.label(text="Coordinates:")
            col.prop(sima, "draw_repeated", text="Repeat")
            if show_uvedit:
                col.prop(uvedit, "normalized_coordinates", text="Normalized")
        elif show_uvedit:
            col.label(text="Coordinates:")
            col.prop(uvedit, "normalized_coordinates", text="Normalized")

        if show_uvedit:

            col = layout.column()
            col.label(text="UVs:")
            row = col.row()
            if wide_ui:
                row.prop(uvedit, "edge_draw_type", expand=True)
            else:
                row.prop(uvedit, "edge_draw_type", text="")

            split = layout.split()
            col = split.column()
            col.prop(uvedit, "draw_smooth_edges", text="Smooth")
            col.prop(uvedit, "draw_modified_edges", text="Modified")
            #col.prop(uvedit, "draw_edges")
            #col.prop(uvedit, "draw_faces")

            if wide_ui:
                col = split.column()
            col.prop(uvedit, "draw_stretch", text="Stretch")
            sub = col.column()
            sub.active = uvedit.draw_stretch
            sub.row().prop(uvedit, "draw_stretch_type", expand=True)


class IMAGE_PT_paint(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Paint"

    def poll(self, context):
        sima = context.space_data
        return sima.show_paint

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        brush = settings.brush
        wide_ui = context.region.width > narrowui

        col = layout.split().column()
        row = col.row()
        row.template_list(settings, "brushes", settings, "active_brush_index", rows=2)

        col.template_ID(settings, "brush", new="brush.add")

        if wide_ui:
            sub = layout.row(align=True)
        else:
            sub = layout.column(align=True)
        sub.prop_enum(settings, "tool", 'DRAW')
        sub.prop_enum(settings, "tool", 'SOFTEN')
        sub.prop_enum(settings, "tool", 'CLONE')
        sub.prop_enum(settings, "tool", 'SMEAR')

        if brush:
            col = layout.column()
            col.template_color_wheel(brush, "color", value_slider=True)
            col.prop(brush, "color", text="")

            row = col.row(align=True)
            row.prop(brush, "size", slider=True)
            row.prop(brush, "use_size_pressure", toggle=True, text="")

            row = col.row(align=True)
            row.prop(brush, "strength", slider=True)
            row.prop(brush, "use_strength_pressure", toggle=True, text="")

            row = col.row(align=True)
            row.prop(brush, "jitter", slider=True)
            row.prop(brush, "use_jitter_pressure", toggle=True, text="")

            col.prop(brush, "blend", text="Blend")


class IMAGE_PT_paint_stroke(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Paint Stroke"
    bl_default_closed = True

    def poll(self, context):
        sima = context.space_data
        settings = context.tool_settings.image_paint
        return sima.show_paint and settings.brush

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        brush = settings.brush

        layout.prop(brush, "use_airbrush")
        col = layout.column()
        col.active = brush.use_airbrush
        col.prop(brush, "rate", slider=True)

        layout.prop(brush, "use_space")
        row = layout.row(align=True)
        row.active = brush.use_space
        row.prop(brush, "spacing", text="Distance", slider=True)
        row.prop(brush, "use_spacing_pressure", toggle=True, text="")


class IMAGE_PT_paint_curve(bpy.types.Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Paint Curve"
    bl_default_closed = True

    def poll(self, context):
        sima = context.space_data
        settings = context.tool_settings.image_paint
        return sima.show_paint and settings.brush

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        brush = settings.brush

        layout.template_curve_mapping(brush, "curve")
        layout.operator_menu_enum("brush.curve_preset", property="shape")

bpy.types.register(IMAGE_MT_view)
bpy.types.register(IMAGE_MT_select)
bpy.types.register(IMAGE_MT_image)
bpy.types.register(IMAGE_MT_uvs_showhide)
bpy.types.register(IMAGE_MT_uvs_transform)
bpy.types.register(IMAGE_MT_uvs_snap)
bpy.types.register(IMAGE_MT_uvs_mirror)
bpy.types.register(IMAGE_MT_uvs_weldalign)
bpy.types.register(IMAGE_MT_uvs)
bpy.types.register(IMAGE_HT_header)
bpy.types.register(IMAGE_PT_image_properties)
bpy.types.register(IMAGE_PT_paint)
bpy.types.register(IMAGE_PT_paint_stroke)
bpy.types.register(IMAGE_PT_paint_curve)
bpy.types.register(IMAGE_PT_game_properties)
bpy.types.register(IMAGE_PT_view_properties)
