# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.props import EnumProperty, StringProperty
from bpy.app.translations import (
    pgettext_n as n_,
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)

from . import interface

from .utils.constants import nice_hotkey_name
from dataclasses import dataclass


# Principled prefs
class NWPrincipledPreferences(bpy.types.PropertyGroup):
    base_color: StringProperty(
        name='Base Color',
        default='diffuse diff albedo base col color basecolor',
        description='Naming components for base color maps')
    metallic: StringProperty(
        name='Metallic',
        default='metallic metalness metal mtl',
        description='Naming components for metalness maps')
    specular: StringProperty(
        name='Specular',
        default='specularity specular spec spc',
        description='Naming components for specular maps')
    normal: StringProperty(
        name='Normal',
        default='normal nor nrm nrml norm',
        description='Naming components for normal maps')
    bump: StringProperty(
        name='Bump',
        default='bump bmp',
        description='Naming components for bump maps')
    rough: StringProperty(
        name='Roughness',
        default='roughness rough rgh',
        description='Naming components for roughness maps')
    gloss: StringProperty(
        name='Gloss',
        default='gloss glossy glossiness',
        description='Naming components for glossy maps')
    displacement: StringProperty(
        name='Displacement',
        default='displacement displace disp dsp height heightmap',
        description='Naming components for displacement maps')
    transmission: StringProperty(
        name='Transmission',
        default='transmission transparency',
        description='Naming components for transmission maps')
    emission: StringProperty(
        name='Emission',
        default='emission emissive emit',
        description='Naming components for emission maps')
    alpha: StringProperty(
        name='Alpha',
        default='alpha opacity',
        description='Naming components for alpha maps')
    ambient_occlusion: StringProperty(
        name='Ambient Occlusion',
        default='ao ambient occlusion',
        description='Naming components for AO maps')


# Addon prefs
class NWNodeWrangler(bpy.types.AddonPreferences):
    bl_idname = __package__

    merge_hide: EnumProperty(
        name="Hide Mix Nodes",
        items=(
            ("ALWAYS", "Always", "Always collapse the new merge nodes"),
            ("NON_SHADER", "Non-Shader", "Collapse in all cases except for shaders"),
            ("NEVER", "Never", "Never collapse the new merge nodes")
        ),
        default='NON_SHADER',
        description=(
            "When merging nodes with the Ctrl+Numpad0 hotkey (and similar) "
            "specify whether to collapse them or show the full node with options expanded"
        ),
    )
    merge_position: EnumProperty(
        name="Mix Node Position",
        items=(
            ("CENTER", "Center", "Place the Mix node between the two nodes"),
            ("BOTTOM", "Bottom", "Place the Mix node at the same height as the lowest node")
        ),
        default='CENTER',
        description=(
            "When merging nodes with the Ctrl+Numpad0 hotkey (and similar) "
            "specify the position of the new nodes"
        ),
    )
    hotkey_list_filter: StringProperty(
        name="        Filter by Name",
        default="",
        description="Show only hotkeys that have this text in their name",
        options={'TEXTEDIT_UPDATE'}
    )
    principled_tags: bpy.props.PointerProperty(type=NWPrincipledPreferences)

    def draw_principled_tags(self, layout):
        header, panel = layout.panel("NW_PT_prefs_principled_tags", default_closed=True)
        header.label(text="Principled Texture Tags")

        if panel:
            panel.separator(factor=0.5)

            tags = self.principled_tags
            col = panel.column(align=True)
            col.prop(tags, "base_color")
            col.prop(tags, "metallic")
            col.prop(tags, "specular")
            col.prop(tags, "rough")
            col.prop(tags, "gloss")
            col.prop(tags, "normal")
            col.prop(tags, "bump")
            col.prop(tags, "displacement")
            col.prop(tags, "transmission")
            col.prop(tags, "emission")
            col.prop(tags, "alpha")
            col.prop(tags, "ambient_occlusion")

    def draw_hotkey_list(self, layout):
        header, panel = layout.panel("NW_PT_prefs_hotkey_list", default_closed=True)
        header.label(text="Hotkey List")

        if panel:
            panel.separator(factor=0.5)
            col = panel.column(align=True)

            col.prop(self, "hotkey_list_filter", icon="VIEWZOOM")
            col.separator()

            for hotkey in kmi_defs:
                if hotkey_name := hotkey.label:
                    if (self.hotkey_list_filter.lower() in hotkey_name.lower()
                            or self.hotkey_list_filter.lower() in iface_(hotkey_name).lower()):
                        row = col.row(align=True)
                        row.label(text=hotkey_name)
                        keystr = iface_(nice_hotkey_name(hotkey.key), i18n_contexts.ui_events_keymaps)
                        if hotkey.shift:
                            keystr = iface_("Shift", i18n_contexts.ui_events_keymaps) + " " + keystr
                        if hotkey.alt:
                            keystr = iface_("Alt", i18n_contexts.ui_events_keymaps) + " " + keystr
                        if hotkey.ctrl:
                            keystr = iface_("Ctrl", i18n_contexts.ui_events_keymaps) + " " + keystr
                        row.label(text=keystr, translate=False)

    def draw(self, _context):
        layout = self.layout
        col = layout.column()
        col.prop(self, "merge_position")
        col.prop(self, "merge_hide")

        col = layout.column(align=True)

        box = col.box().column(align=True)
        self.draw_principled_tags(box)

        box = col.box().column(align=True)
        self.draw_hotkey_list(box)


#
#  REGISTER/UNREGISTER CLASSES AND KEYMAP ITEMS
#
@dataclass(frozen=True, slots=True)
class Hotkey:
    bl_idname: str
    label: str
    key: str = 'NONE'
    input_mode: str = 'PRESS'

    ctrl: bool = False
    shift: bool = False
    alt: bool = False
    props: dict = ()


addon_keymaps = []
# kmi_defs entry: (identifier, key, action, CTRL, SHIFT, ALT, props, nice name)
# props entry: (property name, property value)
kmi_defs = (
    # MERGE NODES
    # NWMergeNodes with Ctrl (AUTO).
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Automatic)"), key='NUMPAD_0',
           ctrl=True, props=(('mode', 'MIX'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Automatic)"), key='ZERO',
           ctrl=True, props=(('mode', 'MIX'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Add)"), key='NUMPAD_PLUS',
           ctrl=True, props=(('mode', 'ADD'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Add)"), key='EQUAL',
           ctrl=True, props=(('mode', 'ADD'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Multiply)"), key='NUMPAD_ASTERIX',
           ctrl=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Multiply)"), key='EIGHT',
           ctrl=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Subtract)"), key='NUMPAD_MINUS',
           ctrl=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Subtract)"), key='MINUS',
           ctrl=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Divide)"), key='NUMPAD_SLASH',
           ctrl=True, props=(('mode', 'DIVIDE'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Divide)"), key='SLASH',
           ctrl=True, props=(('mode', 'DIVIDE'), ('merge_type', 'AUTO'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Less Than)"), key='COMMA',
           ctrl=True, props=(('mode', 'LESS_THAN'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Greater Than)"), key='PERIOD',
           ctrl=True, props=(('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Depth Combine)"), key='NUMPAD_PERIOD',
           ctrl=True, props=(('mode', 'MIX'), ('merge_type', 'DEPTH_COMBINE'),)),
    # NWMergeNodes with Ctrl Alt (MIX or ALPHAOVER)
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Alpha Over)"), key='NUMPAD_0',
           ctrl=True, alt=True, props=(('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Alpha Over)"), key='ZERO',
           ctrl=True, alt=True, props=(('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Add)"), key='NUMPAD_PLUS',
           ctrl=True, alt=True, props=(('mode', 'ADD'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Add)"), key='EQUAL',
           ctrl=True, alt=True, props=(('mode', 'ADD'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Multiply)"), key='NUMPAD_ASTERIX',
           ctrl=True, alt=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Multiply)"), key='EIGHT',
           ctrl=True, alt=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Subtract)"), key='NUMPAD_MINUS',
           ctrl=True, alt=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Subtract)"), key='MINUS',
           ctrl=True, alt=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Divide)"), key='NUMPAD_SLASH',
           ctrl=True, alt=True, props=(('mode', 'DIVIDE'), ('merge_type', 'MIX'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Color, Divide)"), key='SLASH',
           ctrl=True, alt=True, props=(('mode', 'DIVIDE'), ('merge_type', 'MIX'),)),
    # NWMergeNodes with Ctrl Shift (MATH)
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Add)"), key='NUMPAD_PLUS',
           ctrl=True, shift=True, props=(('mode', 'ADD'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Add)"), key='EQUAL',
           ctrl=True, shift=True, props=(('mode', 'ADD'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Multiply)"), key='NUMPAD_ASTERIX',
           ctrl=True, shift=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Multiply)"), key='EIGHT',
           ctrl=True, shift=True, props=(('mode', 'MULTIPLY'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Subtract)"), key='NUMPAD_MINUS',
           ctrl=True, shift=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Subtract)"), key='MINUS',
           ctrl=True, shift=True, props=(('mode', 'SUBTRACT'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Divide)"), key='NUMPAD_SLASH',
           ctrl=True, shift=True, props=(('mode', 'DIVIDE'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Divide)"), key='SLASH',
           ctrl=True, shift=True, props=(('mode', 'DIVIDE'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Less than)"), key='COMMA',
           ctrl=True, shift=True, props=(('mode', 'LESS_THAN'), ('merge_type', 'MATH'),)),
    Hotkey("node.nw_merge_nodes", n_("Merge Nodes (Math, Greater than)"), key='PERIOD',
           ctrl=True, shift=True, props=(('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),)),
    # BATCH CHANGE NODES
    # NWBatchChangeNodes with Alt
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Mix)"), key='NUMPAD_0',
           alt=True, props=(('blend_type', 'MIX'), ('operation', 'CURRENT'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Mix)"), key='ZERO',
           alt=True, props=(('blend_type', 'MIX'), ('operation', 'CURRENT'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Add)"), key='NUMPAD_PLUS',
           alt=True, props=(('blend_type', 'ADD'), ('operation', 'ADD'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Add)"), key='EQUAL',
           alt=True, props=(('blend_type', 'ADD'), ('operation', 'ADD'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Multiply)"), key='NUMPAD_ASTERIX',
           alt=True, props=(('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Multiply)"), key='EIGHT',
           alt=True, props=(('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Subtract)"), key='NUMPAD_MINUS',
           alt=True, props=(('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Subtract)"), key='MINUS',
           alt=True, props=(('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Divide)"), key='NUMPAD_SLASH',
           alt=True, props=(('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Divide)"), key='SLASH',
           alt=True, props=(('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Current)"), key='COMMA',
           alt=True, props=(('blend_type', 'CURRENT'), ('operation', 'LESS_THAN'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Current)"), key='PERIOD',
           alt=True, props=(('blend_type', 'CURRENT'), ('operation', 'GREATER_THAN'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Next)"), key='DOWN_ARROW',
           alt=True, props=(('blend_type', 'NEXT'), ('operation', 'NEXT'),)),
    Hotkey("node.nw_batch_change", n_("Batch Change Blend Type (Previous)"), key='UP_ARROW',
           alt=True, props=(('blend_type', 'PREV'), ('operation', 'PREV'),)),
    # LINK ACTIVE TO SELECTED
    # Don't use names, don't replace links (K)
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Don't Replace Links)"),
           key='K', props=(('replace', False), ('use_node_name', False), ('use_outputs_names', False),)),
    # Don't use names, replace links (Shift K)
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Replace Links)"), key='K',
           shift=True, props=(('replace', True), ('use_node_name', False), ('use_outputs_names', False),)),
    # Use node name, don't replace links (')
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Don't Replace Links, Node Names)"),
           key='QUOTE', props=(('replace', False), ('use_node_name', True), ('use_outputs_names', False),)),
    # Use node name, replace links (Shift ')
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Replace Links, Node Names)"),
           key='QUOTE', shift=True, props=(('replace', True), ('use_node_name', True), ('use_outputs_names', False),)),
    # Don't use names, don't replace links (;)
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Don't Replace Links, Output Names)"),
           key='SEMI_COLON', props=(('replace', False), ('use_node_name', False), ('use_outputs_names', True),)),
    # Don't use names, replace links (')
    Hotkey("node.nw_link_active_to_selected", n_("Link Active to Selected (Replace Links, Output Names)"),
           key='SEMI_COLON', shift=True, props=(('replace', True), ('use_node_name', False), ('use_outputs_names', True),)),
    # CHANGE MIX FACTOR
    Hotkey("node.nw_factor", n_("Reduce Mix Factor by 0.1"), key='LEFT_ARROW', alt=True, props=(('option', -0.1),)),
    Hotkey("node.nw_factor", n_("Increase Mix Factor by 0.1"), key='RIGHT_ARROW', alt=True, props=(('option', 0.1),)),
    Hotkey("node.nw_factor", n_("Reduce Mix Factor by 0.01"),
           key='LEFT_ARROW', shift=True, alt=True, props=(('option', -0.01),)),
    Hotkey("node.nw_factor", n_("Increase Mix Factor by 0.01"),
           key='RIGHT_ARROW', shift=True, alt=True, props=(('option', 0.01),)),
    Hotkey("node.nw_factor", n_("Set Mix Factor to 0.0"), key='LEFT_ARROW',
           ctrl=True, shift=True, alt=True, props=(('option', 0.0),)),
    Hotkey("node.nw_factor", n_("Set Mix Factor to 1.0"), key='RIGHT_ARROW',
           ctrl=True, shift=True, alt=True, props=(('option', 1.0),)),
    Hotkey("node.nw_factor", n_("Set Mix Factor to 0.0"), key='NUMPAD_0',
           ctrl=True, shift=True, alt=True, props=(('option', 0.0),)),
    Hotkey("node.nw_factor", n_("Set Mix Factor to 0.0"), key='ZERO',
           ctrl=True, shift=True, alt=True, props=(('option', 0.0),)),
    Hotkey("node.nw_factor", n_("Mix Factor to 1.0"), key='NUMPAD_1',
           ctrl=True, shift=True, alt=True, props=(('option', 1.0),)),
    Hotkey("node.nw_factor", n_("Set Mix Factor to 1.0"), key='ONE',
           ctrl=True, shift=True, alt=True, props=(('option', 1.0),)),
    # CLEAR LABEL (Alt L)
    Hotkey("node.nw_clear_label", n_("Clear Node Labels"), key='L', alt=True, props=(('option', False),)),
    # MODIFY LABEL (Alt Shift L)
    Hotkey("node.nw_modify_labels", n_("Modify Node Labels"), key='L', shift=True, alt=True),
    # Copy Label from active to selected
    Hotkey("node.nw_copy_label", n_("Copy label from active to selected"),
           key='V', shift=True, props=(('option', 'FROM_ACTIVE'),)),
    # DETACH OUTPUTS (Alt Shift D)
    Hotkey("node.nw_detach_outputs", n_("Detach Outputs"), key='D', shift=True, alt=True),
    # LINK TO OUTPUT NODE (O)
    Hotkey("node.nw_link_out", n_("Link to Output Node"), key='O'),
    # SELECT PARENT/CHILDREN
    # Select Children
    Hotkey("node.nw_select_parent_child", n_("Select Children"), key='RIGHT_BRACKET', props=(('option', 'CHILD'),)),
    # Select Parent
    Hotkey("node.nw_select_parent_child", n_("Select Parent"), key='LEFT_BRACKET', props=(('option', 'PARENT'),)),
    # Add Texture Setup
    Hotkey("node.nw_add_texture", n_("Add Texture Setup"), key='T', ctrl=True),
    # Add Principled BSDF Texture Setup
    Hotkey("node.nw_add_textures_for_principled", n_("Add Principled Texture Setup"), key='T', ctrl=True, shift=True),
    # Reset backdrop
    Hotkey("node.nw_bg_reset", n_("Reset Backdrop Image Zoom"), key='Z'),
    # Delete unused
    Hotkey("node.nw_del_unused", n_("Delete Unused Nodes"), key='X', alt=True),
    # Frame Selected
    Hotkey('node.join', n_("Frame Selected Nodes"), key='P', shift=True),
    # Swap Links
    Hotkey("node.nw_swap_links", n_("Swap Links"), key='S', alt=True),
    # Reload Images
    Hotkey("node.nw_reload_images", n_("Reload Images"), key='R', alt=True),
    # Lazy Mix
    Hotkey("node.nw_lazy_mix", n_("Lazy Mix"), key='RIGHTMOUSE', ctrl=True, shift=True),
    # Lazy Connect
    Hotkey("node.nw_lazy_connect", n_("Lazy Connect"), key='RIGHTMOUSE', alt=True, props=(('with_menu', False),)),
    # Lazy Connect with Menu
    Hotkey("node.nw_lazy_connect", n_("Lazy Connect with Socket Menu"),
           key='RIGHTMOUSE', shift=True, alt=True, props=(('with_menu', True),)),
    # Align Nodes
    Hotkey("node.nw_align_nodes", n_("Align Nodes"), key='EQUAL', shift=True),
    # Reset Nodes (Back Space)
    Hotkey("node.nw_reset_nodes", n_("Reset Nodes"), key='BACK_SPACE'),
    # MENUS
    Hotkey('wm.call_menu', n_("Node Wrangler (Menu)"), key='W', shift=True,
           props=(('name', interface.NodeWranglerMenu.bl_idname),)),
    Hotkey('wm.call_menu', n_("Add Reroutes (Menu)"), key='SLASH',
           props=(('name', interface.NWAddReroutesMenu.bl_idname),)),
    Hotkey('wm.call_menu', n_("Add Reroutes (Menu)"), key='NUMPAD_SLASH',
           props=(('name', interface.NWAddReroutesMenu.bl_idname),)),
    Hotkey('wm.call_menu', n_("Link Active to Selected (Menu)"), key='BACK_SLASH',
           props=(('name', interface.NWLinkActiveToSelectedMenu.bl_idname),)),
    Hotkey('wm.call_menu', n_("Copy to Selected (Menu)"), key='C', shift=True,
           props=(('name', interface.NWCopyToSelectedMenu.bl_idname),)),
)

classes = (
    NWPrincipledPreferences, NWNodeWrangler
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

    # keymaps
    addon_keymaps.clear()
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name='Node Editor', space_type="NODE_EDITOR")
        for hotkey in kmi_defs:
            kmi = km.keymap_items.new(
                hotkey.bl_idname,
                hotkey.key,
                hotkey.input_mode,
                ctrl=hotkey.ctrl,
                shift=hotkey.shift,
                alt=hotkey.alt)
            for prop, value in hotkey.props:
                setattr(kmi.properties, prop, value)
            addon_keymaps.append((km, kmi))


def unregister():

    # keymaps
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()

    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)
