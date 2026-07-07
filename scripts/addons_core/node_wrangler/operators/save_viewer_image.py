# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import StringProperty, EnumProperty
from bpy_extras.io_utils import ExportHelper
from bpy.app.translations import pgettext_rpt as rpt_

from ..utils.nodes import (
    nw_check,
    nw_check_space_type,
    nw_check_viewer_node,
    get_viewer_image,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_save_viewer_image(Operator, ExportHelper):
    """Save the current viewer node to an image file"""
    bl_idname = "node.nw_save_viewer"
    bl_label = "Save Viewer Image"

    filepath: StringProperty(
        subtype="FILE_PATH"
    )
    filename_ext: EnumProperty(
        name="Format",
        description="Choose the file format to save to",
        items=(
            ('.bmp', "BMP", ""),
            ('.rgb', 'IRIS', ""),
            ('.png', 'PNG', ""),
            ('.jpg', 'JPEG', ""),
            ('.jp2', 'JPEG2000', ""),
            ('.tga', 'TARGA', ""),
            ('.cin', 'CINEON', ""),
            ('.dpx', 'DPX', ""),
            ('.exr', 'OPEN_EXR', ""),
            ('.hdr', 'HDR', ""),
            ('.tif', 'TIFF', ""),
            ('.webp', 'WEBP', ""),
        ),
        default='.png',
    )

    @classmethod
    def poll(cls, context):
        return (nw_check(cls, context)
                and nw_check_space_type(cls, context, {'CompositorNodeTree'})
                and nw_check_viewer_node(cls))

    def execute(self, context):
        fp = self.filepath
        if not fp:
            return {'CANCELLED'}

        formats = {
            '.bmp': 'BMP',
            '.rgb': 'IRIS',
            '.png': 'PNG',
            '.jpg': 'JPEG',
            '.jpeg': 'JPEG',
            '.jp2': 'JPEG2000',
            '.tga': 'TARGA',
            '.cin': 'CINEON',
            '.dpx': 'DPX',
            '.exr': 'OPEN_EXR',
            '.hdr': 'HDR',
            '.tiff': 'TIFF',
            '.tif': 'TIFF',
            '.webp': 'WEBP',
        }
        image_settings = context.scene.render.image_settings
        old_media_type = image_settings.media_type
        old_file_format = image_settings.file_format
        image_settings.media_type = 'IMAGE'
        image_settings.file_format = formats[self.filename_ext]

        try:
            get_viewer_image().save_render(fp)
        except RuntimeError as e:
            self.report({'ERROR'}, rpt_("Could not write image: {}").format(e))

        image_settings.media_type = old_media_type
        image_settings.file_format = old_file_format
        return {'FINISHED'}
