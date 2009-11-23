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
from _bpy import types as bpy_types

StructRNA = bpy_types.Struct.__bases__[0]
# StructRNA = bpy_types.Struct


class Context(StructRNA):

    def copy(self):
        new_context = {}
        generic_keys = StructRNA.__dict__.keys()
        for item in dir(self):
            if item not in generic_keys:
                new_context[item] = getattr(self, item)

        return new_context


class Object(bpy_types.ID):

    @property
    def children(self):
        import bpy
        return [child for child in bpy.data.objects if child.parent == self]


class PoseBone(StructRNA):

    def parent_index(self, parent_test):
        '''
        The same as 'bone in other_bone.parent_recursive' but saved generating a list.
        '''
        parent = self.parent
        i = 1
        while parent:
            if parent == parent_test:
                return i
            parent = parent.parent
            i += 1
        
        return 0

    @property
    def children(self):
        import bpy
        obj = self.id_data
        return [child for child in obj.pose.bones if child.parent == self]

    @property
    def parent_recursive(self):
        parent_list = []
        parent = self.parent
        
        while parent:
            if parent:
                parent_list.append(parent)
            
            parent = parent.parent
        
        return parent_list
    
    @property
    def children_recursive(self):
        obj = self.id_data
        bones_children = []
        for bone in obj.pose.bones:
            index = bone.parent_index(self)
            if index:
                bones_children.append((index, bone))
        
        # sort by distance to parent
        bones_children.sort(key=lambda bone_pair: bone_pair[0])
        return [bone for index, bone in bones_children]


class Bone(StructRNA):
    @property
    def length(self):
        return (self.head - self.tail).length


def ord_ind(i1,i2):
    if i1<i2: return i1,i2
    return i2,i1

class Mesh(bpy_types.ID):

    @property
    def edge_keys(self):
        return [edge_key for face in self.faces for edge_key in face.edge_keys]

    @property
    def edge_face_count_dict(self):
        face_edge_keys = [face.edge_keys for face in self.faces]
        face_edge_count = {}
        for face_keys in face_edge_keys:
            for key in face_keys:
                try:
                    face_edge_count[key] += 1
                except:
                    face_edge_count[key] = 1

        return face_edge_count

    @property
    def edge_face_count(self):
        edge_face_count_dict = self.edge_face_count_dict
        return [edge_face_count_dict.get(ed.key, 0) for ed in mesh.edges]


class MeshEdge(StructRNA):

    @property
    def key(self):
        return ord_ind(*tuple(self.verts))


class MeshFace(StructRNA):

    @property
    def edge_keys(self):
        verts = tuple(self.verts)
        if len(verts)==3:
            return ord_ind(verts[0], verts[1]),  ord_ind(verts[1], verts[2]),  ord_ind(verts[2], verts[0])

        return ord_ind(verts[0], verts[1]),  ord_ind(verts[1], verts[2]),  ord_ind(verts[2], verts[3]),  ord_ind(verts[3], verts[0])


import collections
class OrderedMeta(type):
    def __init__(cls, name, bases, attributes):
        super(OrderedMeta, cls).__init__(name, bases, attributes)
        cls.order = list(attributes.keys())
    def __prepare__(name, bases, **kwargs):
        return collections.OrderedDict()


# Only defined so operators members can be used by accessing self.order
class Operator(StructRNA, metaclass=OrderedMeta):
    pass


class Menu(StructRNA):
    
    def path_menu(self, searchpaths, operator):
        layout = self.layout
        # hard coded to set the operators 'path' to the filename.
        
        import os

        def path_to_name(f):
            ''' Only capitalize all lowercase names, mixed case use them as is.
            '''
            f_base = os.path.splitext(f)[0]
            
            # string replacements
            f_base = f_base.replace("_colon_", ":")
            
            f_base = f_base.replace("_", " ")
            
            if f_base.lower() == f_base:
                return ' '.join([w[0].upper() + w[1:] for w in f_base.split()])
            else:
                return f_base

        layout = self.layout

        # collect paths
        files = []
        for path in searchpaths:
            files.extend([(f, os.path.join(path, f)) for f in os.listdir(path)])

        files.sort()

        for f, path in files:

            if f.startswith("."):
                continue

            layout.operator(operator, text=path_to_name(f)).path = path
    
    def draw_preset(self, context):
        '''Define these on the subclass
         - preset_operator
         - preset_subdir
        '''
        import bpy
        self.path_menu(bpy.utils.preset_paths(self.preset_subdir), self.preset_operator)
