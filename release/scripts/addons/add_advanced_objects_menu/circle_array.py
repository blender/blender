# gpl author: Antonis Karvelas

# -*- coding: utf-8 -*-

bl_info = {
    "name": "Circle Array",
    "author": "Antonis Karvelas",
    "version": (1, 0, 1),
    "blender": (2, 6, 7),
    "location": "View3D > Object > Circle_Array",
    "description": "Uses an existing array and creates an empty, "
                   "rotates it properly and makes a Circle Array",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"
    }


import bpy
from bpy.types import Operator
from math import radians


class Circle_Array(Operator):
    bl_label = "Circle Array"
    bl_idname = "objects.circle_array_operator"
    bl_description = ("Creates an Array Modifier with offset empty object\n"
                      "Works with Mesh, Curve, Text and Surface\n"
                      "Use an object with an existing Array modifier\n"
                      "or rotate the newly created Empty with the name pattern\n"
                      "EMPTY_C_Array_ if the Array doesn't exist (angle: 360/Count)")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def check_empty_name(self, context):
        new_name, def_name = "", "EMPTY_C_Array"
        suffix = 1
        try:
            # first slap a simple linear count + 1 for numeric suffix, if it fails
            # harvest for the rightmost numbers and append the max value
            list_obj = []
            obj_all = context.scene.objects
            list_obj = [obj.name for obj in obj_all if obj.name.startswith(def_name)]
            new_name = "{}_{}".format(def_name, len(list_obj) + 1)

            if new_name in list_obj:
                from re import findall
                test_num = [findall("\d+", words) for words in list_obj]
                suffix += max([int(l[-1]) for l in test_num])
                new_name = "{}_{}".format(def_name, suffix)
            return new_name
        except:
            return None

    def execute(self, context):
        is_allowed = True
        try:
            allowed_obj = ['MESH', 'CURVE', 'SURFACE', 'FONT']
            for obj in context.selected_objects:
                if obj.type not in allowed_obj:
                    is_allowed = False
                    break

            if not is_allowed:
                self.report(
                    {"WARNING"},
                    "The Active/Selected objects are not of "
                    "Mesh, Curve, Surface or Font type. Operation Cancelled"
                    )
                return {'CANCELLED'}

            default_name = self.check_empty_name(context) or "EMPTY_C_Array"
            bpy.ops.object.modifier_add(type='ARRAY')

            if len(context.selected_objects) == 2:
                selected = context.selected_objects
                lists = [obj for obj in selected if obj != context.active_object]
                active = lists[0]
                # check if the list object has a modifier
                check_mod = None
                for mod in active.modifiers[:]:
                    if mod.type == "ARRAY":
                        check_mod = mod
                        break

                if check_mod:
                    check_mod.use_object_offset = True
                    check_mod.use_relative_offset = False
                else:
                    # fallback
                    bpy.context.scene.objects.active = active
                    bpy.ops.object.modifier_add(type='ARRAY')
                    active.modifiers[0].use_object_offset = True
                    active.modifiers[0].use_relative_offset = False

                active.modifiers[0].use_object_offset = True
                active.modifiers[0].use_relative_offset = False
                active.select = False
                bpy.context.scene.objects.active = context.active_object
                bpy.ops.view3d.snap_cursor_to_selected()

                if active.modifiers[0].offset_object is None:
                    bpy.ops.object.add(type='EMPTY')
                    empty_name = bpy.context.active_object
                    empty_name.name = default_name
                    active.modifiers[0].offset_object = empty_name
                else:
                    empty_name = active.modifiers[0].offset_object

                bpy.context.scene.objects.active = active
                num = active.modifiers["Array"].count
                rotate_num = 360 / num
                active.select = True
                bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
                empty_name.rotation_euler = (0, 0, radians(rotate_num))
                empty_name.select = False
                active.select = True
                bpy.ops.object.origin_set(type="ORIGIN_CURSOR")

                return {'FINISHED'}
            else:
                active = context.active_object
                active.modifiers[0].use_object_offset = True
                active.modifiers[0].use_relative_offset = False
                bpy.ops.view3d.snap_cursor_to_selected()

                if active.modifiers[0].offset_object is None:
                    bpy.ops.object.add(type='EMPTY')
                    empty_name = bpy.context.active_object
                    empty_name.name = default_name
                    active.modifiers[0].offset_object = empty_name
                else:
                    empty_name = active.modifiers[0].offset_object

                bpy.context.scene.objects.active = active
                num = active.modifiers["Array"].count
                rotate_num = 360 / num
                active.select = True
                bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
                empty_name.rotation_euler = (0, 0, radians(rotate_num))
                empty_name.select = False
                active.select = True

                return {'FINISHED'}

        except Exception as e:
            self.report({'WARNING'},
                        "Circle Array operator could not be executed (See the console for more info)")
            print("\n[objects.circle_array_operator]\nError: {}\n".format(e))

            return {'CANCELLED'}


# Register
def register():
    bpy.utils.register_class(Circle_Array)


def unregister():
    bpy.utils.unregister_class(Circle_Array)


if __name__ == "__main__":
    register()
