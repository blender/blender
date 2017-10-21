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

# <pep8 compliant>
import bpy

__all__ = (
    "ToolSelectPanelHelper",
)


class ToolSelectPanelHelper:
    """
    Generic Class, can be used for any toolbar.

    - keymap_prefix:
      The text prefix for each key-map for this spaces tools.
    - tools_all():
      Returns all tools defined.
    - tools_from_context(context):
      Returns tools available in this context.

    Each tool is a triplet:
      ``(tool_name, manipulator_group_idname, keymap_actions)``
    For a separator in the toolbar, use ``None``.

      Where:
      ``tool_name``
        is the name to display in the interface.
      ``manipulator_group_idname``
        is an optional manipulator group to activate when the tool is set.
      ``keymap_actions``
        an optional triple of: ``(operator_id, operator_properties, keymap_item_args)``
    """

    @classmethod
    def _km_actionmouse_simple(cls, kc, text, actions):

        # standalone
        def props_assign_recursive(rna_props, py_props):
            for prop_id, value in py_props.items():
                if isinstance(value, dict):
                    props_assign_recursive(getattr(rna_props, prop_id), value)
                else:
                    setattr(rna_props, prop_id, value)

        km_idname = cls.keymap_prefix + text
        km = kc.keymaps.new(km_idname, space_type=cls.bl_space_type, region_type='WINDOW')
        for op_idname, op_props_dict, kmi_kwargs in actions:
            kmi = km.keymap_items.new(op_idname, **kmi_kwargs)
            kmi_props = kmi.properties
            if op_props_dict:
                props_assign_recursive(kmi.properties, op_props_dict)
        return km, km_idname

    @classmethod
    def register(cls):
        wm = bpy.context.window_manager

        # XXX, should we be manipulating the user-keyconfig on load?
        # Perhaps this should only add when keymap items don't already exist.
        #
        # This needs some careful consideration.
        kc = wm.keyconfigs.user

        # {tool_name: (keymap, keymap_idname, manipulator_group_idname), ...}
        cls._tool_keymap = {}

        for t in cls.tools_all():
            text, mp_idname, actions = t
            if actions is not None:
                km, km_idname = cls._km_actionmouse_simple(kc, text, actions)
                cls._tool_keymap[text] = km, km_idname

    def draw(self, context):
        # XXX, this UI isn't very nice.
        # We might need to create new button types for this.
        # Since we probably want:
        # - tool-tips that include multiple key shortcuts.
        # - ability to click and hold to expose sub-tools.

        workspace = context.workspace
        km_idname_active = workspace.tool_keymap or None
        mp_idname_active = workspace.tool_manipulator_group or None
        layout = self.layout

        for tool_items in self.tools_from_context(context):
            if tool_items:
                col = layout.box().column()
                for item in tool_items:
                    if item is None:
                        col = layout.box().column()
                        continue
                    text, mp_idname, actions = item

                    if actions is not None:
                        km, km_idname = self._tool_keymap[text]
                    else:
                        km = None
                        km_idname = None

                    props = col.operator(
                        "wm.tool_set",
                        text=text,
                        emboss=(
                            km_idname_active == km_idname and
                            mp_idname_active == mp_idname
                        ),
                    )

                    props.keymap = km_idname or ""
                    props.manipulator_group = mp_idname or ""
