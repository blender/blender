# gpl author: Ryan Inch (Imaginer)

import bpy
from bpy.types import Menu
from . import utils_core


class TextureMenu(Menu):
    bl_label = "Texture Options"
    bl_idname = "VIEW3D_MT_sv3_texture_menu"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'SCULPT',
                        'VERTEX_PAINT',
                        'TEXTURE_PAINT'
                        )

    def draw(self, context):
        layout = self.layout

        if utils_core.get_mode() == 'SCULPT':
            self.sculpt(layout, context)

        elif utils_core.get_mode() == 'VERTEX_PAINT':
            self.vertpaint(layout, context)

        else:
            self.texpaint(layout, context)

    def sculpt(self, layout, context):
        has_brush = utils_core.get_brush_link(context, types="brush")
        tex_slot = has_brush.texture_slot if has_brush else None

        # Menus
        layout.row().menu(Textures.bl_idname)
        layout.row().menu(TextureMapMode.bl_idname)
        layout.row().separator()

        # Checkboxes
        if tex_slot:
            if tex_slot.map_mode != '3D':
                if tex_slot.map_mode in ('RANDOM', 'VIEW_PLANE', 'AREA_PLANE'):
                    layout.row().prop(tex_slot, "use_rake", toggle=True)
                    layout.row().prop(tex_slot, "use_random", toggle=True)

                # Sliders
                layout.row().prop(tex_slot, "angle",
                                  text=utils_core.PIW + "Angle", slider=True)

                if tex_slot.tex_paint_map_mode in ('RANDOM', 'VIEW_PLANE') and tex_slot.use_random:
                    layout.row().prop(tex_slot, "random_angle",
                                      text=utils_core.PIW + "Random Angle", slider=True)

                # Operator
                if tex_slot.tex_paint_map_mode == 'STENCIL':
                    if has_brush.texture and has_brush.texture.type == 'IMAGE':
                        layout.row().operator("brush.stencil_fit_image_aspect")

                    layout.row().operator("brush.stencil_reset_transform")

        else:
            layout.row().label("No Texture Slot available", icon="INFO")

    def vertpaint(self, layout, context):
        has_brush = utils_core.get_brush_link(context, types="brush")
        tex_slot = has_brush.texture_slot if has_brush else None

        # Menus
        layout.row().menu(Textures.bl_idname)
        layout.row().menu(TextureMapMode.bl_idname)

        # Checkboxes
        if tex_slot:
            if tex_slot.tex_paint_map_mode != '3D':
                if tex_slot.tex_paint_map_mode in ('RANDOM', 'VIEW_PLANE'):
                    layout.row().prop(tex_slot, "use_rake", toggle=True)
                    layout.row().prop(tex_slot, "use_random", toggle=True)

                # Sliders
                layout.row().prop(tex_slot, "angle",
                                  text=utils_core.PIW + "Angle", slider=True)

                if tex_slot.tex_paint_map_mode in ('RANDOM', 'VIEW_PLANE') and tex_slot.use_random:
                    layout.row().prop(tex_slot, "random_angle",
                                      text=utils_core.PIW + "Random Angle", slider=True)

                # Operator
                if tex_slot.tex_paint_map_mode == 'STENCIL':
                    if has_brush.texture and has_brush.texture.type == 'IMAGE':
                        layout.row().operator("brush.stencil_fit_image_aspect")

                    layout.row().operator("brush.stencil_reset_transform")

        else:
            layout.row().label("No Texture Slot available", icon="INFO")

    def texpaint(self, layout, context):
        has_brush = utils_core.get_brush_link(context, types="brush")
        tex_slot = has_brush.texture_slot if has_brush else None
        mask_tex_slot = has_brush.mask_texture_slot if has_brush else None

        # Texture Section
        layout.row().label(text="Texture", icon='TEXTURE')

        # Menus
        layout.row().menu(Textures.bl_idname)
        layout.row().menu(TextureMapMode.bl_idname)

        # Checkboxes
        if tex_slot:
            if tex_slot.tex_paint_map_mode != '3D':
                if tex_slot.tex_paint_map_mode in ('RANDOM', 'VIEW_PLANE'):
                    layout.row().prop(tex_slot, "use_rake", toggle=True)
                    layout.row().prop(tex_slot, "use_random", toggle=True)

                # Sliders
                layout.row().prop(tex_slot, "angle",
                                  text=utils_core.PIW + "Angle", slider=True)

                if tex_slot.tex_paint_map_mode in ('RANDOM', 'VIEW_PLANE') and tex_slot.use_random:
                    layout.row().prop(tex_slot, "random_angle",
                                      text=utils_core.PIW + "Random Angle", slider=True)

                # Operator
                if tex_slot.tex_paint_map_mode == 'STENCIL':
                    if has_brush.texture and has_brush.texture.type == 'IMAGE':
                        layout.row().operator("brush.stencil_fit_image_aspect")

                    layout.row().operator("brush.stencil_reset_transform")

        else:
            layout.row().label("No Texture Slot available", icon="INFO")

        layout.row().separator()

        # Texture Mask Section
        layout.row().label(text="Texture Mask", icon='MOD_MASK')

        # Menus
        layout.row().menu(MaskTextures.bl_idname)
        layout.row().menu(MaskMapMode.bl_idname)
        layout.row().menu(MaskPressureModeMenu.bl_idname)

        # Checkboxes
        if mask_tex_slot:
            if mask_tex_slot.mask_map_mode in ('RANDOM', 'VIEW_PLANE'):
                layout.row().prop(mask_tex_slot, "use_rake", toggle=True)
                layout.row().prop(mask_tex_slot, "use_random", toggle=True)

            # Sliders
            layout.row().prop(mask_tex_slot, "angle",
                              text=utils_core.PIW + "Angle", icon_value=5, slider=True)

            if mask_tex_slot.mask_map_mode in ('RANDOM', 'VIEW_PLANE') and mask_tex_slot.use_random:
                layout.row().prop(mask_tex_slot, "random_angle",
                           text=utils_core.PIW + "Random Angle", slider=True)

            # Operator
            if mask_tex_slot.mask_map_mode == 'STENCIL':
                if has_brush.mask_texture and has_brush.mask_texture.type == 'IMAGE':
                        layout.row().operator("brush.stencil_fit_image_aspect")

                prop = layout.row().operator("brush.stencil_reset_transform")
                prop.mask = True

        else:
            layout.row().label("Mask Texture not available", icon="INFO")


class Textures(Menu):
    bl_label = "Brush Texture"
    bl_idname = "VIEW3D_MT_sv3_texture_list"

    def init(self):
        if utils_core.get_mode() == 'SCULPT':
            datapath = "tool_settings.sculpt.brush.texture"

        elif utils_core.get_mode() == 'VERTEX_PAINT':
            datapath = "tool_settings.vertex_paint.brush.texture"

        elif utils_core.get_mode() == 'TEXTURE_PAINT':
            datapath = "tool_settings.image_paint.brush.texture"

        else:
            datapath = ""

        return datapath

    def draw(self, context):
        datapath = self.init()
        has_brush = utils_core.get_brush_link(context, types="brush")
        current_texture = eval("bpy.context.{}".format(datapath)) if \
                          has_brush else None
        layout = self.layout

        # get the current texture's name
        if current_texture:
            current_texture = current_texture.name

        layout.row().label(text="Brush Texture")
        layout.row().separator()

        # add an item to set the texture to None
        utils_core.menuprop(layout.row(), "None", "None",
                 datapath, icon='RADIOBUT_OFF', disable=True,
                 disable_icon='RADIOBUT_ON',
                 custom_disable_exp=(None, current_texture),
                 path=True)

        # add the menu items
        for item in bpy.data.textures:
            utils_core.menuprop(layout.row(), item.name,
                     'bpy.data.textures["%s"]' % item.name,
                     datapath, icon='RADIOBUT_OFF',
                     disable=True,
                     disable_icon='RADIOBUT_ON',
                     custom_disable_exp=(item.name, current_texture),
                     path=True)


class TextureMapMode(Menu):
    bl_label = "Brush Mapping"
    bl_idname = "VIEW3D_MT_sv3_texture_map_mode"

    def draw(self, context):
        layout = self.layout
        has_brush = utils_core.get_brush_link(context, types="brush")

        layout.row().label(text="Brush Mapping")
        layout.row().separator()

        if has_brush:
            if utils_core.get_mode() == 'SCULPT':
                path = "tool_settings.sculpt.brush.texture_slot.map_mode"

                # add the menu items
                for item in has_brush. \
                  texture_slot.bl_rna.properties['map_mode'].enum_items:
                    utils_core.menuprop(
                            layout.row(), item.name, item.identifier, path,
                            icon='RADIOBUT_OFF',
                            disable=True,
                            disable_icon='RADIOBUT_ON'
                            )
            elif utils_core.get_mode() == 'VERTEX_PAINT':
                path = "tool_settings.vertex_paint.brush.texture_slot.tex_paint_map_mode"

                # add the menu items
                for item in has_brush. \
                  texture_slot.bl_rna.properties['tex_paint_map_mode'].enum_items:
                    utils_core.menuprop(
                            layout.row(), item.name, item.identifier, path,
                            icon='RADIOBUT_OFF',
                            disable=True,
                            disable_icon='RADIOBUT_ON'
                            )
            else:
                path = "tool_settings.image_paint.brush.texture_slot.tex_paint_map_mode"

                # add the menu items
                for item in has_brush. \
                  texture_slot.bl_rna.properties['tex_paint_map_mode'].enum_items:
                    utils_core.menuprop(
                            layout.row(), item.name, item.identifier, path,
                            icon='RADIOBUT_OFF',
                            disable=True,
                            disable_icon='RADIOBUT_ON'
                            )
        else:
            layout.row().label("No brushes available", icon="INFO")


class MaskTextures(Menu):
    bl_label = "Mask Texture"
    bl_idname = "VIEW3D_MT_sv3_mask_texture_list"

    def draw(self, context):
        layout = self.layout
        datapath = "tool_settings.image_paint.brush.mask_texture"
        has_brush = utils_core.get_brush_link(context, types="brush")
        current_texture = eval("bpy.context.{}".format(datapath)) if \
                          has_brush else None

        layout.row().label(text="Mask Texture")
        layout.row().separator()

        if has_brush:
            # get the current texture's name
            if current_texture:
                current_texture = current_texture.name

            # add an item to set the texture to None
            utils_core.menuprop(
                    layout.row(), "None", "None",
                    datapath, icon='RADIOBUT_OFF', disable=True,
                    disable_icon='RADIOBUT_ON',
                    custom_disable_exp=(None, current_texture),
                    path=True
                    )

            # add the menu items
            for item in bpy.data.textures:
                utils_core.menuprop(
                        layout.row(), item.name, 'bpy.data.textures["%s"]' % item.name,
                        datapath, icon='RADIOBUT_OFF', disable=True,
                        disable_icon='RADIOBUT_ON',
                        custom_disable_exp=(item.name, current_texture),
                        path=True
                        )
        else:
            layout.row().label("No brushes available", icon="INFO")


class MaskMapMode(Menu):
    bl_label = "Mask Mapping"
    bl_idname = "VIEW3D_MT_sv3_mask_map_mode"

    def draw(self, context):
        layout = self.layout
        path = "tool_settings.image_paint.brush.mask_texture_slot.mask_map_mode"
        has_brush = utils_core.get_brush_link(context, types="brush")

        layout.row().label(text="Mask Mapping")
        layout.row().separator()
        if has_brush:
            items = has_brush. \
                    mask_texture_slot.bl_rna.properties['mask_map_mode'].enum_items
            # add the menu items
            for item in items:
                utils_core.menuprop(
                        layout.row(), item.name, item.identifier, path,
                        icon='RADIOBUT_OFF',
                        disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No brushes available", icon="INFO")


class TextureAngleSource(Menu):
    bl_label = "Texture Angle Source"
    bl_idname = "VIEW3D_MT_sv3_texture_angle_source"

    def draw(self, context):
        layout = self.layout
        has_brush = utils_core.get_brush_link(context, types="brush")

        if has_brush:
            if utils_core.get_mode() == 'SCULPT':
                items = has_brush. \
                        bl_rna.properties['texture_angle_source_random'].enum_items
                path = "tool_settings.sculpt.brush.texture_angle_source_random"

            elif utils_core.get_mode() == 'VERTEX_PAINT':
                items = has_brush. \
                        bl_rna.properties['texture_angle_source_random'].enum_items
                path = "tool_settings.vertex_paint.brush.texture_angle_source_random"

            else:
                items = has_brush. \
                        bl_rna.properties['texture_angle_source_random'].enum_items
                path = "tool_settings.image_paint.brush.texture_angle_source_random"

            # add the menu items
            for item in items:
                utils_core.menuprop(
                        layout.row(), item[0], item[1], path,
                        icon='RADIOBUT_OFF',
                        disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No brushes available", icon="INFO")

class MaskPressureModeMenu(Menu):
    bl_label = "Mask Pressure Mode"
    bl_idname = "VIEW3D_MT_sv3_mask_pressure_mode_menu"

    def draw(self, context):
        layout = self.layout
        path = "tool_settings.image_paint.brush.use_pressure_masking"

        layout.row().label(text="Mask Pressure Mode")
        layout.row().separator()

        # add the menu items
        for item in context.tool_settings.image_paint.brush. \
          bl_rna.properties['use_pressure_masking'].enum_items:
            utils_core.menuprop(
                    layout.row(), item.name, item.identifier, path,
                    icon='RADIOBUT_OFF',
                    disable=True,
                    disable_icon='RADIOBUT_ON'
                    )

