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
    "name": "Kinoraw Tools",
    "author": "Carlos Padial, Turi Scandurra",
    "version": (0, 5, 2),
    "blender": (2, 74, 0),
    "location": "Sequencer",
    "description": "Compilation of tools to improve video editing with Blender's VSE",
    "wiki_url": "https://github.com/kinoraw/kinoraw_tools/blob/master/README.md",
    "tracker_url": "https://github.com/kinoraw/kinoraw_tools",
    "support": "COMMUNITY",
    "category": "Sequencer"
    }


if "bpy" in locals():
    import importlib
    importlib.reload(jumptocut)
    importlib.reload(operators_extra_actions)
    importlib.reload(audio_tools)
    importlib.reload(proxy_tools)
    importlib.reload(recursive_loader)
    importlib.reload(eco)
    importlib.reload(random_editor)
    importlib.reload(ui)
    importlib.reload(datamosh)
else:
    from . import jumptocut
    from . import operators_extra_actions
    from . import audio_tools
    from . import proxy_tools
    from . import recursive_loader
    from . import eco
    from . import random_editor
    from . import ui
    from . import datamosh

import bpy
from bpy.types import (
        AddonPreferences,
        )
from bpy.props import (
        IntProperty,
        StringProperty,
        BoolProperty,
        EnumProperty,
        )


class KinorawToolsAddon(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __package__
    bl_option = {'REGISTER'}

    # extra_actions
    kr_show_tools = BoolProperty(
            name="Show tools",
            description="Show extra tools in the panel",
            default=False
            )
    kr_mini_ui = BoolProperty(
            name="Mini UI",
            description="Enable mini UI",
            default=True
            )
    kr_show_info = BoolProperty(
            name="Show info",
            description="Show basic info from selected strip",
            default=False
            )
    kr_show_trim = BoolProperty(
            name="Show trim",
            default=False
            )
    kr_show_modifiers = BoolProperty(
            name="Show modifiers",
            description="Show modifiers from selected strip",
            default=False
            )
    kr_extra_info = BoolProperty(
            name="Show extra info",
            description="Show extra info and settings from selected strip",
            default=False
            )
    # exif
    use_exif_panel = BoolProperty(
            name="Exif info Panel | depends on external programs, see Documentation",
            default=False
            )
    # glitch
    use_glitch_panel = BoolProperty(
            name="Glitch panel | depends on external programs, see Documentation",
            default=False
            )
    all_keyframes = BoolProperty(
            name="Remove all keyframes",
            default=True
            )
    load_glitch = BoolProperty(
            name="Load glitch after conversion > UNSTABLE!!!",
            default=True
            )
    # jump to cut
    use_jumptocut = BoolProperty(
            name="Jump to Cut Panel",
            default=True
            )
    use_io_tools = BoolProperty(
            name="Enable in and out tools in Jump to Cut Panel",
            default=False
            )
    # Proxy Tools
    use_proxy_tools = BoolProperty(
            name="Proxy tools Panel | depends on external programs, see Documentation",
            default=False
            )
    proxy_dir = StringProperty(
            name="Proxy Custom Directory",
            default="//proxies/"
            )
    proxy_scripts_path = StringProperty(
            name="Directory to store proxy scripts",
            default="//proxy_scripts/"
            )
    proxy_scripts = BoolProperty(
            name="Generate ffmpeg scripts",
            default=False
            )
    ffmpeg_command = StringProperty(
            name="Command to generate proxy",
            default='''ffmpeg -i {} -vcodec mjpeg -q:v 10 -s {}x{} -an -y {}'''
            )
    use_internal_proxy = BoolProperty(
            name="Use internal Blender's proxy system",
            default=True
            )
    use_bi_custom_directory = BoolProperty(
            name="Proxy Custom Directory",
            default=True
            )
    quality = IntProperty(
            name="Quality",
            default=90,
            min=0, max=32767
            )
    tc_list = [
            ("NONE", "No TC in use", ""), ("RECORD_RUN", "Record Run", ""),
            ("FREE_RUN", "Free Run", ""), ("FREE_RUN_REC_DATE", "Free Run (rec date)", ""),
            ("RECORD_RUN_NO_GAPS", "Record Run No Gaps", "")
            ]
    timecode = EnumProperty(
            name="Settings Type",
            items=tc_list,
            default="NONE",
            description="Timecode"
            )
    # Audio Tools
    use_audio_tools = BoolProperty(
            name="Audio tools Panel | depends on external programs, see Documentation",
            default=False
            )
    audio_dir = StringProperty(
            name="Path to store extracted audio",
            default="//audio/"
            )
    audio_scripts_path = StringProperty(
            name="Path to store audio scripts",
            default="//audio_scripts/"
            )
    audio_scripts = BoolProperty(
            name="Generate ffmpeg scripts",
            default=False
            )
    #  Audio Tools - external links
    audio_use_external_links = BoolProperty(
            name="Use external audio linked to movie strips",
            default=False
            )
    audio_external_filename = StringProperty(
            name="File to store info about linked audio",
            default="//external_audio_sync_info.txt"
            )
    # audio tools vu-meter
    meterbridge = [
            ("VU", "Classic moving needle VU meter", ""), ("PPM", "PPM meter", ""),
            ("DPM", "Digital peak meter", ""), ("JF", "'Jellyfish' phase meter", ""),
            ("SCO", "Oscilloscope meter", "")
            ]
    metertype = EnumProperty(
            name="Meter type",
            items=meterbridge,
            default="DPM",
            description="Meterbridge meter type"
            )
    # eco
    use_eco_tools = BoolProperty(
            name="Eco tools Panel",
            default=True
            )
    eco_value = IntProperty(
            name="Number of echoes",
            default=5,
            min=1, max=25
            )
    eco_offset = IntProperty(
            name="Echo Offset",
            default=1,
            min=-25000, max=25000
            )
    eco_use_add_blend_mode = BoolProperty(
            name='use_add_blend_mode',
            default=False
            )
    # random editor
    use_random_editor = BoolProperty(
            name="Random editor Panel | Experimental",
            default=False
            )
    random_frames = IntProperty(
            name="Frames",
            default=1,
            min=1, max=1000
            )
    random_selected_scene = StringProperty(
            name="Selected Scene",
            default=""
            )
    random_use_marker_subsets = BoolProperty(
            name="Use_Marker subsets",
            default=True
            )
    random_number_of_subsets = IntProperty(
            name="Number of subsets",
            default=3,
            min=1, max=5
            )
    show_shortcuts = BoolProperty(
            name="Hot Keys",
            default=False,
            description="List of the shortcuts used for the included various tools",
            )
    show_experimental = BoolProperty(
            name="Tools with External Dependencies / Experimental",
            default=False,
            description="List of various tools that need an External Library "
                        "or are Experimental\nPlease read the Documentation "
                        "before enabling them",
            )

    def draw(self, context):
        layout = self.layout
        icon_1 = "TRIA_RIGHT" if not self.show_shortcuts else "TRIA_DOWN"
        icon_2 = "TRIA_RIGHT" if not self.show_experimental else "TRIA_DOWN"

        box = layout.box()
        box.prop(self, "use_jumptocut")
        box = layout.box()
        box.prop(self, "use_eco_tools")

        box_exp = layout.box()
        box_exp.prop(self, "show_experimental", emboss=False, icon=icon_2)

        if self.show_experimental:
            box = box_exp.box()
            box.prop(self, "use_audio_tools")
            box = box_exp.box()
            box.prop(self, "use_proxy_tools")
            box = box_exp.box()
            box.prop(self, "use_exif_panel")
            box = box_exp.box()
            box.prop(self, "use_glitch_panel")
            box = box_exp.box()
            box.prop(self, "use_random_editor")

        box = layout.box()
        box.prop(self, "show_shortcuts", emboss=False, icon=icon_1)

        if self.show_shortcuts:
            box.label(text="Skip One Second Forward: Ctrl + Shift + Right Arrow", icon="LAYER_USED")
            box.label(text="Skip One Second Backwards: Ctrl + Shift + Left Arrow", icon="LAYER_USED")
            box.label(text="Jump to Previous Strip: Q", icon="LAYER_USED")
            box.label(text="Jump to Next Strip: W", icon="LAYER_USED")
            box.label(text="Source IN: Ctrl + Shift + I", icon="LAYER_USED")
            box.label(text="Source OUT: Ctrl + Shift + O", icon="LAYER_USED")
            box.label(text="Jump to Previous Marker: Shift + Q", icon="LAYER_USED")
            box.label(text="Jump to Next Marker: Shift + W", icon="LAYER_USED")


# Registration
def register():
    bpy.utils.register_class(KinorawToolsAddon)

    bpy.utils.register_module(__name__)

    # Append menu entries
    bpy.types.SEQUENCER_MT_add.prepend(ui.sequencer_add_menu_func)
    bpy.types.SEQUENCER_MT_select.prepend(ui.sequencer_select_menu_func)
    bpy.types.SEQUENCER_MT_strip.prepend(ui.sequencer_strip_menu_func)
    bpy.types.SEQUENCER_HT_header.append(ui.sequencer_header_func)
    bpy.types.CLIP_HT_header.append(ui.clip_header_func)
    bpy.types.CLIP_MT_clip.prepend(ui.clip_clip_menu_func)
    bpy.types.TIME_MT_frame.prepend(ui.time_frame_menu_func)
    bpy.types.TIME_HT_header.append(ui.time_header_func)

    # Add keyboard shortcut configuration
    kc = bpy.context.window_manager.keyconfigs.addon
    km = kc.keymaps.new(name='Frames')

    # jump 1 second
    kmi = km.keymap_items.new('screenextra.frame_skip',
                'RIGHT_ARROW', 'PRESS', ctrl=True, shift=True)
    kmi.properties.back = False
    kmi = km.keymap_items.new('screenextra.frame_skip',
                'LEFT_ARROW', 'PRESS', ctrl=True, shift=True)
    kmi.properties.back = True

    # jump to cut
    kmi = km.keymap_items.new("sequencer.strip_jump",
                'Q', 'PRESS', ctrl=False, shift=False)
    kmi.properties.next = False
    kmi.properties.center = False
    kmi = km.keymap_items.new("sequencer.strip_jump",
                'W', 'PRESS', ctrl=False, shift=False)
    kmi.properties.next = True
    kmi.properties.center = False

    # in and out
    kmi = km.keymap_items.new("sequencerextra.sourcein",
                'I', 'PRESS', ctrl=True, shift=True)
    kmi = km.keymap_items.new("sequencerextra.sourceout",
                'O', 'PRESS', ctrl=True, shift=True)

    # markers
    kc = bpy.context.window_manager.keyconfigs.active
    km = kc.keymaps.new(name='Screen')
    kmi = km.keymap_items.new("screen.marker_jump",
                'Q', 'PRESS', ctrl=False, shift=True)
    kmi.properties.next = False
    kmi = km.keymap_items.new("screen.marker_jump",
                'W', 'PRESS', ctrl=False, shift=True)
    kmi.properties.next = True


def unregister():
    bpy.utils.unregister_module(__name__)

    try:
        bpy.utils.unregister_class(KinorawToolsAddon)
    except RuntimeError:
        pass

    #  Remove menu entries
    bpy.types.SEQUENCER_MT_add.remove(ui.sequencer_add_menu_func)
    bpy.types.SEQUENCER_MT_select.remove(ui.sequencer_select_menu_func)
    bpy.types.SEQUENCER_MT_strip.remove(ui.sequencer_strip_menu_func)
    bpy.types.SEQUENCER_HT_header.remove(ui.sequencer_header_func)
    bpy.types.CLIP_HT_header.remove(ui.clip_header_func)
    bpy.types.CLIP_MT_clip.remove(ui.clip_clip_menu_func)
    bpy.types.TIME_MT_frame.remove(ui.time_frame_menu_func)
    bpy.types.TIME_HT_header.remove(ui.time_header_func)

    #  Remove keyboard shortcut configuration
    kc = bpy.context.window_manager.keyconfigs.addon
    km = kc.keymaps['Frames']
    km.keymap_items.remove(km.keymap_items['screenextra.frame_skip'])
    km.keymap_items.remove(km.keymap_items['screenextra.frame_skip'])

    km.keymap_items.remove(km.keymap_items['sequencer.strip_jump'])
    km.keymap_items.remove(km.keymap_items['sequencer.strip_jump'])

    km.keymap_items.remove(km.keymap_items['sequencerextra.sourcein'])
    km.keymap_items.remove(km.keymap_items['sequencerextra.sourceout'])

    kc = bpy.context.window_manager.keyconfigs.active
    km = kc.keymaps['Screen']
    km.keymap_items.remove(km.keymap_items['screen.marker_jump'])
    km.keymap_items.remove(km.keymap_items['screen.marker_jump'])


if __name__ == '__main__':
    register()
