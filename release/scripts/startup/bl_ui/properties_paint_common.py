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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
from bpy.types import Operator
from bpy.props import IntProperty, StringProperty
from bpy.types import Menu, Panel

classes = []

builtin_channel_categories = ["Cloth Tool",
    "Color",
    "Clay",
    "Pose Tool",
    "Smear",
    "Basic",
    "Smoothing",
    "Stroke",
    "Automasking"]

class DynamicBrushCategoryPanel(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"

    @classmethod
    def poll(self, context):
        ok = context.mode == "SCULPT" and context.tool_settings.sculpt and context.tool_settings.sculpt.brush
        ok = ok and len(self.get_channels(context)) > 0

        return ok

    @classmethod
    def get_channels(self, context):
        brush = context.tool_settings.sculpt.brush

        idname = self.get_category(self)

        channels = list(filter(lambda ch: ch.show_in_workspace and ch.category == idname, brush.channels))
        channels.sort(key=lambda ch: ch.ui_order)

        return channels

    def draw(self, context):
        layout = self.layout
        brush = context.tool_settings.sculpt.brush

        idname = self.get_category()
        opt = self.get_options()

        layout.use_property_split = True

        channels = self.get_channels(context)

        for ch in channels:
            ok = ch.show_in_workspace
            ok = ok and ch.category == idname

            if not ok:
                continue

            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                ch.idname,
                slider=True,
                ui_editing=opt["ui_editing"],
                show_reorder=opt["show_reorder"],
                show_mappings=opt["show_mappings"])


class DynamicPaintPanelGen:
    class Group:
        def __init__(self, idname, name, prefix, parent):
            self.idname = idname
            self.name = name
            self.prefix = prefix
            self.rnaclass = None
            self.parent = parent
            self.options = {}

    groups = {}

    @staticmethod
    def ensureCategory(idname, name=None, prefix="VIEW3D_PT_brush_category_", parent=None, show_reorder=False, ui_editing=False, show_mappings=None):
        if name is None:
            name = idname

        groupid = prefix + idname.lower()

        if groupid in DynamicPaintPanelGen.groups:
            return DynamicPaintPanelGen.groups[groupid]

        group = DynamicPaintPanelGen.Group(idname, name, prefix, parent)
        DynamicPaintPanelGen.groups[groupid] = group

        group.options = {
            "ui_editing": ui_editing,
            "show_reorder": show_reorder,
            "show_mappings" : show_mappings
        }

        def callback():
            DynamicPaintPanelGen.createPanel(group)
            pass

        import bpy
        bpy.app.timers.register(callback)

        return group

    @staticmethod
    def get(idname, prefix):
        return DynamicPaintPanelGen.groups[idname]

    @staticmethod
    def createPanel(group):
        from bpy.utils import register_class, unregister_class

        from bpy.types import Panel
        global classes

        name = group.prefix + group.idname.lower()
        name1 = name
        name2 = ""

        for c in name:
            n = ord(c)

            ok = n >= ord("a") and n <= ord("z")
            ok = ok or (n >= ord("A") and n <= ord("Z"))
            ok = ok or (n >= ord("0") and n <= ord("9"))
            ok = ok or c == "_"

            if not ok:
                c = "_"

            name2 += c
        name = name2

        for cls in classes[:]:
            #print("_", cls.bl_rna.identifier, cls.bl_rna.identifier == name) #
            #r, dir(cls.bl_rna)) #.name)

            if cls.bl_rna.identifier == name:
                try:
                    unregister_class(cls)
                except:
                    print("failed to unregister", name)

                classes.remove(cls)

        if group.parent:
            parent = 'bl_parent_id = "%s"' % group.parent
        else:
            parent = ""

        opt = repr(group.options)

        code = """

global classes

class CLASSNAME (DynamicBrushCategoryPanel):
    bl_label = "LABEL"
    PARENT

    def get_category(self):
        return "IDNAME"

    def get_options(self):
        return OPT

register_class(CLASSNAME)
classes.append(CLASSNAME)

""".strip().replace("CLASSNAME", name).replace("PARENT", parent).replace("LABEL", group.name).replace("OPT", opt)
        code = code.replace("IDNAME", group.idname)

        exec(code)

#pre create category panels in correct order
for cat in builtin_channel_categories:
    DynamicPaintPanelGen.ensureCategory(cat, cat, parent="VIEW3D_PT_tools_brush_settings_channels", prefix="VIEW3D_PT_brush_category_",
                                        ui_editing=False, show_mappings=True)
    DynamicPaintPanelGen.ensureCategory(cat, cat,  prefix="VIEW3D_PT_brush_category_edit_",
                                parent="VIEW3D_PT_tools_brush_settings_channels_preview")

channel_name_map = {
    "size": "radius",
    "autosmooth_fset_slide": "fset_slide",
    "auto_smooth_factor": "autosmooth",
    "auto_smooth_projection": "autosmooth_projection",
    "auto_smooth_radius_factor": "autosmooth_radius_scale",
    "boundary_smooth_factor": "boundary_smooth",
    "autosmooth_fset_slide": "fset_slide",
    "topology_rake_factor": "topology_rake",
    "use_locked_size": "radius_unit",
    "use_cloth_collision" : "cloth_use_collision"
}
expand_channels = {"direction", "radius_unit", "automasking"}


def template_curve(layout, base, propname, full_path):
    layout.template_curve_mapping(base, propname, brush=True)

    path = full_path

    col = layout.column(align=True)
    row = col.row(align=True)

    shapes = ['SMOOTH', 'ROUND', 'ROOT', 'SHARP', 'LINE', 'MAX']
    icons = ['SMOOTHCURVE', 'SPHERECURVE', 'ROOTCURVE', 'SHARPCURVE', 'LINCURVE', 'NOCURVE']

    for i, shape in enumerate(shapes):
        props = row.operator("brush.curve_preset_load", icon=icons[i], text="")
        props.shape = shape
        props.path = path


class UnifiedPaintPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    # bl_region_type = 'UI'

    @staticmethod
    def get_brush_mode(context):
        """ Get the correct mode for this context. For any context where this returns None,
            no brush options should be displayed."""
        mode = context.mode

        if mode == 'PARTICLE':
            # Particle brush settings currently completely do their own thing.
            return None

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)

        if not tool:
            # If there is no active tool, then there can't be an active brush.
            return None

        if not tool.has_datablock:
            # tool.has_datablock is always true for tools that use brushes.
            return None

        space_data = context.space_data
        tool_settings = context.tool_settings

        if space_data:
            space_type = space_data.type
            if space_type == 'IMAGE_EDITOR':
                if space_data.show_uvedit:
                    return 'UV_SCULPT'
                return 'PAINT_2D'
            elif space_type in {'VIEW_3D', 'PROPERTIES'}:
                if mode == 'PAINT_TEXTURE':
                    if tool_settings.image_paint:
                        return mode
                    else:
                        return None
                return mode
        return None

    @staticmethod
    def paint_settings(context):
        tool_settings = context.tool_settings

        mode = UnifiedPaintPanel.get_brush_mode(context)

        # 3D paint settings
        if mode == 'SCULPT':
            return tool_settings.sculpt
        elif mode == 'PAINT_VERTEX':
            return tool_settings.vertex_paint
        elif mode == 'PAINT_WEIGHT':
            return tool_settings.weight_paint
        elif mode == 'PAINT_TEXTURE':
            return tool_settings.image_paint
        elif mode == 'PARTICLE':
            return tool_settings.particle_edit
        # 2D paint settings
        elif mode == 'PAINT_2D':
            return tool_settings.image_paint
        elif mode == 'UV_SCULPT':
            return tool_settings.uv_sculpt
        # Grease Pencil settings
        elif mode == 'PAINT_GPENCIL':
            return tool_settings.gpencil_paint
        elif mode == 'SCULPT_GPENCIL':
            return tool_settings.gpencil_sculpt_paint
        elif mode == 'WEIGHT_GPENCIL':
            return tool_settings.gpencil_weight_paint
        elif mode == 'VERTEX_GPENCIL':
            return tool_settings.gpencil_vertex_paint
        return None

    @staticmethod
    def get_channel(context, brush, prop_name, toolsettings_only=False, need_path=False):
        ch = brush.channels[prop_name] if prop_name in brush.channels else None

        path = None

        if ch:
            path = "sculpt.brush.channels[\"%s\"]" % prop_name

        if not ch or ch.inherit or toolsettings_only:
            sd = context.tool_settings.sculpt

            if ch:
                # ensure channel exists in tool settings channel set
                sd.channels.ensure(ch)

            ch = sd.channels[prop_name] if prop_name in sd.channels else None
            if ch:
                path = "sculpt.channels[\"%s\"]" % prop_name

        if need_path:
            return (ch, path)
        else:
            return ch

    @staticmethod
    def get_channel_value(context, brush, prop_name, toolsettings_only=False):
        if context.mode != "SCULPT":
            if prop_name in channel_name_map:
                prop_name = channel_name_map[prop_name]

            return getattr(brush, prop_name)

        ch = brush.channels[prop_name]

        if ch.inherit or toolsettings_only:
            sd = context.tool_settings.sculpt
            # ensure channel exists in tool settings channel set
            sd.channels.ensure(ch)
            ch = sd.channels[prop_name]

        return ch.value


    @staticmethod
    def channel_unified(layout, context, brush, prop_name, icon='NONE', pressure=None, text=None, baselayout=None,
                        slider=False, header=False, show_reorder=False, expand=None, toolsettings_only=False, ui_editing=None,
                        show_mappings=None):
        """ Generalized way of adding brush options to the UI,
            along with their pen pressure setting and global toggle

            note that ui_editing is no longer a bool, it can also be "mappings_only"
            to just show the input mappings controls.
            """

        if baselayout is None:
            baselayout = layout

        if slider is None:
            slider = False

        if header:
            ui_editing = False
            show_mappings = False
        elif ui_editing is None:
            ui_editing = True

        if not context.tool_settings.unified_paint_settings.brush_editor_mode:
            ui_editing = False
            show_reorder = False

        if ui_editing and show_mappings is None:
            show_mappings = True

        if context.mode != "SCULPT":
            return UnifiedPaintPanel.prop_unified(layout, context, brush, prop_name, icon=icon, text=text, slider=slider, header=header, expand=expand)

        if prop_name == "size":
            prop_name = "radius"
        elif prop_name == "use_locked_size":
            prop_name = "radius_unit"

        ch = brush.channels[prop_name]

        # dynamically switch to unprojected radius if necassary
        if prop_name == "radius":
            size_mode = brush.channels["radius_unit"].enum_value == "SCENE"
            if size_mode:
                prop_name = "unprojected_radius"
                ch = brush.channels[prop_name]

        finalch = ch

        if prop_name in expand_channels:
            expand = True

        l1 = layout

        # if ch.ui_expanded:
        #    layout = layout.box().column() #.column() is a bit more compact

        if ch.type == "BITMASK":
            layout = layout.column(align=True)

        row = layout.row(align=True)
        row.use_property_split = False
        row.use_property_decorate = False

        typeprop = "value"

        if pressure is None:
            pressure = ch.type not in ["VEC3", "VEC4", "BITMASK", "ENUM", "BOOL"]

        if text is None:
            text = ch.name

        path = ""
        is_toolset = False

        pressurech = ch

        if ch.inherit or toolsettings_only:
            sd = context.tool_settings.sculpt
            # ensure channel exists in tool settings channel set
            sd.channels.ensure(ch)

            finalch = sd.channels[prop_name]
            if ch.mappings["PRESSURE"].inherit:
                pressurech = finalch

            is_toolset = True
            path = "tool_settings.sculpt.channels[\"%s\"]" % ch.idname
        else:
            path = "tool_settings.sculpt.brush.channels[\"%s\"]" % ch.idname

        if show_reorder:
            props = row.operator("brush.change_channel_order", text="", icon="TRIA_UP")
            props.channel = ch.idname
            props.filterkey = "show_in_workspace"
            props.direction = -1

            props = row.operator("brush.change_channel_order", text="", icon="TRIA_DOWN")
            props.filterkey = "show_in_workspace"
            props.channel = ch.idname
            props.direction = 1

        if ui_editing and not header:
            row2 = row.row(align=True)
            row2.prop(ch, "show_in_workspace", text="", icon="HIDE_OFF")
            row2.prop(ch, "show_in_context_menu", text="", icon="MENU_PANEL")

        if ch.type == "CURVE":
            row.prop(finalch.curve, "curve_preset", text=text)
            if finalch.curve.curve_preset == "CUSTOM":
                path2 = path + ".curve.curve"
                template_curve(layout, finalch.curve, "curve", path2)

        elif ch.type == "BITMASK":
            if header or not expand:
                row.label(text=text)
                row.prop_menu_enum(finalch, typeprop, text=text)
            else:
                # why is it so hard to make bitflag checkboxes?

                row.label(text=text)
                col = layout.row(align=True)
                #col.emboss = "NONE"
                row1 = col.column(align=True)
                row2 = col.column(align=True)

                row1.emboss = "NONE"
                row2.emboss = "NONE"

                row1.use_property_split = False
                row2.use_property_split = False
                row1.use_property_decorate = False
                row2.use_property_decorate = False

                for j, item in enumerate(finalch.enum_items):
                    row3 = row1 if j % 2 == 0 else row2

                    if item.identifier in finalch.value:
                        itemicon = "CHECKBOX_HLT"
                    else:
                        itemicon = "CHECKBOX_DEHLT"
                    row3.prop_enum(finalch, typeprop, item.identifier, icon=itemicon)

        elif header and ch.idname == "direction":
            row2 = row.row(align=True)
            row2.use_property_split = False
            row2.use_property_decorate = False

            # replicate pre-existing functionality of direction showing up as
            # +/- in the header
            row2.prop_enum(finalch, typeprop, "ADD", text="")
            row2.prop_enum(finalch, typeprop, "SUBTRACT", text="")
            pass
        elif expand is not None:
            row.prop(finalch, typeprop, icon=icon, text=text, slider=slider, expand=expand)
        else:
            row.prop(finalch, typeprop, icon=icon, text=text, slider=slider)

        pressure = pressure and ch.type not in ["BOOL", "ENUM", "BITMASK", "CURVE"]

        if pressure:
            row.prop(pressurech.mappings["PRESSURE"], "enabled", text="", icon="STYLUS_PRESSURE")

        # if ch.is_color:
        #    UnifiedPaintPanel.prop_unified_color_picker(row, context, brush,
        #    prop_name)

        # if pressure_name:
        #    row.prop(brush, pressure_name, text="")

        # if unified_name and not header:
        #    # NOTE: We don't draw UnifiedPaintSettings in the header to reduce
        #    clutter.  D5928#136281
        #    row.prop(ups, unified_name, text="", icon='BRUSHES_ALL')
        if not header:
            if ch.type == "BITMASK" and not toolsettings_only and ch == finalch:
                row.prop(ch, "inherit_if_unset", text="Combine With Defaults")

            if not toolsettings_only:
                row.prop(ch, "inherit", text="", icon='BRUSHES_ALL')

            if ch.type in ["BITMASK", "BOOL", "CURVE", "ENUM"]:
                return

            if not show_mappings and not show_reorder:
                return

            row.prop(ch, "ui_expanded", text="", icon="TRIA_DOWN" if ch.ui_expanded else "TRIA_RIGHT")

            if ch.ui_expanded:
                layout = baselayout.column()

                for i, mp in enumerate(ch.mappings):
                    mp0 = mp
                    if mp.inherit:
                        mp = finalch.mappings[i]

                    row2 = layout.row(align=True)
                    row2.use_property_split = False
                    row2.use_property_decorate = False

                    name = mp.type.lower()

                    if len(name) > 0:
                        name = name[0].upper() + name[1:]
                    else:
                        name = "name error"

                    row2.label(text=name)
                    row2.prop(mp0, "inherit", text="", icon="BRUSHES_ALL")
                    row2.prop(mp, "enabled", text="", icon="STYLUS_PRESSURE")
                    row2.prop(mp, "invert", text="", icon="ARROW_LEFTRIGHT")

                    row2.prop(mp0, "ui_expanded", text="", icon="TRIA_DOWN" if mp.ui_expanded else "TRIA_RIGHT")

                    if mp0.ui_expanded:
                        #XXX why do I have to feed use_negative_slope as true
                        #here?
                        box = layout.box()

                        box.template_curve_mapping(mp, "curve", brush=True, use_negative_slope=True)

                        col = box.column(align=True)
                        row = col.row(align=True)

                        if mp0.inherit or toolsettings_only:
                            path2 = path + ".mappings[\"%s\"].curve" % (mp.type)
                        else:
                            brushpath = "tool_settings.sculpt.brush.channels[\"%s\"]" % ch.idname
                            path2 = brushpath + ".mappings[\"%s\"].curve" % (mp.type)

                        shapes = ['SMOOTH', 'ROUND', 'ROOT', 'SHARP', 'LINE', 'MAX']
                        icons = ['SMOOTHCURVE', 'SPHERECURVE', 'ROOTCURVE', 'SHARPCURVE', 'LINCURVE', 'NOCURVE']

                        for i, shape in enumerate(shapes):
                            props = row.operator("brush.curve_preset_load", icon=icons[i], text="")
                            props.shape = shape
                            props.path = path2

                        col.prop(mp, "factor")
                        col.prop(mp, "blendmode")

                        col.label(text="Input Mapping")
                        row = col.row()
                        row.prop(mp, "premultiply")
                        row.prop(mp, "mapfunc")

                        if mp.mapfunc in ("SQUARE", "CUTOFF"):
                            col.prop(mp, "func_cutoff")

                        col.label(text="Output Mapping")
                        row = col.row()
                        row.prop(mp, "min")
                        row.prop(mp, "max")

                    #row2.prop(mp, "curve")

        return row

    @staticmethod
    def prop_unified(layout,
            context,
            brush,
            prop_name,
            unified_name=None,
            pressure_name=None,
            icon='NONE',
            text=None,
            slider=False,
            header=False,
            expand=None):
        """ Generalized way of adding brush options to the UI,
            along with their pen pressure setting and global toggle, if they exist. """

        if context.mode == "SCULPT":
            if prop_name in channel_name_map:
                prop_name = channel_name_map[prop_name]

            if prop_name in brush.channels:
                #    def channel_unified(layout, context, brush, prop_name,
                #    icon='NONE', pressure=True, text=None, slider=False,
                #    header=False):
                return UnifiedPaintPanel.channel_unified(layout, context, brush, prop_name, icon=icon, text=text, slider=slider, header=header)

        row = layout.row(align=True)
        ups = context.tool_settings.unified_paint_settings
        prop_owner = brush
        if unified_name and getattr(ups, unified_name):
            prop_owner = ups

        if expand is not None:
            row.prop(prop_owner, prop_name, icon=icon, text=text, slider=slider, expand=expand)
        else:
            row.prop(prop_owner, prop_name, icon=icon, text=text, slider=slider)

        if pressure_name:
            row.prop(brush, pressure_name, text="")

        if unified_name and not header:
            # NOTE: We don't draw UnifiedPaintSettings in the header to reduce
            # clutter.  D5928#136281
            row.prop(ups, unified_name, text="", icon='BRUSHES_ALL')

        return row

    @staticmethod
    def prop_unified_color(parent, context, brush, prop_name, *, text=None):
        if context.mode == 'SCULPT':
            return UnifiedPaintPanel.channel_unified(parent, context, brush, prop_name, text=text)

        ups = context.tool_settings.unified_paint_settings
        prop_owner = ups if ups.use_unified_color else brush
        parent.prop(prop_owner, prop_name, text=text)

    @staticmethod
    def prop_unified_color_picker(parent, context, brush, prop_name, value_slider=True):
        ups = context.tool_settings.unified_paint_settings
        prop_owner = ups if ups.use_unified_color else brush

        if context.mode == "SCULPT":
            ch, path = UnifiedPaintPanel.get_channel(context, brush, prop_name, need_path=True)

            if ch is not None:
                print("FOUND CH", ch.idname)
                prop_owner = ch
                prop_name = "value"

        parent.template_color_picker(prop_owner, prop_name, value_slider=value_slider)


### Classes to let various paint modes' panels share code, by sub-classing
### these classes.  ###
class BrushPanel(UnifiedPaintPanel):
    @classmethod
    def poll(cls, context):
        return cls.get_brush_mode(context) is not None


class BrushSelectPanel(BrushPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)
        brush = settings.brush

        row = layout.row()
        large_preview = True
        if large_preview:
            row.column().template_ID_preview(settings, "brush", new="brush.add", rows=3, cols=8, hide_buttons=False)
        else:
            row.column().template_ID(settings, "brush", new="brush.add")
        col = row.column()
        col.menu("VIEW3D_MT_brush_context_menu", icon='DOWNARROW_HLT', text="")

        if brush is not None:
            col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

            if brush.use_custom_icon:
                layout.prop(brush, "icon_filepath", text="")


class ColorPalettePanel(BrushPanel):
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        settings = cls.paint_settings(context)
        brush = settings.brush

        if context.space_data.type == 'IMAGE_EDITOR' or context.image_paint_object:
            capabilities = brush.image_paint_capabilities
            return capabilities.has_color

        elif context.vertex_paint_object:
            capabilities = brush.vertex_paint_capabilities
            return capabilities.has_color

        elif context.sculpt_object:
            capabilities = brush.sculpt_capabilities
            return capabilities.has_color
        return False

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)

        layout.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            layout.template_palette(settings, "palette", color=True)


class ClonePanel(BrushPanel):
    bl_label = "Clone"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        settings = cls.paint_settings(context)

        mode = cls.get_brush_mode(context)
        if mode == 'PAINT_TEXTURE':
            brush = settings.brush
            return brush.image_tool == 'CLONE'
        return False

    def draw_header(self, context):
        settings = self.paint_settings(context)
        self.layout.prop(settings, "use_clone_layer", text="")

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)

        layout.active = settings.use_clone_layer

        ob = context.active_object
        col = layout.column()

        if settings.mode == 'MATERIAL':
            if len(ob.material_slots) > 1:
                col.label(text="Materials")
                col.template_list("MATERIAL_UL_matslots", "",
                    ob, "material_slots",
                    ob, "active_material_index",
                    rows=2,)

            mat = ob.active_material
            if mat:
                col.label(text="Source Clone Slot")
                col.template_list("TEXTURE_UL_texpaintslots", "",
                    mat, "texture_paint_images",
                    mat, "paint_clone_slot",
                    rows=2,)

        elif settings.mode == 'IMAGE':
            mesh = ob.data

            clone_text = mesh.uv_layer_clone.name if mesh.uv_layer_clone else ""
            col.label(text="Source Clone Image")
            col.template_ID(settings, "clone_image")
            col.label(text="Source Clone UV Map")
            col.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text, translate=False)


class TextureMaskPanel(BrushPanel):
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.image_paint.brush
        mask_tex_slot = brush.mask_texture_slot

        col = layout.column()
        col.template_ID_preview(mask_tex_slot, "texture", new="texture.new", rows=3, cols=8)

        # map_mode
        layout.row().prop(mask_tex_slot, "mask_map_mode", text="Mask Mapping")

        if mask_tex_slot.map_mode == 'STENCIL':
            if brush.mask_texture and brush.mask_texture.type == 'IMAGE':
                layout.operator("brush.stencil_fit_image_aspect").mask = True
            layout.operator("brush.stencil_reset_transform").mask = True

        col = layout.column()
        col.prop(brush, "use_pressure_masking", text="Pressure Masking")
        # angle and texture_angle_source
        if mask_tex_slot.has_texture_angle:
            col = layout.column()
            col.prop(mask_tex_slot, "angle", text="Angle")
            if mask_tex_slot.has_texture_angle_source:
                col.prop(mask_tex_slot, "use_rake", text="Rake")

                if brush.brush_capabilities.has_random_texture_angle and mask_tex_slot.has_random_texture_angle:
                    col.prop(mask_tex_slot, "use_random", text="Random")
                    if mask_tex_slot.use_random:
                        col.prop(mask_tex_slot, "random_angle", text="Random Angle")

        # scale and offset
        col.prop(mask_tex_slot, "offset")
        col.prop(mask_tex_slot, "scale")


class StrokePanel(BrushPanel):
    bl_label = "Stroke"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        mode = self.get_brush_mode(context)
        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()

        col.prop(brush, "stroke_method")
        col.separator()

        if brush.use_anchor:
            col.prop(brush, "use_edge_to_edge", text="Edge to Edge")

        if brush.use_airbrush:
            col.prop(brush, "rate", text="Rate", slider=True)

        if brush.use_space:
            row = col.row(align=True)
            if mode == 'SCULPT':
                UnifiedPaintPanel.channel_unified(col,
                    context,
                    brush,
                    "spacing")
            else:
                row.prop(brush, "spacing", text="Spacing")
                row.prop(brush, "use_pressure_spacing", toggle=True, text="")

            UnifiedPaintPanel.channel_unified(col,
                context,
                brush,
                "use_smoothed_rake")

        if brush.use_line or brush.use_curve:
            row = col.row(align=True)
            if mode == 'SCULPT':
                UnifiedPaintPanel.channel_unified(col,
                    context,
                    brush,
                    "spacing")
            else:
                row.prop(brush, "spacing", text="Spacing")

        if mode == 'SCULPT':
            col.row().prop(brush, "use_scene_spacing", text="Spacing Distance", expand=True)

        if mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
            if brush.image_paint_capabilities.has_space_attenuation or brush.sculpt_capabilities.has_space_attenuation:
                col.prop(brush, "use_space_attenuation")
        elif mode == 'SCULPT':
            if brush.image_paint_capabilities.has_space_attenuation or brush.sculpt_capabilities.has_space_attenuation:
                UnifiedPaintPanel.channel_unified(col,
                    context,
                    brush,
                    "use_space_attenuation")

        if brush.use_curve:
            col.separator()
            col.template_ID(brush, "paint_curve", new="paintcurve.new")
            col.operator("paintcurve.draw")
            col.separator()

        if brush.use_space or brush.use_line or brush.use_curve:
            col.separator()
            row = col.row(align=True)
            col.prop(brush, "dash_ratio", text="Dash Ratio")
            col.prop(brush, "dash_samples", text="Dash Length")

        if mode != 'SCULPT':
            col.separator()
            row = col.row(align=True)
            if brush.jitter_unit == 'BRUSH':
                row.prop(brush, "jitter", slider=True)
            else:
                row.prop(brush, "jitter_absolute")
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")
            col.row().prop(brush, "jitter_unit", expand=True)
        elif mode == 'SCULPT' and brush.sculpt_capabilities.has_jitter:
            col.separator()
            row = col.row(align=True)
            if UnifiedPaintPanel.get_channel_value(context, brush, "jitter_unit") == 'BRUSH':
                UnifiedPaintPanel.channel_unified(row,
                    context,
                    brush,
                    "jitter", slider=True)
            else:
                UnifiedPaintPanel.channel_unified(row,
                    context,
                    brush,
                    "jitter_absolute")

            #row.prop(brush, "use_pressure_jitter", toggle=True, text="")
            UnifiedPaintPanel.channel_unified(col.row(),
                    context,
                    brush,
                    "jitter_unit", expand=True)
            #col.row().prop(brush, "jitter_unit", expand=True)

        col.separator()
        col.prop(settings, "input_samples")


class SmoothStrokePanel(BrushPanel):
    bl_label = "Stabilize Stroke"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        settings = cls.paint_settings(context)
        brush = settings.brush
        if brush.brush_capabilities.has_smooth_stroke:
            return True
        return False

    def draw_header(self, context):
        settings = self.paint_settings(context)
        brush = settings.brush

        if context.mode == "SCULPT":
            self.layout.prop(brush.channels["use_smooth_stroke"], "value", text="")
        else:
            self.layout.prop(brush, "use_smooth_stroke", text="")

    def draw(self, context):
        ui_editing = context.tool_settings.unified_paint_settings.brush_editor_mode

        settings = self.paint_settings(context)
        brush = settings.brush

        if ui_editing:
            UnifiedPaintPanel.channel_unified(self.layout,
                context,
                brush,
                "use_smooth_stroke", ui_editing=True, text="Stabilize Stroke")

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()

        col.active = UnifiedPaintPanel.get_channel_value(context, brush, "use_smooth_stroke")

        #col.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
        #col.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)
        UnifiedPaintPanel.channel_unified(col,
            context,
            brush,
            "smooth_stroke_radius", text="Radius", ui_editing=ui_editing, slider=True)
        UnifiedPaintPanel.channel_unified(col,
            context,
            brush,
            "smooth_stroke_factor", text="Factor", ui_editing=ui_editing, slider=True)


class FalloffPanel(BrushPanel):
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        settings = cls.paint_settings(context)
        return (settings and settings.brush and settings.brush.curve)

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)
        mode = self.get_brush_mode(context)
        brush = settings.brush

        if brush is None:
            return

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(brush, "curve_preset", text="")

        if brush.curve_preset == 'CUSTOM':
            layout.template_curve_mapping(brush, "curve", brush=True, use_negative_slope=False)

            col = layout.column(align=True)
            row = col.row(align=True)
            row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
            row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
            row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
            row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
            row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
            row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'

        if mode in {'SCULPT', 'PAINT_VERTEX', 'PAINT_WEIGHT'} and brush.sculpt_tool != 'POSE':
            col.separator()
            row = col.row(align=True)
            row.use_property_split = True
            row.use_property_decorate = False
            row.prop(brush, "falloff_shape", expand=True)


class DisplayPanel(BrushPanel):
    bl_label = "Brush Cursor"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = self.paint_settings(context)
        if settings and not self.is_popover:
            self.layout.prop(settings, "show_brush", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        mode = self.get_brush_mode(context)
        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot
        tex_slot_mask = brush.mask_texture_slot

        if self.is_popover:
            row = layout.row(align=True)
            row.prop(settings, "show_brush", text="")
            row.label(text="Display Cursor")

        col = layout.column()
        col.active = brush.brush_capabilities.has_overlay and settings.show_brush

        col.prop(brush, "cursor_color_add", text="Cursor Color")
        if mode == 'SCULPT' and brush.sculpt_capabilities.has_secondary_color:
            col.prop(brush, "cursor_color_subtract", text="Inverse Color")

        col.separator()

        row = col.row(align=True)
        row.prop(brush, "cursor_overlay_alpha", text="Falloff Opacity")
        row.prop(brush, "use_cursor_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
        row.prop(brush, "use_cursor_overlay", text="", toggle=True,
            icon='HIDE_OFF' if brush.use_cursor_overlay else 'HIDE_ON',)

        if mode in ['PAINT_2D', 'PAINT_TEXTURE', 'PAINT_VERTEX', 'SCULPT']:
            row = col.row(align=True)
            row.prop(brush, "texture_overlay_alpha", text="Texture Opacity")
            row.prop(brush, "use_primary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
            if tex_slot.map_mode != 'STENCIL':
                row.prop(brush, "use_primary_overlay", text="", toggle=True,
                    icon='HIDE_OFF' if brush.use_primary_overlay else 'HIDE_ON',)

        if mode in ['PAINT_TEXTURE', 'PAINT_2D']:
            row = col.row(align=True)
            row.prop(brush, "mask_overlay_alpha", text="Mask Texture Opacity")
            row.prop(brush, "use_secondary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
            if tex_slot_mask.map_mode != 'STENCIL':
                row.prop(brush, "use_secondary_overlay", text="", toggle=True,
                    icon='HIDE_OFF' if brush.use_secondary_overlay else 'HIDE_ON',)


class VIEW3D_MT_tools_projectpaint_clone(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layer_clone_index"
            props.value = i


def brush_settings(layout, context, brush, popover=False):
    """ Draw simple brush settings for Sculpt,
        Texture/Vertex/Weight Paint modes, or skip certain settings for the popover """

    mode = UnifiedPaintPanel.get_brush_mode(context)

    layout.prop(context.tool_settings.unified_paint_settings, "brush_editor_mode")
    layout.prop(context.tool_settings.unified_paint_settings, "brush_editor_advanced")

    advanced = context.tool_settings.unified_paint_settings.brush_editor_advanced
    editor = context.tool_settings.unified_paint_settings.brush_editor_mode

    ### Draw simple settings unique to each paint mode.  ###
    brush_shared_settings(layout, context, brush, popover)

    # Sculpt Mode #
    if mode == 'SCULPT':
        capabilities = brush.sculpt_capabilities
        sculpt_tool = brush.sculpt_tool

        if advanced:
            # normal_radius_factor
            UnifiedPaintPanel.prop_unified(layout,
                context,
                brush,
                "normal_radius_factor",
                slider=True,)

            if context.preferences.experimental.use_sculpt_tools_tilt and capabilities.has_tilt:
                UnifiedPaintPanel.prop_unified(layout,
                    context,
                    brush,
                    "tilt_strength_factor",
                    slider=True,)

        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "hard_edge_mode",
            slider=True,
            unified_name="use_unified_hard_edge_mode",)

        row = layout.row(align=True)

        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "hardness",
            slider=True,
            pressure_name="use_hardness_pressure")

        #row.prop(brush, "hardness", slider=True)
        #row.prop(brush, "invert_hardness_pressure", text="")
        #row.prop(brush, "use_hardness_pressure", text="")

        # auto_smooth_factor and use_inverse_smooth_pressure
        if capabilities.has_auto_smooth:
            box = layout.box().column()  # .column() is a bit more compact

            box.label(text="Auto-Smooth")

            UnifiedPaintPanel.prop_unified(box,
                context,
                brush,
                "auto_smooth_factor",
                text="Factor",
                pressure_name="use_inverse_smooth_pressure",
                slider=True,)

            if advanced:
                UnifiedPaintPanel.prop_unified(box, context, brush, "boundary_smooth_factor", slider=True)
                UnifiedPaintPanel.prop_unified(box, context, brush, "use_weighted_smooth")
                UnifiedPaintPanel.prop_unified(box, context, brush, "preserve_faceset_boundary")

                if UnifiedPaintPanel.get_channel_value(context, brush, "preserve_faceset_boundary"):
                    UnifiedPaintPanel.prop_unified(box, context, brush, "autosmooth_fset_slide", slider=True)

            if advanced:
                UnifiedPaintPanel.prop_unified(box,
                    context,
                    brush,
                    "autosmooth_use_spacing",
                    slider=True,
                    text="Custom Spacing")

                if brush.channels["autosmooth_use_spacing"].value:
                    UnifiedPaintPanel.channel_unified(box,
                        context,
                        brush,
                        "autosmooth_spacing",
                        slider=True,
                        text="Smooth Spacing")

            UnifiedPaintPanel.prop_unified(box,
                context,
                brush,
                "auto_smooth_projection",
                text="Projection",
                slider=True)

            if advanced:
                UnifiedPaintPanel.prop_unified(box,
                    context,
                    brush,
                    "auto_smooth_radius_factor",
                    slider=True)
                UnifiedPaintPanel.channel_unified(box,
                    context,
                    brush,
                    "autosmooth_falloff_curve")
        elif brush.sculpt_tool == "SMOOTH":
            UnifiedPaintPanel.prop_unified(layout,
                context,
                brush,
                "projection",
                slider=True)

        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "use_smoothed_rake")

        box = layout.box().column()  # .column() is a bit more compact
        box.label(text="Auto Face Set")

        UnifiedPaintPanel.prop_unified(box,
            context,
            brush,
            "use_autofset")

        if UnifiedPaintPanel.get_channel_value(context, brush, "use_autofset"):
            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "autofset_radius_scale",
                slider=True)
            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "autofset_use_spacing")
            if UnifiedPaintPanel.get_channel_value(context, brush, "autofset_use_spacing"):
                UnifiedPaintPanel.channel_unified(box,
                    context,
                    brush,
                    "autofset_spacing")
            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "autofset_start")
            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "autofset_count")
            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "autofset_curve")

        
        if capabilities.has_vcol_boundary_smooth:
            UnifiedPaintPanel.prop_unified(layout,
                context,
                brush,
                "vcol_boundary_factor",
                slider=True)

        if (capabilities.has_topology_rake and context.sculpt_object.use_dynamic_topology_sculpting):
            box = layout.box().column()  # .column() is a bit more compact

            box.label(text="Topology Rake")

            #box.prop(brush, "topology_rake_factor", slider=True)
            UnifiedPaintPanel.prop_unified(box,
                context,
                brush,
                "topology_rake_factor",
                slider=True,
                text="Factor")

            if advanced:
                box.prop(brush, "use_custom_topology_rake_spacing", text="Custom Spacing")

                if brush.channels["topology_rake_use_spacing"].value:
                    UnifiedPaintPanel.prop_unified(box,
                        context,
                        brush,
                        "topology_rake_spacing",
                        slider=True,
                        text="Rake Spacing")

                UnifiedPaintPanel.prop_unified(box,
                    context,
                    brush,
                    "topology_rake_projection",
                    slider=True)
                UnifiedPaintPanel.prop_unified(box,
                    context,
                    brush,
                    "topology_rake_radius_scale",
                    slider=True)

            UnifiedPaintPanel.channel_unified(box,
                context,
                brush,
                "topology_rake_mode",
                expand=True)

            if advanced:
                UnifiedPaintPanel.channel_unified(box,
                    context,
                    brush,
                    "topology_rake_falloff_curve")

            #box.prop(brush, "use_curvature_rake")
            box.prop(brush, "ignore_falloff_for_topology_rake")

        if context.sculpt_object.use_dynamic_topology_sculpting:
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "dyntopo_disabled",
                #text="Weight By Face Area",
                header=True)
            #layout.prop(brush.dyntopo, "disabled", text="Disable Dyntopo")

        # normal_weight
        if capabilities.has_normal_weight:
            layout.prop(brush, "normal_weight", slider=True)

        # crease_pinch_factor
        if capabilities.has_pinch_factor:
            text = "Pinch"
            if sculpt_tool in {'BLOB', 'SNAKE_HOOK'}:
                text = "Magnify"

            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "crease_pinch_factor",
                #text="Weight By Face Area",
                slider=True,
                text = text)

        # rake_factor
        if capabilities.has_rake_factor:
            layout.prop(brush, "rake_factor", slider=True)

        # plane_offset, use_offset_pressure, use_plane_trim, plane_trim
        if capabilities.has_plane_offset:
            layout.separator()
            UnifiedPaintPanel.prop_unified(layout,
                context,
                brush,
                "plane_offset",
                pressure_name="use_offset_pressure",
                slider=True,)

            if editor:
                col = layout.column()

                #row.prop(brush, "use_plane_trim", text="")
                #"""
                UnifiedPaintPanel.channel_unified(col,
                    context,
                    brush,
                    "use_plane_trim",
                    text="Plane Trim")
                UnifiedPaintPanel.channel_unified(col,
                    context,
                    brush,
                    "plane_trim",
                    slider=True)
                #"""
            else:
                row = layout.row(heading="Plane Trim")
                row.prop(brush.channels["use_plane_trim"], "value", text="")

                sub = row.row()
                sub.active = brush.channels["use_plane_trim"].value
                sub.prop(brush.channels["plane_trim"], "value", slider=True, text="")

            layout.separator()

        # height
        if capabilities.has_height:
            layout.prop(brush, "height", slider=True, text="Height")

        # use_persistent, set_persistent_base
        if 0: #capabilities.has_persistence:
            layout.separator()
            layout.prop(brush, "use_persistent")
            layout.operator("sculpt.set_persistent_base")
            layout.separator()

        if capabilities.has_color:
            ups = context.scene.tool_settings.unified_paint_settings

            if context.mode == "SCULPT":
                row = layout.column(align=True)
            else:
                row = layout.row(align=True)

            UnifiedPaintPanel.prop_unified_color(row, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color(row, context, brush, "secondary_color", text="")

            if context.mode != "SCULPT":
                row.separator()
                row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="", emboss=False)
                row.prop(ups, "use_unified_color", text="", icon='BRUSHES_ALL')
            layout.prop(brush, "blend", text="Blend Mode")

        # Per sculpt tool options.

        def doprop(col, prop, slider=None, text=None, baselayout=None):
            UnifiedPaintPanel.channel_unified(col,
                context,
                brush,
                prop,
                slider=slider, text=text, baselayout=baselayout)

        if sculpt_tool == "VCOL_BOUNDARY":
            row = layout.row()
            UnifiedPaintPanel.channel_unified(row,
                context,
                brush,
                "vcol_boundary_exponent",
                slider=True)

        if sculpt_tool == 'CLAY_STRIPS':
            row = layout.row()
            UnifiedPaintPanel.channel_unified(row,
                context,
                brush,
                "tip_roundness",
                slider=True)

        elif sculpt_tool == 'ELASTIC_DEFORM':
            layout.separator()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "elastic_deform_type",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "elastic_deform_volume_preservation",
                slider=True)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "use_surface_falloff",)
            layout.separator()

        elif sculpt_tool == 'SNAKE_HOOK':
            layout.separator()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "snake_hook_deform_type",)
            layout.separator()

        elif sculpt_tool == 'POSE':
            layout.separator()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "deform_target",)
            layout.separator()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "pose_deform_type",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "pose_origin_type",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "pose_offset",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "pose_smooth_iterations",)

            if brush.channels["pose_deform_type"].value == 'ROTATE_TWIST' and \
              brush.channels["pose_origin_type"].value in {'TOPOLOGY', 'FACE_SETS'}:
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "pose_ik_segments",)
            if brush.channels["pose_deform_type"].value == 'SCALE_TRANSLATE':
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "use_pose_lock_rotation",)

            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "use_pose_ik_anchored",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "use_connected_only",)
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "disconnected_distance_max",)

            layout.separator()

        elif sculpt_tool == 'CLOTH':
            layout.separator()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "cloth_simulation_area_type",)

            if brush.channels["cloth_simulation_area_type"].value != 'GLOBAL':
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "cloth_sim_limit",)
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "cloth_sim_falloff",)

            if brush.channels["cloth_simulation_area_type"].value == 'LOCAL':
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "cloth_sim_falloff",)
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "cloth_pin_simulation_boundary",)

            layout.separator()
            doprop(layout, "cloth_deform_type")
            doprop(layout, "cloth_force_falloff_type")
            layout.separator()
            doprop(layout, "cloth_mass")
            doprop(layout, "cloth_damping")
            doprop(layout, "cloth_constraint_softbody_strength")
            layout.separator()
            doprop(layout, "use_cloth_collision")
            layout.separator()

        elif sculpt_tool == 'SCRAPE':
            row = layout.row(align=True)
            doprop(row, "area_radius_factor", baselayout=layout)
            #doprop(row, "use_pressure_area_radius", text="",
            #baselayout=layout)
            row = layout.row()
            doprop(row, "invert_to_scrape_fill", text="Invert to Fill", baselayout=layout)

        elif sculpt_tool == 'FILL':
            row = layout.row(align=True)
            doprop(row, "area_radius_factor", baselayout=layout)
            #doprop(row, "use_pressure_area_radius", text="")
            row = layout.row()
            doprop(row, "invert_to_scrape_fill", text="Invert to Scrape", baselayout=layout)

        elif sculpt_tool == 'GRAB':
            doprop(layout, "use_grab_active_vertex")
            doprop(layout, "grab_silhouette")
            doprop(layout, "use_surface_falloff")

        elif sculpt_tool == 'PAINT':
            row = layout.row(align=True)
            doprop(row, "flow", baselayout=layout)
            row.prop(brush.channels["flow"].mappings["PRESSURE"], "invert", text="")

            row = layout.row(align=True)
            doprop(row, "wet_mix", baselayout=layout)
            row.prop(brush.channels["wet_mix"].mappings["PRESSURE"], "invert", text="")

            row = layout.row(align=True)
            doprop(row, "wet_persistence", baselayout=layout)
            row.prop(brush.channels["wet_persistence"].mappings["PRESSURE"], "invert", text="")

            row = layout.row(align=True)
            doprop(row, "wet_paint_radius_factor", baselayout=layout)

            row = layout.row(align=True)
            doprop(row, "density", baselayout=layout)
            row.prop(brush.channels["density"].mappings["PRESSURE"], "invert", text="")

            row = layout.row()
            doprop(row, "tip_roundness", baselayout=layout)

            row = layout.row()
            doprop(row, "tip_scale_x", baselayout=layout)

            doprop(layout.column(), "hue_offset", baselayout=layout)

        elif sculpt_tool == 'SMEAR':
            col = layout.column()
            doprop(col, "smear_deform_type")

        elif sculpt_tool == 'BOUNDARY':
            doprop(layout, "deform_target")
            layout.separator()
            col = layout.column()
            doprop(col, "boundary_deform_type")
            doprop(col, "boundary_falloff_type")
            doprop(col, "boundary_offset")

        elif sculpt_tool == 'TOPOLOGY':
            col = layout.column()
            doprop(col, "slide_deform_type")

        elif sculpt_tool == 'MULTIPLANE_SCRAPE':
            col = layout.column()
            doprop(col, "multiplane_scrape_angle")
            doprop(col, "use_multiplane_scrape_dynamic")
            doprop(col, "show_multiplane_scrape_planes_preview")

        elif sculpt_tool == 'SCENE_PROJECT':
            col = layout.column()
            doprop(col, "scene_project_direction_type")

        elif sculpt_tool == 'ARRAY':
            col = layout.column()
            doprop(col, "array_deform_type")
            doprop(col, "array_count")
            doprop(col, "use_array_lock_orientation")
            doprop(col, "use_array_fill_holes")

        elif sculpt_tool == 'SMOOTH':
            col = layout.column()
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "boundary_smooth",
                slider=True
                #text="Weight By Face Area",
            )

            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "use_weighted_smooth"
                #text="Weight By Face Area",
            )
            UnifiedPaintPanel.channel_unified(layout,
                context,
                brush,
                "preserve_faceset_boundary")

            #col.prop(brush, "use_weighted_smooth")
            #col.prop(brush, "preserve_faceset_boundary")

            if UnifiedPaintPanel.get_channel_value(context, brush, "preserve_faceset_boundary"):
                UnifiedPaintPanel.channel_unified(layout,
                    context,
                    brush,
                    "fset_slide")

            doprop(col, "smooth_deform_type")

            if brush.smooth_deform_type == 'SURFACE':
                doprop(col, "surface_smooth_shape_preservation")
                doprop(col, "surface_smooth_current_vertex")
                doprop(col, "surface_smooth_iterations")

        elif sculpt_tool == 'DISPLACEMENT_SMEAR':
            col = layout.column()
            doprop(col, "smear_deform_type")

        elif sculpt_tool == 'MASK':
            doprop(layout.row(), "mask_tool", expand=True, baselayout=layout)

        # End sculpt_tool interface.

    # 3D and 2D Texture Paint Mode.
    elif mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        capabilities = brush.image_paint_capabilities

        if brush.image_tool == 'FILL':
            # For some reason fill threshold only appears to be implemented in
            # 2D paint.
            if brush.color_type == 'COLOR':
                if mode == 'PAINT_2D':
                    layout.prop(brush, "fill_threshold", text="Fill Threshold", slider=True)
            elif brush.color_type == 'GRADIENT':
                layout.row().prop(brush, "gradient_fill_mode", expand=True)


def brush_shared_settings(layout, context, brush, popover=False):
    """ Draw simple brush settings that are shared between different paint modes. """

    mode = UnifiedPaintPanel.get_brush_mode(context)

    ### Determine which settings to draw.  ###
    blend_mode = False
    size = False
    size_mode = False
    strength = False
    strength_pressure = False
    weight = False
    direction = False

    # 3D and 2D Texture Paint #
    if mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        if not popover:
            blend_mode = brush.image_paint_capabilities.has_color
            size = brush.image_paint_capabilities.has_radius
            strength = strength_pressure = True

    # Sculpt #
    if mode == 'SCULPT':
        size_mode = True
        if not popover:
            size = True
            strength = True
            strength_pressure = brush.sculpt_capabilities.has_strength_pressure
            direction = not brush.sculpt_capabilities.has_direction

    # Vertex Paint #
    if mode == 'PAINT_VERTEX':
        if not popover:
            blend_mode = True
            size = True
            strength = True
            strength_pressure = True

    # Weight Paint #
    if mode == 'PAINT_WEIGHT':
        if not popover:
            size = True
            weight = brush.weight_paint_capabilities.has_weight
            strength = strength_pressure = True
        # Only draw blend mode for the Draw tool, because for other tools it is
        # pointless.  D5928#137944
        if brush.weight_tool == 'DRAW':
            blend_mode = True

    # UV Sculpt #
    if mode == 'UV_SCULPT':
        size = True
        strength = True

    ### Draw settings.  ###
    ups = context.scene.tool_settings.unified_paint_settings

    if blend_mode:
        layout.prop(brush, "blend", text="Blend")
        layout.separator()

    if weight:
        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "weight",
            unified_name="use_unified_weight",
            slider=True,)

    size_owner = ups if ups.use_unified_size else brush
    size_prop = "size"
    if size_mode and (size_owner.use_locked_size == 'SCENE'):
        size_prop = "unprojected_radius"

    if size or size_mode:
        if size:
            UnifiedPaintPanel.prop_unified(layout,
                context,
                brush,
                size_prop,
                unified_name="use_unified_size",
                pressure_name="use_pressure_size",
                text="Radius",
                slider=True,)
        if size_mode:
            #layout.row().prop(size_owner, "use_locked_size", expand=True)
            UnifiedPaintPanel.prop_unified(layout.row(),
                context,
                brush,
                "use_locked_size",
                unified_name="use_unified_size",
                pressure_name="use_pressure_size",
                expand=True,
                slider=True)
            layout.separator()

    if strength:
        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "strength",
            unified_name="use_unified_strength",
            pressure_name="use_pressure_strength",
            slider=True)
        layout.separator()
    elif strength:
        pressure_name = "use_pressure_strength" if strength_pressure else None
        UnifiedPaintPanel.prop_unified(layout,
            context,
            brush,
            "strength",
            unified_name="use_unified_strength",
            pressure_name=pressure_name,
            slider=True,)
        layout.separator()
    if direction:
        UnifiedPaintPanel.channel_unified(layout,
            context,
            brush,
            "direction", expand=True)
        #layout.row().prop(brush, "direction", expand=True)
def get_ui_channels(channels, filterkeys=["show_in_workspace"]):
    ret = []
    for ch in channels:
        ok = len(filterkeys) == 0
        for key in filterkeys:
            if getattr(ch, key):
                ok = True
                break
        if ok:
            ret.append(ch)

    ret.sort(key=lambda x: x.ui_order)

    return ret


class ReorderBrushChannel(Operator):
    """Tooltip"""
    bl_idname = "brush.change_channel_order"
    bl_label = "Change Channel Order"
    bl_options = {"UNDO"}

    direction : IntProperty()
    channel : StringProperty()
    filterkey : StringProperty()

    @classmethod
    def poll(cls, context):
        return context.mode == "SCULPT" and context.tool_settings.sculpt.brush

    def execute(self, context):
        ts = context.tool_settings

        brush = ts.sculpt.brush

        channels = brush.channels
        if self.channel not in channels:
            print("bad channel ", self.channel)
            return {'CANCELLED'}

        uinames = get_ui_channels(channels, [self.filterkey])
        uinames = set(map(lambda x: x.idname, uinames))

        channel = channels[self.channel]

        channels = list(channels)
        channels.sort(key=lambda x: x.ui_order)

        i = channels.index(channel)
        i2 = i + self.direction

        print("ORDERING", i, i2, self.direction, i2 < 0 or i2 >= len(channels))

        if i2 < 0 or i2 >= len(channels):
            return {'CANCELLED'}

        while i2 >= 0 and i2 < len(channels) and channels[i2].idname not in uinames:
            i2 += self.direction

        i2 = min(max(i2, 0), len(channels) - 1)

        tmp = channels[i2]
        channels[i2] = channels[i]
        channels[i] = tmp

        # ensure ui_order is 1-to-1
        for i, ch in enumerate(channels):
            ch.ui_order = i
            print(ch.idname, i)

        return {'FINISHED'}

classes.append(ReorderBrushChannel)

def brush_settings_channels(layout, context, brush, ui_editing=False, popover=False, show_reorder=None, filterkey="show_in_workspace",
                            parent="VIEW3D_PT_tools_brush_settings_channels", prefix="VIEW3D_PT_brush_category_"):
    channels = get_ui_channels(brush.channels, [filterkey])

    if show_reorder is None:
        show_reorder = ui_editing

    DynamicPaintPanelGen.ensureCategory("Basic", "Basic", parent=parent,
                                        prefix=prefix, ui_editing=ui_editing,
                                        show_reorder=show_reorder)

    for ch in channels:
        if len(ch.category) > 0:
            DynamicPaintPanelGen.ensureCategory(ch.category, ch.category, parent=parent,
                                                prefix=prefix, ui_editing=ui_editing,
                                                show_reorder=show_reorder, show_mappings=True)
            continue

        # VIEW3D_PT_brush_category_edit_
        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            ch.idname, show_reorder=show_reorder, expand=False, ui_editing=ui_editing, show_mappings=True)


def brush_settings_advanced(layout, context, brush, popover=False):
    """Draw advanced brush settings for Sculpt, Texture/Vertex/Weight Paint modes."""

    mode = UnifiedPaintPanel.get_brush_mode(context)

    # In the popover we want to combine advanced brush settings with
    # non-advanced brush settings.
    if popover:
        brush_settings(layout, context, brush, popover=True)
        layout.separator()
        layout.label(text="Advanced")

    # These options are shared across many modes.
    use_accumulate = False
    use_frontface = False

    if mode == 'SCULPT':
        capabilities = brush.sculpt_capabilities
        use_accumulate = capabilities.has_accumulate
        use_frontface = True

        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            "automasking", expand=True)
        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            "automasking_boundary_edges_propagation_steps")
        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            "concave_mask_factor",
            slider=True)

        """
        col = layout.column(heading="Auto-Masking", align=True)

        # topology automasking
        col.prop(brush, "use_automasking_topology", text="Topology")

        col.prop(brush, "use_automasking_concave")

        col2 = col.column()
        col2.enabled = brush.use_automasking_concave

        col2.prop(brush, "concave_mask_factor", text="Cavity Factor")
        col2.prop(brush, "invert_automasking_concavity", text="Invert Cavity Mask")

        # face masks automasking
        col.prop(brush, "use_automasking_face_sets", text="Face Sets")

        # boundary edges/face sets automasking
        col.prop(brush, "use_automasking_boundary_edges", text="Mesh Boundary")
        col.prop(brush, "use_automasking_boundary_face_sets", text="Face Sets Boundary")
        col.prop(brush, "automasking_boundary_edges_propagation_steps")
        """

        layout.separator()

        # sculpt plane settings
        if capabilities.has_sculpt_plane:
            layout.prop(brush, "sculpt_plane")

            col = layout.column(heading="Use Original", align=False)
            col = col.column()

            UnifiedPaintPanel.channel_unified(col,
                context,
                brush,
                "original_normal",
                text="Normal",
                expand=False)
            UnifiedPaintPanel.channel_unified(col,
                context,
                brush,
                "original_plane",
                text="Plane",
                expand=False)
            layout.separator()

    # 3D and 2D Texture Paint.
    elif mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        capabilities = brush.image_paint_capabilities
        use_accumulate = capabilities.has_accumulate

        if mode == 'PAINT_2D':
            layout.prop(brush, "use_paint_antialiasing")
        else:
            layout.prop(brush, "use_alpha")

        # Tool specific settings
        if brush.image_tool == 'SOFTEN':
            layout.separator()
            layout.row().prop(brush, "direction", expand=True)
            layout.prop(brush, "sharp_threshold")
            if mode == 'PAINT_2D':
                layout.prop(brush, "blur_kernel_radius")
            layout.prop(brush, "blur_mode")

        elif brush.image_tool == 'MASK':
            layout.prop(brush, "weight", text="Mask Value", slider=True)

        elif brush.image_tool == 'CLONE':
            if mode == 'PAINT_2D':
                layout.prop(brush, "clone_image", text="Image")
                layout.prop(brush, "clone_alpha", text="Alpha")

    # Vertex Paint #
    elif mode == 'PAINT_VERTEX':
        layout.prop(brush, "use_alpha")
        if brush.vertex_tool != 'SMEAR':
            use_accumulate = True
        use_frontface = True

    # Weight Paint
    elif mode == 'PAINT_WEIGHT':
        if brush.weight_tool != 'SMEAR':
            use_accumulate = True
        use_frontface = True

    # Draw shared settings.
    if use_accumulate:
        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            "accumulate")

    if use_frontface:
        UnifiedPaintPanel.channel_unified(layout.column(),
            context,
            brush,
            "use_frontface", text="Front Faces Only")


def draw_color_settings(context, layout, brush, color_type=False):
    """Draw color wheel and gradient settings."""
    ups = context.scene.tool_settings.unified_paint_settings

    if color_type:
        row = layout.row()
        row.use_property_split = False
        row.prop(brush, "color_type", expand=True)

    # Color wheel
    if brush.color_type == 'COLOR':
        UnifiedPaintPanel.prop_unified_color_picker(layout, context, brush, "color", value_slider=True)

        row = layout.row(align=True)
        UnifiedPaintPanel.prop_unified_color(row, context, brush, "color", text="")
        UnifiedPaintPanel.prop_unified_color(row, context, brush, "secondary_color", text="")
        row.separator()
        row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="", emboss=False)
        row.prop(ups, "use_unified_color", text="", icon='BRUSHES_ALL')
    # Gradient
    elif brush.color_type == 'GRADIENT':
        layout.template_color_ramp(brush, "gradient", expand=True)

        layout.use_property_split = True

        col = layout.column()

        if brush.image_tool == 'DRAW':
            UnifiedPaintPanel.prop_unified(col,
                context,
                brush,
                "secondary_color",
                unified_name="use_unified_color",
                text="Background Color",
                header=True,)

            col.prop(brush, "gradient_stroke_mode", text="Gradient Mapping")
            if brush.gradient_stroke_mode in {'SPACING_REPEAT', 'SPACING_CLAMP'}:
                col.prop(brush, "grad_spacing")


# Used in both the View3D toolbar and texture properties
def brush_texture_settings(layout, brush, sculpt):
    tex_slot = brush.texture_slot

    layout.use_property_split = True
    layout.use_property_decorate = False

    # map_mode
    layout.prop(tex_slot, "map_mode", text="Mapping")

    layout.separator()

    if tex_slot.map_mode == 'STENCIL':
        if brush.texture and brush.texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect")
        layout.operator("brush.stencil_reset_transform")

    # angle and texture_angle_source
    if tex_slot.has_texture_angle:
        col = layout.column()
        col.prop(tex_slot, "angle", text="Angle")
        if tex_slot.has_texture_angle_source:
            col.prop(tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and tex_slot.has_random_texture_angle:
                if sculpt:
                    if brush.sculpt_capabilities.has_random_texture_angle:
                        col.prop(tex_slot, "use_random", text="Random")
                        if tex_slot.use_random:
                            col.prop(tex_slot, "random_angle", text="Random Angle")
                else:
                    col.prop(tex_slot, "use_random", text="Random")
                    if tex_slot.use_random:
                        col.prop(tex_slot, "random_angle", text="Random Angle")

    # scale and offset
    layout.prop(tex_slot, "offset")
    layout.prop(tex_slot, "scale")

    if sculpt:
        # texture_sample_bias
        layout.prop(brush, "texture_sample_bias", slider=True, text="Sample Bias")


def brush_mask_texture_settings(layout, brush):
    mask_tex_slot = brush.mask_texture_slot

    layout.use_property_split = True
    layout.use_property_decorate = False

    # map_mode
    layout.row().prop(mask_tex_slot, "mask_map_mode", text="Mask Mapping")

    if mask_tex_slot.map_mode == 'STENCIL':
        if brush.mask_texture and brush.mask_texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect").mask = True
        layout.operator("brush.stencil_reset_transform").mask = True

    col = layout.column()
    col.prop(brush, "use_pressure_masking", text="Pressure Masking")
    # angle and texture_angle_source
    if mask_tex_slot.has_texture_angle:
        col = layout.column()
        col.prop(mask_tex_slot, "angle", text="Angle")
        if mask_tex_slot.has_texture_angle_source:
            col.prop(mask_tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and mask_tex_slot.has_random_texture_angle:
                col.prop(mask_tex_slot, "use_random", text="Random")
                if mask_tex_slot.use_random:
                    col.prop(mask_tex_slot, "random_angle", text="Random Angle")

    # scale and offset
    col.prop(mask_tex_slot, "offset")
    col.prop(mask_tex_slot, "scale")


def brush_basic_texpaint_settings(layout, context, brush, *, compact=False):
    """Draw Tool Settings header for Vertex Paint and 2D and 3D Texture Paint modes."""
    capabilities = brush.image_paint_capabilities

    if capabilities.has_color:
        UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
        layout.prop(brush, "blend", text="" if compact else "Blend")

    UnifiedPaintPanel.prop_unified(layout,
        context,
        brush,
        "size",
        pressure_name="use_pressure_size",
        unified_name="use_unified_size",
        slider=True,
        text="Radius",
        header=True)
    UnifiedPaintPanel.prop_unified(layout,
        context,
        brush,
        "strength",
        pressure_name="use_pressure_strength",
        unified_name="use_unified_strength",
        header=True)


def brush_basic__draw_color_selector(context, layout, brush, gp_settings, props):
    tool_settings = context.scene.tool_settings
    settings = tool_settings.gpencil_paint
    ma = gp_settings.material

    row = layout.row(align=True)
    if not gp_settings.use_material_pin:
        ma = context.object.active_material
    icon_id = 0
    txt_ma = ""
    if ma:
        ma.id_data.preview_ensure()
        if ma.id_data.preview:
            icon_id = ma.id_data.preview.icon_id
            txt_ma = ma.name
            maxw = 25
            if len(txt_ma) > maxw:
                txt_ma = txt_ma[:maxw - 5] + '..' + txt_ma[-3:]

    sub = row.row(align=True)
    sub.enabled = not gp_settings.use_material_pin
    sub.ui_units_x = 8
    sub.popover(panel="TOPBAR_PT_gpencil_materials",
        text=txt_ma,
        icon_value=icon_id,)

    row.prop(gp_settings, "use_material_pin", text="")

    if brush.gpencil_tool in {'DRAW', 'FILL'}:
        row.separator(factor=1.0)
        sub_row = row.row(align=True)
        sub_row.enabled = not gp_settings.pin_draw_mode
        if gp_settings.pin_draw_mode:
            sub_row.prop_enum(gp_settings, "brush_draw_mode", 'MATERIAL', text="", icon='MATERIAL')
            sub_row.prop_enum(gp_settings, "brush_draw_mode", 'VERTEXCOLOR', text="", icon='VPAINT_HLT')
        else:
            sub_row.prop_enum(settings, "color_mode", 'MATERIAL', text="", icon='MATERIAL')
            sub_row.prop_enum(settings, "color_mode", 'VERTEXCOLOR', text="", icon='VPAINT_HLT')

        sub_row = row.row(align=True)
        sub_row.enabled = settings.color_mode == 'VERTEXCOLOR' or gp_settings.brush_draw_mode == 'VERTEXCOLOR'
        sub_row.prop_with_popover(brush, "color", text="", panel="TOPBAR_PT_gpencil_vertexcolor")
        row.prop(gp_settings, "pin_draw_mode", text="")

    if props:
        row = layout.row(align=True)
        row.prop(props, "subdivision")


def brush_basic_gpencil_paint_settings(layout, context, brush, *, compact=False):
    tool_settings = context.tool_settings
    settings = tool_settings.gpencil_paint
    gp_settings = brush.gpencil_settings
    tool = context.workspace.tools.from_space_view3d_mode(context.mode, create=False)
    if gp_settings is None:
        return

    # Brush details
    if brush.gpencil_tool == 'ERASE':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
        row.prop(gp_settings, "use_occlude_eraser", text="", icon='XRAY')
        row.prop(gp_settings, "use_default_eraser", text="")

        row = layout.row(align=True)
        row.prop(gp_settings, "eraser_mode", expand=True)
        if gp_settings.eraser_mode == 'SOFT':
            row = layout.row(align=True)
            row.prop(gp_settings, "pen_strength", slider=True)
            row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_strength_factor")
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_thickness_factor")

        row = layout.row(align=True)
        row.prop(settings, "show_brush", text="Display Cursor")

    # FIXME: tools must use their own UI drawing!
    elif brush.gpencil_tool == 'FILL':
        use_property_split_prev = layout.use_property_split
        if compact:
            row = layout.row(align=True)
            row.prop(gp_settings, "fill_direction", text="", expand=True)
        else:
            layout.use_property_split = False
            row = layout.row(align=True)
            row.prop(gp_settings, "fill_direction", expand=True)

        row = layout.row(align=True)
        row.prop(gp_settings, "fill_factor")
        row = layout.row(align=True)
        row.prop(gp_settings, "dilate")
        row = layout.row(align=True)
        row.prop(brush, "size", text="Thickness")
        layout.use_property_split = use_property_split_prev

    else:  # brush.gpencil_tool == 'DRAW/TINT':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')

        if gp_settings.use_pressure and not compact:
            col = layout.column()
            col.template_curve_mapping(gp_settings, "curve_sensitivity", brush=True,
                                       use_negative_slope=True)

        row = layout.row(align=True)
        row.prop(gp_settings, "pen_strength", slider=True)
        row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

        if gp_settings.use_strength_pressure and not compact:
            col = layout.column()
            col.template_curve_mapping(gp_settings, "curve_strength", brush=True,
                                       use_negative_slope=True)

        if brush.gpencil_tool == 'TINT':
            row = layout.row(align=True)
            row.prop(gp_settings, "vertex_mode", text="Mode")
        else:
            row = layout.row(align=True)
            if context.region.type == 'TOOL_HEADER':
                row.prop(gp_settings, "caps_type", text="", expand=True)
            else:
                row.prop(gp_settings, "caps_type", text="Caps Type")

    # FIXME: tools must use their own UI drawing!
    if tool.idname in {
            "builtin.arc",
            "builtin.curve",
            "builtin.line",
            "builtin.box",
            "builtin.circle",
            "builtin.polyline"
    }:
        settings = context.tool_settings.gpencil_sculpt
        if compact:
            row = layout.row(align=True)
            row.prop(settings, "use_thickness_curve", text="", icon='SPHERECURVE')
            sub = row.row(align=True)
            sub.active = settings.use_thickness_curve
            sub.popover(panel="TOPBAR_PT_gpencil_primitive",
                text="Thickness Profile",)
        else:
            row = layout.row(align=True)
            row.prop(settings, "use_thickness_curve", text="Use Thickness Profile")
            sub = row.row(align=True)
            if settings.use_thickness_curve:
                # Curve
                layout.template_curve_mapping(settings, "thickness_primitive_curve", brush=True)


def brush_basic_gpencil_sculpt_settings(layout, _context, brush, *, compact=False):
    gp_settings = brush.gpencil_settings
    tool = brush.gpencil_sculpt_tool

    row = layout.row(align=True)
    row.prop(brush, "size", slider=True)
    sub = row.row(align=True)
    sub.enabled = tool not in {'GRAB', 'CLONE'}
    sub.prop(gp_settings, "use_pressure", text="")

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    if compact:
        if tool in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
            row.separator()
            row.prop(gp_settings, "direction", expand=True, text="")
    else:
        use_property_split_prev = layout.use_property_split
        layout.use_property_split = False
        if tool in {'THICKNESS', 'STRENGTH'}:
            layout.row().prop(gp_settings, "direction", expand=True)
        elif tool == 'PINCH':
            row = layout.row(align=True)
            row.prop_enum(gp_settings, "direction", value='ADD', text="Pinch")
            row.prop_enum(gp_settings, "direction", value='SUBTRACT', text="Inflate")
        elif tool == 'TWIST':
            row = layout.row(align=True)
            row.prop_enum(gp_settings, "direction", value='ADD', text="CCW")
            row.prop_enum(gp_settings, "direction", value='SUBTRACT', text="CW")
        layout.use_property_split = use_property_split_prev


def brush_basic_gpencil_weight_settings(layout, _context, brush, *, compact=False):
    layout.prop(brush, "size", slider=True)

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    layout.prop(brush, "weight", slider=True)


def brush_basic_gpencil_vertex_settings(layout, _context, brush, *, compact=False):
    gp_settings = brush.gpencil_settings

    # Brush details
    row = layout.row(align=True)
    row.prop(brush, "size", text="Radius")
    row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')

    if brush.gpencil_vertex_tool in {'DRAW', 'BLUR', 'SMEAR'}:
        row = layout.row(align=True)
        row.prop(gp_settings, "pen_strength", slider=True)
        row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

    if brush.gpencil_vertex_tool in {'DRAW', 'REPLACE'}:
        row = layout.row(align=True)
        row.prop(gp_settings, "vertex_mode", text="Mode")


