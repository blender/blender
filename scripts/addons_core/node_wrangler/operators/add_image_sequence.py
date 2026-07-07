# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty, CollectionProperty, StringProperty, IntProperty
from bpy_extras.io_utils import ImportHelper
from bpy.app.translations import pgettext_rpt as rpt_

from os import path
from glob import glob

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_space_type,
    get_nodes_links,
    node_mid_pt,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_add_image_sequence(Operator, NWBase, ImportHelper):
    """Add an Image Sequence"""
    bl_idname = 'node.nw_add_sequence'
    bl_label = 'Import Image Sequence'
    bl_options = {'REGISTER', 'UNDO'}

    directory: StringProperty(
        subtype="DIR_PATH"
    )
    filename: StringProperty(
        subtype="FILE_NAME"
    )
    files: CollectionProperty(
        type=bpy.types.OperatorFileListElement,
        options={'HIDDEN', 'SKIP_SAVE'}
    )
    relative_path: BoolProperty(
        name='Relative Path',
        description='Set the file path relative to the blend file, when possible',
        default=True
    )
    frame_start: IntProperty(
        name='Start Frame',
        description='Global starting frame of the movie/sequence, assuming first picture has a #1',
        default=1
    )

    @classmethod
    def poll(cls, context):
        return (nw_check(cls, context)
                and nw_check_space_type(cls, context, {'ShaderNodeTree', 'CompositorNodeTree'}))

    def draw(self, context):
        layout = self.layout
        layout.alignment = 'LEFT'

        layout.prop(self, 'relative_path')
        layout.prop(self, 'frame_start')

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        directory = bpy.path.abspath(self.directory)
        filename = self.filename
        files = self.files
        frame_start = self.frame_start
        tree = context.space_data.node_tree

        # DEBUG
        # print ("\nDIR:", directory)
        # print ("FN:", filename)
        # print ("Fs:", list(f.name for f in files), '\n')

        if tree.type == 'SHADER':
            node_type = "ShaderNodeTexImage"
        elif tree.type == 'COMPOSITING':
            node_type = "CompositorNodeImage"
        else:
            self.report({'ERROR'}, "Unsupported node tree type")
            return {'CANCELLED'}

        if not files[0].name and not filename:
            self.report({'ERROR'}, "No file chosen")
            return {'CANCELLED'}
        elif files[0].name and (not filename or not path.exists(directory + filename)):
            # User has selected multiple files without an active one, or the active one is non-existent
            filename = files[0].name

        if not path.exists(directory + filename):
            self.report({'ERROR'}, rpt_("{} does not exist").format(filename))
            return {'CANCELLED'}

        without_ext = '.'.join(filename.split('.')[:-1])

        # if last digit isn't a number, it's not a sequence
        if not without_ext[-1].isdigit():
            self.report({'ERROR'}, rpt_("{} does not seem to be part of a sequence").format(filename))
            return {'CANCELLED'}

        extension = filename.split('.')[-1]
        reverse = without_ext[::-1]  # reverse string

        count_numbers = 0
        for char in reverse:
            if char.isdigit():
                count_numbers += 1
            else:
                break

        without_num = without_ext[:count_numbers * -1]

        files = sorted(glob(directory + without_num + "[0-9]" * count_numbers + "." + extension))

        num_frames = len(files)

        nodes_list = [node for node in nodes]
        if nodes_list:
            nodes_list.sort(key=lambda k: k.location.x)
            xloc = nodes_list[0].location.x - 220  # place new nodes at far left
            yloc = 0
            for node in nodes:
                node.select = False
                yloc += node_mid_pt(node, 'y')
            yloc = yloc / len(nodes)
        else:
            xloc = 0
            yloc = 0

        name_with_hashes = without_num + "#" * count_numbers + '.' + extension

        bpy.ops.node.add_node('INVOKE_DEFAULT', use_transform=True, type=node_type)
        node = nodes.active
        node.label = name_with_hashes

        filepath = directory + (without_ext + '.' + extension)
        if self.relative_path:
            if bpy.data.filepath:
                try:
                    filepath = bpy.path.relpath(filepath)
                except ValueError:
                    pass

        img = bpy.data.images.load(filepath)
        img.source = 'SEQUENCE'
        img.name = name_with_hashes
        node.image = img
        image_user = node.image_user if tree.type == 'SHADER' else node
        # separate the number from the file name of the first  file
        image_user.frame_offset = int(files[0][len(without_num) + len(directory):-1 * (len(extension) + 1)]) - 1
        image_user.frame_duration = num_frames
        image_user.frame_start = frame_start

        return {'FINISHED'}
