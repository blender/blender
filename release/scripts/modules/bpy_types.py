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

# <pep8-80 compliant>

from _bpy import types as bpy_types
import _bpy

StructRNA = bpy_types.Struct.__bases__[0]
StructMetaPropGroup = _bpy.StructMetaPropGroup
# StructRNA = bpy_types.Struct

bpy_types.BlendDataLibraries.load = _bpy._library_load


class Context(StructRNA):
    __slots__ = ()

    def copy(self):
        from types import BuiltinMethodType
        new_context = {}
        generic_attrs = (list(StructRNA.__dict__.keys()) +
                         ["bl_rna", "rna_type", "copy"])
        for attr in dir(self):
            if not (attr.startswith("_") or attr in generic_attrs):
                value = getattr(self, attr)
                if type(value) != BuiltinMethodType:
                    new_context[attr] = value

        return new_context


class Library(bpy_types.ID):
    __slots__ = ()

    @property
    def users_id(self):
        """ID data blocks which use this library"""
        import bpy

        # See: readblenentry.c, IDTYPE_FLAGS_ISLINKABLE,
        # we could make this an attribute in rna.
        attr_links = ("actions", "armatures", "brushes", "cameras",
                      "curves", "grease_pencil", "groups", "images",
                      "lamps", "lattices", "materials", "metaballs",
                      "meshes", "node_groups", "objects", "scenes",
                      "sounds", "speakers", "textures", "texts",
                      "fonts", "worlds")

        return tuple(id_block
                     for attr in attr_links
                     for id_block in getattr(bpy.data, attr)
                     if id_block.library == self)


class Texture(bpy_types.ID):
    __slots__ = ()

    @property
    def users_material(self):
        """Materials that use this texture"""
        import bpy
        return tuple(mat for mat in bpy.data.materials
                     if self in [slot.texture
                                 for slot in mat.texture_slots
                                 if slot]
                     )

    @property
    def users_object_modifier(self):
        """Object modifiers that use this texture"""
        import bpy
        return tuple(obj for obj in bpy.data.objects if
                     self in [mod.texture
                              for mod in obj.modifiers
                              if mod.type == 'DISPLACE']
                     )


class Group(bpy_types.ID):
    __slots__ = ()

    @property
    def users_dupli_group(self):
        """The dupli group this group is used in"""
        import bpy
        return tuple(obj for obj in bpy.data.objects
                     if self == obj.dupli_group)


class Object(bpy_types.ID):
    __slots__ = ()

    @property
    def children(self):
        """All the children of this object"""
        import bpy
        return tuple(child for child in bpy.data.objects
                     if child.parent == self)

    @property
    def users_group(self):
        """The groups this object is in"""
        import bpy
        return tuple(group for group in bpy.data.groups
                     if self in group.objects[:])

    @property
    def users_scene(self):
        """The scenes this object is in"""
        import bpy
        return tuple(scene for scene in bpy.data.scenes
                     if self in scene.objects[:])


class WindowManager(bpy_types.ID):
    __slots__ = ()

    def popup_menu(self, draw_func, title="", icon='NONE'):
        import bpy
        popup = self.pupmenu_begin__internal(title, icon)

        try:
            draw_func(popup, bpy.context)
        finally:
            self.pupmenu_end__internal(popup)


class _GenericBone:
    """
    functions for bones, common between Armature/Pose/Edit bones.
    internal subclassing use only.
    """
    __slots__ = ()

    def translate(self, vec):
        """Utility function to add *vec* to the head and tail of this bone"""
        self.head += vec
        self.tail += vec

    def parent_index(self, parent_test):
        """
        The same as 'bone in other_bone.parent_recursive'
        but saved generating a list.
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
        from mathutils import Vector
        return self.matrix.to_3x3() * Vector((1.0, 0.0, 0.0))

    @property
    def y_axis(self):
        """ Vector pointing down the x-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() * Vector((0.0, 1.0, 0.0))

    @property
    def z_axis(self):
        """ Vector pointing down the x-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() * Vector((0.0, 0.0, 1.0))

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
        """
        The distance from head to tail,
        when set the head is moved to fit the length.
        """
        return self.vector.length

    @length.setter
    def length(self, value):
        self.tail = self.head + ((self.tail - self.head).normalized() * value)

    @property
    def vector(self):
        """
        The direction this bone is pointing.
        Utility function for (tail - head)
        """
        return (self.tail - self.head)

    @property
    def children(self):
        """A list of all the bones children."""
        return [child for child in self._other_bones if child.parent == self]

    @property
    def children_recursive(self):
        """A list of all children from this bone."""
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
        Returns a chain of children with the same base name as this bone.
        Only direct chains are supported, forks caused by multiple children
        with matching base names will terminate the function
        and not be returned.
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
                    print("multiple basenames found, "
                          "this is probably not what you want!",
                          self.name, children_basename)

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
            if not bones:  # not in edit mode
                bones = id_data.bones

        return bones


class PoseBone(StructRNA, _GenericBone, metaclass=StructMetaPropGroup):
    __slots__ = ()

    @property
    def children(self):
        obj = self.id_data
        pbones = obj.pose.bones
        self_bone = self.bone

        return tuple(pbones[bone.name] for bone in obj.data.bones
                     if bone.parent == self_bone)


class Bone(StructRNA, _GenericBone, metaclass=StructMetaPropGroup):
    __slots__ = ()


class EditBone(StructRNA, _GenericBone, metaclass=StructMetaPropGroup):
    __slots__ = ()

    def align_orientation(self, other):
        """
        Align this bone to another by moving its tail and settings its roll
        the length of the other bone is not used.
        """
        vec = other.vector.normalized() * self.length
        self.tail = self.head + vec
        self.roll = other.roll

    def transform(self, matrix, scale=True, roll=True):
        """
        Transform the the bones head, tail, roll and envelope
        (when the matrix has a scale component).

        :arg matrix: 3x3 or 4x4 transformation matrix.
        :type matrix: :class:`mathutils.Matrix`
        :arg scale: Scale the bone envelope by the matrix.
        :type scale: bool
        :arg roll:

           Correct the roll to point in the same relative
           direction to the head and tail.

        :type roll: bool
        """
        from mathutils import Vector
        z_vec = self.matrix.to_3x3() * Vector((0.0, 0.0, 1.0))
        self.tail = matrix * self.tail
        self.head = matrix * self.head

        if scale:
            scalar = matrix.median_scale
            self.head_radius *= scalar
            self.tail_radius *= scalar

        if roll:
            self.align_roll(matrix * z_vec)


def ord_ind(i1, i2):
    if i1 < i2:
        return i1, i2
    return i2, i1


class Mesh(bpy_types.ID):
    __slots__ = ()

    def from_pydata(self, vertices, edges, faces):
        """
        Make a mesh from a list of vertices/edges/faces
        Until we have a nicer way to make geometry, use this.

        :arg vertices:

           float triplets each representing (X, Y, Z)
           eg: [(0.0, 1.0, 0.5), ...].

        :type vertices: iterable object
        :arg edges:

           int pairs, each pair contains two indices to the
           *vertices* argument. eg: [(1, 2), ...]

        :type edges: iterable object
        :arg faces:

           iterator of faces, each faces contains three or more indices to
           the *vertices* argument. eg: [(5, 6, 8, 9), (1, 2, 3), ...]

        :type faces: iterable object
        """
        self.vertices.add(len(vertices))
        self.edges.add(len(edges))
        self.loops.add(sum((len(f) for f in faces)))
        self.polygons.add(len(faces))

        vertices_flat = [f for v in vertices for f in v]
        self.vertices.foreach_set("co", vertices_flat)
        del vertices_flat

        edges_flat = [i for e in edges for i in e]
        self.edges.foreach_set("vertices", edges_flat)
        del edges_flat

        # this is different in bmesh
        loop_index = 0
        for i, p in enumerate(self.polygons):
            f = faces[i]
            loop_len = len(f)
            p.loop_start = loop_index
            p.loop_total = loop_len
            p.vertices = f
            loop_index += loop_len

        # if no edges - calculate them
        if faces and (not edges):
            self.update(calc_edges=True)

    @property
    def edge_keys(self):
        return [ed.key for ed in self.edges]


class MeshEdge(StructRNA):
    __slots__ = ()

    @property
    def key(self):
        return ord_ind(*tuple(self.vertices))


class MeshTessFace(StructRNA):
    __slots__ = ()

    @property
    def center(self):
        """The midpoint of the face."""
        face_verts = self.vertices[:]
        mesh_verts = self.id_data.vertices
        if len(face_verts) == 3:
            return (mesh_verts[face_verts[0]].co +
                    mesh_verts[face_verts[1]].co +
                    mesh_verts[face_verts[2]].co
                    ) / 3.0
        else:
            return (mesh_verts[face_verts[0]].co +
                    mesh_verts[face_verts[1]].co +
                    mesh_verts[face_verts[2]].co +
                    mesh_verts[face_verts[3]].co
                    ) / 4.0

    @property
    def edge_keys(self):
        verts = self.vertices[:]
        if len(verts) == 3:
            return (ord_ind(verts[0], verts[1]),
                    ord_ind(verts[1], verts[2]),
                    ord_ind(verts[2], verts[0]),
                    )
        else:
            return (ord_ind(verts[0], verts[1]),
                    ord_ind(verts[1], verts[2]),
                    ord_ind(verts[2], verts[3]),
                    ord_ind(verts[3], verts[0]),
                    )


class MeshPolygon(StructRNA):
    __slots__ = ()

    @property
    def edge_keys(self):
        verts = self.vertices[:]
        vlen = len(self.vertices)
        return [ord_ind(verts[i], verts[(i + 1) % vlen]) for i in range(vlen)]

    @property
    def loop_indices(self):
        start = self.loop_start
        end = start + self.loop_total
        return range(start, end)


class Text(bpy_types.ID):
    __slots__ = ()

    def as_string(self):
        """Return the text as a string."""
        return "\n".join(line.body for line in self.lines)

    def from_string(self, string):
        """Replace text with this string."""
        self.clear()
        self.write(string)

    @property
    def users_logic(self):
        """Logic bricks that use this text"""
        import bpy
        return tuple(obj for obj in bpy.data.objects
                     if self in [cont.text for cont in obj.game.controllers
                                 if cont.type == 'PYTHON']
                     )


# values are module: [(cls, path, line), ...]
TypeMap = {}


class Sound(bpy_types.ID):
    __slots__ = ()

    @property
    def factory(self):
        """The aud.Factory object of the sound."""
        import aud
        return aud._sound_from_pointer(self.as_pointer())


class RNAMeta(type):
    def __new__(cls, name, bases, classdict, **args):
        result = type.__new__(cls, name, bases, classdict)
        if bases and bases[0] is not StructRNA:
            from _weakref import ref as ref
            module = result.__module__

            # first part of packages only
            if "." in module:
                module = module[:module.index(".")]

            TypeMap.setdefault(module, []).append(ref(result))

        return result

    @property
    def is_registered(cls):
        return "bl_rna" in cls.__dict__


class OrderedDictMini(dict):
    def __init__(self, *args):
        self.order = []
        dict.__init__(self, args)

    def __setitem__(self, key, val):
        dict.__setitem__(self, key, val)
        if key not in self.order:
            self.order.append(key)

    def __delitem__(self, key):
        dict.__delitem__(self, key)
        self.order.remove(key)


class RNAMetaPropGroup(StructMetaPropGroup, RNAMeta):
    pass


class OrderedMeta(RNAMeta):
    def __init__(cls, name, bases, attributes):
        if attributes.__class__ is OrderedDictMini:
            cls.order = attributes.order

    def __prepare__(name, bases, **kwargs):
        return OrderedDictMini()  # collections.OrderedDict()


# Only defined so operators members can be used by accessing self.order
# with doc generation 'self.properties.bl_rna.properties' can fail
class Operator(StructRNA, metaclass=OrderedMeta):
    __slots__ = ()

    def __getattribute__(self, attr):
        properties = StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return getattr(properties, attr)
        return super().__getattribute__(attr)

    def __setattr__(self, attr, value):
        properties = StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return setattr(properties, attr, value)
        return super().__setattr__(attr, value)

    def __delattr__(self, attr):
        properties = StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return delattr(properties, attr)
        return super().__delattr__(attr)

    def as_keywords(self, ignore=()):
        """Return a copy of the properties as a dictionary"""
        ignore = ignore + ("rna_type",)
        return {attr: getattr(self, attr)
                for attr in self.properties.rna_type.properties.keys()
                if attr not in ignore}


class Macro(StructRNA, metaclass=OrderedMeta):
    # bpy_types is imported before ops is defined
    # so we have to do a local import on each run
    __slots__ = ()

    @classmethod
    def define(self, opname):
        from _bpy import ops
        return ops.macro_define(self, opname)


class PropertyGroup(StructRNA, metaclass=RNAMetaPropGroup):
        __slots__ = ()


class RenderEngine(StructRNA, metaclass=RNAMeta):
    __slots__ = ()


class KeyingSetInfo(StructRNA, metaclass=RNAMeta):
    __slots__ = ()


class AddonPreferences(StructRNA, metaclass=RNAMeta):
    __slots__ = ()


class _GenericUI:
    __slots__ = ()

    @classmethod
    def _dyn_ui_initialize(cls):
        draw_funcs = getattr(cls.draw, "_draw_funcs", None)

        if draw_funcs is None:

            def draw_ls(self, context):
                # ensure menus always get default context
                operator_context_default = self.layout.operator_context

                for func in draw_ls._draw_funcs:
                    # so bad menu functions don't stop
                    # the entire menu from drawing
                    try:
                        func(self, context)
                    except:
                        import traceback
                        traceback.print_exc()

                    self.layout.operator_context = operator_context_default

            draw_funcs = draw_ls._draw_funcs = [cls.draw]
            cls.draw = draw_ls

        return draw_funcs

    @classmethod
    def append(cls, draw_func):
        """
        Append a draw function to this menu,
        takes the same arguments as the menus draw function
        """
        draw_funcs = cls._dyn_ui_initialize()
        draw_funcs.append(draw_func)

    @classmethod
    def prepend(cls, draw_func):
        """
        Prepend a draw function to this menu, takes the same arguments as
        the menus draw function
        """
        draw_funcs = cls._dyn_ui_initialize()
        draw_funcs.insert(0, draw_func)

    @classmethod
    def remove(cls, draw_func):
        """Remove a draw function that has been added to this menu"""
        draw_funcs = cls._dyn_ui_initialize()
        try:
            draw_funcs.remove(draw_func)
        except:
            pass


class Panel(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class UIList(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class Header(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class Menu(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()

    def path_menu(self, searchpaths, operator,
                  props_default={}, filter_ext=None):

        layout = self.layout
        # hard coded to set the operators 'filepath' to the filename.

        import os
        import bpy.utils

        layout = self.layout

        if not searchpaths:
            layout.label("* Missing Paths *")

        # collect paths
        files = []
        for directory in searchpaths:
            files.extend([(f, os.path.join(directory, f))
                          for f in os.listdir(directory)
                          if (not f.startswith("."))
                          if ((filter_ext is None) or
                              (filter_ext(os.path.splitext(f)[1])))
                          ])

        files.sort()

        for f, filepath in files:
            props = layout.operator(operator,
                                    text=bpy.path.display_name(f),
                                    translate=False)

            for attr, value in props_default.items():
                setattr(props, attr, value)

            props.filepath = filepath
            if operator == "script.execute_preset":
                props.menu_idname = self.bl_idname

    def draw_preset(self, context):
        """
        Define these on the subclass
        - preset_operator
        - preset_subdir
        """
        import bpy
        self.path_menu(bpy.utils.preset_paths(self.preset_subdir),
                       self.preset_operator,
                       filter_ext=lambda ext: ext.lower() in {".py", ".xml"})

    @classmethod
    def draw_collapsible(cls, context, layout):
        # helper function for (optionally) collapsed header menus
        # only usable within headers
        if context.area.show_menus:
            cls.draw_menus(layout, context)
        else:
            layout.menu(cls.__name__, icon='COLLAPSEMENU')


class Region(StructRNA):
    __slots__ = ()

    def callback_add(self, cb, args, draw_mode):
        """
        Append a draw function to this region,
        deprecated, instead use bpy.types.SpaceView3D.draw_handler_add
        """
        for area in self.id_data.areas:
            for region in area.regions:
                if region == self:
                    spacetype = type(area.spaces[0])
                    return spacetype.draw_handler_add(cb, args, self.type,
                                                      draw_mode)

        return None


class NodeTree(bpy_types.ID, metaclass=RNAMetaPropGroup):
    __slots__ = ()


class Node(StructRNA, metaclass=RNAMetaPropGroup):
    __slots__ = ()

    @classmethod
    def poll(cls, ntree):
        return True


class NodeInternal(Node):
    __slots__ = ()


class NodeSocket(StructRNA, metaclass=RNAMetaPropGroup):
    __slots__ = ()

    @property
    def links(self):
        """List of node links from or to this socket"""
        return tuple(link for link in self.id_data.links
                     if (link.from_socket == self or
                         link.to_socket == self))


class NodeSocketInterface(StructRNA, metaclass=RNAMetaPropGroup):
    __slots__ = ()


# These are intermediate subclasses, need a bpy type too
class CompositorNode(NodeInternal):
    __slots__ = ()

    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'CompositorNodeTree'

    def update(self):
        self.tag_need_exec()


class ShaderNode(NodeInternal):
    __slots__ = ()

    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'ShaderNodeTree'


class TextureNode(NodeInternal):
    __slots__ = ()

    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'TextureNodeTree'
