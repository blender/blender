# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

class AttributeGetterSetter:
    """
    Helper class to get and set attributes at an index for a domain.
    """

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

    def _set_attribute(self, name, type, value):
        if attribute := self._attributes.get(name, self._attributes.new(name, type, self._domain)):
            if type in {'FLOAT', 'INT', 'STRING', 'BOOLEAN', 'INT8', 'INT32_2D', 'QUATERNION', 'FLOAT4X4'}:
                attribute.data[self._index].value = value
            elif type == 'FLOAT_VECTOR':
                attribute.data[self._index].vector = value
            elif type in {'FLOAT_COLOR', 'BYTE_COLOR'}:
                attribute.data[self._index].color = value
            else:
                raise Exception("Unknown type {!r}".format(type))
        else:
            raise Exception("Could not create attribute {:s} of type {!r}".format(name, type))


def def_prop_for_attribute(attr_name, type, default, doc):
    """
    Creates a property that can read and write an attribute.
    """

    def fget(self):
        # Define `getter` callback for property.
        return self._get_attribute(attr_name, type, default)

    def fset(self, value):
        # Define `setter` callback for property.
        self._set_attribute(attr_name, type, value)
    prop = property(fget=fget, fset=fset, doc=doc)
    return prop


def DefAttributeGetterSetters(attributes_list):
    """
    A class decorator that reads a list of attribute information &
    creates properties on the class with `getters` & `setters`.
    """
    def wrapper(cls):
        for prop_name, attr_name, type, default, doc in attributes_list:
            prop = def_prop_for_attribute(attr_name, type, default, doc)
            setattr(cls, prop_name, prop)
        return cls
    return wrapper


# Define the list of attributes that should be exposed as read/write properties on the class.
@DefAttributeGetterSetters([
    # Property Name, Attribute Name, Type, Default Value, Doc-string.
    ('position', 'position', 'FLOAT_VECTOR', (0.0, 0.0, 0.0), "The position of the point (in local space)."),
    ('radius', 'radius', 'FLOAT', 0.01, "The radius of the point."),
    ('opacity', 'opacity', 'FLOAT', 0.0, "The opacity of the point."),
    ('select', '.selection', 'BOOLEAN', True, "The selection state for this point."),
    ('vertex_color', 'vertex_color', 'FLOAT_COLOR', (0.0, 0.0, 0.0, 0.0),
     "The color for this point. The alpha value is used as a mix factor with the base color of the stroke."),
    ('rotation', 'rotation', 'FLOAT', 0.0, "The rotation for this point. Used to rotate textures."),
    ('delta_time', 'delta_time', 'FLOAT', 0.0, "The time delta in seconds since the start of the stroke."),
])
class GreasePencilStrokePoint(AttributeGetterSetter):
    """
    A helper class to get access to stroke point data.
    """

    def __init__(self, drawing, point_index):
        super().__init__(drawing.attributes, point_index, 'POINT')


class GreasePencilStrokePointSlice:
    """
    A helper class that represents a slice of GreasePencilStrokePoint's.
    """

    def __init__(self, drawing, start, stop):
        self._drawing = drawing
        self._start = start
        self._stop = stop
        self._size = stop - start

    def __len__(self):
        return self._size

    def _is_valid_index(self, key):
        if self._size <= 0:
            return False
        if key < 0:
            # Support indexing from the end.
            return abs(key) <= self._size
        return abs(key) < self._size

    def __getitem__(self, key):
        if isinstance(key, int):
            if not self._is_valid_index(key):
                raise IndexError("Key {:d} is out of range".format(key))
            # Turn the key into an index.
            point_i = self._start + (key % self._size)
            return GreasePencilStrokePoint(self._drawing, point_i)
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
            return GreasePencilStrokePointSlice(self._drawing, self._start + start, self._start + stop)
        else:
            raise TypeError("Unexpected index of type {!r}".format(type(key)))


# Define the list of attributes that should be exposed as read/write properties on the class.
@DefAttributeGetterSetters([
    # Property Name, Attribute Name, Type, Default Value, Doc-string.
    ('cyclic', 'cyclic', 'BOOLEAN', False, "The closed state for this stroke."),
    ('material_index', 'material_index', 'INT', 0, "The index of the material for this stroke."),
    ('select', '.selection', 'BOOLEAN', True, "The selection state for this stroke."),
    ('softness', 'softness', 'FLOAT', 0.0, "Used by the renderer to generate a soft gradient from the stroke center line to the edges."),
    ('start_cap', 'start_cap', 'INT8', 0, "The type of start cap of this stroke."),
    ('end_cap', 'end_cap', 'INT8', 0, "The type of end cap of this stroke."),
    ('curve_type', 'curve_type', 'INT8', 0, "The type of curve."),
    ('aspect_ratio', 'aspect_ratio', 'FLOAT', 1.0, "The aspect ratio (x/y) used for textures. "),
    ('fill_opacity', 'fill_opacity', 'FLOAT', 0.0, "The opacity of the fill."),
    ('fill_color', 'fill_color', 'FLOAT_COLOR', (0.0, 0.0, 0.0, 0.0), "The color of the fill."),
    ('time_start', 'init_time', 'FLOAT', 0.0, "A time value for when the stroke was created."),
])
class GreasePencilStroke(AttributeGetterSetter):
    """
    A helper class to get access to stroke data.
    """

    def __init__(self, drawing, curve_index, points_start_index, points_end_index):
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
        return GreasePencilStrokePointSlice(self._drawing, self._points_start_index, self._points_end_index)

    def add_points(self, count):
        """
        Add new points at the end of the stroke and returns the new points as a list.
        """
        previous_end = self._points_end_index
        new_size = self._points_end_index - self._points_start_index + count
        self._drawing.resize_curves(sizes=[new_size], indices=[self._curve_index])
        self._points_end_index = self._points_start_index + new_size
        return GreasePencilStrokePointSlice(self._drawing, previous_end, self._points_end_index)

    def remove_points(self, count):
        """
        Remove points at the end of the stroke.
        """
        new_size = self._points_end_index - self._points_start_index - count
        # A stroke need to have at least one point.
        if new_size < 1:
            new_size = 1
        self._drawing.resize_curves(sizes=[new_size], indices=[self._curve_index])
        self._points_end_index = self._points_start_index + new_size


class GreasePencilStrokeSlice:
    """
    A helper class that represents a slice of GreasePencilStroke's.
    """

    def __init__(self, drawing, start, stop):
        self._drawing = drawing
        self._curve_offsets = drawing.curve_offsets
        self._start = start
        self._stop = stop
        self._size = stop - start

    def __len__(self):
        return self._size

    def _is_valid_index(self, key):
        if self._size <= 0:
            return False
        if key < 0:
            # Support indexing from the end.
            return abs(key) <= self._size
        return abs(key) < self._size

    def __getitem__(self, key):
        if isinstance(key, int):
            if not self._is_valid_index(key):
                raise IndexError("Key {:d} is out of range".format(key))
            # Turn the key into an index.
            curve_i = self._start + (key % self._size)
            offsets = self._curve_offsets
            return GreasePencilStroke(self._drawing, curve_i, offsets[curve_i].value, offsets[curve_i + 1].value)
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
            return GreasePencilStrokeSlice(self._drawing, self._start + start, self._start + stop)
        else:
            raise TypeError("Unexpected index of type {!r}".format(type(key)))
