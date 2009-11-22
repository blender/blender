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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy


def act_strip(context):
    try:
        return context.scene.sequence_editor.active_strip
    except AttributeError:
        return None


class SEQUENCER_HT_header(bpy.types.Header):
    bl_space_type = 'SEQUENCE_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.itemM("SEQUENCER_MT_view")

            row.itemS()

            if st.display_mode == 'SEQUENCER':
                sub.itemM("SEQUENCER_MT_select")
                sub.itemM("SEQUENCER_MT_marker")
                sub.itemM("SEQUENCER_MT_add")
                sub.itemM("SEQUENCER_MT_strip")

        layout.itemR(st, "display_mode", text="")

        if st.display_mode == 'SEQUENCER':
            layout.itemS()
            layout.itemO("sequencer.refresh_all")
        else:
            layout.itemR(st, "display_channel", text="Channel")


class SEQUENCER_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.column()

        """
    uiBlock *block= uiBeginBlock(C, ar, "seq_viewmenu", UI_EMBOSSP);
    short yco= 0, menuwidth=120;

    if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
        uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
                 "Play Back Animation "
                 "in all Sequence Areas|Alt A", 0, yco-=20,
                 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
    }
    else {
        uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL,
                 "Grease Pencil...", 0, yco-=20,
                 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
        uiDefMenuSep(block);

        uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
                 "Play Back Animation "
                 "in this window|Alt A", 0, yco-=20,
                 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
    }
    uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
             "Play Back Animation in all "
             "3D Views and Sequence Areas|Alt Shift A",
             0, yco-=20,
             menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

        """
        layout.itemS()
        layout.itemO("sequencer.view_all")
        layout.itemO("sequencer.view_selected")
        layout.itemS()
        layout.itemO("screen.screen_full_area", text="Toggle Full Screen")
        """


    /* Lock Time */
    uiDefIconTextBut(block, BUTM, 1, (v2d->flag & V2D_VIEWSYNC_SCREEN_TIME)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
            "Lock Time to Other Windows|", 0, yco-=20,
            menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

    /* Draw time or frames.*/
    uiDefMenuSep(block);
        """

        layout.itemR(st, "draw_frames")
        layout.itemR(st, "show_cframe_indicator")
        if st.display_mode == 'IMAGE':
            layout.itemR(st, "draw_safe_margin")
        if st.display_mode == 'WAVEFORM':
            layout.itemR(st, "separate_color_preview")

        """
    if(!sa->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
    else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

        """


class SEQUENCER_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.item_enumO("sequencer.select_active_side", "side", 'LEFT', text="Strips to the Left")
        layout.item_enumO("sequencer.select_active_side", "side", 'RIGHT', text="Strips to the Right")
        layout.itemS()
        layout.item_enumO("sequencer.select_handles", "side", 'BOTH', text="Surrounding Handles")
        layout.item_enumO("sequencer.select_handles", "side", 'LEFT', text="Left Handle")
        layout.item_enumO("sequencer.select_handles", "side", 'RIGHT', text="Right Handle")
        layout.itemS()
        layout.itemO("sequencer.select_linked")
        layout.itemO("sequencer.select_all_toggle")
        layout.itemO("sequencer.select_inverse")


class SEQUENCER_MT_marker(bpy.types.Menu):
    bl_label = "Marker (TODO)"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.itemO("marker.add", text="Add Marker")
        layout.itemO("marker.duplicate", text="Duplicate Marker")
        layout.itemO("marker.move", text="Grab/Move Marker")
        layout.itemO("marker.delete", text="Delete Marker")
        layout.itemS()
        layout.itemL(text="ToDo: Name Marker")

        #layout.itemO("sequencer.sound_strip_add", text="Transform Markers") # toggle, will be rna - (sseq->flag & SEQ_MARKER_TRANS)


class SEQUENCER_MT_add(bpy.types.Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.column()
        layout.itemO("sequencer.scene_strip_add", text="Scene")
        layout.itemO("sequencer.movie_strip_add", text="Movie")
        layout.itemO("sequencer.image_strip_add", text="Image")
        layout.itemO("sequencer.sound_strip_add", text="Sound")

        layout.itemM("SEQUENCER_MT_add_effect")


class SEQUENCER_MT_add_effect(bpy.types.Menu):
    bl_label = "Effect Strip..."

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.column()
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'ADD')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'SUBTRACT')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'ALPHA_OVER')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'ALPHA_UNDER')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'GAMMA_CROSS')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'MULTIPLY')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'OVER_DROP')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'PLUGIN')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'WIPE')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'GLOW')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'TRANSFORM')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'COLOR')
        layout.item_enumO("sequencer.effect_strip_add", 'type', 'SPEED')


class SEQUENCER_MT_strip(bpy.types.Menu):
    bl_label = "Strip"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.column()
        layout.item_enumO("tfm.transform", "mode", 'TRANSLATION', text="Grab/Move")
        layout.item_enumO("tfm.transform", "mode", 'TIME_EXTEND', text="Grab/Extend from frame")
        #  uiItemO(layout, NULL, 0, "sequencer.strip_snap"); // TODO - add this operator
        layout.itemS()

        layout.item_enumO("sequencer.cut", "type", 'HARD', text="Cut (hard) at frame")
        layout.item_enumO("sequencer.cut", "type", 'SOFT', text="Cut (soft) at frame")
        layout.itemO("sequencer.images_separate")
        layout.itemS()

        layout.itemO("sequencer.duplicate")
        layout.itemO("sequencer.delete")

        strip = act_strip(context)

        if strip:
            stype = strip.type

            if	stype == 'EFFECT':
                layout.itemS()
                layout.itemO("sequencer.effect_change")
                layout.itemO("sequencer.effect_reassign_inputs")
            elif stype == 'IMAGE':
                layout.itemS()
                layout.itemO("sequencer.image_change")
                layout.itemO("sequencer.rendersize")
            elif stype == 'SCENE':
                layout.itemS()
                layout.itemO("sequencer.scene_change", text="Change Scene")
            elif stype == 'MOVIE':
                layout.itemS()
                layout.itemO("sequencer.movie_change")
                layout.itemO("sequencer.rendersize")

        layout.itemS()

        layout.itemO("sequencer.meta_make")
        layout.itemO("sequencer.meta_separate")

        #if (ed && (ed->metastack.first || (ed->act_seq && ed->act_seq->type == SEQ_META))) {
        #	uiItemS(layout);
        #	uiItemO(layout, NULL, 0, "sequencer.meta_toggle");
        #}

        layout.itemS()
        layout.itemO("sequencer.reload")
        layout.itemS()
        layout.itemO("sequencer.lock")
        layout.itemO("sequencer.unlock")
        layout.itemO("sequencer.mute")
        layout.itemO("sequencer.unmute")

        layout.item_booleanO("sequencer.mute", "unselected", 1, text="Mute Deselected Strips")

        layout.itemO("sequencer.snap")

        layout.itemO("sequencer.swap_right")
        layout.itemO("sequencer.swap_left")


class SequencerButtonsPanel(bpy.types.Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    def poll(self, context):
        return (context.space_data.display_mode == 'SEQUENCER') and (act_strip(context) is not None)


class SequencerButtonsPanel_Output(bpy.types.Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    def poll(self, context):
        return context.space_data.display_mode != 'SEQUENCER'


class SEQUENCER_PT_edit(SequencerButtonsPanel):
    bl_label = "Edit Strip"

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        split = layout.split(percentage=0.3)
        split.itemL(text="Name:")
        split.itemR(strip, "name", text="")

        split = layout.split(percentage=0.3)
        split.itemL(text="Type:")
        split.itemR(strip, "type", text="")

        split = layout.split(percentage=0.3)
        split.itemL(text="Blend:")
        split.itemR(strip, "blend_mode", text="")

        row = layout.row()
        if strip.mute == True:
            row.itemR(strip, "mute", toggle=True, icon='ICON_RESTRICT_VIEW_ON', text="")
        elif strip.mute is False:
            row.itemR(strip, "mute", toggle=True, icon='ICON_RESTRICT_VIEW_OFF', text="")

        sub = row.row()
        sub.active = (not strip.mute)

        sub.itemR(strip, "blend_opacity", text="Opacity", slider=True)

        row = layout.row()
        row.itemR(strip, "lock")
        row.itemR(strip, "frame_locked", text="Frame Lock")

        col = layout.column()
        col.enabled = not strip.lock
        col.itemR(strip, "channel")
        col.itemR(strip, "start_frame")
        col.itemR(strip, "length")

        col = layout.column(align=True)
        col.itemL(text="Offset:")
        col.itemR(strip, "start_offset", text="Start")
        col.itemR(strip, "end_offset", text="End")

        col = layout.column(align=True)
        col.itemL(text="Still:")
        col.itemR(strip, "start_still", text="Start")
        col.itemR(strip, "end_still", text="End")


class SEQUENCER_PT_effect(SequencerButtonsPanel):
    bl_label = "Effect Strip"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('COLOR', 'WIPE', 'GLOW', 'SPEED', 'TRANSFORM')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        if strip.type == 'COLOR':
            layout.itemR(strip, "color")

        elif strip.type == 'WIPE':

            col = layout.column()
            col.itemR(strip, "transition_type")
            col.itemL(text="Direction:")
            col.row().itemR(strip, "direction", expand=True)

            col = layout.column()
            col.itemR(strip, "blur_width", slider=True)
            if strip.transition_type in ('SINGLE', 'DOUBLE'):
                col.itemR(strip, "angle")

        elif strip.type == 'GLOW':
            flow = layout.column_flow()
            flow.itemR(strip, "threshold", slider=True)
            flow.itemR(strip, "clamp", slider=True)
            flow.itemR(strip, "boost_factor")
            flow.itemR(strip, "blur_distance")

            row = layout.row()
            row.itemR(strip, "quality", slider=True)
            row.itemR(strip, "only_boost")

        elif strip.type == 'SPEED':
            layout.itemR(strip, "global_speed")

            flow = layout.column_flow()
            flow.itemR(strip, "curve_velocity")
            flow.itemR(strip, "curve_compress_y")
            flow.itemR(strip, "frame_blending")

        elif strip.type == 'TRANSFORM':

            col = layout.column()
            col.itemR(strip, "interpolation")
            col.itemR(strip, "translation_unit")

            col = layout.column(align=True)
            col.itemL(text="Position X:")
            col.itemR(strip, "translate_start_x", text="Start")
            col.itemR(strip, "translate_end_x", text="End")

            col = layout.column(align=True)
            col.itemL(text="Position Y:")
            col.itemR(strip, "translate_start_y", text="Start")
            col.itemR(strip, "translate_end_y", text="End")

            layout.itemS()

            col = layout.column(align=True)
            col.itemL(text="Scale X:")
            col.itemR(strip, "scale_start_x", text="Start")
            col.itemR(strip, "scale_end_x", text="End")

            col = layout.column(align=True)
            col.itemL(text="Scale Y:")
            col.itemR(strip, "scale_start_y", text="Start")
            col.itemR(strip, "scale_end_y", text="End")

            layout.itemS()

            col = layout.column(align=True)
            col.itemL(text="Rotation:")
            col.itemR(strip, "rotation_start", text="Start")
            col.itemR(strip, "rotation_end", text="End")

        col = layout.column(align=True)
        if strip.type == 'SPEED':
            col.itemR(strip, "speed_fader", text="Speed fader")
        else:
            col.itemR(strip, "effect_fader", text="Effect fader")


class SEQUENCER_PT_input(SequencerButtonsPanel):
    bl_label = "Strip Input"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('MOVIE', 'IMAGE')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        split = layout.split(percentage=0.2)
        col = split.column()
        col.itemL(text="Path:")
        col = split.column()
        col.itemR(strip, "directory", text="")

        # Current element for the filename

        elem = strip.getStripElem(context.scene.current_frame)
        if elem:
            split = layout.split(percentage=0.2)
            col = split.column()
            col.itemL(text="File:")
            col = split.column()
            col.itemR(elem, "filename", text="") # strip.elements[0] could be a fallback

        layout.itemR(strip, "use_translation", text="Image Offset:")
        if strip.transform:
            col = layout.column(align=True)
            col.active = strip.use_translation
            col.itemR(strip.transform, "offset_x", text="X")
            col.itemR(strip.transform, "offset_y", text="Y")

        layout.itemR(strip, "use_crop", text="Image Crop:")
        if strip.crop:
            col = layout.column(align=True)
            col.active = strip.use_crop
            col.itemR(strip.crop, "top")
            col.itemR(strip.crop, "left")
            col.itemR(strip.crop, "bottom")
            col.itemR(strip.crop, "right")

        col = layout.column(align=True)
        col.itemL(text="Trim Duration:")
        col.itemR(strip, "animation_start_offset", text="Start")
        col.itemR(strip, "animation_end_offset", text="End")


class SEQUENCER_PT_sound(SequencerButtonsPanel):
    bl_label = "Sound"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('SOUND')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        layout.template_ID(strip, "sound", open="sound.open")

        layout.itemS()
        layout.itemR(strip.sound, "filename", text="")

        row = layout.row()
        if strip.sound.packed_file:
            row.itemO("sound.unpack", icon='ICON_PACKAGE', text="Unpack")
        else:
            row.itemO("sound.pack", icon='ICON_UGLYPACKAGE', text="Pack")

        row.itemR(strip.sound, "caching")

        layout.itemR(strip, "volume")
        

class SEQUENCER_PT_scene(SequencerButtonsPanel):
    bl_label = "Scene"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('SCENE')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)
        
        layout.template_ID(strip, "scene")


class SEQUENCER_PT_filter(SequencerButtonsPanel):
    bl_label = "Filter"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        col = layout.column()
        col.itemL(text="Video:")
        col.itemR(strip, "strobe")
        col.itemR(strip, "de_interlace")

        col = layout.column()
        col.itemL(text="Colors:")
        col.itemR(strip, "multiply_colors", text="Multiply")
        col.itemR(strip, "premultiply")
        col.itemR(strip, "convert_float")

        col = layout.column()
        col.itemL(text="Flip:")
        col.itemR(strip, "flip_x", text="X")
        col.itemR(strip, "flip_y", text="Y")
        col.itemR(strip, "reverse_frames", text="Backwards")

        layout.itemR(strip, "use_color_balance")
        if strip.color_balance: # TODO - need to add this somehow
            row = layout.row()
            row.active = strip.use_color_balance
            col = row.column()
            col.itemR(strip.color_balance, "lift")
            col.itemR(strip.color_balance, "inverse_lift", text="Inverse")
            col = row.column()
            col.itemR(strip.color_balance, "gamma")
            col.itemR(strip.color_balance, "inverse_gamma", text="Inverse")
            col = row.column()
            col.itemR(strip.color_balance, "gain")
            col.itemR(strip.color_balance, "inverse_gain", text="Inverse")


class SEQUENCER_PT_proxy(SequencerButtonsPanel):
    bl_label = "Proxy"

    def poll(self, context):
        if context.space_data.display_mode != 'SEQUENCER':
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')

    def draw_header(self, context):
        strip = act_strip(context)

        self.layout.itemR(strip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        flow = layout.column_flow()
        flow.itemR(strip, "proxy_custom_directory")
        if strip.proxy: # TODO - need to add this somehow
            flow.itemR(strip.proxy, "directory")
            flow.itemR(strip.proxy, "file")


class SEQUENCER_PT_view(SequencerButtonsPanel_Output):
    bl_label = "View Settings"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        col = layout.column()
        col.itemR(st, "draw_overexposed") # text="Zebra"
        col.itemR(st, "draw_safe_margin")

bpy.types.register(SEQUENCER_HT_header) # header/menu classes
bpy.types.register(SEQUENCER_MT_view)
bpy.types.register(SEQUENCER_MT_select)
bpy.types.register(SEQUENCER_MT_marker)
bpy.types.register(SEQUENCER_MT_add)
bpy.types.register(SEQUENCER_MT_add_effect)
bpy.types.register(SEQUENCER_MT_strip)

bpy.types.register(SEQUENCER_PT_edit) # sequencer panels
bpy.types.register(SEQUENCER_PT_effect)
bpy.types.register(SEQUENCER_PT_input)
bpy.types.register(SEQUENCER_PT_sound)
bpy.types.register(SEQUENCER_PT_scene)
bpy.types.register(SEQUENCER_PT_filter)
bpy.types.register(SEQUENCER_PT_proxy)

bpy.types.register(SEQUENCER_PT_view) # view panels
