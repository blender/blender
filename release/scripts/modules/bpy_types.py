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

from _bpy import types as bpy_types
from Mathutils import Vector

StructRNA = bpy_types.Struct.__bases__[0]
# StructRNA = bpy_types.Struct


class Context(StructRNA):
    __slots__ = ()

    def copy(self):
        new_context = {}
        generic_keys = StructRNA.__dict__.keys()
        for item in dir(self):
            if item not in generic_keys:
                new_context[item] = getattr(self, item)

        return new_context


class Object(bpy_types.ID):
    __slots__ = ()

    @property
    def children(self):
        """All the children of this object"""
        import bpy
        return [child for child in bpy.data.objects if child.parent == self]

    @property
    def group_users(self):
        """The groups this object is in"""
        import bpy
        name = self.name
        return [group for group in bpy.data.groups if name in group.objects]

    @property
    def scene_users(self):
        """The scenes this object is in"""
        import bpy
        name = self.name
        return [scene for scene in bpy.data.scenes if name in scene.objects]


class _GenericBone:
    """
    functions for bones, common between Armature/Pose/Edit bones.
    internal subclassing use only.
    """
    __slots__ = ()

    def translate(self, vec):
        """Utility function to add *vec* to the head and tail of this bone."""
        self.head += vec
        self.tail += vec

    def parent_index(self, parent_test):
        """
        The same as 'bone in other_bone.parent_recursive' but saved generating a list.
        """
        # use the name so different types can be tested.
        name = parent_test.name

        parent = self.parent
        i = 1
        while parent:
            if parent.name == name:
                return i
            parent = parent.parent
            i += 1

        return 0

    @property
    def x_axis(self):
        """ Vector pointing down the x-axis of the bone.
        """
        return self.matrix.rotation_part() * Vector(1.0, 0.0, 0.0)

    @property
    def y_axis(self):
        """ Vector pointing down the x-axis of the bone.
        """
        return self.matrix.rotation_part() * Vector(0.0, 1.0, 0.0)

    @property
    def z_axis(self):
        """ Vector pointing down the x-axis of the bone.
        """
        return self.matrix.rotation_part() * Vector(0.0, 0.0, 1.0)

    @property
    def basename(self):
        """The name of this bone before any '.' character"""
        #return self.name.rsplit(".", 1)[0]
        return self.name.split(".")[0]

    @property
    def parent_recursive(self):
        """A list of parents, starting with the immediate parent"""
        parent_list = []
        parent = self.parent

        while parent:
            if parent:
                parent_list.append(parent)

            parent = parent.parent

        return parent_list

    @property
    def center(self):
        """The midpoint between the head and the tail."""
        return (self.head + self.tail) * 0.5

    @property
    def length(self):
        """The distance from head to tail, when set the head is moved to fit the length."""
        return self.vector.length

    @length.setter
    def length(self, value):
        self.tail = self.head + ((self.tail - self.head).normalize() * value)

    @property
    def vector(self):
        """The direction this bone is pointing. Utility function for (tail - head)"""
        return (self.tail - self.head)

    @property
    def children(self):
        """A list of all the bones children."""
        return [child for child in self._other_bones if child.parent == self]

    @property
    def children_recursive(self):
        """a list of all children from this bone."""
        bones_children = []
        for bone in self._other_bones:
            index = bone.parent_index(self)
            if index:
                bones_children.append((index, bone))

        # sort by distance to parent
        bones_children.sort(key=lambda bone_pair: bone_pair[0])
        return [bone for index, bone in bones_children]

    @property
    def children_recursive_basename(self):
        """
        Returns a chain of children with the same base name as this bone
        Only direct chains are supported, forks caused by multiple children with matching basenames will
        terminate the function and not be returned.
        """
        basename = self.basename
        chain = []

        child = self
        while True:
            children = child.children
            children_basename = []

            for child in children:
                if basename == child.basename:
                    children_basename.append(child)

            if len(children_basename) == 1:
                child = children_basename[0]
                chain.append(child)
            else:
                if len(children_basename):
                    print("multiple basenames found, this is probably not what you want!", bone.name, children_basename)

                break

        return chain

    @property
    def _other_bones(self):
        id_data = self.id_data
        id_data_type = type(id_data)

        if id_data_type == bpy_types.Object:
            bones = id_data.pose.bones
        elif id_data_type == bpy_types.Armature:
            bones = id_data.edit_bones
            if not bones: # not in editmode
                bones = id_data.bones

        return bones


class PoseBone(StructRNA, _GenericBone):
    __slots__ = ()


class Bone(StructRNA, _GenericBone):
    __slots__ = ()


class EditBone(StructRNA, _GenericBone):
    __slots__ = ()

    def align_orientation(self, other):
        """
        Align this bone to another by moving its tail and settings its roll
        the length of the other bone is not used.
        """
        vec = other.vector.normalize() * self.length
        self.tail = self.head + vec
        self.roll = other.roll

    def transform(self, matrix):
        """
        Transform the the bones head, tail, roll and envalope (when the matrix has a scale component).
        Expects a 4x4 or 3x3 matrix.
        """
        from Mathutils import Vector
        z_vec = self.matrix.rotation_part() * Vector(0.0, 0.0, 1.0)
        self.tail = matrix * self.tail
        self.head = matrix * self.head
        scalar = matrix.median_scale
        self.head_radius *= scalar
        self.tail_radius *= scalar
        self.align_roll(matrix * z_vec)


def ord_ind(i1, i2):
    if i1 < i2:
        return i1, i2
    return i2, i1


class Mesh(bpy_types.ID):
    __slots__ = ()

    def from_pydata(self, verts, edges, faces):
        """
        Make a mesh from a list of verts/edges/faces
        Until we have a nicer way to make geometry, use this.
        """
        self.add_geometry(len(verts), len(edges), len(faces))

        verts_flat = [f for v in verts for f in v]
        self.verts.foreach_set("co", verts_flat)
        del verts_flat

        edges_flat = [i for e in edges for i in e]
        self.edges.foreach_set("verts", edges_flat)
        del edges_flat

        def treat_face(f):
            if len(f) == 3:
                return f[0], f[1], f[2], 0
            elif f[3] == 0:
                return f[3], f[0], f[1], f[2]
            return f

        faces_flat = [v for f in faces for v in treat_face(f)]
        self.faces.foreach_set("verts_raw", faces_flat)
        del faces_flat

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
        return [edge_face_count_dict.get(ed.key, 0) for ed in self.edges]

    def edge_loops(self, faces=None, seams=()):
        """
        Edge loops defined by faces

        Takes me.faces or a list of faces and returns the edge loops
        These edge loops are the edges that sit between quads, so they dont touch
        1 quad, note: not connected will make 2 edge loops, both only containing 2 edges.

        return a list of edge key lists
        [ [(0,1), (4, 8), (3,8)], ...]

        optionaly, seams are edge keys that will be removed
        """

        OTHER_INDEX = 2, 3, 0, 1 # opposite face index

        if faces is None:
            faces = self.faces

        edges = {}

        for f in faces:
#            if len(f) == 4:
            if f.verts_raw[3] != 0:
                edge_keys = f.edge_keys
                for i, edkey in enumerate(f.edge_keys):
                    edges.setdefault(edkey, []).append(edge_keys[OTHER_INDEX[i]])

        for edkey in seams:
            edges[edkey] = []

        # Collect edge loops here
        edge_loops = []

        for edkey, ed_adj in edges.items():
            if 0 < len(ed_adj) < 3: # 1 or 2
                # Seek the first edge
                context_loop = [edkey, ed_adj[0]]
                edge_loops.append(context_loop)
                if len(ed_adj) == 2:
                    other_dir = ed_adj[1]
                else:
                    other_dir = None

                ed_adj[:] = []

                flipped = False

                while 1:
                    # from knowing the last 2, look for th next.
                    ed_adj = edges[context_loop[-1]]
                    if len(ed_adj) != 2:

                        if other_dir and flipped == False: # the original edge had 2 other edges
                            flipped = True # only flip the list once
                            context_loop.reverse()
                            ed_adj[:] = []
                            context_loop.append(other_dir) # save 1 lookiup

                            ed_adj = edges[context_loop[-1]]
                            if len(ed_adj) != 2:
                                ed_adj[:] = []
                                break
                        else:
                            ed_adj[:] = []
                            break

                    i = ed_adj.index(context_loop[-2])
                    context_loop.append(ed_adj[not  i])

                    # Dont look at this again
                    ed_adj[:] = []


        return edge_loops


class MeshEdge(StructRNA):
    __slots__ = ()

    @property
    def key(self):
        return ord_ind(*tuple(self.verts))


class MeshFace(StructRNA):
    __slots__ = ()

    @property
    def center(self):
        """The midpoint of the face."""
        face_verts = self.verts[:]
        mesh_verts = self.id_data.verts
        if len(face_verts) == 3:
            return (mesh_verts[face_verts[0]].co + mesh_verts[face_verts[1]].co + mesh_verts[face_verts[2]].co) / 3.0
        else:
            return (mesh_verts[face_verts[0]].co + mesh_verts[face_verts[1]].co + mesh_verts[face_verts[2]].co + mesh_verts[face_verts[3]].co) / 4.0

    @property
    def edge_keys(self):
        verts = self.verts[:]
        if len(verts) == 3:
            return ord_ind(verts[0], verts[1]), ord_ind(verts[1], verts[2]), ord_ind(verts[2], verts[0])

        return ord_ind(verts[0], verts[1]), ord_ind(verts[1], verts[2]), ord_ind(verts[2], verts[3]), ord_ind(verts[3], verts[0])


import collections


class OrderedMeta(type):

    def __init__(cls, name, bases, attributes):
        super(OrderedMeta, cls).__init__(name, bases, attributes)
        cls.order = list(attributes.keys())

    def __prepare__(name, bases, **kwargs):
        return collections.OrderedDict()


# Only defined so operators members can be used by accessing self.order
class Operator(StructRNA, metaclass=OrderedMeta):
    __slots__ = ()


class Macro(StructRNA, metaclass=OrderedMeta):
    # bpy_types is imported before ops is defined
    # so we have to do a local import on each run
    __slots__ = ()

    @classmethod
    def define(self, opname):
        from _bpy import ops
        return ops.macro_define(self, opname)


class _GenericUI:
    __slots__ = ()

    @classmethod
    def _dyn_ui_initialize(cls):
        draw_funcs = getattr(cls.draw, "_draw_funcs", None)

        if draw_funcs is None:

            def draw_ls(*args):
                for func in draw_ls._draw_funcs:
                    func(*args)

            draw_funcs = draw_ls._draw_funcs = [cls.draw]
            cls.draw = draw_ls

        return draw_funcs

    @classmethod
    def append(cls, draw_func):
        """Prepend an draw function to this menu, takes the same arguments as the menus draw function."""
        draw_funcs = cls._dyn_ui_initialize()
        draw_funcs.append(draw_func)

    @classmethod
    def prepend(cls, draw_func):
        """Prepend a draw function to this menu, takes the same arguments as the menus draw function."""
        draw_funcs = cls._dyn_ui_initialize()
        draw_funcs.insert(0, draw_func)


class Panel(StructRNA, _GenericUI):
    __slots__ = ()


class Header(StructRNA, _GenericUI):
    __slots__ = ()


class Menu(StructRNA, _GenericUI):
    __slots__ = ()

    def path_menu(self, searchpaths, operator):
        layout = self.layout
        # hard coded to set the operators 'path' to the filename.

        import os
        import bpy.utils

        layout = self.layout

        # collect paths
        files = []
        for path in searchpaths:
            files.extend([(f, os.path.join(path, f)) for f in os.listdir(path)])

        files.sort()

        for f, path in files:

            if f.startswith("."):
                continue

            layout.operator(operator, text=bpy.utils.display_name(f)).path = path

    def draw_preset(self, context):
        """Define these on the subclass
         - preset_operator
         - preset_subdir
        """
        import bpy
        self.path_menu(bpy.utils.preset_paths(self.preset_subdir), self.preset_operator)
