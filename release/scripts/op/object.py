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

import bpy

class SubsurfSet(bpy.types.Operator):
    '''TODO, doc.'''

    bl_idname = "object.subsurf_set"
    bl_label = "Subsurf Set"
    bl_register = True
    bl_undo = True
    
    level = bpy.props.IntProperty(name="Level",
            default=1, min=0, max=6)

    def poll(self, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        ob = context.active_object
        for mod in ob.modifiers:
            if mod.type == 'SUBSURF':
                if mod.levels != level:
                    mod.levels = level
                return
        
        # adda new modifier
        bpy.ops.object.modifier_add(type='SUBSURF') # TODO, support adding directly
        mod = ob.modifiers[-1]
        mod.levels = level
        return ('FINISHED',)


# Register the operator
bpy.ops.add(SubsurfSet)
