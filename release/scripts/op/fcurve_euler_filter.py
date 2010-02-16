from math import *
import bpy
from Mathutils import *

def main(context):
    def cleanupEulCurve(fcv):
        keys = []
    
        for k in fcv.keyframe_points:
            keys.append([k.handle1.copy(), k.co.copy(), k.handle2.copy()])   
        print(keys)
    
        for i in range(len(keys)):
            cur = keys[i]
            prev = keys[i-1] if i > 0 else None
            next = keys[i+1] if i < len(keys)-1 else None
    
            if prev == None:
                continue
            
            th = pi
            if abs(prev[1][1] - cur[1][1]) >= th: # more than 180 degree jump
                fac = pi*2
                if prev[1][1] > cur[1][1]:
                    while abs(cur[1][1]-prev[1][1]) >= th: # < prev[1][1]:
                        cur[0][1] += fac
                        cur[1][1] += fac
                        cur[2][1] += fac
                elif prev[1][1] < cur[1][1]:
                    while abs(cur[1][1]-prev[1][1]) >= th:
                        cur[0][1] -= fac
                        cur[1][1] -= fac
                        cur[2][1] -= fac
    
        for i in range(len(keys)):
            for x in range(2):
               fcv.keyframe_points[i].handle1[x] = keys[i][0][x]
               fcv.keyframe_points[i].co[x] = keys[i][1][x]
               fcv.keyframe_points[i].handle2[x] = keys[i][2][x]
      
    flist = bpy.context.active_object.animation_data.action.fcurves
    for f in flist:
        if f.selected and f.data_path.endswith("rotation_euler"):
            cleanupEulCurve(f)

class DiscontFilterOp(bpy.types.Operator):
    """Fixes the most common causes of gimbal lock in the fcurves of the active bone"""
    bl_idname = "graph.discont_filter"
    bl_label = "Filter out discontinuities in the active fcurves"

    def poll(self, context):
        return context.active_object != None

    def execute(self, context):
        main(context)
        return {'FINISHED'}

def register():
    bpy.types.register(DiscontFilterOp)

def unregister():
    bpy.types.unregister(DiscontFilterOp)

if __name__ == "__main__":
    register()

