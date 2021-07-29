# -*- coding: utf-8 -*-
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# --------------------------------------------------------------------------
# Blender 2.5 Extensions Framework
# --------------------------------------------------------------------------
#
# Authors:
# Doug Hammond
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# ***** END GPL LICENCE BLOCK *****
#
import time

import bpy

from extensions_framework.ui import EF_OT_msg
bpy.utils.register_class(EF_OT_msg)
del EF_OT_msg

def log(str, popup=False, module_name='EF'):
    """Print a message to the console, prefixed with the module_name
    and the current time. If the popup flag is True, the message will
    be raised in the UI as a warning using the operator bpy.ops.ef.msg.

    """
    print("[%s %s] %s" %
        (module_name, time.strftime('%Y-%b-%d %H:%M:%S'), str))
    if popup:
        bpy.ops.ef.msg(
            msg_type='WARNING',
            msg_text=str
        )


added_property_cache = {}

def init_properties(obj, props, cache=True):
    """Initialise custom properties in the given object or type.
    The props list is described in the declarative_property_group
    class definition. If the cache flag is False, this function
    will attempt to redefine properties even if they have already been
    added.

    """

    if not obj in added_property_cache.keys():
        added_property_cache[obj] = []

    for prop in props:
        try:
            if cache and prop['attr'] in added_property_cache[obj]:
                continue

            if prop['type'] == 'bool':
                t = bpy.props.BoolProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","options","subtype","update"]}
            elif prop['type'] == 'bool_vector':
                t = bpy.props.BoolVectorProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","options","subtype","size",
                    "update"]}
            elif prop['type'] == 'collection':
                t = bpy.props.CollectionProperty
                a = {k: v for k,v in prop.items() if k in ["ptype","name",
                    "description","default","options"]}
                a['type'] = a['ptype']
                del a['ptype']
            elif prop['type'] == 'enum':
                t = bpy.props.EnumProperty
                a = {k: v for k,v in prop.items() if k in ["items","name",
                    "description","default","options","update"]}
            elif prop['type'] == 'float':
                t = bpy.props.FloatProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","min","max","soft_min","soft_max",
                    "step","precision","options","subtype","unit","update"]}
            elif prop['type'] == 'float_vector':
                t = bpy.props.FloatVectorProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","min","max","soft_min","soft_max",
                    "step","precision","options","subtype","size","update"]}
            elif prop['type'] == 'int':
                t = bpy.props.IntProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","min","max","soft_min","soft_max",
                    "step","options","subtype","update"]}
            elif prop['type'] == 'int_vector':
                t = bpy.props.IntVectorProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","min","max","soft_min","soft_max",
                    "options","subtype","size","update"]}
            elif prop['type'] == 'pointer':
                t = bpy.props.PointerProperty
                a = {k: v for k,v in prop.items() if k in ["ptype", "name",
                    "description","options","update"]}
                a['type'] = a['ptype']
                del a['ptype']
            elif prop['type'] == 'string':
                t = bpy.props.StringProperty
                a = {k: v for k,v in prop.items() if k in ["name",
                    "description","default","maxlen","options","subtype",
                    "update"]}
            else:
                continue

            setattr(obj, prop['attr'], t(**a))

            added_property_cache[obj].append(prop['attr'])
        except KeyError:
            # Silently skip invalid entries in props
            continue

class declarative_property_group(bpy.types.PropertyGroup):
    """A declarative_property_group describes a set of logically
    related properties, using a declarative style to list each
    property type, name, values, and other relevant information.
    The information provided for each property depends on the
    property's type.

    The properties list attribute in this class describes the
    properties present in this group.

    Some additional information about the properties in this group
    can be specified, so that a UI can be generated to display them.
    To that end, the controls list attribute and the visibility dict
    attribute are present here, to be read and interpreted by a
    property_group_renderer object.
    See extensions_framework.ui.property_group_renderer.

    """

    ef_initialised = False

    """This property tells extensions_framework which bpy.type(s)
    to attach this PropertyGroup to. If left as an empty list,
    it will not be attached to any type, but its properties will
    still be initialised. The type(s) given in the list should be
    a string, such as 'Scene'.

    """
    ef_attach_to = []

    @classmethod
    def initialise_properties(cls):
        """This is a function that should be called on
        sub-classes of declarative_property_group in order
        to ensure that they are initialised when the addon
        is loaded.
        the init_properties is called without caching here,
        as it is assumed that any addon calling this function
        will also call ef_remove_properties when it is
        unregistered.

        """

        if not cls.ef_initialised:
            for property_group_parent in cls.ef_attach_to:
                if property_group_parent is not None:
                    prototype = getattr(bpy.types, property_group_parent)
                    if not hasattr(prototype, cls.__name__):
                        init_properties(prototype, [{
                            'type': 'pointer',
                            'attr': cls.__name__,
                            'ptype': cls,
                            'name': cls.__name__,
                            'description': cls.__name__
                        }], cache=False)

            init_properties(cls, cls.properties, cache=False)
            cls.ef_initialised = True

        return cls

    @classmethod
    def register_initialise_properties(cls):
        """As ef_initialise_properties, but also registers the
        class with RNA. Note that this isn't a great idea
        because it's non-trivial to unregister the class, unless
        you keep track of it yourself.
        """

        bpy.utils.register_class(cls)
        cls.initialise_properties()
        return cls

    @classmethod
    def remove_properties(cls):
        """This is a function that should be called on
        sub-classes of declarative_property_group in order
        to ensure that they are un-initialised when the addon
        is unloaded.

        """

        if cls.ef_initialised:
            prototype = getattr(bpy.types, cls.__name__)
            for prop in cls.properties:
                if hasattr(prototype, prop['attr']):
                    delattr(prototype, prop['attr'])

            for property_group_parent in cls.ef_attach_to:
                if property_group_parent is not None:
                    prototype = getattr(bpy.types, property_group_parent)
                    if hasattr(prototype, cls.__name__):
                        delattr(prototype, cls.__name__)

            cls.ef_initialised = False

        return cls


    """This list controls the order of property layout when rendered
    by a property_group_renderer. This can be a nested list, where each
    list becomes a row in the panel layout. Nesting may be to any depth.

    """
    controls = []

    """The visibility dict controls the visibility of properties based on
    the value of other properties. See extensions_framework.validate
    for test syntax.

    """
    visibility = {}

    """The enabled dict controls the enabled state of properties based on
    the value of other properties. See extensions_framework.validate
    for test syntax.

    """
    enabled = {}

    """The alert dict controls the alert state of properties based on
    the value of other properties. See extensions_framework.validate
    for test syntax.

    """
    alert = {}

    """The properties list describes each property to be created. Each
    item should be a dict of args to pass to a
    bpy.props.<?>Property function, with the exception of 'type'
    which is used and stripped by extensions_framework in order to
    determine which Property creation function to call.

    Example item:
    {
        'type': 'int',                              # bpy.props.IntProperty
        'attr': 'threads',                          # bpy.types.<type>.threads
        'name': 'Render Threads',                   # Rendered next to the UI
        'description': 'Number of threads to use',  # Tooltip text in the UI
        'default': 1,
        'min': 1,
        'soft_min': 1,
        'max': 64,
        'soft_max': 64
    }

    """
    properties = []

    def draw_callback(self, context):
        """Sub-classes can override this to get a callback when
        rendering is completed by a property_group_renderer sub-class.

        """

        pass

    @classmethod
    def get_exportable_properties(cls):
        """Return a list of properties which have the 'save_in_preset' key
        set to True, and hence should be saved into preset files.

        """

        out = []
        for prop in cls.properties:
            if 'save_in_preset' in prop.keys() and prop['save_in_preset']:
                out.append(prop)
        return out

    def reset(self):
        """Reset all properties in this group to the default value,
        if specified"""
        for prop in self.properties:
            pk = prop.keys()
            if 'attr' in pk and 'default' in pk and hasattr(self, prop['attr']):
                setattr(self, prop['attr'], prop['default'])

class Addon(object):
    """A list of classes registered by this addon"""
    static_addon_count = 0

    addon_serial = 0
    addon_classes = None
    bl_info = None

    BL_VERSION = None
    BL_IDNAME = None

    def __init__(self, bl_info=None):
        self.addon_classes = []
        self.bl_info = bl_info

        # Keep a count in case we have to give this addon an anonymous name
        self.addon_serial = Addon.static_addon_count
        Addon.static_addon_count += 1

        if self.bl_info:
            self.BL_VERSION = '.'.join(['%s'%v for v in self.bl_info['version']]).lower()
            self.BL_IDNAME = self.bl_info['name'].lower() + '-' + self.BL_VERSION
        else:
            # construct anonymous name
            self.BL_VERSION = '0'
            self.BL_IDNAME = 'Addon-%03d'%self.addon_serial

    def addon_register_class(self, cls):
        """This method is designed to be used as a decorator on RNA-registerable
        classes defined by the addon. By using this decorator, this class will
        keep track of classes registered by this addon so that they can be
        unregistered later in the correct order.

        """
        self.addon_classes.append(cls)
        return cls

    def register(self):
        """This is the register function that should be exposed in the addon's
        __init__.

        """
        for cls in self.addon_classes:
            bpy.utils.register_class(cls)
            if hasattr(cls, 'ef_attach_to'): cls.initialise_properties()

    def unregister(self):
        """This is the unregister function that should be exposed in the addon's
        __init__.

        """
        for cls in self.addon_classes[::-1]:    # unregister in reverse order
            if hasattr(cls, 'ef_attach_to'): cls.remove_properties()
            bpy.utils.unregister_class(cls)

    def init_functions(self):
        """Returns references to the three functions that this addon needs
        for successful class registration management. In the addon's __init__
        you would use like this:

        addon_register_class, register, unregister = Addon().init_functions()

        """

        return self.register, self.unregister
