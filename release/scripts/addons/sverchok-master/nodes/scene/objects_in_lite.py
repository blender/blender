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

import bpy
from bpy.props import BoolProperty, StringProperty, EnumProperty

import sverchok
from sverchok.utils.mesh_repr_utils import flatten, unflatten, generate_object
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


import json

class SvObjLiteCallback(bpy.types.Operator):
    """ GET / Reject object callback"""
    bl_idname = "node.sverchok_objectinlite_cb"
    bl_label = "Sverchok object in lite callback"
    bl_options = {'REGISTER', 'UNDO'}

    cmd = StringProperty()

    def execute(self, context):
        getattr(context.node, self.cmd)()
        return {'FINISHED'}


class SvObjInLite(bpy.types.Node, SverchCustomTreeNode):
    ''' Objects Input Lite'''
    bl_idname = 'SvObjInLite'
    bl_label = 'Objects in Lite'
    bl_icon = 'OUTLINER_OB_EMPTY'

    modifiers = BoolProperty(
        description='Apply modifier geometry to import (original untouched)',
        name='Modifiers', default=False, update=updateNode)

    currently_storing = BoolProperty()
    obj_name = StringProperty(update=updateNode)
    node_dict = {}

    def drop(self):
        self.obj_name = ""
        self.currently_storing = False
        self.node_dict = {}

    def dget(self, obj_name=None):
        if not obj_name:
            obj = bpy.context.active_object
            self.obj_name = obj.name
        else:
            obj = bpy.data.objects.get(obj_name)

        if obj:
            obj_data = obj.to_mesh(bpy.context.scene, self.modifiers, 'PREVIEW')
            self.node_dict[hash(self)] = {
                'Vertices': list([v.co[:] for v in obj_data.vertices]),
                'Edges': obj_data.edge_keys,
                'Polygons': [list(p.vertices) for p in obj_data.polygons],
                'Matrix': [list(m) for m in obj.matrix_world]
            }
            
            bpy.data.meshes.remove(obj_data)
            self.currently_storing = True

        else:
            self.report({'WARNING'}, 'No object selected')


    def sv_init(self, context):
        out = self.outputs.new
        out('VerticesSocket', 'Vertices')
        out('StringsSocket', 'Edges')
        out('StringsSocket', 'Polygons')
        out('MatrixSocket', 'Matrix')

    def draw_buttons(self, context, layout):
        addon = context.user_preferences.addons.get(sverchok.__name__)
        prefs = addon.preferences
        callback = 'node.sverchok_objectinlite_cb'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.scale_y = 4.0 if prefs.over_sized_buttons else 1
        
        if not self.currently_storing:
            row.operator(callback, text='G E T').cmd = 'dget'
            row.prop(self, 'modifiers', text='', icon='MODIFIER')
            layout.label('--None--')
        else:
            row.operator(callback, text='D R O P').cmd = 'drop'
            row.prop(self, 'modifiers', text='', icon='MODIFIER')
            layout.label(self.obj_name)


    def process(self):

        if not hash(self) in self.node_dict:
            if self.obj_name and bpy.data.objects.get(self.obj_name):
                self.dget(self.obj_name)
            else:
                print('ending early, no node_dict')
                return
        
        mesh_data = self.node_dict.get(hash(self))
        
        for socket in self.outputs:
            if socket.is_linked:
                socket.sv_set([mesh_data[socket.name]])


    def storage_set_data(self, storage):
        geom = storage['geom']
        name = storage['params']["obj_name"]
        geom_dict = json.loads(geom)

        if not geom_dict:
            print(self.name, 'contains no flatten geom')
            return

        unrolled_geom = unflatten(geom_dict)
        verts = unrolled_geom['Vertices']
        edges = unrolled_geom['Edges']
        polygons = unrolled_geom['Polygons']
        matrix = unrolled_geom['Matrix']

        bm = bmesh_from_pydata(verts, edges, polygons)
        obj = generate_object(name, bm)
        obj.matrix_world = matrix

        # rename if obj existed
        if not obj.name == name:
            storage['params']["obj_name"] = obj.name
            self.id_data.freeze(hard=True)
            self.obj_name = obj.name
            self.id_data.unfreeze(hard=True)


    def storage_get_data(self, storage):
        # generate flat data, and inject into incoming storage variable
        obj = self.node_dict.get(hash(self))
        if not obj:
            print('failed to obtain local geometry, can not add to json')
            return
        
        storage['geom'] = json.dumps(flatten(obj))



def register():
    bpy.utils.register_class(SvObjLiteCallback)
    bpy.utils.register_class(SvObjInLite)


def unregister():
    bpy.utils.unregister_class(SvObjInLite)
    bpy.utils.unregister_class(SvObjLiteCallback)
