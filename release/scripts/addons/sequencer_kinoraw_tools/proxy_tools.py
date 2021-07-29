# gpl: authors Carlos Padial, Turi Scandurra

import bpy
import os
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import IntProperty
import subprocess
from . import functions


proxy_qualities = [
        ("1", "25%", ""), ("2", "50%", ""),
        ("3", "75%", ""), ("4", "100%", ""),
        ("5", "none", "")
        ]


# Functions
def setup_proxy(context, strip, size):
    preferences = context.user_preferences
    prefs = preferences.addons[__package__].preferences

    # set up proxy settings
    strip.use_proxy = True

    if prefs.use_bi_custom_directory:
        strip.use_proxy_custom_directory = True
        filename = strip.filepath.rpartition("/")[2].rpartition(".")[0]
        strip.proxy.directory = bpy.path.relpath(prefs.proxy_dir + filename)
    else:
        strip.use_proxy_custom_directory = False

    if strip.use_proxy_custom_file is True:
        strip.use_proxy_custom_file = False

    strip.proxy.quality = prefs.quality
    strip.proxy.timecode = prefs.timecode

    if size == 5:
        strip.use_proxy = False
        strip.proxy.build_25 = False
        strip.proxy.build_50 = False
        strip.proxy.build_75 = False
        strip.proxy.build_100 = False

    else:
        proxysuffix = proxy_qualities[size - 1][1].split("%")[0]

        if (proxysuffix == "25"):
            strip.proxy.build_25 = True
        if (proxysuffix == "50"):
            strip.proxy.build_50 = True
        if (proxysuffix == "75"):
            strip.proxy.build_75 = True
        if (proxysuffix == "100"):
            strip.proxy.build_100 = True

    return {"FINISHED"}


def create_proxy(context, strip, size, res):
    # calculate proxy resolution
    div = 4 / size
    newres = (int(int(res[0]) / div), int(int(res[1]) / div))

    preferences = context.user_preferences
    proxy_dir = preferences.addons[__package__].preferences.proxy_dir
    scripts = preferences.addons[__package__].preferences.proxy_scripts
    ffmpeg_command = preferences.addons[__package__].preferences.ffmpeg_command

    functions.create_folder(proxy_dir)

    if scripts:
        commands = []

    # get filename
    if strip.type == "MOVIE":
        filename = bpy.path.abspath(strip.filepath)
        proxysuffix = proxy_qualities[size - 1][1].split("%")[0]
        proxy_dir = bpy.path.abspath(proxy_dir)
        newfilename = os.path.join(proxy_dir, filename.rpartition("/")[2])
        fileoutput = newfilename.rpartition(".")[0] + "-" + proxysuffix + ".avi"

        # default value for ffmpeg_command = "fmpeg -i {} -vcodec mjpeg -qv 1 -s {}x{} -y {}"

        command = ffmpeg_command.format(filename, newres[0], newres[1], fileoutput)
        print(command)

        if scripts:
            commands.append(command)
        else:
            # check for existing file
            if not os.path.isfile(fileoutput):
                subprocess.call(command, shell=True)
            else:
                print("File already exists")

        # set up proxy settings
        strip.use_proxy = True
        try:
            strip.use_proxy_custom_file = True
            strip.proxy.filepath = bpy.path.relpath(fileoutput)
        except:
            pass

        if (proxysuffix == "25"):
            strip.proxy.build_25 = True
        if (proxysuffix == "50"):
            strip.proxy.build_50 = True
        if (proxysuffix == "75"):
            strip.proxy.build_75 = True
        if (proxysuffix == "100"):
            strip.proxy.build_100 = True

    if scripts:
        return commands
    else:
        return None


def create_proxy_scripts(scripts_dir, commands, strip_name=None):

    functions.create_folder(bpy.path.abspath(scripts_dir))
    for i in commands:
        # print(i)
        filename = "{}/proxy_script_{}.sh".format(scripts_dir, strip_name)
        text_file = open(bpy.path.abspath(filename), "w")
        # print(filename)
        text_file.write(i)
        text_file.close()


# classes
class CreateProxyOperator(Operator):
    bl_idname = "sequencer.create_proxy_operator"
    bl_label = "Create Proxy"
    bl_description = ("Use ffmpeg to create a proxy from video\n"
                      "and setup proxies for selected strip")
    bl_options = {'REGISTER', 'UNDO'}

    size = IntProperty(
            name="Proxy Size",
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
        preferences = context.user_preferences
        proxy_scripts_path = preferences.addons[__package__].preferences.proxy_scripts_path

        for strip in context.selected_editable_sequences:
            # get resolution from active strip
            try:
                bpy.ops.sequencerextra.read_exif()
            except:
                pass

            sce = context.scene
            try:
                res = sce['metadata'][0]['Composite:ImageSize'].split("x")
            except:
                res = (sce.render.resolution_x, sce.render.resolution_y)

            commands = create_proxy(context, strip, self.size, res)

            if commands is None:
                # Update scene
                context.scene.update()
                newstrip = context.scene.sequence_editor.active_strip

                # deselect all other strips
                for i in context.selected_editable_sequences:
                    if i.name != newstrip.name:
                        i.select = False

                # Update scene
                context.scene.update()
            else:
                create_proxy_scripts(proxy_scripts_path, commands, strip.name)

        return {'FINISHED'}


class CreateBIProxyOperator(Operator):
    bl_idname = "sequencer.create_bi_proxy_operator"
    bl_label = "Create proxy with Blender Internal"
    bl_description = "Use BI system to create a proxy"
    bl_options = {'REGISTER', 'UNDO'}

    size = IntProperty(
            name="Proxy Size",
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
        try:
            strips = functions.get_selected_strips(context)

            for strip in strips:
                # deselect all other strips
                for i in strips:
                    i.select = False
                # select current strip
                strip.select = True
                if strip.type == "MOVIE":
                    setup_proxy(context, strip, self.size)
        except Exception as e:
            functions.error_handlers(
                        self,
                        "sequencer.create_bi_proxy_operator", e,
                        "Create proxy with blender internal"
                        )
            return {"CANCELLED"}

        # select all strips again
        for strip in strips:
            try:
                strip.select = True
            except ReferenceError:
                pass
        bpy.ops.sequencer.reload()

        return {'FINISHED'}


class CreateProxyToolPanel(Panel):
    bl_label = "Proxy Tools"
    bl_idname = "OBJECT_PT_ProxyTool"
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
                if prefs.use_proxy_tools:
                    return strip.type in ('MOVIE')
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="AUTO")

    def draw(self, context):

        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        layout = self.layout
        layout.prop(prefs, "use_internal_proxy", text="Use BI proxy builder")

        strip = functions.act_strip(context)

        if prefs.use_internal_proxy:
            layout = self.layout
            row = layout.row(align=True)
            row.prop(prefs, "use_bi_custom_directory")

            if prefs.use_bi_custom_directory:
                row.prop(prefs, "proxy_dir", text="")
                filename = strip.filepath.rpartition("/")[2].rpartition(".")[0]
                layout.label("sample dir: //" + bpy.path.abspath(prefs.proxy_dir + filename))

            layout = self.layout
            col = layout.column()
            col.label(text="Build JPEG quality")
            col.prop(prefs, "quality")

            if strip.type == 'MOVIE':
                col = layout.column()
                col.label(text="Use timecode index:")

                col.prop(prefs, "timecode")

            layout = self.layout
            layout.label("Setup and create BI proxy:")
            row = layout.row(align=True)

            for i in range(4):
                proxysuffix = proxy_qualities[i][1]
                row.operator("sequencer.create_bi_proxy_operator",
                             text=proxysuffix).size = i + 1

            layout = self.layout
            layout.operator("sequencer.create_bi_proxy_operator",
                            text="Clear proxy sizes").size = 5

        else:
            layout = self.layout
            layout.prop(prefs, "proxy_dir", text="Path for proxies")

            layout = self.layout
            layout.label("Create and import proxy from clip:")
            row = layout.row(align=True)

            layout = self.layout
            layout.prop(prefs, "ffmpeg_command", text="command")

            layout.label("{} = filename, with, height, fileoutput")
            label = prefs.ffmpeg_command.format("filename", "with", "height", "fileoutput")
            layout.label(label)

            for i in range(4):
                proxysuffix = proxy_qualities[i][1]
                row.operator("sequencer.create_proxy_operator",
                             text=proxysuffix).size = i + 1

            layout = self.layout
            layout.prop(prefs, "proxy_scripts")

            if prefs.proxy_scripts:
                layout = self.layout
                layout.prop(prefs, "proxy_scripts_path", text="Path for scripts")

        layout = self.layout
        box = layout.box()
        box.prop(context.space_data, "proxy_render_size")
        box.operator("sequencer.rebuild_proxy", text="Rebuild Proxies and TC")
