# gpl: authors Carlos Padial, Turi Scandurra

import bpy
import os
from bpy.types import (
        Operator,
        Panel,
        )
import subprocess
from . import functions


proxy_qualities = [
        ("1", "25%", ""), ("2", "50%", ""),
        ("3", "75%", ""), ("4", "100%", "")]

#
#  ls *.sh | parallel -j 8 sh {}
#


# functions
def createsyncfile(filename):
    if not os.path.isfile(bpy.path.abspath(filename)):
        f = open(bpy.path.abspath(filename), "w")
        data = []

        try:
            f.writelines(data)  # Write a sequence of strings to a file
        finally:
            f.close()


def readsyncfile(filename):
    try:
        file = open(bpy.path.abspath(filename))
        data = file.readlines()
        file.close()

        return data

    except IOError:
        pass


def writesyncfile(filename, data):
    try:
        f = open(bpy.path.abspath(filename), "w")
        try:
            for line in data:
                f.writelines(line)  # Write a sequence of strings to a file
        finally:
            f.close()

    except IOError:
        pass


# classes

class ExtractWavOperator(Operator):
    bl_idname = "sequencer.extract_wav_operator"
    bl_label = "Extract Wav from movie strip Operator"
    bl_description = "Use ffmpeg to extract audio from video and import it synced"

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in
               {'SEQUENCER', 'SEQUENCER_PREVIEW'})

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
        audio_dir = preferences.addons[__package__].preferences.audio_dir

        functions.create_folder(bpy.path.abspath(audio_dir))

        for strip in context.selected_editable_sequences:

            # get filename
            if strip.type == "MOVIE":
                filename = bpy.path.abspath(strip.filepath)
                newfilename = bpy.path.abspath(strip.filepath).rpartition(
                    "/")[2]
                fileoutput = os.path.join(
                                    bpy.path.abspath(audio_dir),
                                    newfilename) + ".wav"

            # check for wav existing file
            if not os.path.isfile(fileoutput):
                # if not, extract the file
                extract_audio = "ffmpeg -i '{}' -acodec pcm_s16le -ac 2 {}".\
                format(filename, fileoutput)
                print(extract_audio)
                os.system(extract_audio)
            else:
                print("The audio File exists")

            if strip.type == "MOVIE":
                # import the file and trim in the same way the original
                bpy.ops.sequencer.sound_strip_add(
                        filepath=fileoutput,
                        frame_start=strip.frame_start,
                        channel=strip.channel + 1,
                        replace_sel=True, overlap=False,
                        cache=False
                        )

                # Update scene
                context.scene.update()

                newstrip = context.scene.sequence_editor.active_strip

                # deselect all other strips
                for i in context.selected_editable_sequences:
                    if i.name != newstrip.name:
                        i.select = False

                # Update scene
                context.scene.update()

                # Match the original clip's length
                newstrip.frame_start = strip.frame_start - strip.animation_offset_start

                functions.triminout(newstrip,
                            strip.frame_start + strip.frame_offset_start,
                            strip.frame_start + strip.frame_offset_start +
                            strip.frame_final_duration)

        return {'FINISHED'}


class ExternalAudioSetSyncOperator(Operator):
    bl_idname = "sequencer.external_audio_set_sync"
    bl_label = "set sync info"
    bl_description = ("Get sync info from selected audio and video strip "
                      "and store it into a text file")

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in
               {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        if cls.has_sequencer(context):
            if len(context.selected_editable_sequences) == 2:
                types = []
                for i in context.selected_editable_sequences:
                    types.append(i.type)
                if 'MOVIE' and 'SOUND' in types:
                    return True
                else:
                    return False

    def execute(self, context):

        preferences = context.user_preferences
        filename = preferences.addons[__package__].preferences.audio_external_filename

        for strip in context.selected_editable_sequences:
            if strip.type == "MOVIE":
                moviestrip = strip
            elif strip.type == "SOUND":
                soundstrip = strip

        offset = str(moviestrip.frame_start - soundstrip.frame_start)

        data1 = readsyncfile(filename)
        data2 = []
        newline = moviestrip.filepath + " " + soundstrip.filepath + " " + offset + "\n"

        if data1 is not None:
            repeated = False
            for line in data1:
                if line.split()[0] == moviestrip.filepath and line.split()[1] == soundstrip.filepath:
                    data2.append(newline)
                    repeated = True
                else:
                    data2.append(line)
            if not repeated:
                data2.append(newline)
        else:
            data2.append(newline)

        createsyncfile(filename)
        writesyncfile(filename, data2)

        return {'FINISHED'}


class ExternalAudioReloadOperator(Operator):
    bl_idname = "sequencer.external_audio_reload"
    bl_label = "Reload External audio"
    bl_description = ("Reload external audio synced to selected movie strip "
                      "acording to info from a text file")

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in
               {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        if cls.has_sequencer(context):
            if len(context.selected_editable_sequences) == 1:
                if context.selected_editable_sequences[0].type == 'MOVIE':
                    return True
                else:
                    return False

    def execute(self, context):
        preferences = context.user_preferences
        filename = preferences.addons[__package__].preferences.audio_external_filename

        data = readsyncfile(filename)

        for strip in context.selected_editable_sequences:
            sounds = []

            for line in data:
                if line.split()[0] == strip.filepath:
                    moviefile = bpy.path.abspath(line.split()[0])
                    soundfile = bpy.path.abspath(line.split()[1])
                    offset = int(line.split()[2])
                    sounds.append((soundfile, offset))

            for soundfile, offset in sounds:
                print(soundfile, offset)
                print(strip.filepath)
                # find start frame for sound strip (using offset from file)
                sound_frame_start = strip.frame_start - strip.animation_offset_start - offset

                # import the file and trim in the same way the original
                bpy.ops.sequencer.sound_strip_add(
                        filepath=soundfile,
                        frame_start=sound_frame_start,
                        channel=strip.channel + 1,
                        replace_sel=True, overlap=False,
                        cache=False
                        )

                # Update scene
                context.scene.update()

                newstrip = context.scene.sequence_editor.active_strip

                # deselect all other strips
                for i in context.selected_editable_sequences:
                    if i.name != newstrip.name:
                        i.select = False

                # Update scene
                context.scene.update()

                # trim sound strip like original one
                functions.triminout(newstrip,
                            strip.frame_start + strip.frame_offset_start,
                            strip.frame_start + strip.frame_offset_start +
                            strip.frame_final_duration
                            )

        return {'FINISHED'}


class AudioToolPanel(Panel):
    bl_label = "Audio Tools"
    bl_idname = "OBJECT_PT_AudioTool"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(self, context):
        if context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            scn = context.scene
            preferences = context.user_preferences
            prefs = preferences.addons[__package__].preferences
            if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
                if prefs.use_audio_tools:
                    return True
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="PLAY_AUDIO")

    def draw(self, context):
        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        strip = functions.act_strip(context)

        if strip.type == "MOVIE":
            layout = self.layout
            layout.prop(prefs, "audio_dir", text="Path for Audio files")

            layout.operator("sequencer.extract_wav_operator", text="Extract Wav")

            layout = self.layout
            layout.prop(prefs, "audio_scripts")

            if prefs.audio_scripts:
                layout = self.layout
                layout.prop(prefs, "audio_scripts_path", text="Path for scripts")

            layout = self.layout
            layout.prop(prefs, "audio_use_external_links", text="External Audio sync")

            if prefs.audio_use_external_links:
                layout = self.layout
                layout.prop(prefs, "audio_external_filename", text="Sync data")

                row = layout.row(align=True)
                row.operator("sequencer.external_audio_set_sync", text="Set sync")
                row.operator("sequencer.external_audio_reload", text="Reload Audio")

        layout = self.layout

        row = layout.row()
        row.prop(prefs, "metertype", text="")
        row.operator("sequencer.openmeterbridge",
                     text="Launch Audio Meter", icon="SOUND")


class OpenMeterbridgeOperator(Operator):
    bl_idname = "sequencer.openmeterbridge"
    bl_label = "External VU meter"
    bl_description = "Open external VU meter to work with Jack"

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        if cls.has_sequencer(context):
            if len(context.selected_editable_sequences) == 1:
                return True

    def execute(self, context):
        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        command = "meterbridge -t {} 'PulseAudio JACK Sink:front-left' " \
                  "'PulseAudio JACK Sink:front-right' &".format(prefs.metertype.lower())
        p = subprocess.Popen(command, stdout=subprocess.PIPE, shell=True)

        return {'FINISHED'}
