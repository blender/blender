# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####

import sys

import bpy
from bpy.types import PropertyGroup
from bpy.props import (
    FloatProperty, FloatVectorProperty, IntProperty, StringProperty, EnumProperty)


class PropsBase:
    # ignored /internal names
    internal_names  = {"prop_name", "attr", "update", "bl_rna", "rna_type"}
    prop_name = StringProperty(description="Internal name")

    def get_settings(self):
        settings = {k:v for k, v in self.items() if k not in self.internal_names}
        return settings

    def set_settings(self, settings):
        for key, value in settings.items():
            setattr(self, key, value)

    def from_socket(self, socket):
        p_type, p_dict = getattr(socket.node.rna_type, socket.prop_name)
        self.set_settings(p_dict)
        self.prop_name = socket.prop_name

    def draw(self, context, layout):
        """
        names = [name for name in dir(self) if not name in self.internal_names and not name.startswith("__")]
        for name in sorted(names):
            layout.prop(self, name)
        """
        layout.prop(self, "default")
        layout.prop(self, "soft_min")
        layout.prop(self, "soft_max")
        layout.prop(self, "description")


# FloatProperty

'''
bpy.props.FloatProperty(name="", description="", default=0.0,
                        min=sys.float_info.min, max=sys.float_info.max,
                        soft_min=sys.float_info.min, soft_max=sys.float_info.max,
                        step=3, precision=2, options={'ANIMATABLE'}, subtype='NONE',
                        unit='NONE', update=None, get=None, set=None)

subtype (string) – Enumerator in ['PIXEL', 'UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', 'TIME', 'DISTANCE', 'NONE'].
unit (string) – Enumerator in ['NONE', 'LENGTH', 'AREA', 'VOLUME', 'ROTATION', 'TIME', 'VELOCITY', 'ACCELERATION'].
'''

unit_items = (
    ('NONE', 'NONE', 'NONE', 0),
    ('LENGTH', 'LENGTH', 'LENGTH', 1),
    ('AREA', 'AREA', 'AREA', 2),
    ('VOLUME', 'VOLUME', 'VOLUME', 3),
    ('ROTATION', 'ROTATION', 'ROTATION', 4),
    ('TIME', 'TIME', 'TIME', 5),
    ('VELOCITY', 'VELOCITY', 'VELOCITY', 6),
    ('ACCELERATION', 'ACCELERATION', 'ACCELERATION', 7)
)

float_items = (
    ('PIXEL', 'PIXEL', 'PIXEL', 0),
    ('UNSIGNED', 'UNSIGNED', 'UNSIGNED', 1),
    ('PERCENTAGE', 'PERCENTAGE', 'PERCENTAGE', 2),
    ('FACTOR', 'FACTOR', 'FACTOR', 3),
    ('ANGLE', 'ANGLE', 'ANGLE', 4),
    ('TIME', 'TIME', 'TIME', 5),
    ('DISTANCE', 'DISTANCE', 'DISTANCE', 6),
    ('NONE', 'NONE', 'NONE', 7)
)


class SvFloatPropertySettingsGroup(PropertyGroup, PropsBase):

    def get_settings(self):
        settings = super().get_settings()
        # okay this code could perhaps be more clever
        # but protects from gettings the index of the EnumProperty
        # instead of the appropriate string value
        if "subtype" in settings:
            ref_nr = settings['subtype']
            for item in float_items:
                if ref_nr == item[-1]:
                    settings['subtype'] = item[0]
                    break
        if "unit" in settings:
            ref_nr = settings['unit']
            for item in unit_items:
                if ref_nr == item[-1]:
                    settings['unit'] = item[0]
                    break
        return settings

    name = StringProperty(description="Show name")
    description = StringProperty()
    default = FloatProperty(default=0.0)
    min = FloatProperty(default=sys.float_info.min)
    max = FloatProperty(default=sys.float_info.max)
    soft_min = FloatProperty(default=sys.float_info.min)
    soft_max = FloatProperty(default=sys.float_info.max)
    step = IntProperty(default=3)
    precision = IntProperty(default=2)
    subtype = EnumProperty(items=float_items, name="Subtype", default='NONE')
    unit = EnumProperty(items=unit_items, name="Unit", default='NONE')


# INT PROPERTY

'''
bpy.props.IntProperty(name="", description="",
                    default=0, min=-2**31, max=2**31-1,
                    soft_min=-2**31, soft_max=2**31-1,
                    step=1, options={'ANIMATABLE'},
                    subtype='NONE', update=None, get=None, set=None)
Returns a new int property definition.

Parameters:
name (string) – Name used in the user interface.
description (string) – Text used for the tooltip and api documentation.
min (int) – Hard minimum, trying to assign a value below will silently assign this minimum instead.
max (int) – Hard maximum, trying to assign a value above will silently assign this maximum instead.
soft_max (int) – Soft maximum (<= max), user won’t be able to drag the widget above this value in the UI.
soft_min (int) – Soft minimum (>= min), user won’t be able to drag the widget below this value in the UI.
step (int) – Step of increment/decrement in UI, in [1, 100], defaults to 1 (WARNING: unused currently!).
options (set) – Enumerator in [‘HIDDEN’, ‘SKIP_SAVE’, ‘ANIMATABLE’, ‘LIBRARY_EDITABLE’, ‘PROPORTIONAL’,’TEXTEDIT_UPDATE’].
subtype (string) – Enumerator in [‘PIXEL’, ‘UNSIGNED’, ‘PERCENTAGE’, ‘FACTOR’, ‘ANGLE’, ‘TIME’, ‘DISTANCE’, ‘NONE’].
update (function) – Function to be called when this value is modified, This function must take 2 values (self, context) and return None. Warning there are no safety checks to avoid infinite recursion.
get (function) – Function to be called when this value is ‘read’, This function must take 1 value (self) and return the value of the property.
set (function) – Function to be called when this value is ‘written’, This function must take 2 values (self, value) and return None.
'''

int_subtypes =  [
    ('PIXEL', 'PIXEL', 'PIXEL', 0),
    ('UNSIGNED', 'UNSIGNED', 'UNSIGNED', 1),
    ('PERCENTAGE', 'PERCENTAGE', 'PERCENTAGE', 2),
    ('FACTOR', 'FACTOR', 'FACTOR', 3),
    ('ANGLE', 'ANGLE', 'ANGLE', 4),
    ('TIME', 'TIME', 'TIME', 5),
    ('DISTANCE', 'DISTANCE', 'DISTANCE', 6),
    ('NONE', 'NONE', 'NONE', 7)
]

class SvIntPropertySettingsGroup(PropertyGroup, PropsBase):

    def get_settings(self):
        # okay this code could perhaps be more clever
        # but protects from gettings the index of the EnumProperty
        # instead of the appropriate string value
        settings = super().get_settings()
        if "subtype" in settings:
            ref_nr = settings['subtype']
            for item in int_subtypes:
                if ref_nr == item[-1]:
                    settings['subtype'] = item[0]
                    break
        return settings

    name = StringProperty(description="Show name")
    description = StringProperty()
    default = IntProperty(default=0)
    min = IntProperty(default=-2**31)
    max = IntProperty(default=2**31-1)
    soft_min = IntProperty(default=-2**31)
    soft_max = IntProperty(default=2**31-1)
    step = IntProperty(default=1) # not used
    subtype = EnumProperty(items=float_items, name="Subtype", default='NONE')


classes = [
    SvFloatPropertySettingsGroup,
    SvIntPropertySettingsGroup
]


def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)


def unregister():
    for class_name in reversed(classes):
        bpy.utils.unregister_class(class_name)
