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

StructRNA = bpy_types.bpy_struct
StructMetaPropGroup = bpy_types.bpy_struct_meta_idprop
# StructRNA = bpy_types.Struct

bpy_types.BlendDataLibraries.load = _bpy._library_load
bpy_types.BlendDataLibraries.write = _bpy._library_write
bpy_types.BlendData.user_map = _bpy._rna_id_collection_user_map
bpy_types.BlendData.batch_remove = _bpy._rna_id_collection_batch_remove


class Context(StructRNA):
    __slots__ = ()

    def copy(self):
        from types import BuiltinMethodType
        new_context = {}
        generic_attrs = (
            *StructRNA.__dict__.keys(),
            "bl_rna", "rna_type", "copy",
        )
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
                      "curves", "grease_pencils", "collections", "images",
                      "lights", "lattices", "materials", "metaballs",
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


class Collection(bpy_types.ID):
    __slots__ = ()

    @property
    def users_dupli_group(self):
        """The collection instance objects this collection is used in"""
        import bpy
        return tuple(obj for obj in bpy.data.objects
                     if self == obj.instance_collection)


class Object(bpy_types.ID):
    __slots__ = ()

    @property
    def children(self):
        """All the children of this object. Warning: takes O(len(bpy.data.objects)) time."""
        import bpy
        return tuple(child for child in bpy.data.objects
                     if child.parent == self)

    @property
    def users_collection(self):
        """The collections this object is in. Warning: takes O(len(bpy.data.collections) + len(bpy.data.scenes)) time."""
        import bpy
        return tuple(collection for collection in bpy.data.collections
                     if self in collection.objects[:]) + \
               tuple(scene.collection for scene in bpy.data.scenes
                     if self in scene.collection.objects[:])

    @property
    def users_scene(self):
        """The scenes this object is in. Warning: takes O(len(bpy.data.scenes) * len(bpy.data.objects)) time."""
        import bpy
        return tuple(scene for scene in bpy.data.scenes
                     if self in scene.objects[:])


class WindowManager(bpy_types.ID):
    __slots__ = ()

    def popup_menu(self, draw_func, title="", icon='NONE'):
        import bpy
        popup = self.popmenu_begin__internal(title, icon=icon)

        try:
            draw_func(popup, bpy.context)
        finally:
            self.popmenu_end__internal(popup)

    def popover(
            self, draw_func, *,
            ui_units_x=0,
            keymap=None,
    ):
        import bpy
        popup = self.popover_begin__internal(
            ui_units_x=ui_units_x,
        )

        try:
            draw_func(popup, bpy.context)
        finally:
            self.popover_end__internal(popup, keymap=keymap)

    def popup_menu_pie(self, event, draw_func, title="", icon='NONE'):
        import bpy
        pie = self.piemenu_begin__internal(title, icon=icon, event=event)

        if pie:
            try:
                draw_func(pie, bpy.context)
            finally:
                self.piemenu_end__internal(pie)


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
        return self.matrix.to_3x3() @ Vector((1.0, 0.0, 0.0))

    @property
    def y_axis(self):
        """ Vector pointing down the y-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() @ Vector((0.0, 1.0, 0.0))

    @property
    def z_axis(self):
        """ Vector pointing down the z-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() @ Vector((0.0, 0.0, 1.0))

    @property
    def basename(self):
        """The name of this bone before any '.' character"""
        # return self.name.rsplit(".", 1)[0]
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
        """A list of all the bones children. Warning: takes O(len(bones)) time."""
        return [child for child in self._other_bones if child.parent == self]

    @property
    def children_recursive(self):
        """A list of all children from this bone. Warning: takes O(len(bones)**2) time."""
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
        and not be returned. Warning: takes O(len(bones)**2) time.
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
                if children_basename:
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
        z_vec = self.matrix.to_3x3() @ Vector((0.0, 0.0, 1.0))
        self.tail = matrix @ self.tail
        self.head = matrix @ self.head

        if scale:
            scalar = matrix.median_scale
            self.head_radius *= scalar
            self.tail_radius *= scalar

        if roll:
            self.align_roll(matrix @ z_vec)


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

        .. warning::

           Invalid mesh data
           *(out of range indices, edges with matching indices,
           2 sided faces... etc)* are **not** prevented.
           If the data used for mesh creation isn't known to be valid,
           run :class:`Mesh.validate` after this function.
        """
        from itertools import chain, islice, accumulate

        face_lengths = tuple(map(len, faces))

        self.vertices.add(len(vertices))
        self.edges.add(len(edges))
        self.loops.add(sum(face_lengths))
        self.polygons.add(len(faces))

        self.vertices.foreach_set("co", tuple(chain.from_iterable(vertices)))
        self.edges.foreach_set("vertices", tuple(chain.from_iterable(edges)))

        vertex_indices = tuple(chain.from_iterable(faces))
        loop_starts = tuple(islice(chain([0], accumulate(face_lengths)), len(faces)))

        self.polygons.foreach_set("loop_total", face_lengths)
        self.polygons.foreach_set("loop_start", loop_starts)
        self.polygons.foreach_set("vertices", vertex_indices)

        # if no edges - calculate them
        if faces and (not edges):
            self.update(calc_edges=True)
        elif edges:
            self.update(calc_edges_loose=True)

    @property
    def edge_keys(self):
        return [ed.key for ed in self.edges]


class MeshEdge(StructRNA):
    __slots__ = ()

    @property
    def key(self):
        return ord_ind(*tuple(self.vertices))


class MeshLoopTriangle(StructRNA):
    __slots__ = ()

    @property
    def center(self):
        """The midpoint of the face."""
        face_verts = self.vertices[:]
        mesh_verts = self.id_data.vertices
        return (mesh_verts[face_verts[0]].co +
                mesh_verts[face_verts[1]].co +
                mesh_verts[face_verts[2]].co
                ) / 3.0

    @property
    def edge_keys(self):
        verts = self.vertices[:]
        return (ord_ind(verts[0], verts[1]),
                ord_ind(verts[1], verts[2]),
                ord_ind(verts[2], verts[0]),
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

    def as_module(self):
        from os.path import splitext
        from types import ModuleType
        mod = ModuleType(splitext(self.name)[0])
        # TODO: We could use Text.compiled (C struct member)
        # if this is called often it will be much faster.
        exec(self.as_string(), mod.__dict__)
        return mod


class Sound(bpy_types.ID):
    __slots__ = ()

    @property
    def factory(self):
        """The aud.Factory object of the sound."""
        import aud
        return aud._sound_from_pointer(self.as_pointer())


class RNAMeta(type):
    # TODO(campbell): move to C-API
    @property
    def is_registered(cls):
        return "bl_rna" in cls.__dict__


class RNAMetaPropGroup(StructMetaPropGroup, RNAMeta):
    pass


# Same as 'Operator'
# only without 'as_keywords'
class Gizmo(StructRNA):
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

    from _bpy import (
        _rna_gizmo_target_set_handler as target_set_handler,
        _rna_gizmo_target_get_value as target_get_value,
        _rna_gizmo_target_set_value as target_set_value,
        _rna_gizmo_target_get_range as target_get_range,
    )

    # Convenience wrappers around private `_gpu` module.
    def draw_custom_shape(self, shape, *, matrix=None, select_id=None):
        """
        Draw a shape created form :class:`bpy.types.Gizmo.draw_custom_shape`.

        :arg shape: The cached shape to draw.
        :type shape: Undefined.
        :arg matrix: 4x4 matrix, when not given
           :class:`bpy.types.Gizmo.matrix_world` is used.
        :type matrix: :class:`mathutils.Matrix`
        :arg select_id: The selection id.
           Only use when drawing within :class:`bpy.types.Gizmo.draw_select`.
        :type select_it: int
        """
        import gpu

        if matrix is None:
            matrix = self.matrix_world

        batch, shader = shape
        shader.bind()

        if select_id is not None:
            gpu.select.load_id(select_id)
        else:
            if self.is_highlight:
                color = (*self.color_highlight, self.alpha_highlight)
            else:
                color = (*self.color, self.alpha)
            shader.uniform_float("color", color)

        with gpu.matrix.push_pop():
            gpu.matrix.multiply_matrix(matrix)
            batch.draw()

    @staticmethod
    def new_custom_shape(type, verts):
        """
        Create a new shape that can be passed to :class:`bpy.types.Gizmo.draw_custom_shape`.

        :arg type: The type of shape to create in (POINTS, LINES, TRIS, LINE_STRIP).
        :type type: string
        :arg verts: Coordinates.
        :type verts: sequence of of 2D or 3D coordinates.
        :arg display_name: Optional callback that takes the full path, returns the name to display.
        :type display_name: Callable that takes a string and returns a string.
        :return: The newly created shape.
        :rtype: Undefined (it may change).
        """
        import gpu
        from gpu.types import (
            GPUBatch,
            GPUVertBuf,
            GPUVertFormat,
        )
        dims = len(verts[0])
        if dims not in {2, 3}:
            raise ValueError("Expected 2D or 3D vertex")
        fmt = GPUVertFormat()
        pos_id = fmt.attr_add(id="pos", comp_type='F32', len=dims, fetch_mode='FLOAT')
        vbo = GPUVertBuf(len=len(verts), format=fmt)
        vbo.attr_fill(id=pos_id, data=verts)
        batch = GPUBatch(type=type, buf=vbo)
        shader = gpu.shader.from_builtin('3D_UNIFORM_COLOR' if dims == 3 else '2D_UNIFORM_COLOR')
        batch.program_set(shader)
        return (batch, shader)


    # Dummy class to keep the reference in `bpy_types_dict` and avoid
# erros like: "TypeError: expected GizmoGroup subclass of class ..."
class GizmoGroup(StructRNA):
    __slots__ = ()


# Only defined so operators members can be used by accessing self.order
# with doc generation 'self.properties.bl_rna.properties' can fail
class Operator(StructRNA, metaclass=RNAMeta):
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


class Macro(StructRNA):
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

                # Support filtering out by owner
                workspace = context.workspace
                if workspace.use_filter_by_owner:
                    owner_names = {owner_id.name for owner_id in workspace.owner_ids}
                else:
                    owner_names = None

                for func in draw_ls._draw_funcs:

                    # Begin 'owner_id' filter.
                    if owner_names is not None:
                        owner_id = getattr(func, "_owner", None)
                        if owner_id is not None:
                            if func._owner not in owner_names:
                                continue
                    # End 'owner_id' filter.

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

    @staticmethod
    def _dyn_owner_apply(draw_func):
        from _bpy import _bl_owner_id_get
        owner_id = _bl_owner_id_get()
        if owner_id is not None:
            draw_func._owner = owner_id

    @classmethod
    def is_extended(cls):
        return bool(getattr(cls.draw, "_draw_funcs", None))

    @classmethod
    def append(cls, draw_func):
        """
        Append a draw function to this menu,
        takes the same arguments as the menus draw function
        """
        draw_funcs = cls._dyn_ui_initialize()
        cls._dyn_owner_apply(draw_func)
        draw_funcs.append(draw_func)

    @classmethod
    def prepend(cls, draw_func):
        """
        Prepend a draw function to this menu, takes the same arguments as
        the menus draw function
        """
        draw_funcs = cls._dyn_ui_initialize()
        cls._dyn_owner_apply(draw_func)
        draw_funcs.insert(0, draw_func)

    @classmethod
    def remove(cls, draw_func):
        """Remove a draw function that has been added to this menu"""
        draw_funcs = cls._dyn_ui_initialize()
        try:
            draw_funcs.remove(draw_func)
        except ValueError:
            pass


class Panel(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class UIList(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class Header(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()


class Menu(StructRNA, _GenericUI, metaclass=RNAMeta):
    __slots__ = ()

    def path_menu(self, searchpaths, operator, *,
                  props_default=None, prop_filepath="filepath",
                  filter_ext=None, filter_path=None, display_name=None,
                  add_operator=None):
        """
        Populate a menu from a list of paths.

        :arg searchpaths: Paths to scan.
        :type searchpaths: sequence of strings.
        :arg operator: The operator id to use with each file.
        :type operator: string
        :arg prop_filepath: Optional operator filepath property (defaults to "filepath").
        :type prop_filepath: string
        :arg props_default: Properties to assign to each operator.
        :type props_default: dict
        :arg filter_ext: Optional callback that takes the file extensions.

           Returning false excludes the file from the list.

        :type filter_ext: Callable that takes a string and returns a bool.
        :arg display_name: Optional callback that takes the full path, returns the name to display.
        :type display_name: Callable that takes a string and returns a string.
        """

        layout = self.layout

        import os
        import bpy.utils

        layout = self.layout

        if not searchpaths:
            layout.label(text="* Missing Paths *")

        # collect paths
        files = []
        for directory in searchpaths:
            files.extend(
                [(f, os.path.join(directory, f))
                 for f in os.listdir(directory)
                 if (not f.startswith("."))
                 if ((filter_ext is None) or
                     (filter_ext(os.path.splitext(f)[1])))
                 if ((filter_path is None) or
                     (filter_path(f)))
                 ])

        files.sort()

        col = layout.column(align=True)

        for f, filepath in files:
            # Intentionally pass the full path to 'display_name' callback,
            # since the callback may want to use part a directory in the name.
            row = col.row(align=True)
            name = display_name(filepath) if display_name else bpy.path.display_name(f)
            props = row.operator(
                operator,
                text=name,
                translate=False,
            )

            if props_default is not None:
                for attr, value in props_default.items():
                    setattr(props, attr, value)

            setattr(props, prop_filepath, filepath)
            if operator == "script.execute_preset":
                props.menu_idname = self.bl_idname

            if add_operator:
                props = row.operator(add_operator, text="", icon='REMOVE')
                props.name = name
                props.remove_name = True

        if add_operator:
            wm = bpy.data.window_managers[0]

            layout.separator()
            row = layout.row()

            sub = row.row()
            sub.emboss = 'NORMAL'
            sub.prop(wm, "preset_name", text="")

            props = row.operator(add_operator, text="", icon='ADD')
            props.name = wm.preset_name

    def draw_preset(self, _context):
        """
        Define these on the subclass:
        - preset_operator (string)
        - preset_subdir (string)

        Optionally:
        - preset_add_operator (string)
        - preset_extensions (set of strings)
        - preset_operator_defaults (dict of keyword args)
        """
        import bpy
        ext_valid = getattr(self, "preset_extensions", {".py", ".xml"})
        props_default = getattr(self, "preset_operator_defaults", None)
        add_operator = getattr(self, "preset_add_operator", None)
        self.path_menu(bpy.utils.preset_paths(self.preset_subdir),
                       self.preset_operator,
                       props_default=props_default,
                       filter_ext=lambda ext: ext.lower() in ext_valid,
                       add_operator=add_operator)

    @classmethod
    def draw_collapsible(cls, context, layout):
        # helper function for (optionally) collapsed header menus
        # only usable within headers
        if context.area.show_menus:
            # Align menus to space them closely.
            layout.row(align=True).menu_contents(cls.__name__)
        else:
            layout.menu(cls.__name__, icon='COLLAPSEMENU')


class NodeTree(bpy_types.ID, metaclass=RNAMetaPropGroup):
    __slots__ = ()


class Node(StructRNA, metaclass=RNAMetaPropGroup):
    __slots__ = ()

    @classmethod
    def poll(cls, _ntree):
        return True


class NodeInternal(Node):
    __slots__ = ()


class NodeSocket(StructRNA, metaclass=RNAMetaPropGroup):
    __slots__ = ()

    @property
    def links(self):
        """List of node links from or to this socket. Warning: takes O(len(nodetree.links)) time."""
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
