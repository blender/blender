# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import FloatVectorProperty, IntProperty


class SCULPT_OT_face_set_custom_color_set(Operator):
    """Set custom color for the active Face Set"""
    bl_idname = "sculpt.face_set_custom_color_set"
    bl_label = "Set Face Set Custom Color"
    bl_options = {'REGISTER', 'UNDO'}

    color: FloatVectorProperty(
        name="Color",
        description="Custom color for the Face Set",
        subtype='COLOR',
        default=(1.0, 0.0, 0.0),
        min=0.0,
        max=1.0,
        size=3
    )

    face_set_id: IntProperty(
        name="Face Set ID",
        description="ID of the Face Set to color",
        default=1,
        min=1
    )

    @classmethod
    def poll(cls, context):
        return (context.active_object and 
                context.active_object.type == 'MESH' and
                context.mode == 'SCULPT')

    def execute(self, context):
        obj = context.active_object
        mesh = obj.data
        
        # DEBUG: Print detailed information
        print(f"[DEBUG] SCULPT_OT_face_set_custom_color_set.execute()")
        print(f"[DEBUG] Object: {obj.name}")
        print(f"[DEBUG] Mesh: {mesh.name}")
        print(f"[DEBUG] Face Set ID: {self.face_set_id}")
        print(f"[DEBUG] Color: RGB({self.color[0]:.3f}, {self.color[1]:.3f}, {self.color[2]:.3f})")
        print(f"[DEBUG] Mesh face_set_colors_num: {getattr(mesh, 'face_set_colors_num', 'NOT_FOUND')}")
        print(f"[DEBUG] Mesh face_set_colors: {getattr(mesh, 'face_set_colors', 'NOT_FOUND')}")
        
        # First, try to create Face Sets if they don't exist
        try:
            print(f"[DEBUG] Checking if Face Sets exist...")
            
            # Check if sculpt_face_set attribute exists
            if not hasattr(mesh, 'attributes') or not mesh.attributes.get('.sculpt_face_set'):
                print(f"[DEBUG] No Face Sets found, creating them...")
                # Create Face Sets using the existing operator
                create_result = bpy.ops.sculpt.face_sets_init()
                print(f"[DEBUG] Face Sets creation result: {create_result}")
                
                if create_result != {'FINISHED'}:
                    self.report({'WARNING'}, f"Failed to create Face Sets. Result: {create_result}")
                    return {'CANCELLED'}
            else:
                print(f"[DEBUG] Face Sets already exist")
                
        except Exception as e:
            print(f"[DEBUG] Error creating Face Sets: {e}")
            self.report({'WARNING'}, f"Could not create Face Sets: {e}")
        
        # Call the existing C++ operator
        try:
            print(f"[DEBUG] Calling bpy.ops.sculpt.face_set_set_custom_color...")
            result = bpy.ops.sculpt.face_set_set_custom_color(
                color=self.color,
                face_set_id=self.face_set_id
            )
            print(f"[DEBUG] C++ operator result: {result}")
            
            if result == {'FINISHED'}:
                self.report({'INFO'}, f"Successfully set Face Set {self.face_set_id} color to RGB({self.color[0]:.2f}, {self.color[1]:.2f}, {self.color[2]:.2f})")
            else:
                self.report({'WARNING'}, f"Failed to set Face Set color. Result: {result}")
                
        except Exception as e:
            print(f"[DEBUG] Error calling C++ operator: {e}")
            self.report({'ERROR'}, f"Error setting Face Set color: {e}")
        
        return {'FINISHED'}

    def invoke(self, context, event):
        # Get active face set ID if possible
        if hasattr(context, 'sculpt_object') and context.sculpt_object:
            # Try to get the active face set ID from sculpt context
            pass
        
        return context.window_manager.invoke_props_dialog(self)


class SCULPT_OT_face_set_custom_color_clear(Operator):
    """Clear custom color for the active Face Set"""
    bl_idname = "sculpt.face_set_custom_color_clear"
    bl_label = "Clear Face Set Custom Color"
    bl_options = {'REGISTER', 'UNDO'}

    face_set_id: IntProperty(
        name="Face Set ID",
        description="ID of the Face Set to clear color",
        default=1,
        min=1
    )

    @classmethod
    def poll(cls, context):
        return (context.active_object and 
                context.active_object.type == 'MESH' and
                context.mode == 'SCULPT')

    def execute(self, context):
        obj = context.active_object
        mesh = obj.data
        
        # DEBUG: Print detailed information
        print(f"[DEBUG] SCULPT_OT_face_set_custom_color_clear.execute()")
        print(f"[DEBUG] Object: {obj.name}")
        print(f"[DEBUG] Mesh: {mesh.name}")
        print(f"[DEBUG] Face Set ID: {self.face_set_id}")
        print(f"[DEBUG] Mesh face_set_colors_num: {getattr(mesh, 'face_set_colors_num', 'NOT_FOUND')}")
        
        # Call the C++ operator
        try:
            print(f"[DEBUG] Calling bpy.ops.sculpt.face_set_clear_custom_color...")
            result = bpy.ops.sculpt.face_set_clear_custom_color(
                face_set_id=self.face_set_id
            )
            print(f"[DEBUG] C++ operator result: {result}")
            
            if result == {'FINISHED'}:
                self.report({'INFO'}, f"Successfully cleared Face Set {self.face_set_id} custom color")
            else:
                self.report({'WARNING'}, f"Failed to clear Face Set color. Result: {result}")
                
        except Exception as e:
            print(f"[DEBUG] Error calling C++ operator: {e}")
            self.report({'ERROR'}, f"Error clearing Face Set color: {e}")
        
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)


class SCULPT_OT_face_set_custom_color_clear_all(Operator):
    """Clear all custom Face Set colors"""
    bl_idname = "sculpt.face_set_custom_color_clear_all"
    bl_label = "Clear All Face Set Custom Colors"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.active_object and 
                context.active_object.type == 'MESH' and
                context.mode == 'SCULPT')

    def execute(self, context):
        obj = context.active_object
        mesh = obj.data
        
        # DEBUG: Print detailed information
        print(f"[DEBUG] SCULPT_OT_face_set_custom_color_clear_all.execute()")
        print(f"[DEBUG] Object: {obj.name}")
        print(f"[DEBUG] Mesh: {mesh.name}")
        print(f"[DEBUG] Mesh face_set_colors_num: {getattr(mesh, 'face_set_colors_num', 'NOT_FOUND')}")
        
        # Call the C++ operator
        try:
            print(f"[DEBUG] Calling bpy.ops.sculpt.face_set_clear_all_custom_colors...")
            result = bpy.ops.sculpt.face_set_clear_all_custom_colors()
            print(f"[DEBUG] C++ operator result: {result}")
            
            if result == {'FINISHED'}:
                self.report({'INFO'}, "Successfully cleared all Face Set custom colors")
            else:
                self.report({'WARNING'}, f"Failed to clear all Face Set colors. Result: {result}")
                
        except Exception as e:
            print(f"[DEBUG] Error calling C++ operator: {e}")
            self.report({'ERROR'}, f"Error clearing all Face Set colors: {e}")
        
        return {'FINISHED'}


classes = (
    SCULPT_OT_face_set_custom_color_set,
    SCULPT_OT_face_set_custom_color_clear,
    SCULPT_OT_face_set_custom_color_clear_all,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()