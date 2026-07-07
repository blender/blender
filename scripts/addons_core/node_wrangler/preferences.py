# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.props import EnumProperty, BoolProperty, StringProperty
from bpy.app.translations import (
    pgettext_n as n_,
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)

from . import interface

from .utils.constants import nice_hotkey_name


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

    show_hotkey_list: BoolProperty(
        name="Show Hotkey List",
        default=False,
        description="Expand this box into a list of all the hotkeys for functions in this addon"
    )
    hotkey_list_filter: StringProperty(
        name="        Filter by Name",
        default="",
        description="Show only hotkeys that have this text in their name",
        options={'TEXTEDIT_UPDATE'}
    )
    show_principled_lists: BoolProperty(
        name="Show Principled Naming Tags",
        default=False,
        description="Expand this box into a list of all naming tags for Principled Texture setup"
    )
    principled_tags: bpy.props.PointerProperty(type=NWPrincipledPreferences)

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(self, "merge_position")
        col.prop(self, "merge_hide")

        box = layout.box()
        col = box.column(align=True)
        col.prop(
            self,
            "show_principled_lists",
            text='Edit tags for auto texture detection in Principled BSDF setup',
            toggle=True)
        if self.show_principled_lists:
            tags = self.principled_tags

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

        box = layout.box()
        col = box.column(align=True)
        hotkey_button_name = iface_("Hide Hotkey List") if self.show_hotkey_list else iface_("Show Hotkey List")
        col.prop(self, "show_hotkey_list", text=hotkey_button_name, translate=False, toggle=True)
        if self.show_hotkey_list:
            col.prop(self, "hotkey_list_filter", icon="VIEWZOOM")
            col.separator()
            for hotkey in kmi_defs:
                if hotkey[7]:
                    hotkey_name = hotkey[7]

                    if (self.hotkey_list_filter.lower() in hotkey_name.lower()
                            or self.hotkey_list_filter.lower() in iface_(hotkey_name).lower()):
                        row = col.row(align=True)
                        row.label(text=hotkey_name)
                        keystr = iface_(nice_hotkey_name(hotkey[1]), i18n_contexts.ui_events_keymaps)
                        if hotkey[4]:
                            keystr = iface_("Shift", i18n_contexts.ui_events_keymaps) + " " + keystr
                        if hotkey[5]:
                            keystr = iface_("Alt", i18n_contexts.ui_events_keymaps) + " " + keystr
                        if hotkey[3]:
                            keystr = iface_("Ctrl", i18n_contexts.ui_events_keymaps) + " " + keystr
                        row.label(text=keystr, translate=False)


#
#  REGISTER/UNREGISTER CLASSES AND KEYMAP ITEMS
#
addon_keymaps = []
# kmi_defs entry: (identifier, key, action, CTRL, SHIFT, ALT, props, nice name)
# props entry: (property name, property value)
kmi_defs = (
    # MERGE NODES
    # NWMergeNodes with Ctrl (AUTO).
    ("node.nw_merge_nodes", 'NUMPAD_0', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Automatic)")),
    ("node.nw_merge_nodes", 'ZERO', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Automatic)")),
    ("node.nw_merge_nodes", 'NUMPAD_PLUS', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Add)")),
    ("node.nw_merge_nodes", 'EQUAL', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Add)")),
    ("node.nw_merge_nodes", 'NUMPAD_ASTERIX', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Multiply)")),
    ("node.nw_merge_nodes", 'EIGHT', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Multiply)")),
    ("node.nw_merge_nodes", 'NUMPAD_MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Subtract)")),
    ("node.nw_merge_nodes", 'MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Subtract)")),
    ("node.nw_merge_nodes", 'NUMPAD_SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Divide)")),
    ("node.nw_merge_nodes", 'SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Divide)")),
    ("node.nw_merge_nodes", 'COMMA', 'PRESS', True, False, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Less Than)")),
    ("node.nw_merge_nodes", 'PERIOD', 'PRESS', True, False, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Greater Than)")),
    ("node.nw_merge_nodes", 'NUMPAD_PERIOD', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'DEPTH_COMBINE'),), n_("Merge Nodes (Depth Combine)")),
    # NWMergeNodes with Ctrl Alt (MIX or ALPHAOVER)
    ("node.nw_merge_nodes", 'NUMPAD_0', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), n_("Merge Nodes (Alpha Over)")),
    ("node.nw_merge_nodes", 'ZERO', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), n_("Merge Nodes (Alpha Over)")),
    ("node.nw_merge_nodes", 'NUMPAD_PLUS', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Add)")),
    ("node.nw_merge_nodes", 'EQUAL', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Add)")),
    ("node.nw_merge_nodes", 'NUMPAD_ASTERIX', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Multiply)")),
    ("node.nw_merge_nodes", 'EIGHT', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Multiply)")),
    ("node.nw_merge_nodes", 'NUMPAD_MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Subtract)")),
    ("node.nw_merge_nodes", 'MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Subtract)")),
    ("node.nw_merge_nodes", 'NUMPAD_SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Divide)")),
    ("node.nw_merge_nodes", 'SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Divide)")),
    # NWMergeNodes with Ctrl Shift (MATH)
    ("node.nw_merge_nodes", 'NUMPAD_PLUS', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Add)")),
    ("node.nw_merge_nodes", 'EQUAL', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Add)")),
    ("node.nw_merge_nodes", 'NUMPAD_ASTERIX', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Multiply)")),
    ("node.nw_merge_nodes", 'EIGHT', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Multiply)")),
    ("node.nw_merge_nodes", 'NUMPAD_MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Subtract)")),
    ("node.nw_merge_nodes", 'MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Subtract)")),
    ("node.nw_merge_nodes", 'NUMPAD_SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Divide)")),
    ("node.nw_merge_nodes", 'SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Divide)")),
    ("node.nw_merge_nodes", 'COMMA', 'PRESS', True, True, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Less than)")),
    ("node.nw_merge_nodes", 'PERIOD', 'PRESS', True, True, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Greater than)")),
    # BATCH CHANGE NODES
    # NWBatchChangeNodes with Alt
    ("node.nw_batch_change", 'NUMPAD_0', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), n_("Batch Change Blend Type (Mix)")),
    ("node.nw_batch_change", 'ZERO', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), n_("Batch Change Blend Type (Mix)")),
    ("node.nw_batch_change", 'NUMPAD_PLUS', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), n_("Batch Change Blend Type (Add)")),
    ("node.nw_batch_change", 'EQUAL', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), n_("Batch Change Blend Type (Add)")),
    ("node.nw_batch_change", 'NUMPAD_ASTERIX', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), n_("Batch Change Blend Type (Multiply)")),
    ("node.nw_batch_change", 'EIGHT', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), n_("Batch Change Blend Type (Multiply)")),
    ("node.nw_batch_change", 'NUMPAD_MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), n_("Batch Change Blend Type (Subtract)")),
    ("node.nw_batch_change", 'MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), n_("Batch Change Blend Type (Subtract)")),
    ("node.nw_batch_change", 'NUMPAD_SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), n_("Batch Change Blend Type (Divide)")),
    ("node.nw_batch_change", 'SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), n_("Batch Change Blend Type (Divide)")),
    ("node.nw_batch_change", 'COMMA', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'LESS_THAN'),), n_("Batch Change Blend Type (Current)")),
    ("node.nw_batch_change", 'PERIOD', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'GREATER_THAN'),), n_("Batch Change Blend Type (Current)")),
    ("node.nw_batch_change", 'DOWN_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'NEXT'), ('operation', 'NEXT'),), n_("Batch Change Blend Type (Next)")),
    ("node.nw_batch_change", 'UP_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'PREV'), ('operation', 'PREV'),), n_("Batch Change Blend Type (Previous)")),
    # LINK ACTIVE TO SELECTED
    # Don't use names, don't replace links (K)
    ("node.nw_link_active_to_selected", 'K', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', False),),
        n_("Link Active to Selected (Don't Replace Links)")),
    # Don't use names, replace links (Shift K)
    ("node.nw_link_active_to_selected", 'K', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', False),),
        n_("Link Active to Selected (Replace Links)")),
    # Use node name, don't replace links (')
    ("node.nw_link_active_to_selected", 'QUOTE', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', True), ('use_outputs_names', False),),
        n_("Link Active to Selected (Don't Replace Links, Node Names)")),
    # Use node name, replace links (Shift ')
    ("node.nw_link_active_to_selected", 'QUOTE', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', True), ('use_outputs_names', False),),
        n_("Link Active to Selected (Replace Links, Node Names)")),
    # Don't use names, don't replace links (;)
    ("node.nw_link_active_to_selected", 'SEMI_COLON', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', True),),
        n_("Link Active to Selected (Don't Replace Links, Output Names)")),
    # Don't use names, replace links (')
    ("node.nw_link_active_to_selected", 'SEMI_COLON', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', True),),
        n_("Link Active to Selected (Replace Links, Output Names)")),
    # CHANGE MIX FACTOR
    ("node.nw_factor", 'LEFT_ARROW', 'PRESS', False,
     False, True, (('option', -0.1),), n_("Reduce Mix Factor by 0.1")),
    ("node.nw_factor", 'RIGHT_ARROW', 'PRESS', False,
     False, True, (('option', 0.1),), n_("Increase Mix Factor by 0.1")),
    ("node.nw_factor", 'LEFT_ARROW', 'PRESS', False,
     True, True, (('option', -0.01),), n_("Reduce Mix Factor by 0.01")),
    ("node.nw_factor", 'RIGHT_ARROW', 'PRESS', False,
     True, True, (('option', 0.01),), n_("Increase Mix Factor by 0.01")),
    ("node.nw_factor", 'LEFT_ARROW', 'PRESS',
     True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    ("node.nw_factor", 'RIGHT_ARROW', 'PRESS',
     True, True, True, (('option', 1.0),), n_("Set Mix Factor to 1.0")),
    ("node.nw_factor", 'NUMPAD_0', 'PRESS',
     True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    ("node.nw_factor", 'ZERO', 'PRESS', True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    ("node.nw_factor", 'NUMPAD_1', 'PRESS', True, True, True, (('option', 1.0),), n_("Mix Factor to 1.0")),
    ("node.nw_factor", 'ONE', 'PRESS', True, True, True, (('option', 1.0),), n_("Set Mix Factor to 1.0")),
    # CLEAR LABEL (Alt L)
    ("node.nw_clear_label", 'L', 'PRESS', False, False, True, (('option', False),), n_("Clear Node Labels")),
    # MODIFY LABEL (Alt Shift L)
    ("node.nw_modify_labels", 'L', 'PRESS', False, True, True, None, n_("Modify Node Labels")),
    # Copy Label from active to selected
    ("node.nw_copy_label", 'V', 'PRESS', False, True, False,
     (('option', 'FROM_ACTIVE'),), n_("Copy label from active to selected")),
    # DETACH OUTPUTS (Alt Shift D)
    ("node.nw_detach_outputs", 'D', 'PRESS', False, True, True, None, n_("Detach Outputs")),
    # LINK TO OUTPUT NODE (O)
    ("node.nw_link_out", 'O', 'PRESS', False, False, False, None, n_("Link to Output Node")),
    # SELECT PARENT/CHILDREN
    # Select Children
    ("node.nw_select_parent_child", 'RIGHT_BRACKET', 'PRESS',
     False, False, False, (('option', 'CHILD'),), n_("Select Children")),
    # Select Parent
    ("node.nw_select_parent_child", 'LEFT_BRACKET', 'PRESS',
     False, False, False, (('option', 'PARENT'),), n_("Select Parent")),
    # Add Texture Setup
    ("node.nw_add_texture", 'T', 'PRESS', True, False, False, None, n_("Add Texture Setup")),
    # Add Principled BSDF Texture Setup
    ("node.nw_add_textures_for_principled", 'T', 'PRESS', True, True, False, None, n_("Add Principled Texture Setup")),
    # Reset backdrop
    ("node.nw_bg_reset", 'Z', 'PRESS', False, False, False, None, n_("Reset Backdrop Image Zoom")),
    # Delete unused
    ("node.nw_del_unused", 'X', 'PRESS', False, False, True, None, n_("Delete Unused Nodes")),
    # Frame Selected
    ('node.join', 'P', 'PRESS', False, True, False, None, n_("Frame Selected Nodes")),
    # Swap Links
    ("node.nw_swap_links", 'S', 'PRESS', False, False, True, None, n_("Swap Links")),
    # Reload Images
    ("node.nw_reload_images", 'R', 'PRESS', False, False, True, None, n_("Reload Images")),
    # Lazy Mix
    ("node.nw_lazy_mix", 'RIGHTMOUSE', 'PRESS', True, True, False, None, n_("Lazy Mix")),
    # Lazy Connect
    ("node.nw_lazy_connect", 'RIGHTMOUSE', 'PRESS', False, False, True, (('with_menu', False),), n_("Lazy Connect")),
    # Lazy Connect with Menu
    ("node.nw_lazy_connect", 'RIGHTMOUSE', 'PRESS', False,
     True, True, (('with_menu', True),), n_("Lazy Connect with Socket Menu")),
    # Align Nodes
    ("node.nw_align_nodes", 'EQUAL', 'PRESS', False, True,
     False, None, n_("Align Nodes")),
    # Reset Nodes (Back Space)
    ("node.nw_reset_nodes", 'BACK_SPACE', 'PRESS', False, False,
     False, None, n_("Reset Nodes")),
    # MENUS
    ('wm.call_menu', 'W', 'PRESS', False, True, False,
     (('name', interface.NodeWranglerMenu.bl_idname),), n_("Node Wrangler (Menu)")),
    ('wm.call_menu', 'SLASH', 'PRESS', False, False, False,
     (('name', interface.NWAddReroutesMenu.bl_idname),), n_("Add Reroutes (Menu)")),
    ('wm.call_menu', 'NUMPAD_SLASH', 'PRESS', False, False, False,
     (('name', interface.NWAddReroutesMenu.bl_idname),), n_("Add Reroutes (Menu)")),
    ('wm.call_menu', 'BACK_SLASH', 'PRESS', False, False, False,
     (('name', interface.NWLinkActiveToSelectedMenu.bl_idname),), n_("Link Active to Selected (Menu)")),
    ('wm.call_menu', 'C', 'PRESS', False, True, False,
     (('name', interface.NWCopyToSelectedMenu.bl_idname),), n_("Copy to Selected (Menu)")),
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
        for (identifier, key, action, CTRL, SHIFT, ALT, props, nicename) in kmi_defs:
            kmi = km.keymap_items.new(identifier, key, action, ctrl=CTRL, shift=SHIFT, alt=ALT)
            if props:
                for prop, value in props:
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
