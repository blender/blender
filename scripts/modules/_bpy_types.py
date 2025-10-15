# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# NOTE: all members of this module which are _not_ exposed by `bpy.types`
# must use a `_` prefix to prevent attempts to have their `bl_rna` initialized.

from _bpy import types as _types

_StructRNA = _types.bpy_struct
_StructMetaPropGroup = _types.bpy_struct_meta_idprop
# StructRNA = _types.Struct

# Private dummy object use for comparison only.
_sentinel = object()

# Note that methods extended in the C-API are defined in: `bpy_rna_types_capi.cc`.


class Context(_StructRNA):
    __slots__ = ()

    def path_resolve(self, path, coerce=True):
        """
        Returns the property from the path, raise an exception when not found.

        :arg path: patch which this property resolves.
        :type path: str
        :arg coerce: optional argument, when True, the property will be converted into its Python representation.
        :type coerce: bool
        """
        # This is a convenience wrapper around `_StructRNA.path_resolve` which doesn't support accessing
        # context members. Without this wrapper many users were writing `exec("context.{:s}".format(data_path))`
        # which is a security concern if the `data_path` comes from an unknown source.
        # This function performs the initial lookup, after that the regular `path_resolve` function is used.

        # Extract the initial attribute into `(attr, path_rest)`.
        sep = len(path)
        div = ""
        for div_test in (".", "["):
            sep_test = path.find(div_test, 0, sep)
            if sep_test != -1 and sep_test < sep:
                sep = sep_test
                div = div_test
        if div:
            attr = path[:sep]
            if div == ".":
                sep += 1
            path_rest = path[sep:]
        else:
            attr = path
            path_rest = ""

        # Retrieve the value for `attr`.
        # Match the value error exception with that of "path_resolve"
        # to simplify exception handling for the caller.
        value = getattr(self, attr, _sentinel)
        if value is _sentinel:
            raise ValueError("Path could not be resolved: {!r}".format(attr))

        if value is None:
            return value

        # If the attribute is a list property, apply subscripting.
        if isinstance(value, list) and path_rest.startswith("["):
            index_str, div, index_tail = path_rest[1:].partition("]")
            if not div:
                raise ValueError("Path index is not terminated: {:s}{:s}".format(attr, path_rest))
            try:
                index = int(index_str)
            except ValueError:
                raise ValueError("Path index is invalid: {:s}[{:s}]".format(attr, index_str))
            if 0 <= index < len(value):
                path_rest = index_tail
                value = value[index]
            else:
                raise IndexError("Path index out of range: {:s}[{:s}]".format(attr, index_str))

        # Resolve the rest of the path if necessary.
        if path_rest:
            path_resolve_fn = getattr(value, "path_resolve", None)
            if path_resolve_fn is None:
                raise ValueError("Path {:s} resolves to a non RNA value".format(attr))
            return path_resolve_fn(path_rest, coerce)

        return value

    def copy(self):
        """
        Get context members as a dictionary.

        :rtype: dict[str, Any]
        """
        from types import BuiltinMethodType
        new_context = {}
        generic_attrs = (
            *_StructRNA.__dict__.keys(),
            "bl_rna",
            "rna_type",
            "copy",
        )
        function_types = {BuiltinMethodType, _types.bpy_func}
        for attr in dir(self):
            if attr.startswith("_"):
                continue
            if attr in generic_attrs:
                continue
            value = getattr(self, attr)
            if type(value) in function_types:
                continue

            new_context[attr] = value

        return new_context


class Library(_types.ID):
    __slots__ = ()

    @property
    def users_id(self):
        """
        ID data-blocks that use this library

        :type: tuple of :class:`bpy.types.ID`

        .. note::

            Takes ``O(n)`` time, where ``n`` is the total number of all
            linkable ID types in ``bpy.data``.
        """
        import bpy

        # See: `readblenentry.cc`, IDTYPE_FLAGS_ISLINKABLE,
        # we could make this an attribute in rna.
        attr_links = (
            "actions", "armatures", "brushes", "cameras",
            "curves", "grease_pencils", "collections", "images",
            "lights", "lattices", "materials", "metaballs",
            "meshes", "node_groups", "objects", "scenes",
            "sounds", "speakers", "textures", "texts",
            "fonts", "worlds",
        )

        return tuple(
            id_block
            for attr in attr_links
            for id_block in getattr(bpy.data, attr)
            if id_block.library == self
        )


class Texture(_types.ID):
    __slots__ = ()

    @property
    def users_material(self):
        """
        Materials that use this texture

        :type: tuple of :class:`Material`

        .. note:: Takes ``O(len(bpy.data.materials) * len(material.texture_slots))`` time.
        """
        import bpy
        return tuple(
            mat for mat in bpy.data.materials
            if self in (
                slot.texture for slot in mat.texture_slots
                if slot is not None
            )
        )

    @property
    def users_object_modifier(self):
        """
        Object modifiers that use this texture

        :type: tuple of :class:`Object`

        .. note:: Takes ``O(len(bpy.data.objects) * len(obj.modifiers))`` time.
        """
        import bpy
        return tuple(
            obj for obj in bpy.data.objects
            if self in (
                mod.texture for mod in obj.modifiers
                if mod.type == 'DISPLACE'
            )
        )


class Collection(_types.ID):
    __slots__ = ()

    @property
    def children_recursive(self):
        """
        A list of all children from this collection.

        :type: list of :class:`Collection`

        .. note::

            Takes ``O(n)`` time, where ``n`` is the total number of all
            descendant collections.
        """
        children_recursive = []

        def recurse(parent):
            for child in parent.children:
                children_recursive.append(child)
                recurse(child)

        recurse(self)
        return children_recursive

    @property
    def users_dupli_group(self):
        """
        The collection instance objects this collection is used in

        :type: tuple of :class:`Object`

        .. note:: Takes ``O(len(bpy.data.objects))`` time.
        """
        import bpy
        return tuple(
            obj for obj in bpy.data.objects
            if self == obj.instance_collection
        )


class Object(_types.ID):
    __slots__ = ()

    @property
    def children(self):
        """
        All the children of this object.

        :type: tuple of :class:`Object`

        .. note:: Takes ``O(len(bpy.data.objects))`` time.
        """
        import bpy
        return tuple(
            child for child in bpy.data.objects
            if child.parent == self
        )

    @property
    def children_recursive(self):
        """
        A list of all children from this object.

        :type: list of :class:`Object`

        .. note:: Takes ``O(len(bpy.data.objects))`` time.
        """
        import bpy
        parent_child_map = {}
        for child in bpy.data.objects:
            if (parent := child.parent) is not None:
                parent_child_map.setdefault(parent, []).append(child)

        children_recursive = []

        def recurse(parent):
            for child in parent_child_map.get(parent, ()):
                children_recursive.append(child)
                recurse(child)

        recurse(self)
        return children_recursive

    @property
    def users_collection(self):
        """
        The collections this object is in.

        :type: tuple of :class:`Collection`

        .. note:: Takes ``O(len(bpy.data.collections) + len(bpy.data.scenes))`` time.
        """
        import bpy
        return (
            tuple(
                collection for collection in bpy.data.collections
                if self in collection.objects[:]
            ) + tuple(
                scene.collection for scene in bpy.data.scenes
                if self in scene.collection.objects[:]
            )
        )

    @property
    def users_scene(self):
        """
        The scenes this object is in.

        :type: tuple of :class:`Scene`

        .. note:: Takes ``O(len(bpy.data.scenes) * len(bpy.data.objects))`` time.
        """
        import bpy
        return tuple(
            scene for scene in bpy.data.scenes
            if self in scene.objects[:]
        )

    def evaluated_geometry(self):
        """
        Get the evaluated geometry set of this evaluated object. This only works for
        objects that contain geometry data like meshes and curves but not e.g. cameras.

        :return: The evaluated geometry.
        :rtype: :class:`bpy.types.GeometrySet`
        """
        from bpy.types import GeometrySet
        return GeometrySet.from_evaluated_object(self)


class WindowManager(_types.ID):
    __slots__ = ()

    def popup_menu(
            self, draw_func, *,
            title="",
            icon='NONE',
    ):
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
            from_active_button=False,
    ):
        import bpy
        popup = self.popover_begin__internal(
            ui_units_x=ui_units_x,
            from_active_button=from_active_button,
        )

        try:
            draw_func(popup, bpy.context)
        finally:
            self.popover_end__internal(popup, keymap=keymap)

    def popup_menu_pie(
            self, event, draw_func, *,
            title="",
            icon='NONE',
    ):
        import bpy
        pie = self.piemenu_begin__internal(title, icon=icon, event=event)

        if pie:
            try:
                draw_func(pie, bpy.context)
            finally:
                self.piemenu_end__internal(pie)


class WorkSpace(_types.ID):
    __slots__ = ()

    def status_text_set(self, text):
        """
        Set the status text or None to clear,
        When text is a function, this will be called with the (header, context) arguments.
        """
        from bl_ui.space_statusbar import STATUSBAR_HT_header
        draw_fn = getattr(STATUSBAR_HT_header, "_draw_orig", None)
        if draw_fn is None:
            draw_fn = STATUSBAR_HT_header._draw_orig = STATUSBAR_HT_header.draw

        if not (text is None or isinstance(text, str)):
            draw_fn = text
            text = None

        self.status_text_set_internal(text)
        STATUSBAR_HT_header.draw = draw_fn


class _GenericBone:
    """
    functions for bones, common between Armature/Pose/Edit bones.
    internal subclassing use only.
    """
    __slots__ = ()

    def translate(self, vec):
        """
        Utility function to add *vec* to the head and tail of this bone.
        """
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
        """
        Vector pointing down the x-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() @ Vector((1.0, 0.0, 0.0))

    @property
    def y_axis(self):
        """
        Vector pointing down the y-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() @ Vector((0.0, 1.0, 0.0))

    @property
    def z_axis(self):
        """
        Vector pointing down the z-axis of the bone.
        """
        from mathutils import Vector
        return self.matrix.to_3x3() @ Vector((0.0, 0.0, 1.0))

    @property
    def basename(self):
        """
        The name of this bone before any ``.`` character.
        """
        # return self.name.rsplit(".", 1)[0]
        return self.name.split(".")[0]

    @property
    def parent_recursive(self):
        """
        A list of parents, starting with the immediate parent.
        """
        parent_list = []
        parent = self.parent

        while parent:
            if parent:
                parent_list.append(parent)

            parent = parent.parent

        return parent_list

    @property
    def center(self):
        """
        The midpoint between the head and the tail.
        """
        return (self.head + self.tail) * 0.5

    @property
    def vector(self):
        """
        The direction this bone is pointing.
        Utility function for (tail - head)
        """
        return (self.tail - self.head)

    # NOTE: each bone type is responsible for implementing `children`.
    # This is done since `Bone` has direct access to this data in RNA.
    @property
    def children_recursive(self):
        """
        A list of all children from this bone.

        .. note:: Takes ``O(len(bones)**2)`` time.
        """
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

        .. note:: Takes ``O(len(bones)**2)`` time.
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

        # `id_data` is an `Object` for `PosePone`, otherwise it's an `Armature`.
        if isinstance(self, PoseBone):
            return id_data.pose.bones
        if isinstance(self, EditBone):
            return id_data.edit_bones
        if isinstance(self, Bone):
            return id_data.bones
        raise RuntimeError("Invalid type {!r}".format(self))


class PoseBone(_StructRNA, _GenericBone, metaclass=_StructMetaPropGroup):
    __slots__ = ()

    @property
    def children(self):
        obj = self.id_data
        pbones = obj.pose.bones

        # Use Bone.children, which is a native RNA property.
        return tuple(pbones[bone.name] for bone in self.bone.children)


class Bone(_StructRNA, _GenericBone, metaclass=_StructMetaPropGroup):
    __slots__ = ()

    # NOTE: `children` is implemented in RNA.


class EditBone(_StructRNA, _GenericBone, metaclass=_StructMetaPropGroup):
    __slots__ = ()

    @property
    def children(self):
        """
        A list of all the bones children.

        .. note:: Takes ``O(len(bones))`` time.
        """
        return [child for child in self._other_bones if child.parent == self]

    def align_orientation(self, other):
        """
        Align this bone to another by moving its tail and settings its roll
        the length of the other bone is not used.
        """
        vec = other.vector.normalized() * self.length
        self.tail = self.head + vec
        self.roll = other.roll

    def transform(self, matrix, *, scale=True, roll=True):
        """
        Transform the bones head, tail, roll and envelope
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


class BoneCollection(_StructRNA, metaclass=_StructMetaPropGroup):
    __slots__ = ()

    @property
    def bones_recursive(self):
        """
        A set of all bones assigned to this bone collection and its child collections.
        """
        bones = set()
        collections = [self]

        while collections:
            visit = collections.pop()
            bones.update(visit.bones)
            collections.extend(visit.children)
        return bones


def _ord_ind(i1, i2):
    if i1 < i2:
        return i1, i2
    return i2, i1


def _name_convention_attribute_get(attributes, name, domain, data_type):
    try:
        attribute = attributes[name]
    except KeyError:
        return None
    if attribute.domain != domain:
        return None
    if attribute.data_type != data_type:
        return None
    return attribute


def _name_convention_attribute_ensure(attributes, name, domain, data_type):
    try:
        attribute = attributes[name]
    except KeyError:
        return attributes.new(name, data_type, domain)
    if attribute.domain == domain and attribute.data_type == data_type:
        return attribute
    attributes.remove(attribute)
    return attributes.new(name, data_type, domain)


def _name_convention_attribute_remove(attributes, name):
    try:
        attributes.remove(attributes[name])
    except KeyError:
        pass


class Mesh(_types.ID):
    __slots__ = ()

    def from_pydata(self, vertices, edges, faces, shade_flat=True):
        """
        Make a mesh from a list of vertices/edges/faces
        Until we have a nicer way to make geometry, use this.

        :arg vertices:

           float triplets each representing (X, Y, Z)
           eg: [(0.0, 1.0, 0.5), ...].

        :type vertices: Iterable[Sequence[float]]
        :arg edges:

           int pairs, each pair contains two indices to the
           *vertices* argument. eg: [(1, 2), ...]

           When an empty iterable is passed in, the edges are inferred from the polygons.

        :type edges: Iterable[Sequence[int]]
        :arg faces:

           iterator of faces, each faces contains three or more indices to
           the *vertices* argument. eg: [(5, 6, 8, 9), (1, 2, 3), ...]

        :type faces: Iterable[Sequence[int]]

        .. warning::

           Invalid mesh data
           *(out of range indices, edges with matching indices,
           2 sided faces... etc)* are **not** prevented.
           If the data used for mesh creation isn't known to be valid,
           run :class:`Mesh.validate` after this function.
        """
        from itertools import chain, islice, accumulate

        face_lengths = tuple(map(len, faces))

        # NOTE: check non-empty lists by length because of how `numpy` handles truth tests, see: #90268.
        vertices_len = len(vertices)
        edges_len = len(edges)
        faces_len = len(faces)

        self.vertices.add(vertices_len)
        self.edges.add(edges_len)
        self.loops.add(sum(face_lengths))
        self.polygons.add(faces_len)

        self.vertices.foreach_set("co", tuple(chain.from_iterable(vertices)))
        self.edges.foreach_set("vertices", tuple(chain.from_iterable(edges)))

        vertex_indices = tuple(chain.from_iterable(faces))
        loop_starts = tuple(islice(chain([0], accumulate(face_lengths)), faces_len))

        self.polygons.foreach_set("loop_start", loop_starts)
        self.polygons.foreach_set("vertices", vertex_indices)

        if shade_flat:
            self.shade_flat()

        if edges_len or faces_len:
            self.update(
                # Needed to either:
                # - Calculate edges that don't exist for polygons.
                # - Assign edges to polygon loops.
                calc_edges=bool(faces_len),
                # Flag loose edges.
                calc_edges_loose=bool(edges_len),
            )

    @property
    def edge_keys(self):
        return [ed.key for ed in self.edges]

    @property
    def vertex_creases(self):
        """
        Vertex crease values for subdivision surface, corresponding to the "crease_vert" attribute.
        """
        return _name_convention_attribute_get(self.attributes, "crease_vert", 'POINT', 'FLOAT')

    def vertex_creases_ensure(self):
        return _name_convention_attribute_ensure(self.attributes, "crease_vert", 'POINT', 'FLOAT')

    def vertex_creases_remove(self):
        _name_convention_attribute_remove(self.attributes, "crease_vert")

    @property
    def edge_creases(self):
        """
        Edge crease values for subdivision surface, corresponding to the "crease_edge" attribute.
        """
        return _name_convention_attribute_get(self.attributes, "crease_edge", 'EDGE', 'FLOAT')

    def edge_creases_ensure(self):
        return _name_convention_attribute_ensure(self.attributes, "crease_edge", 'EDGE', 'FLOAT')

    def edge_creases_remove(self):
        _name_convention_attribute_remove(self.attributes, "crease_edge")

    @property
    def vertex_paint_mask(self):
        """
        Mask values for sculpting and painting, corresponding to the ".sculpt_mask" attribute.
        """
        return _name_convention_attribute_get(self.attributes, ".sculpt_mask", 'POINT', 'FLOAT')

    def vertex_paint_mask_ensure(self):
        return _name_convention_attribute_ensure(self.attributes, ".sculpt_mask", 'POINT', 'FLOAT')

    def vertex_paint_mask_remove(self):
        _name_convention_attribute_remove(self.attributes, ".sculpt_mask")

    def shade_flat(self):
        """
        Render and display faces uniform, using face normals,
        setting the "sharp_face" attribute true for every face
        """
        sharp_faces = _name_convention_attribute_ensure(self.attributes, "sharp_face", 'FACE', 'BOOLEAN')
        for value in sharp_faces.data:
            value.value = True

    def shade_smooth(self):
        """
        Render and display faces smooth, using interpolated vertex normals,
        removing the "sharp_face" attribute
        """
        _name_convention_attribute_remove(self.attributes, "sharp_face")


class MeshEdge(_StructRNA):
    __slots__ = ()

    @property
    def key(self):
        return _ord_ind(*tuple(self.vertices))


class MeshLoopTriangle(_StructRNA):
    __slots__ = ()

    @property
    def center(self):
        """
        The midpoint of the face.
        """
        face_verts = self.vertices[:]
        mesh_verts = self.id_data.vertices
        return (
            mesh_verts[face_verts[0]].co +
            mesh_verts[face_verts[1]].co +
            mesh_verts[face_verts[2]].co
        ) / 3.0

    @property
    def edge_keys(self):
        verts = self.vertices[:]
        return (
            _ord_ind(verts[0], verts[1]),
            _ord_ind(verts[1], verts[2]),
            _ord_ind(verts[2], verts[0]),
        )


class MeshPolygon(_StructRNA):
    __slots__ = ()

    @property
    def edge_keys(self):
        verts = self.vertices[:]
        vlen = len(self.vertices)
        return [_ord_ind(verts[i], verts[(i + 1) % vlen]) for i in range(vlen)]

    @property
    def loop_indices(self):
        start = self.loop_start
        end = start + self.loop_total
        return range(start, end)


class Text(_types.ID):
    __slots__ = ()

    def as_module(self):
        import bpy
        from os.path import splitext, join
        from types import ModuleType
        name = self.name
        mod = ModuleType(splitext(name)[0])
        # This is a fake file-path, set this since some scripts check `__file__`,
        # error messages may include this as well.
        # NOTE: the file path may be a blank string if the file hasn't been saved.
        mod.__dict__.update({
            "__file__": join(bpy.data.filepath, name),
        })
        # TODO: We could use Text.compiled (C struct member)
        # if this is called often it will be much faster.
        exec(self.as_string(), mod.__dict__)
        return mod


class Sound(_types.ID):
    __slots__ = ()

    @property
    def factory(self):
        """
        The aud.Factory object of the sound.
        """
        import aud
        return aud._sound_from_pointer(self.as_pointer())


class _RNAMeta(type):
    # TODO(campbell): move to C-API
    @property
    def is_registered(cls):
        return "bl_rna" in cls.__dict__


class _RNAMetaPropGroup(_StructMetaPropGroup, _RNAMeta):
    pass


# Same as `Operator`.
# only without 'as_keywords'
class Gizmo(_StructRNA):
    __slots__ = ()

    def __getattribute__(self, attr):
        properties = _StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return getattr(properties, attr)
        return super().__getattribute__(attr)

    def __setattr__(self, attr, value):
        properties = _StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return setattr(properties, attr, value)
        return super().__setattr__(attr, value)

    def __delattr__(self, attr):
        properties = _StructRNA.path_resolve(self, "properties")
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
        Draw a shape created form :class:`Gizmo.draw_custom_shape`.

        :arg shape: The cached shape to draw.
        :type shape: Any
        :arg matrix: 4x4 matrix, when not given :class:`Gizmo.matrix_world` is used.
        :type matrix: :class:`mathutils.Matrix`
        :arg select_id: The selection id.
           Only use when drawing within :class:`Gizmo.draw_select`.
        :type select_id: int
        """
        import gpu

        if matrix is None:
            matrix = self.matrix_world

        batch, shader = shape

        if select_id is not None:
            gpu.select.load_id(select_id)
            use_blend = False
        else:
            if self.is_highlight:
                color = (*self.color_highlight, self.alpha_highlight)
            else:
                color = (*self.color, self.alpha)
            shader.uniform_float("color", color)
            use_blend = color[3] < 1.0

        if use_blend:
            gpu.state.blend_set('ALPHA')

        with gpu.matrix.push_pop():
            gpu.matrix.multiply_matrix(matrix)
            batch.draw(shader)

        if use_blend:
            gpu.state.blend_set('NONE')

    @staticmethod
    def new_custom_shape(type, verts):
        """
        Create a new shape that can be passed to :class:`Gizmo.draw_custom_shape`.

        :arg type: The type of shape to create in (POINTS, LINES, TRIS, LINE_STRIP).
        :type type: str
        :arg verts: Sequence of 2D or 3D coordinates.
        :type verts: Sequence[Sequence[float]]
        :return: The newly created shape (the return type make change).
        :rtype: Any
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
        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        return (batch, shader)


# Dummy class to keep the reference in `bpy_types_dict` and avoid
# errors like: "TypeError: expected GizmoGroup subclass of class ..."
class GizmoGroup(_StructRNA):
    __slots__ = ()


# Only defined so operators members can be used by accessing self.order
# with doc generation 'self.properties.bl_rna.properties' can fail
class Operator(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()

    def __getattribute__(self, attr):
        properties = _StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return getattr(properties, attr)
        return super().__getattribute__(attr)

    def __setattr__(self, attr, value):
        properties = _StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return setattr(properties, attr, value)
        return super().__setattr__(attr, value)

    def __delattr__(self, attr):
        properties = _StructRNA.path_resolve(self, "properties")
        bl_rna = getattr(properties, "bl_rna", None)
        if (bl_rna is not None) and (attr in bl_rna.properties):
            return delattr(properties, attr)
        return super().__delattr__(attr)

    def as_keywords(self, *, ignore=()):
        """
        Return a copy of the properties as a dictionary.
        """
        ignore = ignore + ("rna_type",)
        return {
            attr: getattr(self, attr)
            for attr in self.properties.rna_type.properties.keys()
            if attr not in ignore
        }


class Macro(_StructRNA):
    # Chain multiple operators together and invokes them sequentially as a single operator.

    # _types is imported before ops is defined
    # so we have to do a local import on each run
    __slots__ = ()

    @classmethod
    def define(cls, operator):
        """
        Append an operator to a registered macro class.

        :arg operator: Identifier of the operator. This does not have to be defined when this function is called.
        :type operator: str
        :return: The operator macro for property access.
        :rtype: :class:`OperatorMacro`
        """
        from _bpy import ops
        return ops.macro_define(cls, operator)


class PropertyGroup(_StructRNA, metaclass=_RNAMetaPropGroup):
    __slots__ = ()


class KeyingSetInfo(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()


class USDHook(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()


class AddonPreferences(_StructRNA, metaclass=_RNAMeta):
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
                    # Exclude Import/Export menus from this filtering (IO add-ons should always show there).
                    if not getattr(self, "bl_owner_use_filter", True):
                        pass
                    elif owner_names is not None:
                        owner_id = getattr(func, "_owner", None)
                        if owner_id is not None:
                            if func._owner not in owner_names:
                                continue
                    # End 'owner_id' filter.

                    # so bad menu functions don't stop
                    # the entire menu from drawing
                    try:
                        func(self, context)
                    except Exception:
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
        draw_funcs = getattr(cls.draw, "_draw_funcs", None)
        if draw_funcs is None:
            return False
        # Ignore the first item (the original draw function).
        # This can happen when enabling then disabling add-ons.
        return len(draw_funcs) > 1

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
        """
        Remove a draw function that has been added to this menu.
        """
        draw_funcs = cls._dyn_ui_initialize()
        try:
            draw_funcs.remove(draw_func)
        except ValueError:
            pass


class Panel(_StructRNA, _GenericUI, metaclass=_RNAMeta):
    __slots__ = ()


class UIList(_StructRNA, _GenericUI, metaclass=_RNAMeta):
    __slots__ = ()


class Header(_StructRNA, _GenericUI, metaclass=_RNAMeta):
    __slots__ = ()


class Menu(_StructRNA, _GenericUI, metaclass=_RNAMeta):
    __slots__ = ()

    def path_menu(
        self, searchpaths, operator,
        *,
        props_default=None,
        prop_filepath="filepath",
        filter_ext=None,
        filter_path=None,
        display_name=None,
        add_operator=None,
        add_operator_props=None,
        translate=True,
    ):
        """
        Populate a menu from a list of paths.

        :arg searchpaths: Paths to scan.
        :type searchpaths: Sequence[str]
        :arg operator: The operator id to use with each file.
        :type operator: str
        :arg prop_filepath: Optional operator filepath property (defaults to "filepath").
        :type prop_filepath: str
        :arg props_default: Properties to assign to each operator.
        :type props_default: dict[str, Any]
        :arg filter_ext: Optional callback that takes the file extensions.

           Returning false excludes the file from the list.

        :type filter_ext: Callable[[str], bool] | None
        :arg display_name: Optional callback that takes the full path, returns the name to display.
        :type display_name: Callable[[str], str]
        """

        layout = self.layout

        import os
        import re
        import bpy.utils
        from bpy.app.translations import pgettext_iface as iface_

        layout = self.layout

        if not searchpaths:
            layout.label(text="* Missing Paths *")

        # collect paths
        files = []
        for directory in searchpaths:
            files.extend([
                (f, os.path.join(directory, f))
                for f in os.listdir(directory)
                if (not f.startswith("."))
                if ((filter_ext is None) or
                    (filter_ext(os.path.splitext(f)[1])))
                if ((filter_path is None) or
                    (filter_path(f)))
            ])

        # Perform a "natural sort", so 20 comes after 3 (for example).
        files.sort(
            key=lambda file_path:
            tuple(int(t) if t.isdigit() else t for t in re.split(r"(\d+)", file_path[0].lower())),
        )

        col = layout.column(align=True)

        for f, filepath in files:
            # Intentionally pass the full path to 'display_name' callback,
            # since the callback may want to use part a directory in the name.
            row = col.row(align=True)
            name = display_name(filepath) if display_name else bpy.path.display_name(f)
            props = row.operator(
                operator,
                text=(iface_(name) if translate else name),
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
                if add_operator_props is not None:
                    for attr, value in add_operator_props.items():
                        setattr(props, attr, value)

        if add_operator:
            wm = bpy.data.window_managers[0]

            layout.separator()
            row = layout.row()

            sub = row.row()
            sub.emboss = 'NORMAL'
            sub.prop(wm, "preset_name", text="")

            props = row.operator(add_operator, text="", icon='ADD')
            props.name = wm.preset_name
            if add_operator_props is not None:
                for attr, value in add_operator_props.items():
                    setattr(props, attr, value)

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
        add_operator_props = getattr(self, "preset_add_operator_properties", None)
        self.path_menu(
            bpy.utils.preset_paths(self.preset_subdir),
            self.preset_operator,
            props_default=props_default,
            filter_ext=lambda ext: ext.lower() in ext_valid,
            add_operator=add_operator,
            add_operator_props=add_operator_props,
            display_name=lambda name: bpy.path.display_name(name, title_case=False)
        )

    @classmethod
    def draw_collapsible(cls, context, layout):
        # helper function for (optionally) collapsed header menus
        # only usable within headers
        if context.area.show_menus:
            # Align menus to space them closely.
            layout.row(align=True).menu_contents(cls.__name__)
        else:
            layout.menu(cls.__name__, icon='COLLAPSEMENU')


class AssetShelf(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()


class FileHandler(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()


class NodeTree(_types.ID, metaclass=_RNAMetaPropGroup):
    __slots__ = ()


class Node(_StructRNA, metaclass=_RNAMetaPropGroup):
    __slots__ = ()

    @classmethod
    def poll(cls, _ntree):
        return True


class NodeInternal(Node):
    __slots__ = ()


class NodeSocket(_StructRNA, metaclass=_RNAMetaPropGroup):
    __slots__ = ()

    @property
    def links(self):
        """
        List of node links from or to this socket.

        :type: :class:`NodeLinks`

        .. note:: Takes ``O(len(nodetree.links))`` time.
        """
        links = (
            link for link in self.id_data.links
            if self in (link.from_socket, link.to_socket)
        )
        if not self.is_output:
            links = sorted(
                links,
                key=lambda link: link.multi_input_sort_id,
                reverse=True,
            )
        return tuple(links)


class NodeTreeInterfaceItem(_StructRNA):
    __slots__ = ()


class NodeTreeInterfaceSocket(NodeTreeInterfaceItem, metaclass=_RNAMetaPropGroup):
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


class GeometryNode(NodeInternal):
    __slots__ = ()

    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'GeometryNodeTree'


class RenderEngine(_StructRNA, metaclass=_RNAMeta):
    __slots__ = ()


class HydraRenderEngine(RenderEngine):
    __slots__ = ()

    bl_use_shading_nodes_custom = False
    bl_delegate_id = 'HdStormRendererPlugin'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.engine_ptr = None

    def __del__(self):
        if hasattr(self, 'engine_ptr'):
            if self.engine_ptr:
                import _bpy_hydra
                _bpy_hydra.engine_free(self.engine_ptr)

    def get_render_settings(self, engine_type: str):
        """
        Provide render settings for ``HdRenderDelegate``.
        """
        return {}

    # Final render.
    def update(self, data, depsgraph):
        import _bpy_hydra

        engine_type = 'PREVIEW' if self.is_preview else 'FINAL'
        if not self.engine_ptr:
            self.engine_ptr = _bpy_hydra.engine_create(self, engine_type, self.bl_delegate_id)
        if not self.engine_ptr:
            return

        _bpy_hydra.engine_update(self.engine_ptr, depsgraph, None)

        for key, val in self.get_render_settings('PREVIEW' if self.is_preview else 'FINAL').items():
            _bpy_hydra.engine_set_render_setting(self.engine_ptr, key, val)

    def render(self, depsgraph):
        if not self.engine_ptr:
            return

        import _bpy_hydra
        _bpy_hydra.engine_render(self.engine_ptr)

    # Viewport render.
    def view_update(self, context, depsgraph):
        import _bpy_hydra
        if not self.engine_ptr:
            self.engine_ptr = _bpy_hydra.engine_create(self, 'VIEWPORT', self.bl_delegate_id)
        if not self.engine_ptr:
            return

        _bpy_hydra.engine_update(self.engine_ptr, depsgraph, context)

        for key, val in self.get_render_settings('VIEWPORT').items():
            _bpy_hydra.engine_set_render_setting(self.engine_ptr, key, val)

    def view_draw(self, context, depsgraph):
        if not self.engine_ptr:
            return

        import _bpy_hydra
        _bpy_hydra.engine_view_draw(self.engine_ptr, context)


class GreasePencilDrawing(_StructRNA):
    __slots__ = ()

    @property
    def strokes(self):
        """
        Return a collection of all the Grease Pencil strokes in this drawing.

        .. note::

           This API should *not* be used for performance critical operations.
           Use the :class:`GreasePencilDrawing.attributes` API instead.

        .. note::

           When point/curves count of a drawing is changed, the slice returned by this
           call prior to the change is no longer valid. You need to get the new stroke
           slice via ``drawing.strokes[n]``.
        """
        from _bpy_internal.grease_pencil.stroke import GreasePencilStrokeSlice
        num_strokes = self.attributes.domain_size('CURVE')
        return GreasePencilStrokeSlice(self, 0, num_strokes)


class Material(_types.ID):
    __slots__ = ()

    def inline_shader_nodes(self):
        """
        Get the inlined shader nodes of this material. This preprocesses the node tree
        to remove nested groups, repeat zones and more.

        :return: The inlined shader nodes.
        :rtype: :class:`bpy.types.InlineShaderNodes`
        """
        from bpy.types import InlineShaderNodes
        return InlineShaderNodes.from_material(self)


class Light(_types.ID):
    __slots__ = ()

    def inline_shader_nodes(self):
        """
        Get the inlined shader nodes of this light. This preprocesses the node tree
        to remove nested groups, repeat zones and more.

        :return: The inlined shader nodes.
        :rtype: :class:`bpy.types.InlineShaderNodes`
        """
        from bpy.types import InlineShaderNodes
        return InlineShaderNodes.from_light(self)


class World(_types.ID):
    __slots__ = ()

    def inline_shader_nodes(self):
        """
        Get the inlined shader nodes of this world. This preprocesses the node tree
        to remove nested groups, repeat zones and more.

        :return: The inlined shader nodes.
        :rtype: :class:`bpy.types.InlineShaderNodes`
        """
        from bpy.types import InlineShaderNodes
        return InlineShaderNodes.from_world(self)
