# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from enum import Enum


class BezierHandle(Enum):
    LEFT = 1
    RIGHT = 2


class AttributeGetterSetter:
    """
    Helper class to get and set attributes at an index for a domain.
    """
    __slots__ = ("_attributes", "_index", "_domain")

    def __init__(self, attributes, index, domain):
        self._attributes = attributes
        self._index = index
        self._domain = domain

    def _get_attribute(self, name, type, default):
        if attribute := self._attributes.get(name):
            if type in {'FLOAT', 'INT', 'STRING', 'BOOLEAN', 'INT8', 'INT32_2D', 'QUATERNION', 'FLOAT4X4'}:
                return attribute.data[self._index].value
            elif type == 'FLOAT_VECTOR':
                return attribute.data[self._index].vector
            elif type in {'FLOAT_COLOR', 'BYTE_COLOR'}:
                return attribute.data[self._index].color
            else:
                raise Exception("Unknown type {!r}".format(type))
        return default

    def _set_attribute_value(self, attribute, index, type, value):
        if type in {'FLOAT', 'INT', 'STRING', 'BOOLEAN', 'INT8', 'INT32_2D', 'QUATERNION', 'FLOAT4X4'}:
            attribute.data[index].value = value
        elif type == 'FLOAT_VECTOR':
            attribute.data[index].vector = value
        elif type in {'FLOAT_COLOR', 'BYTE_COLOR'}:
            attribute.data[index].color = value
        else:
            raise Exception("Unknown type {!r}".format(type))

    def _set_attribute(self, name, type, value, default):
        if attribute := self._attributes.get(name):
            self._set_attribute_value(attribute, self._index, type, value)
        elif attribute := self._attributes.new(name, type, self._domain):
            # Fill attribute with default value
            num = self._attributes.domain_size(self._domain)
            for i in range(num):
                self._set_attribute_value(attribute, i, type, default)
            self._set_attribute_value(attribute, self._index, type, value)
        else:
            raise Exception(
                "Could not create attribute {:s} of type {!r}".format(name, type))


class SliceHelper:
    """
    Helper class to handle custom slicing.
    """
    __slots__ = ("_start", "_stop", "_size")

    def __init__(self, start: int, stop: int):
        self._start = start
        self._stop = stop
        self._size = stop - start

    def __len__(self):
        return self._size

    def _is_valid_index(self, key: int):
        if self._size <= 0:
            return False
        if key < 0:
            # Support indexing from the end.
            return abs(key) <= self._size
        return abs(key) < self._size

    def _getitem_helper(self, key):
        if isinstance(key, int):
            if not self._is_valid_index(key):
                raise IndexError("Key {:d} is out of range".format(key))
            # Turn the key into an index.
            return self._start + (key % self._size)
        elif isinstance(key, slice):
            if key.step is not None and key.step != 1:
                raise ValueError("Step values != 1 not supported")
            # Default to 0 and size for the start and stop values.
            start = key.start if key.start is not None else 0
            stop = key.stop if key.stop is not None else self._size
            # Wrap negative indices.
            start = self._size + start if start < 0 else start
            stop = self._size + stop if stop < 0 else stop
            # Clamp start and stop.
            start = max(0, min(start, self._size))
            stop = max(0, min(stop, self._size))
            return (self._start + start, self._start + stop)
        else:
            raise TypeError("Unexpected index of type {!r}".format(type(key)))


def def_prop_for_attribute(attr_name, type, default, doc):
    """
    Creates a property that can read and write an attribute.
    """

    def fget(self):
        # Define `getter` callback for property.
        return self._get_attribute(attr_name, type, default)

    def fset(self, value):
        # Define `setter` callback for property.
        self._set_attribute(attr_name, type, value, default)

    prop = property(fget=fget, fset=fset, doc=doc)
    return prop


def DefAttributeGetterSetters(attributes_list):
    """
    A class decorator that reads a list of attribute information &
    creates properties on the class with ``getters`` & ``setters``.
    """
    def wrapper(cls):
        for prop_name, attr_name, type, default, doc in attributes_list:
            prop = def_prop_for_attribute(attr_name, type, default, doc)
            setattr(cls, prop_name, prop)
        return cls
    return wrapper


class GreasePencilStrokePointHandle:
    """Proxy giving read-only/write access to Bézier handle data."""

    __slots__ = ("_point", "_handle")

    def __init__(self, point, handle: BezierHandle):
        self._point = point
        self._handle = handle

    @property
    def position(self):
        attribute_name = f"handle_{self._handle.name.lower()}"
        return self._point._get_attribute(attribute_name, "FLOAT_VECTOR", (0.0, 0.0, 0.0))

    @position.setter
    def position(self, value):
        attribute_name = f"handle_{self._handle.name.lower()}"
        self._point._set_attribute(attribute_name, "FLOAT_VECTOR", value, (0.0, 0.0, 0.0))

    @property
    def type(self):
        attribute_name = f"handle_type_{self._handle.name.lower()}"
        return self._point._get_attribute(attribute_name, "INT", 0)

    # Note: Setting the handle type is not allowed because recomputing the handle types isn't exposed to Python yet.

    @property
    def select(self):
        attribute_name = f".selection_handle_{self._handle.name.lower()}"
        return self._point._get_attribute(attribute_name, "BOOLEAN", True)

    @select.setter
    def select(self, value):
        attribute_name = f".selection_handle_{self._handle.name.lower()}"
        self._point._set_attribute(attribute_name, 'BOOLEAN', value, True)

# Define the list of attributes that should be exposed as read/write properties on the class.


@DefAttributeGetterSetters([
    # Property Name, Attribute Name, Type, Default Value, Doc-string.
    ("radius", "radius", 'FLOAT', 0.01, "The radius of the point."),
    ("opacity", "opacity", 'FLOAT', 0.0, "The opacity of the point."),
    ("vertex_color", "vertex_color", 'FLOAT_COLOR', (0.0, 0.0, 0.0, 0.0),
     "The color for this point. The alpha value is used as a mix factor with the base color of the stroke."),
    ("rotation", "rotation", 'FLOAT', 0.0,
     "The rotation for this point. Used to rotate textures."),
    ("delta_time", "delta_time", 'FLOAT', 0.0,
     "The time delta in seconds since the start of the stroke."),
])
class GreasePencilStrokePoint(AttributeGetterSetter):
    """
    A helper class to get access to stroke point data.
    """
    __slots__ = ("_drawing", "_curve_index", "_point_index")

    def __init__(self, drawing, curve_index, point_index):
        super().__init__(drawing.attributes, point_index, 'POINT')
        self._drawing = drawing
        self._curve_index = curve_index
        self._point_index = point_index

    @property
    def position(self):
        """
        The position of the point (in local space).
        """
        if attribute := self._attributes.get("position"):
            return attribute.data[self._point_index].vector
        # Position attribute should always exist, but return default just in case.
        return (0.0, 0.0, 0.0)

    @position.setter
    def position(self, value):
        # Position attribute should always exist
        if attribute := self._attributes.get("position"):
            attribute.data[self._point_index].vector = value
            # Tag the positions of the drawing.
            self._drawing.tag_positions_changed()

    @property
    def select(self):
        """
        The selection state for this point.
        """
        if attribute := self._attributes.get(".selection"):
            if attribute.domain == 'CURVE':
                return attribute.data[self._curve_index].value
            elif attribute.domain == 'POINT':
                return attribute.data[self._point_index].value
        # If the attribute doesn't exist, everything is selected.
        return True

    @select.setter
    def select(self, value):
        if attribute := self._attributes.get(".selection"):
            if attribute.domain == 'CURVE':
                attribute.data[self._curve_index].value = value
            elif attribute.domain == 'POINT':
                attribute.data[self._point_index].value = value
        elif attribute := self._attributes.new(".selection", 'BOOLEAN', 'POINT'):
            attribute.data[self._point_index].value = value

    @property
    def handle_left(self):
        """
        Return the left Bézier handle proxy, or None if this point's stroke isn't Bézier.
        """
        stroke_curve_type = self._drawing.strokes[self._curve_index].curve_type
        if stroke_curve_type == 2:  # 2 == Bézier (enum value in Blender)
            return GreasePencilStrokePointHandle(self, BezierHandle.LEFT)
        return None

    @property
    def handle_right(self):
        """
        Return the right Bézier handle proxy, or None if this point's stroke isn't Bézier.
        """
        stroke_curve_type = self._drawing.strokes[self._curve_index].curve_type
        if stroke_curve_type == 2:
            return GreasePencilStrokePointHandle(self, BezierHandle.RIGHT)
        return None


class GreasePencilStrokePointSlice(SliceHelper):
    """
    A helper class that represents a slice of GreasePencilStrokePoint's.
    """
    __slots__ = ("_drawing", "_curve_index")

    def __init__(self, drawing, curve_index: int, start: int, stop: int):
        super().__init__(start, stop)
        self._drawing = drawing
        self._curve_index = curve_index

    def __len__(self):
        return super().__len__()

    def __getitem__(self, key):
        key = super()._getitem_helper(key)
        if isinstance(key, int):
            return GreasePencilStrokePoint(self._drawing, self._curve_index, key)
        elif isinstance(key, tuple):
            start, stop = key
            return GreasePencilStrokePointSlice(self._drawing, self._curve_index, start, stop)


# Define the list of attributes that should be exposed as read/write properties on the class.
@DefAttributeGetterSetters([
    # Property Name, Attribute Name, Type, Default Value, Doc-string.
    ("cyclic", "cyclic", 'BOOLEAN', False, "The closed state for this stroke."),
    ("material_index", "material_index", 'INT', 0,
     "The index of the material for this stroke."),
    ("softness", "softness", 'FLOAT', 0.0,
     "Used by the renderer to generate a soft gradient from the stroke center line to the edges."),
    ("start_cap", "start_cap", 'INT8', 0, "The type of start cap of this stroke."),
    ("end_cap", "end_cap", 'INT8', 0, "The type of end cap of this stroke."),
    ("aspect_ratio", "aspect_ratio", 'FLOAT', 1.0,
     "The aspect ratio (x/y) used for textures. "),
    ("fill_opacity", "fill_opacity", 'FLOAT', 0.0, "The opacity of the fill."),
    ("fill_color", "fill_color", 'FLOAT_COLOR',
     (0.0, 0.0, 0.0, 0.0), "The color of the fill."),
    ("time_start", "init_time", 'FLOAT', 0.0,
     "A time value for when the stroke was created."),
])
class GreasePencilStroke(AttributeGetterSetter):
    """
    A helper class to get access to stroke data.
    """
    __slots__ = ("_drawing", "_curve_index", "_points_start_index", "_points_end_index")

    def __init__(self, drawing, curve_index: int, points_start_index: int, points_end_index: int):
        super().__init__(drawing.attributes, curve_index, 'CURVE')
        self._drawing = drawing
        self._curve_index = curve_index
        self._points_start_index = points_start_index
        self._points_end_index = points_end_index

    @property
    def points(self):
        """
        Return a slice of points in the stroke.
        """
        return GreasePencilStrokePointSlice(
            self._drawing,
            self._curve_index,
            self._points_start_index,
            self._points_end_index)

    def add_points(self, count: int):
        """
        Add new points at the end of the stroke and returns the new points as a list.
        """
        previous_end = self._points_end_index
        new_size = self._points_end_index - self._points_start_index + count
        self._drawing.resize_strokes(
            sizes=[new_size],
            indices=[self._curve_index],
        )
        self._points_end_index = self._points_start_index + new_size
        return GreasePencilStrokePointSlice(self._drawing, self._curve_index, previous_end, self._points_end_index)

    def remove_points(self, count: int):
        """
        Remove points at the end of the stroke.
        """
        new_size = self._points_end_index - self._points_start_index - count
        # A stroke need to have at least one point.
        if new_size < 1:
            new_size = 1
        self._drawing.resize_strokes(
            sizes=[new_size],
            indices=[self._curve_index],
        )
        self._points_end_index = self._points_start_index + new_size

    @property
    def curve_type(self):
        """
        The curve type of this stroke.
        """
        # Note: This is read-only which is why it is not part of the AttributeGetterSetters.
        return super()._get_attribute("curve_type", 'INT8', 0)

    @property
    def select(self):
        """
        The selection state for this stroke.
        """
        if attribute := self._attributes.get(".selection"):
            if attribute.domain == 'CURVE':
                return attribute.data[self._curve_index].value
            elif attribute.domain == 'POINT':
                return any([attribute.data[point_index].value for point_index in range(
                    self._points_start_index, self._points_end_index)])
        # If the attribute doesn't exist, everything is selected.
        return True

    @select.setter
    def select(self, value):
        if attribute := self._attributes.get(".selection"):
            if attribute.domain == 'CURVE':
                attribute.data[self._curve_index].value = value
            elif attribute.domain == 'POINT':
                for point_index in range(self._points_start_index, self._points_end_index):
                    attribute.data[point_index].value = value
        elif attribute := self._attributes.new(".selection", 'BOOLEAN', 'CURVE'):
            attribute.data[self._curve_index].value = value


class GreasePencilStrokeSlice(SliceHelper):
    """
    A helper class that represents a slice of GreasePencilStroke's.
    """
    __slots__ = ("_drawing", "_curve_offsets")

    def __init__(self, drawing, start: int, stop: int):
        super().__init__(start, stop)
        self._drawing = drawing
        self._curve_offsets = drawing.curve_offsets

    def __len__(self):
        return super().__len__()

    def __getitem__(self, key):
        key = super()._getitem_helper(key)
        if isinstance(key, int):
            offsets = self._curve_offsets
            return GreasePencilStroke(self._drawing, key, offsets[key].value, offsets[key + 1].value)
        elif isinstance(key, tuple):
            start, stop = key
            return GreasePencilStrokeSlice(self._drawing, start, stop)
