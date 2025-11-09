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

from . import operators
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
        description="When merging nodes with the Ctrl+Numpad0 hotkey (and similar) specify whether to collapse them or show the full node with options expanded")
    merge_position: EnumProperty(
        name="Mix Node Position",
        items=(
            ("CENTER", "Center", "Place the Mix node between the two nodes"),
            ("BOTTOM", "Bottom", "Place the Mix node at the same height as the lowest node")
        ),
        default='CENTER',
        description="When merging nodes with the Ctrl+Numpad0 hotkey (and similar) specify the position of the new nodes")

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
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_0', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Automatic)")),
    (operators.NWMergeNodes.bl_idname, 'ZERO', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Automatic)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Add)")),
    (operators.NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Add)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Divide)")),
    (operators.NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), n_("Merge Nodes (Divide)")),
    (operators.NWMergeNodes.bl_idname, 'COMMA', 'PRESS', True, False, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Less Than)")),
    (operators.NWMergeNodes.bl_idname, 'PERIOD', 'PRESS', True, False, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Greater Than)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_PERIOD', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'ZCOMBINE'),), n_("Merge Nodes (Z-Combine)")),
    # NWMergeNodes with Ctrl Alt (MIX or ALPHAOVER)
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_0', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), n_("Merge Nodes (Alpha Over)")),
    (operators.NWMergeNodes.bl_idname, 'ZERO', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), n_("Merge Nodes (Alpha Over)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Add)")),
    (operators.NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Add)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Divide)")),
    (operators.NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), n_("Merge Nodes (Color, Divide)")),
    # NWMergeNodes with Ctrl Shift (MATH)
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Add)")),
    (operators.NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Add)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Multiply)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Subtract)")),
    (operators.NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Divide)")),
    (operators.NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Divide)")),
    (operators.NWMergeNodes.bl_idname, 'COMMA', 'PRESS', True, True, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Less than)")),
    (operators.NWMergeNodes.bl_idname, 'PERIOD', 'PRESS', True, True, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), n_("Merge Nodes (Math, Greater than)")),
    # BATCH CHANGE NODES
    # NWBatchChangeNodes with Alt
    (operators.NWBatchChangeNodes.bl_idname, 'NUMPAD_0', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), n_("Batch Change Blend Type (Mix)")),
    (operators.NWBatchChangeNodes.bl_idname, 'ZERO', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), n_("Batch Change Blend Type (Mix)")),
    (operators.NWBatchChangeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), n_("Batch Change Blend Type (Add)")),
    (operators.NWBatchChangeNodes.bl_idname, 'EQUAL', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), n_("Batch Change Blend Type (Add)")),
    (operators.NWBatchChangeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), n_("Batch Change Blend Type (Multiply)")),
    (operators.NWBatchChangeNodes.bl_idname, 'EIGHT', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), n_("Batch Change Blend Type (Multiply)")),
    (operators.NWBatchChangeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), n_("Batch Change Blend Type (Subtract)")),
    (operators.NWBatchChangeNodes.bl_idname, 'MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), n_("Batch Change Blend Type (Subtract)")),
    (operators.NWBatchChangeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), n_("Batch Change Blend Type (Divide)")),
    (operators.NWBatchChangeNodes.bl_idname, 'SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), n_("Batch Change Blend Type (Divide)")),
    (operators.NWBatchChangeNodes.bl_idname, 'COMMA', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'LESS_THAN'),), n_("Batch Change Blend Type (Current)")),
    (operators.NWBatchChangeNodes.bl_idname, 'PERIOD', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'GREATER_THAN'),), n_("Batch Change Blend Type (Current)")),
    (operators.NWBatchChangeNodes.bl_idname, 'DOWN_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'NEXT'), ('operation', 'NEXT'),), n_("Batch Change Blend Type (Next)")),
    (operators.NWBatchChangeNodes.bl_idname, 'UP_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'PREV'), ('operation', 'PREV'),), n_("Batch Change Blend Type (Previous)")),
    # LINK ACTIVE TO SELECTED
    # Don't use names, don't replace links (K)
    (operators.NWLinkActiveToSelected.bl_idname, 'K', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', False),), n_("Link Active to Selected (Don't Replace Links)")),
    # Don't use names, replace links (Shift K)
    (operators.NWLinkActiveToSelected.bl_idname, 'K', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', False),), n_("Link Active to Selected (Replace Links)")),
    # Use node name, don't replace links (')
    (operators.NWLinkActiveToSelected.bl_idname, 'QUOTE', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', True), ('use_outputs_names', False),), n_("Link Active to Selected (Don't Replace Links, Node Names)")),
    # Use node name, replace links (Shift ')
    (operators.NWLinkActiveToSelected.bl_idname, 'QUOTE', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', True), ('use_outputs_names', False),), n_("Link Active to Selected (Replace Links, Node Names)")),
    # Don't use names, don't replace links (;)
    (operators.NWLinkActiveToSelected.bl_idname, 'SEMI_COLON', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', True),), n_("Link Active to Selected (Don't Replace Links, Output Names)")),
    # Don't use names, replace links (')
    (operators.NWLinkActiveToSelected.bl_idname, 'SEMI_COLON', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', True),), n_("Link Active to Selected (Replace Links, Output Names)")),
    # CHANGE MIX FACTOR
    (operators.NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS', False,
     False, True, (('option', -0.1),), n_("Reduce Mix Factor by 0.1")),
    (operators.NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS', False,
     False, True, (('option', 0.1),), n_("Increase Mix Factor by 0.1")),
    (operators.NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS', False,
     True, True, (('option', -0.01),), n_("Reduce Mix Factor by 0.01")),
    (operators.NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS', False,
     True, True, (('option', 0.01),), n_("Increase Mix Factor by 0.01")),
    (operators.NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS',
     True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    (operators.NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS',
     True, True, True, (('option', 1.0),), n_("Set Mix Factor to 1.0")),
    (operators.NWChangeMixFactor.bl_idname, 'NUMPAD_0', 'PRESS',
     True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    (operators.NWChangeMixFactor.bl_idname, 'ZERO', 'PRESS', True, True, True, (('option', 0.0),), n_("Set Mix Factor to 0.0")),
    (operators.NWChangeMixFactor.bl_idname, 'NUMPAD_1', 'PRESS', True, True, True, (('option', 1.0),), n_("Mix Factor to 1.0")),
    (operators.NWChangeMixFactor.bl_idname, 'ONE', 'PRESS', True, True, True, (('option', 1.0),), n_("Set Mix Factor to 1.0")),
    # CLEAR LABEL (Alt L)
    (operators.NWClearLabel.bl_idname, 'L', 'PRESS', False, False, True, (('option', False),), n_("Clear Node Labels")),
    # MODIFY LABEL (Alt Shift L)
    (operators.NWModifyLabels.bl_idname, 'L', 'PRESS', False, True, True, None, n_("Modify Node Labels")),
    # Copy Label from active to selected
    (operators.NWCopyLabel.bl_idname, 'V', 'PRESS', False, True, False,
     (('option', 'FROM_ACTIVE'),), n_("Copy label from active to selected")),
    # DETACH OUTPUTS (Alt Shift D)
    (operators.NWDetachOutputs.bl_idname, 'D', 'PRESS', False, True, True, None, n_("Detach Outputs")),
    # LINK TO OUTPUT NODE (O)
    (operators.NWLinkToOutputNode.bl_idname, 'O', 'PRESS', False, False, False, None, n_("Link to Output Node")),
    # SELECT PARENT/CHILDREN
    # Select Children
    (operators.NWSelectParentChildren.bl_idname, 'RIGHT_BRACKET', 'PRESS',
     False, False, False, (('option', 'CHILD'),), n_("Select Children")),
    # Select Parent
    (operators.NWSelectParentChildren.bl_idname, 'LEFT_BRACKET', 'PRESS',
     False, False, False, (('option', 'PARENT'),), n_("Select Parent")),
    # Add Texture Setup
    (operators.NWAddTextureSetup.bl_idname, 'T', 'PRESS', True, False, False, None, n_("Add Texture Setup")),
    # Add Principled BSDF Texture Setup
    (operators.NWAddPrincipledSetup.bl_idname, 'T', 'PRESS', True, True, False, None, n_("Add Principled Texture Setup")),
    # Reset backdrop
    (operators.NWResetBG.bl_idname, 'Z', 'PRESS', False, False, False, None, n_("Reset Backdrop Image Zoom")),
    # Delete unused
    (operators.NWDeleteUnused.bl_idname, 'X', 'PRESS', False, False, True, None, n_("Delete Unused Nodes")),
    # Frame Selected
    ('node.join', 'P', 'PRESS', False, True, False, None, n_("Frame Selected Nodes")),
    # Swap Links
    (operators.NWSwapLinks.bl_idname, 'S', 'PRESS', False, False, True, None, n_("Swap Links")),
    # Reload Images
    (operators.NWReloadImages.bl_idname, 'R', 'PRESS', False, False, True, None, n_("Reload Images")),
    # Lazy Mix
    (operators.NWLazyMix.bl_idname, 'RIGHTMOUSE', 'PRESS', True, True, False, None, n_("Lazy Mix")),
    # Lazy Connect
    (operators.NWLazyConnect.bl_idname, 'RIGHTMOUSE', 'PRESS', False, False, True, (('with_menu', False),), n_("Lazy Connect")),
    # Lazy Connect with Menu
    (operators.NWLazyConnect.bl_idname, 'RIGHTMOUSE', 'PRESS', False,
     True, True, (('with_menu', True),), n_("Lazy Connect with Socket Menu")),
    # Align Nodes
    (operators.NWAlignNodes.bl_idname, 'EQUAL', 'PRESS', False, True,
     False, None, n_("Align Nodes")),
    # Reset Nodes (Back Space)
    (operators.NWResetNodes.bl_idname, 'BACK_SPACE', 'PRESS', False, False,
     False, None, n_("Reset Nodes")),
    # MENUS
    ('wm.call_menu', 'W', 'PRESS', False, True, False, (('name', interface.NodeWranglerMenu.bl_idname),), n_("Node Wrangler (Menu)")),
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
