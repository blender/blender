# paint_palette.py (c) 2011 Dany Lebel (Axon_D)

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


bl_info = {
    "name": "Paint Palettes",
    "author": "Dany Lebel (Axon D)",
    "version": (0, 9, 3),
    "blender": (2, 63, 0),
    "location": "Image Editor and 3D View > Any Paint mode > Color Palette or Weight Palette panel",
    "description": "Palettes for color and weight paint modes",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Paint/Palettes",
    "category": "Paint",
}

"""
This add-on brings palettes to the paint modes.

    * Color Palette for Image Painting, Texture Paint and Vertex Paint modes.
    * Weight Palette for the Weight Paint mode.

Set a number of colors (or weights according to the mode) and then associate it
with the brush by using the button under the color.
"""

import bpy
from bpy.types import (
        Operator,
        Menu,
        Panel,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        FloatVectorProperty,
        IntProperty,
        StringProperty,
        PointerProperty,
        CollectionProperty,
        )


def update_panels():
    pp = bpy.context.scene.palette_props
    current_color = pp.colors[pp.current_color_index].color
    pp.color_name = pp.colors[pp.current_color_index].name
    brush = current_brush()
    brush.color = current_color
    pp.index = pp.current_color_index


def sample():
    pp = bpy.context.scene.palette_props
    current_color = pp.colors[pp.current_color_index]
    brush = current_brush()
    current_color.color = brush.color
    return None


def current_brush():
    context = bpy.context
    if context.area.type == 'VIEW_3D' and context.vertex_paint_object:
        brush = context.tool_settings.vertex_paint.brush
    elif context.area.type == 'VIEW_3D' and context.image_paint_object:
        brush = context.tool_settings.image_paint.brush
    elif context.area.type == 'IMAGE_EDITOR' and context.space_data.mode == 'PAINT':
        brush = context.tool_settings.image_paint.brush
    else:
        brush = None
    return brush


def update_weight_value():
    pp = bpy.context.scene.palette_props
    tt = bpy.context.tool_settings
    tt.unified_paint_settings.weight = pp.weight_value
    return None


class PALETTE_MT_menu(Menu):
    bl_label = "Presets"
    preset_subdir = ""
    preset_operator = "palette.load_gimp_palette"

    def path_menu(self, searchpaths, operator, props_default={}):
        layout = self.layout
        # hard coded to set the operators 'filepath' to the filename.
        import os
        import bpy.utils

        layout = self.layout
        if not searchpaths[0]:
            layout.label("* Missing Paths *")

        # collect paths
        else:
            files = []
            for directory in searchpaths:
                files.extend([(f, os.path.join(directory, f)) for f in os.listdir(directory)])

            files.sort()

            for f, filepath in files:

                if f.startswith("."):
                    continue
                # do not load everything from the given folder, only .gpl files
                if f[-4:] != ".gpl":
                    continue

                preset_name = bpy.path.display_name(f)
                props = layout.operator(operator, text=preset_name)

                for attr, value in props_default.items():
                    setattr(props, attr, value)

                props.filepath = filepath
                if operator == "palette.load_gimp_palette":
                    props.menu_idname = self.bl_idname

    def draw_preset(self, context):
        """Define these on the subclass
         - preset_operator
         - preset_subdir
        """
        import bpy
        self.path_menu([bpy.context.scene.palette_props.presets_folder], self.preset_operator)

    draw = draw_preset


class LoadGimpPalette(Operator):
    """Execute a preset"""
    bl_idname = "palette.load_gimp_palette"
    bl_label = "Load a Gimp palette"

    filepath = StringProperty(
            name="Path",
            description="Path of the .gpl file to load",
            default=""
            )
    menu_idname = StringProperty(
            name="Menu ID Name",
            description="ID name of the menu this was called from",
            default=""
            )

    def execute(self, context):
        from os.path import basename
        import re
        filepath = self.filepath

        palette_props = bpy.context.scene.palette_props
        palette_props.current_color_index = 0

        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = bpy.path.display_name(basename(filepath))

        palette_props.columns = 0
        error_palette = False  # errors found
        error_import = []      # collect exception messages
        start_color_index = 0  # store the starting line for color definitions

        if filepath[-4:] != ".gpl":
            error_palette = True
        else:
            gpl = open(filepath, "r")
            lines = gpl.readlines()
            palette_props.notes = ''
            has_color = False
            for index_0, line in enumerate(lines):
                if not line or (line[:12] == "GIMP Palette"):
                    pass
                elif line[:5] == "Name:":
                    palette_props.palette_name = line[5:]
                elif line[:8] == "Columns:":
                    palette_props.columns = int(line[8:])
                elif line[0] == "#":
                    palette_props.notes += line
                elif line[0] == "\n":
                    pass
                else:
                    has_color = True
                    start_color_index = index_0
                    break
            i = -1
            if has_color:
                for i, ln in enumerate(lines[start_color_index:]):
                    try:
                        palette_props.colors[i]
                    except IndexError:
                        palette_props.colors.add()
                    try:
                        # get line - find keywords with re.split, remove the empty ones with filter
                        get_line = list(filter(None, re.split(r'\t+|\s+', ln.rstrip('\n'))))
                        extract_colors = get_line[:3]
                        get_color_name = [str(name) for name in get_line[3:]]
                        color = [float(rgb) / 255 for rgb in extract_colors]
                        palette_props.colors[i].color = color
                        palette_props.colors[i].name = " ".join(get_color_name) or "Color " + str(i)
                    except Exception as e:
                        error_palette = True
                        error_import.append(".gpl file line: {}, error: {}".format(i + 1 + start_color_index, e))
                        pass

            exceeding = i + 1
            while palette_props.colors.__len__() > exceeding:
                palette_props.colors.remove(exceeding)

            if has_color:
                update_panels()
            gpl.close()
            pass

        message = "Loaded palette from file: {}".format(filepath)

        if error_palette:
            message = "Not supported palette format for file: {}".format(filepath)
            if error_import:
                message = "Some of the .gpl palette data can not be parsed. See Console for more info"
                print("\n[Paint Palette]\nOperator: palette.load_gimp_palette\nErrors: %s\n" %
                     ('\n'.join(error_import)))

        self.report({'INFO'}, message)

        return {'FINISHED'}


class WriteGimpPalette():
    """Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir """
    bl_options = {'REGISTER'}  # only because invoke_props_popup requires

    name = StringProperty(
                name="Name",
                description="Name of the preset, used to make the path name",
                maxlen=64, default=""
                )
    remove_active = BoolProperty(
                default=False,
                options={'HIDDEN'}
                )

    @staticmethod
    def as_filename(name):  # could reuse for other presets
        for char in " !@#$%^&*(){}:\";'[]<>,.\\/?":
            name = name.replace(char, '_')
        return name.lower().strip()

    def execute(self, context):
        import os
        pp = bpy.context.scene.palette_props

        if hasattr(self, "pre_cb"):
            self.pre_cb(context)

        preset_menu_class = getattr(bpy.types, self.preset_menu)

        if not self.remove_active:

            if not self.name:
                return {'FINISHED'}

            filename = self.as_filename(self.name)
            target_path = pp.presets_folder

            if not target_path:
                self.report({'WARNING'}, "Failed to create presets path")
                return {'CANCELLED'}

            if not os.path.exists(target_path):
                self.report({'WARNING'},
                            "Failure to open the saved Palletes Folder. Check if the path exists")
                return {'CANCELLED'}

            filepath = os.path.join(target_path, filename) + ".gpl"

            file_preset = open(filepath, 'wb')
            gpl = "GIMP Palette\n"
            gpl += "Name: %s\n" % filename
            gpl += "Columns: %d\n" % pp.columns
            gpl += pp.notes
            if pp.colors.items():
                for i, color in enumerate(pp.colors):
                    gpl += "%3d%4d%4d %s" % (color.color.r * 255, color.color.g * 255,
                                             color.color.b * 255, color.name + '\n')
            file_preset.write(bytes(gpl, 'UTF-8'))

            file_preset.close()

            pp.palette_name = filename

        else:
            preset_active = preset_menu_class.bl_label

            # fairly sloppy but convenient.
            filepath = bpy.utils.preset_find(preset_active, self.preset_subdir)

            if not filepath:
                filepath = bpy.utils.preset_find(preset_active,
                    self.preset_subdir, display_name=True)

            if not filepath:
                return {'CANCELLED'}

            if hasattr(self, "remove"):
                self.remove(context, filepath)
            else:
                try:
                    os.remove(filepath)
                except:
                    import traceback
                    traceback.print_exc()

            # XXX, stupid!
            preset_menu_class.bl_label = "Presets"

        if hasattr(self, "post_cb"):
            self.post_cb(context)

        return {'FINISHED'}

    def check(self, context):
        self.name = self.as_filename(self.name)

    def invoke(self, context, event):
        if not self.remove_active:
            wm = context.window_manager
            return wm.invoke_props_dialog(self)
        else:
            return self.execute(context)


class AddPresetPalette(WriteGimpPalette, Operator):
    bl_idname = "palette.preset_add"
    bl_label = "Add Palette Preset"
    preset_menu = "PALETTE_MT_menu"
    bl_description = "Add a Palette Preset"

    preset_defines = []
    preset_values = []
    preset_subdir = "palette"


class PALETTE_OT_add_color(Operator):
    bl_idname = "palette_props.add_color"
    bl_label = ""
    bl_description = "Add a Color to the Palette"

    def execute(self, context):
        pp = bpy.context.scene.palette_props
        new_index = 0
        if pp.colors.items():
            new_index = pp.current_color_index + 1
        pp.colors.add()

        last = pp.colors.__len__() - 1

        pp.colors.move(last, new_index)
        pp.current_color_index = new_index
        sample()
        update_panels()

        return {'FINISHED'}


class PALETTE_OT_remove_color(Operator):
    bl_idname = "palette_props.remove_color"
    bl_label = ""
    bl_description = "Remove Selected Color"

    @classmethod
    def poll(cls, context):
        pp = bpy.context.scene.palette_props
        return bool(pp.colors.items())

    def execute(self, context):
        pp = context.scene.palette_props
        i = pp.current_color_index
        pp.colors.remove(i)

        if pp.current_color_index >= pp.colors.__len__():
            pp.index = pp.current_color_index = pp.colors.__len__() - 1

        return {'FINISHED'}


class PALETTE_OT_sample_tool_color(Operator):
    bl_idname = "palette_props.sample_tool_color"
    bl_label = ""
    bl_description = "Sample Tool Color"

    def execute(self, context):
        pp = context.scene.palette_props
        brush = current_brush()
        pp.colors[pp.current_color_index].color = brush.color

        return {'FINISHED'}


class IMAGE_OT_select_color(Operator):
    bl_idname = "paint.select_color"
    bl_label = ""
    bl_description = "Select this color"
    bl_options = {'UNDO'}

    color_index = IntProperty()

    def invoke(self, context, event):
        palette_props = context.scene.palette_props
        palette_props.current_color_index = self.color_index

        update_panels()

        return {'FINISHED'}


def color_palette_draw(self, context):
    palette_props = bpy.context.scene.palette_props

    layout = self.layout
    bpy.types.PALETTE_MT_menu.preset_subdir = palette_props.presets_folder

    row = layout.row(align=True)
    row.menu("PALETTE_MT_menu", text=palette_props.palette_name.rstrip())
    row.operator("palette.preset_add", text="", icon="ZOOMIN")
    row.operator("palette.preset_add", text="", icon="ZOOMOUT").remove_active = True

    col = layout.column(align=True)
    row = col.row(align=True)
    row.operator("palette_props.add_color", icon="ZOOMIN")
    row.prop(palette_props, "index")
    row.operator("palette_props.remove_color", icon="PANEL_CLOSE")

    row = col.row(align=True)
    row.prop(palette_props, "columns")
    if palette_props.colors.items():
        layout = col.box()
        row = layout.row(align=True)
        row.prop(palette_props, "color_name")
        row.operator("palette_props.sample_tool_color", icon="COLOR")

    laycol = layout.column(align=False)

    if palette_props.columns:
        columns = palette_props.columns
    else:
        columns = 16

    for i, color in enumerate(palette_props.colors):
        if not i % columns:
            row1 = laycol.row(align=True)
            row1.scale_y = 0.8
            row2 = laycol.row(align=True)
            row2.scale_y = 0.8

        active = True if i == palette_props.current_color_index else False
        icons = "LAYER_ACTIVE" if active else "LAYER_USED"
        row1.prop(palette_props.colors[i], "color", event=True, toggle=True)
        row2.operator("paint.select_color", text="  ",
                      emboss=active, icon=icons).color_index = i

    layout = self.layout
    row = layout.row()
    row.prop(palette_props, "presets_folder", text="")

    pass


class BrushButtonsPanel():
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        toolsettings = context.tool_settings.image_paint
        return sima.show_paint and toolsettings.brush


class PaintPanel():
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = 'Tools'

    @staticmethod
    def paint_settings(context):
        ts = context.tool_settings

        if context.vertex_paint_object:
            return ts.vertex_paint
        elif context.weight_paint_object:
            return ts.weight_paint
        elif context.texture_paint_object:
            return ts.image_paint
        return None


class IMAGE_PT_color_palette(BrushButtonsPanel, Panel):
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        color_palette_draw(self, context)


class VIEW3D_PT_color_palette(PaintPanel, Panel):
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.image_paint_object or context.vertex_paint_object)

    def draw(self, context):
        color_palette_draw(self, context)


class VIEW3D_OT_select_weight(Operator):
    bl_idname = "paint.select_weight"
    bl_label = ""
    bl_description = "Select this weight value slot"
    bl_options = {'UNDO'}

    weight_index = IntProperty()

    def current_weight(self):
        pp = bpy.context.scene.palette_props
        if self.weight_index == 0:
            weight = pp.weight_0
        elif self.weight_index == 1:
            weight = pp.weight_1
        elif self.weight_index == 2:
            weight = pp.weight_2
        elif self.weight_index == 3:
            weight = pp.weight_3
        elif self.weight_index == 4:
            weight = pp.weight_4
        elif self.weight_index == 5:
            weight = pp.weight_5
        elif self.weight_index == 6:
            weight = pp.weight_6
        elif self.weight_index == 7:
            weight = pp.weight_7
        elif self.weight_index == 8:
            weight = pp.weight_8
        elif self.weight_index == 9:
            weight = pp.weight_9
        elif self.weight_index == 10:
            weight = pp.weight_10
        return weight

    def invoke(self, context, event):
        palette_props = context.scene.palette_props
        palette_props.current_weight_index = self.weight_index

        if self.weight_index == 0:
            weight = palette_props.weight_0
        elif self.weight_index == 1:
            weight = palette_props.weight_1
        elif self.weight_index == 2:
            weight = palette_props.weight_2
        elif self.weight_index == 3:
            weight = palette_props.weight_3
        elif self.weight_index == 4:
            weight = palette_props.weight_4
        elif self.weight_index == 5:
            weight = palette_props.weight_5
        elif self.weight_index == 6:
            weight = palette_props.weight_6
        elif self.weight_index == 7:
            weight = palette_props.weight_7
        elif self.weight_index == 8:
            weight = palette_props.weight_8
        elif self.weight_index == 9:
            weight = palette_props.weight_9
        elif self.weight_index == 10:
            weight = palette_props.weight_10
        palette_props.weight = weight

        return {'FINISHED'}


class VIEW3D_OT_reset_weight_palette(Operator):
    bl_idname = "paint.reset_weight_palette"
    bl_label = ""
    bl_description = "Reset the active Weight slot to it's default value"

    def execute(self, context):
        try:
            palette_props = context.scene.palette_props
            dict_defs = {0: 0.0, 1: 0.1, 2: 0.25,
                         3: 0.333, 4: 0.4, 5: 0.5,
                         6: 0.6, 7: 0.6666, 8: 0.75,
                         9: 0.9, 10: 1.0
                        }
            current_idx = palette_props.current_weight_index
            palette_props.weight = dict_defs[current_idx]

            var_name = "weight_" + str(current_idx)
            var_to_change = getattr(palette_props, var_name, None)
            if var_to_change:
                var_to_change = dict_defs[current_idx]

            return {'FINISHED'}

        except Exception as e:
            self.report({'WARNING'},
                        "Reset Weight pallete could not be completed (See Console for more info)")
            print("\n[Paint Palette]\nOperator: paint.reset_weight_palette\nError: %s\n" % e)

            return {'CANCELLED'}


class VIEW3D_PT_weight_palette(PaintPanel, Panel):
    bl_label = "Weight Palette"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.weight_paint_object

    def draw(self, context):
        palette_props = context.scene.palette_props

        layout = self.layout
        row = layout.row()
        row.prop(palette_props, "weight", slider=True)
        box = layout.box()

        selected_weight = palette_props.current_weight_index
        for props in range(0, 11):
            embossed = False if props == selected_weight else True
            prop_name = "weight_" + str(props)
            prop_value = getattr(palette_props, prop_name, "")
            if props in (0, 10):
                row = box.row(align=True)
            elif (props + 2) % 3 == 0:
                col = box.column(align=True)
                row = col.row(align=True)
            else:
                if props == 1:
                    row = box.row(align=True)
                row = row.row(align=True)

            row.operator("paint.select_weight", text="%.2f" % prop_value,
                     emboss=embossed).weight_index = props

        row = layout.row()
        row.operator("paint.reset_weight_palette", text="Reset")


class Colors(PropertyGroup):
    """Class for colors CollectionProperty"""
    color = FloatVectorProperty(
            name="",
            description="",
            default=(0.8, 0.8, 0.8),
            min=0, max=1,
            step=1, precision=3,
            subtype='COLOR_GAMMA',
            size=3
            )


class Weights(PropertyGroup):
    """Class for Weights Collection Property"""
    weight = FloatProperty(
            default=0.0,
            min=0.0,
            max=1.0,
            precision=3
            )


class PaletteProps(PropertyGroup):

    def update_color_name(self, context):
        pp = bpy.context.scene.palette_props
        pp.colors[pp.current_color_index].name = pp.color_name
        return None

    def move_color(self, context):
        pp = bpy.context.scene.palette_props
        if pp.colors.items() and pp.current_color_index != pp.index:
            if pp.index >= pp.colors.__len__():
                pp.index = pp.colors.__len__() - 1

            pp.colors.move(pp.current_color_index, pp.index)
            pp.current_color_index = pp.index
        return None

    def update_weight(self, context):
        pp = context.scene.palette_props
        weight = pp.weight
        if pp.current_weight_index == 0:
            pp.weight_0 = weight
        elif pp.current_weight_index == 1:
            pp.weight_1 = weight
        elif pp.current_weight_index == 2:
            pp.weight_2 = weight
        elif pp.current_weight_index == 3:
            pp.weight_3 = weight
        elif pp.current_weight_index == 4:
            pp.weight_4 = weight
        elif pp.current_weight_index == 5:
            pp.weight_5 = weight
        elif pp.current_weight_index == 6:
            pp.weight_6 = weight
        elif pp.current_weight_index == 7:
            pp.weight_7 = weight
        elif pp.current_weight_index == 8:
            pp.weight_8 = weight
        elif pp.current_weight_index == 9:
            pp.weight_9 = weight
        elif pp.current_weight_index == 10:
            pp.weight_10 = weight
        bpy.context.tool_settings.unified_paint_settings.weight = weight
        return None

    palette_name = StringProperty(
            name="Palette Name",
            default="Preset",
            subtype='FILE_NAME'
            )
    color_name = StringProperty(
            name="",
            description="Color Name",
            default="Untitled",
            update=update_color_name
            )
    columns = IntProperty(
            name="Columns",
            description="Number of Columns",
            min=0, max=16,
            default=0
            )
    index = IntProperty(
            name="Index",
            description="Move Selected Color",
            min=0,
            update=move_color
            )
    notes = StringProperty(
            name="Palette Notes",
            default="#\n"
            )
    current_color_index = IntProperty(
            name="Current Color Index",
            description="",
            default=0,
            min=0
            )
    current_weight_index = IntProperty(
            name="Current Color Index",
            description="",
            default=10,
            min=-1
            )
    presets_folder = StringProperty(name="",
            description="Palettes Folder",
            subtype="DIR_PATH"
            )
    colors = CollectionProperty(
            type=Colors
            )
    weight = FloatProperty(
            name="Weight",
            description="Modify the active Weight preset slot value",
            default=0.0,
            min=0.0, max=1.0,
            precision=3,
            update=update_weight
            )
    weight_0 = FloatProperty(
            default=0.0,
            min=0.0, max=1.0,
            precision=3
            )
    weight_1 = FloatProperty(
            default=0.1,
            min=0.0, max=1.0,
            precision=3
            )
    weight_2 = FloatProperty(
            default=0.25,
            min=0.0, max=1.0,
            precision=3
            )
    weight_3 = FloatProperty(
            default=0.333,
            min=0.0, max=1.0,
            precision=3
            )
    weight_4 = FloatProperty(
            default=0.4,
            min=0.0, max=1.0,
            precision=3
            )
    weight_5 = FloatProperty(
            default=0.5,
            min=0.0, max=1.0,
            precision=3
            )
    weight_6 = FloatProperty(
            default=0.6,
            min=0.0, max=1.0,
            precision=3
            )
    weight_7 = FloatProperty(
            default=0.6666,
            min=0.0, max=1.0,
            precision=3
            )
    weight_8 = FloatProperty(
            default=0.75,
            min=0.0, max=1.0,
            precision=3
            )
    weight_9 = FloatProperty(
            default=0.9,
            min=0.0, max=1.0,
            precision=3
            )
    weight_10 = FloatProperty(
            default=1.0,
            min=0.0, max=1.0,
            precision=3
            )
    pass


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.palette_props = PointerProperty(
                                        type=PaletteProps,
                                        name="Palette Props",
                                        description=""
                                        )
    pass


def unregister():
    bpy.utils.unregister_module(__name__)

    del bpy.types.Scene.palette_props
    pass


if __name__ == "__main__":
    register()
