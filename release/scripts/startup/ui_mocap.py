import bpy
import time

from bpy.props import *
from bpy import *
from mathutils import Vector
from math import isfinite

bpy.types.Scene.performer = bpy.props.StringProperty()
bpy.types.Scene.enduser = bpy.props.StringProperty()

bpy.types.Bone.map = bpy.props.StringProperty()
import retarget
import mocap_tools

class MocapPanel(bpy.types.Panel):
    bl_label = "Mocap tools"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"
 
    def draw(self, context):
        self.layout.label("Preprocessing")
        row = self.layout.row(align=True)
        row.alignment = 'EXPAND'
        row.operator("mocap.samples", text='Samples to Beziers')
        row.operator("mocap.denoise", text='Clean noise')
        row2 = self.layout.row(align=True)
        row2.operator("mocap.looper", text='Loop animation')
        row2.operator("mocap.limitdof", text='Constrain Rig')
        self.layout.label("Retargeting")
        row3 = self.layout.row(align=True)
        column1 = row3.column(align=True)
        column1.label("Performer Rig")
        column1.prop_search(context.scene, "performer",  context.scene, "objects")
        column2 = row3.column(align=True)
        column2.label("Enduser Rig")
        column2.prop_search(context.scene, "enduser",  context.scene, "objects")
        self.layout.label("Hierarchy mapping")
        if context.scene.performer in bpy.data.armatures and context.scene.enduser in bpy.data.armatures:
            perf = bpy.data.armatures[context.scene.performer]
            enduser_arm = bpy.data.armatures[context.scene.enduser]
            for bone in perf.bones:
                row = self.layout.row(align=True)
                row.label(bone.name)
                row.prop_search(bone, "map", enduser_arm, "bones")
            self.layout.operator("mocap.retarget", text='RETARGET!')
        
           
        
        
class OBJECT_OT_RetargetButton(bpy.types.Operator):
    bl_idname = "mocap.retarget"
    bl_label = "Retargets active action from Performer to Enduser"
 
    def execute(self, context):
        retarget.totalRetarget()
        return {"FINISHED"}
    
class OBJECT_OT_ConvertSamplesButton(bpy.types.Operator):
    bl_idname = "mocap.samples"
    bl_label = "Converts samples / simplifies keyframes to beziers"
 
    def execute(self, context):
        mocap_tools.fcurves_simplify()
        return {"FINISHED"}
    
class OBJECT_OT_LooperButton(bpy.types.Operator):
    bl_idname = "mocap.looper"
    bl_label = "loops animation / sampled mocap data"
 
    def execute(self, context):
        mocap_tools.autoloop_anim()
        return {"FINISHED"}
    
class OBJECT_OT_DenoiseButton(bpy.types.Operator):
    bl_idname = "mocap.denoise"
    bl_label = "Denoises sampled mocap data "
 
    def execute(self, context):
        return {"FINISHED"}

class OBJECT_OT_LimitDOFButton(bpy.types.Operator):
    bl_idname = "mocap.limitdof"
    bl_label = "Analyzes animations Max/Min DOF and adds hard/soft constraints"
 
    def execute(self, context):
        return {"FINISHED"}

def register():
   bpy.utils.register_module(__name__)
 
def unregister():
    bpy.utils.unregister_module(__name__)
    
if __name__=="__main__":
    register()   
