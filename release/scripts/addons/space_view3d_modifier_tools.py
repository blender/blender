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
# by meta-androcto, saidenka #

bl_info = {
    "name": "Modifier Tools",
    "author": "Meta Androcto, saidenka",
    "version": (0, 2, 5),
    "blender": (2, 77, 0),
    "location": "Properties > Modifiers",
    "description": "Modifiers Specials Show/Hide/Apply Selected",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6"
                "/Py/Scripts/3D_interaction/modifier_tools",
    "category": "3D View"
    }

import bpy
from bpy.types import Operator


class ApplyAllModifiers(Operator):
    bl_idname = "object.apply_all_modifiers"
    bl_label = "Apply All Modifiers"
    bl_description = ("Apply All modifiers of the selected object(s) \n"
                      "Active object has to have modifiers for the option to show up")
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        is_select, is_mod = False, False
        message_a, message_b = "", ""
        # collect names for objects failed to apply modifiers
        collect_names = []

        for obj in bpy.context.selected_objects:
            is_select = True

            # copying context for the operator's override
            contx = bpy.context.copy()
            contx['object'] = obj

            for mod in obj.modifiers[:]:
                contx['modifier'] = mod
                is_mod = True
                try:
                    bpy.ops.object.modifier_apply(
                                        contx, apply_as='DATA',
                                        modifier=contx['modifier'].name
                                        )
                except:
                    obj_name = getattr(obj, "name", "NO NAME")
                    collect_names.append(obj_name)
                    message_b = True
                    pass

        if is_select:
            if is_mod:
                message_a = "Applying modifiers on all Selected Objects"
            else:
                message_a = "No Modifiers on Selected Objects"
        else:
            self.report({"INFO"}, "No Selection. No changes applied")
            return {'CANCELLED'}

        # applying failed for some objects, show report
        message_obj = (",".join(collect_names) if collect_names and
                       len(collect_names) < 8 else "some objects (Check System Console)")

        self.report({"INFO"},
                    (message_a if not message_b else
                    "Applying modifiers failed for {}".format(message_obj)))

        if (collect_names and message_obj == "some objects (Check System Console)"):
            print("\n[Modifier Tools]\n\nApplying failed on:"
                  "\n\n{} \n".format(", ".join(collect_names)))

        return {'FINISHED'}


class DeleteAllModifiers(Operator):
    bl_idname = "object.delete_all_modifiers"
    bl_label = "Remove All Modifiers"
    bl_description = "Remove All modifiers of the selected object(s)"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        is_select, is_mod = False, False
        message_a = ""

        for obj in context.selected_objects:
            is_select = True
            modifiers = obj.modifiers[:]
            for modi in modifiers:
                is_mod = True
                obj.modifiers.remove(modi)

        if is_select:
            if is_mod:
                message_a = "Removing modifiers on all Selected Objects"
            else:
                message_a = "No Modifiers on Selected Objects"
        else:
            self.report({"INFO"}, "No Selection. No changes applied")
            return {'CANCELLED'}

        self.report({"INFO"}, message_a)
        return {'FINISHED'}


class ToggleApplyModifiersView(Operator):
    bl_idname = "object.toggle_apply_modifiers_view"
    bl_label = "Toggle Visibility of Modifiers"
    bl_description = "Shows/Hide modifiers of the active / selected object(s) in the 3D View"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        is_apply = True
        message_a = ""

        # avoid toggling not exposed modifiers (currently only Collision, see T53406)
        skip_type = ["COLLISION"]   # types of modifiers to skip
        skipped = set()             # collect names
        count_modifiers = 0         # check for message_a (all skipped)

        # check if the active object has only one non exposed modifier as the logic will fail
        if len(context.active_object.modifiers) == 1 and \
                context.active_object.modifiers[0].type in skip_type:

            for obj in context.selected_objects:
                for mod in obj.modifiers:
                    if mod.type in skip_type:
                        skipped.add(mod.name)
                        continue

                    if mod.show_viewport:
                        is_apply = False
                        break
        else:
            for mod in context.active_object.modifiers:
                if mod.type in skip_type:
                    skipped.add(mod.name)
                    continue

                if mod.show_viewport:
                    is_apply = False
                    break

        count_modifiers = len(context.active_object.modifiers)
        # active object - no selection
        for mod in context.active_object.modifiers:
            if mod.type in skip_type:
                count_modifiers -= 1
                skipped.add(mod.name)
                continue

            mod.show_viewport = is_apply

        for obj in context.selected_objects:
            count_modifiers += len(obj.modifiers)

            for mod in obj.modifiers:
                if mod.type in skip_type:
                    skipped.add(mod.name)
                    count_modifiers -= 1
                    continue

                mod.show_viewport = is_apply

        message_a = "{} modifiers in the 3D View".format("Displaying" if is_apply else "Hidding")

        if skipped:
            message_a = "{}, {}".format(message_a, "skipping: " + ", ".join(skipped)) if \
                        count_modifiers > 0 else "No change of Modifiers' Visibility, all skipped"

        self.report({"INFO"}, message_a)

        return {'FINISHED'}


class ToggleAllShowExpanded(Operator):
    bl_idname = "wm.toggle_all_show_expanded"
    bl_label = "Expand/Collapse All Modifiers"
    bl_description = "Expand/Collapse Modifier Stack"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        obj = context.active_object
        if (len(obj.modifiers)):
            vs = 0
            for mod in obj.modifiers:
                if (mod.show_expanded):
                    vs += 1
                else:
                    vs -= 1
            is_close = False
            if (0 < vs):
                is_close = True
            for mod in obj.modifiers:
                mod.show_expanded = not is_close
        else:
            self.report({'WARNING'}, "Not a single modifier to Expand/Collapse")
            return {'CANCELLED'}

        for area in context.screen.areas:
            area.tag_redraw()
        return {'FINISHED'}


# Menus #
def menu(self, context):
    if (context.active_object):
        if (len(context.active_object.modifiers)):
            col = self.layout.column(align=True)

            row = col.row(align=True)
            row.operator(ApplyAllModifiers.bl_idname,
                         icon='IMPORT', text="Apply All")
            row.operator(DeleteAllModifiers.bl_idname,
                         icon='X', text="Delete All")

            row = col.row(align=True)
            row.operator(ToggleApplyModifiersView.bl_idname,
                         icon='RESTRICT_VIEW_OFF',
                         text="Viewport Vis")
            row.operator(ToggleAllShowExpanded.bl_idname,
                         icon='FULLSCREEN_ENTER',
                         text="Toggle Stack")


def menu_func(self, context):
    if (context.active_object):
        if (len(context.active_object.modifiers)):
            layout = self.layout
            layout.separator()
            layout.operator(ApplyAllModifiers.bl_idname,
                            icon='IMPORT',
                            text="Apply All Modifiers")


def register():
    bpy.utils.register_module(__name__)

    # Add "Specials" menu to the "Modifiers" menu
    bpy.types.DATA_PT_modifiers.prepend(menu)

    # Add apply operator to the Apply 3D View Menu
    bpy.types.VIEW3D_MT_object_apply.append(menu_func)


def unregister():
    # Remove "Specials" menu from the "Modifiers" menu.
    bpy.types.DATA_PT_modifiers.remove(menu)

    # Remove apply operator to the Apply 3D View Menu
    bpy.types.VIEW3D_MT_object_apply.remove(menu_func)

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
