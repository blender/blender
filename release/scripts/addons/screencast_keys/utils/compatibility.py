# <pep8-80 compliant>

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

__author__ = "Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "5.2"
__date__ = "17 Nov 2018"


import bpy
import bgl
import blf


def check_version(major, minor, _):
    """
    Check blender version
    """

    if bpy.app.version[0] == major and bpy.app.version[1] == minor:
        return 0
    if bpy.app.version[0] > major:
        return 1
    if bpy.app.version[1] > minor:
        return 1
    return -1


def make_annotations(cls):
    if check_version(2, 80, 0) < 0:
        return cls

    # make annotation from attributes
    props = {k: v for k, v in cls.__dict__.items() if isinstance(v, tuple)}
    if props:
        if '__annotations__' not in cls.__dict__:
            setattr(cls, '__annotations__', {})
        annotations = cls.__dict__['__annotations__']
        for k, v in props.items():
            annotations[k] = v
            delattr(cls, k)

    return cls


class ChangeRegionType:
    def __init__(self, *_, **kwargs):
        self.region_type = kwargs.get('region_type', False)

    def __call__(self, cls):
        if check_version(2, 80, 0) >= 0:
            return cls

        cls.bl_region_type = self.region_type

        return cls


def matmul(m1, m2):
    if check_version(2, 80, 0) < 0:
        return m1 * m2

    return m1 @ m2


def layout_split(layout, factor=0.0, align=False):
    if check_version(2, 80, 0) < 0:
        return layout.split(percentage=factor, align=align)

    return layout.split(factor=factor, align=align)


def get_user_preferences(context):
    if hasattr(context, "user_preferences"):
        return context.user_preferences

    return context.preferences


def get_object_select(obj):
    if check_version(2, 80, 0) < 0:
        return obj.select

    return obj.select_get()


def set_active_object(obj):
    if check_version(2, 80, 0) < 0:
        bpy.context.scene.objects.active = obj
    else:
        bpy.context.view_layer.objects.active = obj


def get_active_object(context):
    if check_version(2, 80, 0) < 0:
        return context.scene.active_object
    else:
        return context.active_object


def object_has_uv_layers(obj):
    if check_version(2, 80, 0) < 0:
        return hasattr(obj.data, "uv_textures")
    else:
        return hasattr(obj.data, "uv_layers")


def get_object_uv_layers(obj):
    if check_version(2, 80, 0) < 0:
        return obj.data.uv_textures
    else:
        return obj.data.uv_layers


def icon(icon):
    if icon == 'IMAGE':
        if check_version(2, 80, 0) < 0:
            return 'IMAGE_COL'

    return icon


def set_blf_font_color(font_id, r, g, b, a):
    if check_version(2, 80, 0) >= 0:
        blf.color(font_id, r, g, b, a)
    else:
        bgl.glColor4f(r, g, b, a)


def set_blf_blur(font_id, radius):
    if check_version(2, 80, 0) < 0:
        blf.blur(font_id, radius)


def get_all_space_types():
    if check_version(2, 80, 0) >= 0:
        return {
            'VIEW_3D': bpy.types.SpaceView3D,
            'CLIP_EDITOR': bpy.types.SpaceClipEditor,
            'CONSOLE': bpy.types.SpaceConsole,
            'DOPESHEET_EDITOR': bpy.types.SpaceDopeSheetEditor,
            'FILE_BROWSER': bpy.types.SpaceFileBrowser,
            'GRAPH_EDITOR': bpy.types.SpaceGraphEditor,
            'IMAGE_EDITOR': bpy.types.SpaceImageEditor,
            'INFO': bpy.types.SpaceInfo,
            'NLA_EDITOR': bpy.types.SpaceNLA,
            'NODE_EDITOR': bpy.types.SpaceNodeEditor,
            'OUTLINER': bpy.types.SpaceOutliner,
            'PROPERTIES': bpy.types.SpaceProperties,
            'SEQUENCE_EDITOR': bpy.types.SpaceSequenceEditor,
            'TEXT_EDITOR': bpy.types.SpaceTextEditor,
            'PREFERENCES': bpy.types.SpacePreferences,
        }
    else:
        return {
            'VIEW_3D': bpy.types.SpaceView3D,
            'TIMELINE': bpy.types.SpaceTimeline,
            'GRAPH_EDITOR': bpy.types.SpaceGraphEditor,
            'DOPESHEET_EDITOR': bpy.types.SpaceDopeSheetEditor,
            'NLA_EDITOR': bpy.types.SpaceNLA,
            'IMAGE_EDITOR': bpy.types.SpaceImageEditor,
            'SEQUENCE_EDITOR': bpy.types.SpaceSequenceEditor,
            'CLIP_EDITOR': bpy.types.SpaceClipEditor,
            'TEXT_EDITOR': bpy.types.SpaceTextEditor,
            'NODE_EDITOR': bpy.types.SpaceNodeEditor,
            'LOGIC_EDITOR': bpy.types.SpaceLogicEditor,
            'PROPERTIES': bpy.types.SpaceProperties,
            'OUTLINER': bpy.types.SpaceOutliner,
            'PREFERENCES': bpy.types.SpaceUserPreferences,
            'INFO': bpy.types.SpaceInfo,
            'FILE_BROWSER': bpy.types.SpaceFileBrowser,
            'CONSOLE': bpy.types.SpaceConsole,
        }
