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

import bpy
import os
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import (
        EnumProperty,
        BoolProperty,
        )
from . import functions
from . import exiftool


class Sequencer_Extra_RecursiveLoader(Operator):
    bl_idname = "sequencerextra.recursiveload"
    bl_label = "Recursive Load"
    bl_options = {'REGISTER', 'UNDO'}

    recursive = BoolProperty(
            name="Recursive",
            description="Load in recursive folders",
            default=False
            )
    recursive_select_by_extension = BoolProperty(
            name="Select by extension",
            description="Load only clips with selected extension",
            default=False
            )
    ext = EnumProperty(
            items=functions.movieextdict,
            name="Extension",
            default='3'
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return (scn.sequence_editor)
        else:
            return False

    def invoke(self, context, event):
        scn = context.scene
        try:
            self.recursive = scn.kr_recursive
            self.recursive_select_by_extension = scn.kr_recursive_select_by_extension
            self.ext = scn.kr_default_ext
        except AttributeError:
            functions.initSceneProperties(context)
            self.recursive = scn.kr_recursive
            self.recursive_select_by_extension = scn.kr_recursive_select_by_extension
            self.ext = scn.kr_default_ext

        return context.window_manager.invoke_props_dialog(self)

    def loader(self, context, filelist):
        scn = context.scene
        if filelist:
            for i in filelist:
                functions.setpathinbrowser(context, i[0], i[1])
                try:
                    bpy.ops.sequencerextra.placefromfilebrowser()
                except:
                    print("Error loading file (recursive loader error): ", i[1])
                    functions.add_marker(context, i[1], scn.frame_current)
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file ')
                    pass

    def execute(self, context):
        scn = context.scene
        if self.recursive is True:
            # recursive
            self.loader(
                    context, functions.sortlist(
                    functions.recursive(context, self.recursive_select_by_extension,
                    self.ext)
                        )
                    )
        else:
            # non recursive
            self.loader(
                    context, functions.sortlist(functions.onefolder(
                    context, self.recursive_select_by_extension,
                    self.ext)
                        )
                    )
        try:
            scn.kr_recursive = self.recursive
            scn.kr_recursive_select_by_extension = self.recursive_select_by_extension
            scn.kr_default_ext = self.ext
        except AttributeError:
            functions.initSceneProperties(context)
            self.recursive = scn.kr_recursive
            self.recursive_select_by_extension = scn.kr_recursive_select_by_extension
            self.ext = scn.kr_default_ext

        return {'FINISHED'}


# Read exif data
# load exifdata from strip to scene['metadata'] property

class Sequencer_Extra_ReadExifData(Operator):
    bl_label = "Read EXIF Data"
    bl_idname = "sequencerextra.read_exif"
    bl_description = "Load exifdata from strip to metadata property in scene"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return scn.sequence_editor.active_strip.type in ('IMAGE', 'MOVIE')
        else:
            return False

    def execute(self, context):
        try:
            exiftool.ExifTool().start()
        except:
            self.report({'ERROR_INVALID_INPUT'}, "exiftool not found in PATH")

            return {'CANCELLED'}

        def getexifdata(strip):

            def getexifvalues_image(lista):
                metadata = []
                with exiftool.ExifTool() as et:
                    try:
                        metadata = et.get_metadata_batch(lista)
                    except UnicodeDecodeError as Err:
                        print(Err)
                # print(metadata[0])
                print(len(metadata))
                return metadata

            def getexifvalues_movie(path):
                metadata = []
                with exiftool.ExifTool() as et:
                    try:
                        metadata = et.get_metadata_batch([path])
                    except UnicodeDecodeError as Err:
                        print(Err)
                print(metadata[0])
                print(len(metadata))
                return metadata

            def getlist(lista):
                for root, dirs, files in os.walk(path):
                    for f in files:
                        if "." + f.rpartition(".")[2].lower() in \
                                functions.imb_ext_image:
                            lista.append(f)
                        """
                        if "." + f.rpartition(".")[2] in imb_ext_movie:
                            lista.append(f)
                        """
                strip.elements
                lista.sort()
                return lista

            if strip.type == "IMAGE":
                path = bpy.path.abspath(strip.directory)
                os.chdir(path)
                # get a list of files
                lista = []
                for i in strip.elements:
                    lista.append(i.filename)
                print(lista)
                return getexifvalues_image(lista)

            if strip.type == "MOVIE":
                path = bpy.path.abspath(strip.filepath)
                print([path])
                return getexifvalues_movie(path)

        sce = bpy.context.scene
        strip = context.scene.sequence_editor.active_strip
        sce['metadata'] = getexifdata(strip)

        return {'FINISHED'}


# TODO: fix poll to hide when unuseful

class ExifInfoPanel(Panel):
    """Creates a Panel in the Object properties window"""
    bl_label = "EXIF Info Panel"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(self, context):
        if context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            strip = functions.act_strip(context)
            scn = context.scene
            preferences = context.user_preferences
            prefs = preferences.addons[__package__].preferences

            if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
                if prefs.use_exif_panel:
                    return strip.type in ('MOVIE', 'IMAGE')
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="RADIO")

    def draw(self, context):
        layout = self.layout
        sce = context.scene
        row = layout.row()
        row.operator("sequencerextra.read_exif")
        row = layout.row()
        row.label(text="Exif Data", icon='RENDER_REGION')
        row = layout.row()

        try:
            strip = context.scene.sequence_editor.active_strip

            f = strip.frame_start
            frame = sce.frame_current
            try:
                if len(sce['metadata']) == 1:
                    for d in sce['metadata'][0]:
                        split = layout.split(percentage=0.5)
                        col = split.column()
                        row = col.row()
                        col.label(text=d)
                        col = split.column()
                        col.label(str(sce['metadata'][0][d]))
                else:
                    for d in sce['metadata'][frame - f]:
                        split = layout.split(percentage=0.5)
                        col = split.column()
                        row = col.row()
                        col.label(text=d)
                        col = split.column()
                        col.label(str(sce['metadata'][frame - f][d]))

            except (IndexError, KeyError):
                pass
        except AttributeError:
            pass
