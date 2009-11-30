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
from bpy.props import *


class SelectPattern(bpy.types.Operator):
    '''Select object matching a naming pattern.'''
    bl_idname = "object.select_pattern"
    bl_label = "Select Pattern"
    bl_register = True
    bl_undo = True

    pattern = StringProperty(name="Pattern", description="Name filter using '*' and '?' wildcard chars", maxlen=32, default="*")
    case_sensitive = BoolProperty(name="Case Sensitive", description="Do a case sensitive compare", default=False)
    extend = BoolProperty(name="Extend", description="Extend the existing selection", default=True)

    def execute(self, context):

        import fnmatch

        if self.properties.case_sensitive:
            pattern_match = fnmatch.fnmatchcase
        else:
            pattern_match = lambda a, b: fnmatch.fnmatchcase(a.upper(), b.upper())

        for ob in context.visible_objects:
            if pattern_match(ob.name, self.properties.pattern):
                ob.selected = True
            elif not self.properties.extend:
                ob.selected = False

        return ('FINISHED',)

    def invoke(self, context, event):
        wm = context.manager
        wm.invoke_props_popup(self, event)
        return ('RUNNING_MODAL',)
    
    def draw(self, context):
        print("WoW")
        layout = self.layout
        props = self.properties
        
        layout.prop(props, "pattern")
        row = layout.row()
        row.prop(props, "case_sensitive")
        row.prop(props, "extend")
        


class SubsurfSet(bpy.types.Operator):
    '''Sets a Subdivision Surface Level (1-5)'''

    bl_idname = "object.subsurf_set"
    bl_label = "Subsurf Set"
    bl_register = True
    bl_undo = True

    level = IntProperty(name="Level",
            default=1, min=0, max=6)

    def poll(self, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        level = self.properties.level
        ob = context.active_object
        for mod in ob.modifiers:
            if mod.type == 'SUBSURF':
                if mod.levels != level:
                    mod.levels = level
                return ('FINISHED',)

        # adda new modifier
        mod = ob.modifiers.new("Subsurf", 'SUBSURF')
        mod.levels = level
        return ('FINISHED',)


class Retopo(bpy.types.Operator):
    '''TODO - doc'''

    bl_idname = "object.retopology"
    bl_label = "Retopology from Grease Pencil"
    bl_register = True
    bl_undo = True

    def execute(self, context):
        import retopo
        retopo.main()
        return ('FINISHED',)


bpy.ops.add(SelectPattern)
bpy.ops.add(SubsurfSet)
bpy.ops.add(Retopo)
