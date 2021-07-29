# -*- coding:utf-8 -*-

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
import bpy
import os
import subprocess
from bl_operators.presets import AddPresetBase
from mathutils import Vector
from bpy.props import StringProperty
from .archipack_gl import (
    ThumbHandle, Screen, GlRect,
    GlPolyline, GlPolygon, GlText, GlHandle
)


class CruxHandle(GlHandle):

    def __init__(self, sensor_size, depth):
        GlHandle.__init__(self, sensor_size, 0, True, False)
        self.branch_0 = GlPolygon((1, 1, 1, 1), d=2)
        self.branch_1 = GlPolygon((1, 1, 1, 1), d=2)
        self.branch_2 = GlPolygon((1, 1, 1, 1), d=2)
        self.branch_3 = GlPolygon((1, 1, 1, 1), d=2)
        self.depth = depth

    def set_pos(self, pos_2d):
        self.pos_2d = pos_2d
        o = pos_2d
        w = 0.5 * self.sensor_width
        d = self.depth
        c = d / 1.4242
        s = w - c
        p0 = o + Vector((s, w))
        p1 = o + Vector((w, s))
        p2 = o + Vector((c, 0))
        p3 = o + Vector((w, -s))
        p4 = o + Vector((s, -w))
        p5 = o + Vector((0, -c))
        p6 = o + Vector((-s, -w))
        p7 = o + Vector((-w, -s))
        p8 = o + Vector((-c, 0))
        p9 = o + Vector((-w, s))
        p10 = o + Vector((-s, w))
        p11 = o + Vector((0, c))
        self.branch_0.set_pos([p11, p0, p1, p2, o])
        self.branch_1.set_pos([p2, p3, p4, p5, o])
        self.branch_2.set_pos([p5, p6, p7, p8, o])
        self.branch_3.set_pos([p8, p9, p10, p11, o])

    @property
    def pts(self):
        return [self.pos_2d]

    @property
    def sensor_center(self):
        return self.pos_2d

    def draw(self, context, render=False):
        self.render = render
        self.branch_0.colour_inactive = self.colour
        self.branch_1.colour_inactive = self.colour
        self.branch_2.colour_inactive = self.colour
        self.branch_3.colour_inactive = self.colour
        self.branch_0.draw(context)
        self.branch_1.draw(context)
        self.branch_2.draw(context)
        self.branch_3.draw(context)


class SeekBox(GlText, GlHandle):
    """
        Text input to filter items by label
        TODO:
            - add cross to empty text
            - get text from keyboard
    """

    def __init__(self):
        GlHandle.__init__(self, 0, 0, True, False, d=2)
        GlText.__init__(self, d=2)
        self.sensor_width = 250
        self.pos_3d = Vector((0, 0))
        self.bg = GlRect(colour=(0, 0, 0, 0.7))
        self.frame = GlPolyline((1, 1, 1, 1), d=2)
        self.frame.closed = True
        self.cancel = CruxHandle(16, 4)
        self.line_pos = 0

    @property
    def pts(self):
        return [self.pos_3d]

    def set_pos(self, context, pos_2d):
        x, ty = self.text_size(context)
        w = self.sensor_width
        y = 12
        pos_2d.y += y
        pos_2d.x -= 0.5 * w
        self.pos_2d = pos_2d.copy()
        self.pos_3d = pos_2d.copy()
        self.pos_3d.x += 6
        self.sensor_height = y
        p0 = pos_2d + Vector((w, -0.5 * y))
        p1 = pos_2d + Vector((w, 1.5 * y))
        p2 = pos_2d + Vector((0, 1.5 * y))
        p3 = pos_2d + Vector((0, -0.5 * y))
        self.bg.set_pos([p0, p2])
        self.frame.set_pos([p0, p1, p2, p3])
        self.cancel.set_pos(pos_2d + Vector((w + 15, 0.5 * y)))

    def keyboard_entry(self, context, event):
        c = event.ascii
        if c:
            if c == ",":
                c = "."
            self.label = self.label[:self.line_pos] + c + self.label[self.line_pos:]
            self.line_pos += 1

        if self.label:
            if event.type == 'BACK_SPACE':
                self.label = self.label[:self.line_pos - 1] + self.label[self.line_pos:]
                self.line_pos -= 1

            elif event.type == 'DEL':
                self.label = self.label[:self.line_pos] + self.label[self.line_pos + 1:]

            elif event.type == 'LEFT_ARROW':
                self.line_pos = (self.line_pos - 1) % (len(self.label) + 1)

            elif event.type == 'RIGHT_ARROW':
                self.line_pos = (self.line_pos + 1) % (len(self.label) + 1)

    def draw(self, context):
        self.bg.draw(context)
        self.frame.draw(context)
        GlText.draw(self, context)
        self.cancel.draw(context)

    @property
    def sensor_center(self):
        return self.pos_3d


preset_paths = bpy.utils.script_paths("presets")
addons_paths = bpy.utils.script_paths("addons")


class PresetMenuItem():
    def __init__(self, thumbsize, preset, image=None):
        name = bpy.path.display_name_from_filepath(preset)
        self.preset = preset
        self.handle = ThumbHandle(thumbsize, name, image, draggable=True)
        self.enable = True

    def filter(self, keywords):
        for key in keywords:
            if key not in self.handle.label.label:
                return False
        return True

    def set_pos(self, context, pos):
        self.handle.set_pos(context, pos)

    def check_hover(self, mouse_pos):
        self.handle.check_hover(mouse_pos)

    def mouse_press(self):
        if self.handle.hover:
            self.handle.hover = False
            self.handle.active = True
            return True
        return False

    def draw(self, context):
        if self.enable:
            self.handle.draw(context)


class PresetMenu():

    keyboard_type = {
            'BACK_SPACE', 'DEL',
            'LEFT_ARROW', 'RIGHT_ARROW'
            }

    def __init__(self, context, category, thumbsize=Vector((150, 100))):
        self.imageList = []
        self.menuItems = []
        self.thumbsize = thumbsize
        file_list = self.scan_files(category)
        self.default_image = None
        self.load_default_image()
        for filepath in file_list:
            self.make_menuitem(filepath)
        self.margin = 50
        self.y_scroll = 0
        self.scroll_max = 1000
        self.spacing = Vector((25, 25))
        self.screen = Screen(self.margin)
        self.mouse_pos = Vector((0, 0))
        self.bg = GlRect(colour=(0, 0, 0, 0.7))
        self.border = GlPolyline((0.7, 0.7, 0.7, 1), d=2)
        self.keywords = SeekBox()
        self.keywords.colour_normal = (1, 1, 1, 1)

        self.border.closed = True
        self.set_pos(context)

    def load_default_image(self):
        img_idx = bpy.data.images.find("missing.png")
        if img_idx > -1:
            self.default_image = bpy.data.images[img_idx]
            self.imageList.append(self.default_image.filepath_raw)
            return
        dir_path = os.path.dirname(os.path.realpath(__file__))
        sub_path = "presets" + os.path.sep + "missing.png"
        filepath = os.path.join(dir_path, sub_path)
        if os.path.exists(filepath) and os.path.isfile(filepath):
            self.default_image = bpy.data.images.load(filepath=filepath)
            self.imageList.append(self.default_image.filepath_raw)
        if self.default_image is None:
            raise EnvironmentError("archipack/presets/missing.png not found")

    def scan_files(self, category):
        file_list = []
        # load default presets
        dir_path = os.path.dirname(os.path.realpath(__file__))
        sub_path = "presets" + os.path.sep + category
        presets_path = os.path.join(dir_path, sub_path)
        if os.path.exists(presets_path):
            file_list += [presets_path + os.path.sep + f[:-3]
                for f in os.listdir(presets_path)
                if f.endswith('.py') and
                not f.startswith('.')]
        # load user def presets
        for path in preset_paths:
            presets_path = os.path.join(path, category)
            if os.path.exists(presets_path):
                file_list += [presets_path + os.path.sep + f[:-3]
                    for f in os.listdir(presets_path)
                    if f.endswith('.py') and
                    not f.startswith('.')]

        file_list.sort()
        return file_list

    def clearImages(self):
        for image in bpy.data.images:
            if image.filepath_raw in self.imageList:
                # image.user_clear()
                bpy.data.images.remove(image, do_unlink=True)
        self.imageList.clear()

    def make_menuitem(self, filepath):
        """
            @TODO:
            Lazy load images
        """
        image = None
        img_idx = bpy.data.images.find(os.path.basename(filepath) + '.png')
        if img_idx > -1 and bpy.data.images[img_idx].filepath_raw == filepath:
            image = bpy.data.images[img_idx]
            self.imageList.append(image.filepath_raw)
        elif os.path.exists(filepath + '.png') and os.path.isfile(filepath + '.png'):
            image = bpy.data.images.load(filepath=filepath + '.png')
            self.imageList.append(image)
        if image is None:
            image = self.default_image
        item = PresetMenuItem(self.thumbsize, filepath + '.py', image)
        self.menuItems.append(item)

    def set_pos(self, context):

        x_min, x_max, y_min, y_max = self.screen.size(context)
        p0, p1, p2, p3 = Vector((x_min, y_min)), Vector((x_min, y_max)), Vector((x_max, y_max)), Vector((x_max, y_min))
        self.bg.set_pos([p0, p2])
        self.border.set_pos([p0, p1, p2, p3])
        x_min += 0.5 * self.thumbsize.x + 0.5 * self.margin
        x_max -= 0.5 * self.thumbsize.x + 0.5 * self.margin
        y_max -= 0.5 * self.thumbsize.y + 0.5 * self.margin
        y_min += 0.5 * self.margin
        x = x_min
        y = y_max + self.y_scroll
        n_rows = 0

        self.keywords.set_pos(context, p1 + 0.5 * (p2 - p1))
        keywords = self.keywords.label.split(" ")

        for item in self.menuItems:
            if y > y_max or y < y_min:
                item.enable = False
            else:
                item.enable = True

            # filter items by name
            if len(keywords) > 0 and not item.filter(keywords):
                item.enable = False
                continue

            item.set_pos(context, Vector((x, y)))
            x += self.thumbsize.x + self.spacing.x
            if x > x_max:
                n_rows += 1
                x = x_min
                y -= self.thumbsize.y + self.spacing.y

        self.scroll_max = max(0, n_rows - 1) * (self.thumbsize.y + self.spacing.y)

    def draw(self, context):
        self.bg.draw(context)
        self.border.draw(context)
        self.keywords.draw(context)
        for item in self.menuItems:
            item.draw(context)

    def mouse_press(self, context, event):
        self.mouse_position(event)

        if self.keywords.cancel.hover:
            self.keywords.label = ""
            self.keywords.line_pos = 0
            self.set_pos(context)

        for item in self.menuItems:
            if item.enable and item.mouse_press():
                # load item preset
                return item.preset
        return None

    def mouse_position(self, event):
        self.mouse_pos.x, self.mouse_pos.y = event.mouse_region_x, event.mouse_region_y

    def mouse_move(self, context, event):
        self.mouse_position(event)
        self.keywords.check_hover(self.mouse_pos)
        self.keywords.cancel.check_hover(self.mouse_pos)
        for item in self.menuItems:
            item.check_hover(self.mouse_pos)

    def scroll_up(self, context, event):
        self.y_scroll = max(0, self.y_scroll - (self.thumbsize.y + self.spacing.y))
        self.set_pos(context)
        # print("scroll_up %s" % (self.y_scroll))

    def scroll_down(self, context, event):
        self.y_scroll = min(self.scroll_max, self.y_scroll + (self.thumbsize.y + self.spacing.y))
        self.set_pos(context)
        # print("scroll_down %s" % (self.y_scroll))

    def keyboard_entry(self, context, event):
        self.keywords.keyboard_entry(context, event)
        self.set_pos(context)


class PresetMenuOperator():

    preset_operator = StringProperty(
        options={'SKIP_SAVE'},
        default="script.execute_preset"
    )

    def __init__(self):
        self.menu = None
        self._handle = None

    def exit(self, context):
        self.menu.clearImages()
        bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')

    def draw_handler(self, _self, context):
        self.menu.draw(context)

    def modal(self, context, event):
        if self.menu is None:
            return {'FINISHED'}
        context.area.tag_redraw()
        if event.type == 'MOUSEMOVE':
            self.menu.mouse_move(context, event)
        elif event.type == 'WHEELUPMOUSE' or \
                (event.type == 'UP_ARROW' and event.value == 'PRESS'):
            self.menu.scroll_up(context, event)
        elif event.type == 'WHEELDOWNMOUSE' or \
                (event.type == 'DOWN_ARROW' and event.value == 'PRESS'):
            self.menu.scroll_down(context, event)
        elif event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
            preset = self.menu.mouse_press(context, event)
            if preset is not None:
                self.exit(context)
                po = self.preset_operator.split(".")
                op = getattr(getattr(bpy.ops, po[0]), po[1])
                if self.preset_operator == 'script.execute_preset':
                    # call from preset menu
                    # ensure right active_object class
                    d = getattr(bpy.types, self.preset_subdir).datablock(context.active_object)
                    if d is not None:
                        d.auto_update = False
                        # print("Archipack execute_preset loading auto_update:%s" % d.auto_update)
                        op('INVOKE_DEFAULT', filepath=preset, menu_idname=self.bl_idname)
                        # print("Archipack execute_preset loaded  auto_update: %s" % d.auto_update)
                        d.auto_update = True
                else:
                    # call draw operator
                    if op.poll():
                        op('INVOKE_DEFAULT', filepath=preset)
                    else:
                        print("Poll failed")
                return {'FINISHED'}
        elif event.ascii or (
                event.type in self.menu.keyboard_type and
                event.value == 'RELEASE'):
            self.menu.keyboard_entry(context, event)
        elif event.type in {'RIGHTMOUSE', 'ESC'}:
            self.exit(context)
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type == 'VIEW_3D':

            # with alt pressed on invoke, will bypass menu operator and
            # call preset_operator
            # allow start drawing linked copy of active object
            if event.alt or event.ctrl:
                po = self.preset_operator.split(".")
                op = getattr(getattr(bpy.ops, po[0]), po[1])
                d = context.active_object.data

                if d is not None and self.preset_subdir in d and op.poll():
                    op('INVOKE_DEFAULT')
                else:
                    self.report({'WARNING'}, "Active object must be a " + self.preset_subdir.split("_")[1].capitalize())
                    return {'CANCELLED'}
                return {'FINISHED'}

            self.menu = PresetMenu(context, self.preset_subdir)

            # the arguments we pass the the callback
            args = (self, context)
            # Add the region OpenGL drawing callback
            # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
            self._handle = bpy.types.SpaceView3D.draw_handler_add(self.draw_handler, args, 'WINDOW', 'POST_PIXEL')
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot show preset flinger")
            return {'CANCELLED'}


class ArchipackPreset(AddPresetBase):

    @classmethod
    def poll(cls, context):
        o = context.active_object
        return o is not None and \
            o.data is not None and \
            "archipack_" + cls.__name__[13:-7] in o.data

    @property
    def preset_subdir(self):
        return "archipack_" + self.__class__.__name__[13:-7]

    @property
    def blacklist(self):
        """
            properties black list for presets
            may override on addon basis
        """
        return []

    @property
    def preset_values(self):
        blacklist = self.blacklist
        blacklist.extend(bpy.types.Mesh.bl_rna.properties.keys())
        d = getattr(bpy.context.active_object.data, self.preset_subdir)[0]
        props = d.rna_type.bl_rna.properties.items()
        ret = []
        for prop_id, prop in props:
            if prop_id not in blacklist:
                if not (prop.is_hidden or prop.is_skip_save):
                    ret.append("d.%s" % prop_id)
        ret.sort()
        return ret

    @property
    def preset_defines(self):
        o = bpy.context.active_object
        m = o.archipack_material[0]
        return [
            "d = bpy.context.active_object.data." + self.preset_subdir + "[0]",
            "bpy.ops.archipack.material(category='" + m.category + "', material='" + m.material + "')"
        ]

    def pre_cb(self, context):
        return

    def remove(self, context, filepath):
        # remove preset
        os.remove(filepath)
        # remove thumb
        os.remove(filepath[:-3] + ".png")

    def background_render(self, context, cls, preset):
        print("bg render")
        generator = os.path.dirname(os.path.realpath(__file__)) + os.path.sep + "archipack_thumbs.py"
        # Run external instance of blender like the original thumbnail generator.
        cmd = [
            bpy.app.binary_path,
            "--background",
            "-noaudio",
            "--python", generator,
            "--",
            "cls:" + cls,
            "preset:" + preset
            ]
        print(repr(cmd))

        subprocess.Popen(cmd)

    def post_cb(self, context):

        if not self.remove_active:

            name = self.name.strip()
            if not name:
                return

            filename = self.as_filename(name)
            target_path = os.path.join("presets", self.preset_subdir)
            target_path = bpy.utils.user_resource('SCRIPTS',
                                                  target_path,
                                                  create=True)

            preset = os.path.join(target_path, filename) + ".py"
            cls = self.preset_subdir[10:]
            print("post cb cls:%s preset:%s" % (cls, preset))
            self.background_render(context, cls, preset)

            return
