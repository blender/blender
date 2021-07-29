# gpl: authors Carlos Padial, Turi Scandurra

import bpy
import os
from bpy.props import IntProperty
from bpy.types import (
        Operator,
        Panel,
        )
from . import functions


proxy_qualities = [
        ("1", "25%", ""), ("2", "50%", ""),
        ("3", "75%", ""), ("4", "100%", ""),
        ("5", "none", "")
        ]


# functions
def createdatamosh(context, strip):
    preferences = context.user_preferences
    prefs = preferences.addons[__package__].preferences

    fileinput = bpy.path.abspath(strip.filepath)
    fileoutput = fileinput.rpartition(".")[0] + "_datamosh.avi"

    if prefs.all_keyframes:
        command = "datamosh '{}' -a -o '{}'".format(fileinput, fileoutput)
    else:
        command = "datamosh '{}' -o '{}'".format(fileinput, fileoutput)
    print(command)
    os.system(command)
    return fileoutput


def createavi(context, strip):
    fileinput = bpy.path.abspath(strip.filepath)
    fileoutput = fileinput.rpartition(".")[0] + "_.avi"

    command = "ffmpeg -i '{}' -vcodec copy '{}'".format(fileinput, fileoutput)

    print(command)
    os.system(command)

    return fileoutput


def createavimjpeg(context, strip):
    fileinput = bpy.path.abspath(strip.filepath)
    fileoutput = fileinput.rpartition(".")[0] + "_mjpeg.avi"

    command = "ffmpeg -i '{}' -vcodec mjpeg -q:v 1 '{}'".format(fileinput, fileoutput)

    print(command)
    os.system(command)

    return fileoutput


# classes
class CreateAvi(Operator):
    bl_idname = "sequencer.createavi"
    bl_label = "Create avi file"
    bl_description = "Create an avi output file"
    bl_options = {'REGISTER', 'UNDO'}

    size = IntProperty(
            name="proxysize",
            default=1
            )

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('MOVIE')
        else:
            return False

    def execute(self, context):
        strips = functions.get_selected_strips(context)

        for strip in strips:
            # deselect all other strips
            for i in strips:
                i.select = False
            # select current strip
            strip.select = True
            if strip.type == "MOVIE":
                if self.size == 1:
                    fileoutput = createavi(context, strip)
                elif self.size == 2:
                    fileoutput = createavimjpeg(context, strip)
                strip.filepath = fileoutput

        # select all strips again
        for strip in strips:
            try:
                strip.select = True
            except ReferenceError:
                pass

        bpy.ops.sequencer.reload()

        return {'FINISHED'}


class CreateDatamosh(Operator):
    bl_idname = "sequencer.createdatamosh"
    bl_label = "Create Datamosh"
    bl_description = "Create Datamosh"

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('MOVIE')
        else:
            return False

    def execute(self, context):
        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences
        strips = functions.get_selected_strips(context)

        for strip in strips:
            # deselect all other strips
            for i in strips:
                i.select = False
            # select current strip
            strip.select = True
            if strip.type == "MOVIE":
                fileoutput = createdatamosh(context, strip)
                if prefs.load_glitch:
                    strip.filepath = fileoutput

        # select all strips again
        for strip in strips:
            try:
                strip.select = True
            except ReferenceError:
                pass

        bpy.ops.sequencer.reload()

        return {'FINISHED'}


class CreateGlitchToolPanel(Panel):
    bl_label = "Glitch Tools"
    bl_idname = "OBJECT_PT_GlitchTool"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(self, context):
        if context.space_data.view_type in {'SEQUENCER',
                                            'SEQUENCER_PREVIEW'}:
            strip = functions.act_strip(context)
            scn = context.scene
            preferences = context.user_preferences
            prefs = preferences.addons[__package__].preferences
            if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
                if prefs.use_glitch_panel:
                    return strip.type in ('MOVIE')
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="GAME")

    def draw(self, context):

        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        layout = self.layout

        layout.operator("sequencer.createavi", text="Create avi (same codec)")
        layout.operator("sequencer.createavi", text="Create avi (mjpeg)").size = 2

        layout.prop(prefs, "all_keyframes")
        layout.prop(prefs, "load_glitch")

        layout.operator("sequencer.createdatamosh")
