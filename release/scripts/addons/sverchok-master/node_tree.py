# -*- coding: utf-8 -*-
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

import sys
import time
import math

import bpy
from bpy.props import StringProperty, BoolProperty, FloatVectorProperty, IntProperty
from bpy.types import NodeTree, NodeSocket, NodeSocketStandard

from mathutils import Matrix

from sverchok import data_structure
from sverchok.data_structure import (
    updateNode,
    get_other_socket,
    socket_id,
    replace_socket)

from sverchok.core.socket_data import (
    SvGetSocketInfo,
    SvGetSocket,
    SvSetSocket,
    SvNoDataError,
    sentinel)

from sverchok.core.update_system import (
    build_update_list,
    process_from_node,
    process_tree,
    get_update_lists, update_error_nodes)

from sverchok.core.socket_conversions import (
    DefaultImplicitConversionPolicy,
    is_vector_to_matrix
    )

from sverchok.core.node_defaults import set_defaults_if_defined

from sverchok.utils import get_node_class_reference
from sverchok.utils.sv_node_utils import recursive_framed_location_finder
from sverchok.utils.context_managers import sv_preferences
from sverchok.utils.docstring import SvDocstring
import sverchok.utils.logging
from sverchok.utils.logging import debug

from sverchok.ui import color_def
from sverchok.ui.nodes_replacement import set_inputs_mapping, set_outputs_mapping

socket_colors = {
    "StringsSocket": (0.6, 1.0, 0.6, 1.0),
    "VerticesSocket": (0.9, 0.6, 0.2, 1.0),
    "SvQuaternionSocket": (0.9, 0.4, 0.7, 1.0),
    "SvColorSocket": (0.9, 0.8, 0.0, 1.0),
    "MatrixSocket": (0.2, 0.8, 0.8, 1.0),
    "SvDummySocket": (0.8, 0.8, 0.8, 0.3),
    "ObjectSocket": (0.69, 0.74, 0.73, 1.0),
    "TextSocket": (0.68, 0.85, 0.90, 1),
}


def process_from_socket(self, context):
    """Update function of exposed properties in Sockets"""
    self.node.process_node(context)


class SvSocketCommon:
    """ Base class for all Sockets """
    use_prop = BoolProperty(default=False)

    use_expander = BoolProperty(default=True)
    use_quicklink = BoolProperty(default=True)
    expanded = BoolProperty(default=False)

    @property
    def other(self):
        return get_other_socket(self)

    def set_default(self, value):
        if self.prop_name:
            setattr(self.node, self.prop_name, value)

    @property
    def socket_id(self):
        """Id of socket used by data_cache"""
        return str(hash(self.id_data.name + self.node.name + self.identifier))

    @property
    def index(self):
        """Index of socket"""
        node = self.node
        sockets = node.outputs if self.is_output else node.inputs
        for i, s in enumerate(sockets):
            if s == self:
                return i

    @property
    def hide_safe(self):
        return self.hide

    @hide_safe.setter
    def hide_safe(self, value):
        # handles both input and output.
        if self.is_linked:
            for link in self.links:
                self.id_data.links.remove(link)

        self.hide = value

    def sv_set(self, data):
        """Set output data"""
        SvSetSocket(self, data)

    def replace_socket(self, new_type, new_name=None):
        """Replace a socket with a socket of new_type and keep links,
        return the new socket, the old reference might be invalid"""
        return replace_socket(self, new_type, new_name)

    @property
    def extra_info(self):
        # print("getting base extra info")
        return ""

    def draw_expander_template(self, context, layout, prop_origin, prop_name="prop"):

        if self.bl_idname == "StringsSocket":
            layout.prop(prop_origin, prop_name)
        else:
            if self.use_expander:
                split = layout.split(percentage=.2, align=True)
                c1 = split.column(align=True)
                c2 = split.column(align=True)

                if self.expanded:
                    c1.prop(self, "expanded", icon='TRIA_UP', text='')
                    c1.label(text=self.name[0])
                    c2.prop(prop_origin, prop_name, text="", expand=True)
                else:
                    c1.prop(self, "expanded", icon='TRIA_DOWN', text="")
                    row = c2.row(align=True)
                    if self.bl_idname == "SvColorSocket":
                        row.prop(prop_origin, prop_name)
                    else:
                        row.template_component_menu(prop_origin, prop_name, name=self.name)

            else:
                layout.template_component_menu(prop_origin, prop_name, name=self.name)

    def draw_quick_link(self, context, layout, node):

        if self.use_quicklink:
            if self.bl_idname == "MatrixSocket":
                new_node_idname = "SvMatrixGenNodeMK2"
            elif self.bl_idname == "VerticesSocket":
                new_node_idname = "GenVectorsNode"
            else:
                return

            op = layout.operator('node.sv_quicklink_new_node_input', text="", icon="PLUGIN")
            op.socket_index = self.index
            op.origin = node.name
            op.new_node_idname = new_node_idname
            op.new_node_offsetx = -200 - 40 * self.index
            op.new_node_offsety = -30 * self.index

    def draw(self, context, layout, node, text):

        # just handle custom draw..be it input or output.
        # hasattr may be excessive here
        if self.bl_idname == 'StringsSocket':
            if hasattr(self, 'custom_draw') and self.custom_draw:

                # does the node have the draw function referred to by 
                # the string stored in socket's custom_draw attribute
                if hasattr(node, self.custom_draw):
                    getattr(node, self.custom_draw)(self, context, layout)
                    return

            if node.bl_idname in {'SvScriptNodeLite', 'SvScriptNode'}:
                if not self.is_output and not self.is_linked and self.prop_type:
                    layout.prop(node, self.prop_type, index=self.prop_index, text=self.name)
                    return

        if self.is_linked:  # linked INPUT or OUTPUT
            info_text = text + '. ' + SvGetSocketInfo(self)
            info_text += self.extra_info
            layout.label(info_text)

        elif self.is_output:  # unlinked OUTPUT
            layout.label(text)

        else:  # unlinked INPUT
            if self.prop_name:  # has property
                self.draw_expander_template(context, layout, prop_origin=node, prop_name=self.prop_name)

            elif self.use_prop:  # no property but use default prop
                self.draw_expander_template(context, layout, prop_origin=self)

            else:  # no property and not use default prop
                self.draw_quick_link(context, layout, node)
                layout.label(text)

    def draw_color(self, context, node):
        return socket_colors[self.bl_idname]

    def needs_data_conversion(self):
        if not self.is_linked:
            return False
        return self.other.bl_idname != self.bl_idname

    def convert_data(self, source_data, implicit_conversions=None):
        if not self.needs_data_conversion():
            return source_data
        else:
            policy = self.node.get_implicit_conversions(self.name, implicit_conversions)
            self.node.debug("Trying to convert data for input socket %s by %s", self.name, policy)
            return policy.convert(self, source_data)

class MatrixSocket(NodeSocket, SvSocketCommon):
    '''4x4 matrix Socket type'''
    bl_idname = "MatrixSocket"
    bl_label = "Matrix Socket"
    prop_name = StringProperty(default='')
    num_matrices = IntProperty(default=0)

    @property
    def extra_info(self):
        # print("getting matrix extra info")
        info = ""
        if is_vector_to_matrix(self):
            info = (" (" + str(self.num_matrices) + ")")

        return info

    def get_prop_data(self):
        return {}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        self.num_matrices = 0
        if self.is_linked and not self.is_output:
            source_data = SvGetSocket(self, deepcopy = True if self.needs_data_conversion() else deepcopy)
            return self.convert_data(source_data, implicit_conversions)

        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default


class VerticesSocket(NodeSocket, SvSocketCommon):
    '''For vertex data'''
    bl_idname = "VerticesSocket"
    bl_label = "Vertices Socket"

    prop = FloatVectorProperty(default=(0, 0, 0), size=3, update=process_from_socket)
    prop_name = StringProperty(default='')
    use_prop = BoolProperty(default=False)

    def get_prop_data(self):
        if self.prop_name:
            return {"prop_name": socket.prop_name}
        elif self.use_prop:
            return {"use_prop": True,
                    "prop": self.prop[:]}
        else:
            return {}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            source_data = SvGetSocket(self, deepcopy = True if self.needs_data_conversion() else deepcopy)
            return self.convert_data(source_data, implicit_conversions)

        if self.prop_name:
            return [[getattr(self.node, self.prop_name)[:]]]
        elif self.use_prop:
            return [[self.prop[:]]]
        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default


class SvQuaternionSocket(NodeSocket, SvSocketCommon):
    '''For quaternion data'''
    bl_idname = "SvQuaternionSocket"
    bl_label = "Quaternion Socket"

    prop = FloatVectorProperty(default=(1, 0, 0, 0), size=4, subtype='QUATERNION', update=process_from_socket)
    prop_name = StringProperty(default='')
    use_prop = BoolProperty(default=False)

    def get_prop_data(self):
        if self.prop_name:
            return {"prop_name": socket.prop_name}
        elif self.use_prop:
            return {"use_prop": True,
                    "prop": self.prop[:]}
        else:
            return {}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            source_data = SvGetSocket(self, deepcopy = True if self.needs_data_conversion() else deepcopy)
            return self.convert_data(source_data, implicit_conversions)

        if self.prop_name:
            return [[getattr(self.node, self.prop_name)[:]]]
        elif self.use_prop:
            return [[self.prop[:]]]
        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default


class SvColorSocket(NodeSocket, SvSocketCommon):
    '''For color data'''
    bl_idname = "SvColorSocket"
    bl_label = "Color Socket"

    prop = FloatVectorProperty(default=(0, 0, 0, 1), size=4, subtype='COLOR', min=0, max=1, update=process_from_socket)
    prop_name = StringProperty(default='')
    use_prop = BoolProperty(default=False)

    def get_prop_data(self):
        if self.prop_name:
            return {"prop_name": socket.prop_name}
        elif self.use_prop:
            return {"use_prop": True,
                    "prop": self.prop[:]}
        else:
            return {}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            return self.convert_data(SvGetSocket(self, deepcopy), implicit_conversions)

        if self.prop_name:
            return [[getattr(self.node, self.prop_name)[:]]]
        elif self.use_prop:
            return [[self.prop[:]]]
        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default


class SvDummySocket(NodeSocket, SvSocketCommon):
    '''Dummy Socket for sockets awaiting assignment of type'''
    bl_idname = "SvDummySocket"
    bl_label = "Dummys Socket"

    prop = FloatVectorProperty(default=(0, 0, 0), size=3, update=process_from_socket)
    prop_name = StringProperty(default='')
    use_prop = BoolProperty(default=False)

    def get_prop_data(self):
        return self.other.get_prop_data()

    def sv_get(self):
        if self.is_linked:
            return self.links[0].bl_idname

    def sv_type_conversion(self, new_self):
        self = new_self


class StringsSocket(NodeSocket, SvSocketCommon):
    '''Generic, mostly numbers, socket type'''
    bl_idname = "StringsSocket"
    bl_label = "Strings Socket"

    prop_name = StringProperty(default='')

    prop_type = StringProperty(default='')
    prop_index = IntProperty()
    nodule_color = FloatVectorProperty(default=socket_colors["StringsSocket"], size=4)

    custom_draw = StringProperty()

    def get_prop_data(self):
        if self.prop_name:
            return {"prop_name": self.prop_name}
        elif self.prop_type:
            return {"prop_type": self.prop_type,
                    "prop_index": self.prop_index}
        else:
            return {}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        # debug("Node %s, socket %s, is_linked: %s, is_output: %s",
        #         self.node.name, self.name, self.is_linked, self.is_output)

        if self.is_linked and not self.is_output:
            return self.convert_data(SvGetSocket(self, deepcopy), implicit_conversions)
        elif self.prop_name:
            # to deal with subtype ANGLE, this solution should be considered temporary...
            _, prop_dict = getattr(self.node.rna_type, self.prop_name, (None, {}))
            subtype = prop_dict.get("subtype", "")
            if subtype == "ANGLE":
                return [[math.degrees(getattr(self.node, self.prop_name))]]
            return [[getattr(self.node, self.prop_name)]]
        elif self.prop_type:
            return [[getattr(self.node, self.prop_type)[self.prop_index]]]
        elif default is not sentinel:
            return default
        else:
            raise SvNoDataError(self)

    def draw_color(self, context, node):
        return self.nodule_color


class SvLinkNewNodeInput(bpy.types.Operator):
    ''' Spawn and link new node to the left of the caller node'''
    bl_idname = "node.sv_quicklink_new_node_input"
    bl_label = "Add a new node to the left"

    socket_index = bpy.props.IntProperty()
    origin = bpy.props.StringProperty()
    new_node_idname = bpy.props.StringProperty()
    new_node_offsetx = bpy.props.IntProperty(default=-200)
    new_node_offsety = bpy.props.IntProperty(default=0)

    def execute(self, context):
        tree = context.space_data.edit_tree
        nodes, links = tree.nodes, tree.links

        caller_node = nodes.get(self.origin)
        new_node = nodes.new(self.new_node_idname)
        new_node.location[0] = caller_node.location[0] + self.new_node_offsetx
        new_node.location[1] = caller_node.location[1] + self.new_node_offsety
        links.new(new_node.outputs[0], caller_node.inputs[self.socket_index])

        if caller_node.parent:
            new_node.parent = caller_node.parent
            loc_xy = new_node.location[:]
            locx, locy = recursive_framed_location_finder(new_node, loc_xy)
            new_node.location = locx, locy

        return {'FINISHED'}


class SvNodeTreeCommon(object):
    '''
    Common methods shared between Sverchok node trees
    '''

    has_changed = BoolProperty(default=False)
    limited_init = BoolProperty(default=False)

    def build_update_list(self):
        build_update_list(self)

    def adjust_reroutes(self):

        reroutes = [n for n in self.nodes if n.bl_idname == 'NodeReroute']
        if not reroutes:
            return
        for n in reroutes:
            s = n.inputs[0]
            if s.links:
                self.freeze(True)

                other = get_other_socket(s)
                s_type = other.bl_idname
                if n.outputs[0].bl_idname != s_type:
                    out_socket = n.outputs.new(s_type, "Output")
                    in_sockets = [l.to_socket for l in n.outputs[0].links]
                    n.outputs.remove(n.outputs[0])
                    for i_s in in_sockets:
                        l = self.links.new(i_s, out_socket)

                self.unfreeze(True)

    def freeze(self, hard=False):
        if hard:
            self["don't update"] = 1
        elif not self.is_frozen():
            self["don't update"] = 0

    def is_frozen(self):
        return "don't update" in self

    def unfreeze(self, hard=False):
        if self.is_frozen():
            if hard or self["don't update"] == 0:
                del self["don't update"]

    def get_update_lists(self):
        return get_update_lists(self)

    @property
    def sv_trees(self):
        res = []
        for ng in bpy.data.node_groups:
            if ng.bl_idname in {'SverchCustomTreeType', 'SverchGroupTreeType'}:
                res.append(ng)
        return res


class SverchCustomTree(NodeTree, SvNodeTreeCommon):
    ''' Sverchok - architectural node programming of geometry in low level '''
    bl_idname = 'SverchCustomTreeType'
    bl_label = 'Sverchok Node Tree'
    bl_icon = 'RNA'

    def turn_off_ng(self, context):
        process_tree(self)

        # should turn off tree. for now it does by updating it whole
        # should work something like this
        # outputs = filter(lambda n: isinstance(n,SvOutput), self.nodes)
        # for node in outputs:
        #   node.disable()

    sv_animate = BoolProperty(name="Animate", default=True, description='Animate this layout')
    sv_show = BoolProperty(name="Show", default=True, description='Show this layout', update=turn_off_ng)
    sv_bake = BoolProperty(name="Bake", default=True, description='Bake this layout')
    sv_process = BoolProperty(name="Process", default=True, description='Process layout')
    sv_user_colors = StringProperty(default="")

    def update(self):
        '''
        Tags tree for update for handle
        get update list for debug info, tuple (fulllist, dictofpartiallists)
        '''
        self.has_changed = True

    def process_ani(self):
        """
        Process the Sverchok node tree if animation layers show true.
        For animation callback/handler
        """
        if self.sv_animate:
            process_tree(self)

    def process(self):
        """
        process the Sverchok tree upon editor changes from handler
        """
        if self.has_changed:
            self.build_update_list()
            self.has_changed = False
        if self.is_frozen():
            return
        if self.sv_process:
            process_tree(self)

        self.has_changed = False


class SverchCustomTreeNode:

    # A cache for get_docstring() method
    _docstring = None

    _implicit_conversion_policy = dict()

    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname in ['SverchCustomTreeType', 'SverchGroupTreeType']

    @property
    def node_id(self):
        if not self.n_id:
            self.n_id = str(hash(self) ^ hash(time.monotonic()))
        return self.n_id

    def mark_error(self, err):
        """
        marks the with system error color
        will automaticly be cleared and restored to
        the old color
        """
        ng = self.id_data
        update_error_nodes(ng, self.name, err)

    def getLogger(self):
        if hasattr(self, "draw_label"):
            name = self.draw_label()
        else:
            name = self.label
        if not name:
            name = self.bl_label
        if not name:
            name = self.__class__.__name__
        return sverchok.utils.logging.getLogger(name)

    def debug(self, msg, *args, **kwargs):
        self.getLogger().debug(msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        self.getLogger().info(msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        self.getLogger().warning(msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        self.getLogger().error(msg, *args, **kwargs)

    def exception(self, msg, *args, **kwargs):
        self.getLogger().exception(msg, *args, **kwargs)

    def set_implicit_conversions(self, input_socket_name, policy):
        """
        Set implicit conversion policy to be used by default for specified input socket.
        This policy will be used by default by subsequent .sv_get() calls to this socket.
        Policy can be passed as direct reference to the class, or as a class name.
        """
        if isinstance(policy, str):
            policy = getattr(sverchok.core.socket_conversions, policy)
        #self.debug("Set default conversion policy for socket %s to %s", input_socket_name, policy)
        self._implicit_conversion_policy[input_socket_name] = policy

    def get_implicit_conversions(self, input_socket_name, override=None):
        """
        Return implicit conversion policy that was set as default for specified socket
        by set_implicit_conversions() call.
        If override is specified, then it is returned in all cases.
        """
        if override is not None:
            return override
        return self._implicit_conversion_policy.get(input_socket_name, DefaultImplicitConversionPolicy)

    def set_color(self):
        color = color_def.get_color(self.bl_idname)
        if color:
            self.use_custom_color = True
            self.color = color

    def create_sockets(self):
        '''Create node input and output sockets from
        their descriptions in self.input_descriptors and self.output_descriptors.
        '''

        if hasattr(self, "input_descriptors"):
            for descriptor in self.input_descriptors:
                descriptor.create(self)
        if hasattr(self, "output_descriptors"):
            for descriptor in self.output_descriptors:
                descriptor.create(self)

    @classmethod
    def get_docstring(cls):
        """
        Get SvDocstring instance parsed from node's docstring.
        """
        docstring = cls._docstring
        if docstring is not None:
            return docstring
        else:
            cls._docstring = SvDocstring(cls.__doc__)
            return cls._docstring

    @classmethod
    def get_tooltip(cls):
        """
        Obtain tooltip for node for use in UI.

        This method is to be overriden in specific node class if node author
        does not like for some reason that tooltip is extracted from node's
        docstring.
        """

        return cls.get_docstring().get_tooltip()

    @classmethod
    def get_shorthand(cls):
        """
        Obtain node shorthand.

        This method is to be overriden in specific node class if node author
        does not like for some reason that shorthand is extracted from node's
        docstring.
        """

        return cls.get_docstring().get_shorthand()

    def node_replacement_menu(self, context, layout):
        """
        Draw menu items with node replacement operators.
        This is called from `rclick_menu()' method by default.
        Items are defined by `replacement_nodes' class property.
        Expected format is

            replacement_nodes = [
                (new_node_bl_idname, inputs_mapping_dict, outputs_mapping_dict)
            ]

        where new_node_bl_idname is bl_idname of replacement node class,
        inputs_mapping_dict is a dictionary mapping names of inputs of this node
        to names of inputs to new node, and outputs_mapping_dict is a dictionary
        mapping names of outputs of this node to names of outputs of new node.
        inputs_mapping_dict and outputs_mapping_dict can be None.
        """
        if hasattr(self, "replacement_nodes"):
            for bl_idname, inputs_mapping, outputs_mapping in self.replacement_nodes:
                node_class = get_node_class_reference(bl_idname)
                text = "Replace with {}".format(node_class.bl_label)
                op = layout.operator("node.sv_replace_node", text=text)
                op.old_node_name = self.name
                op.new_bl_idname = bl_idname
                set_inputs_mapping(op, inputs_mapping)
                set_outputs_mapping(op, outputs_mapping)

    def rclick_menu(self, context, layout):
        """
        Override this method to add specific items into
        node's right-click menu.
        Default implementation calls `node_replacement_menu'.
        """
        self.node_replacement_menu(context, layout)

    def migrate_from(self, old_node):
        """
        This method is called by node replacement operator.
        Override it to correctly copy settings from old_node
        to this (new) node.
        Default implementation does nothing.
        """
        pass

    def sv_init(self, context):
        self.create_sockets()

    def init(self, context):
        """
        this function is triggered upon node creation,
        - freezes the node
        - delegates further initialization information to sv_init
        - sets node color
        - unfreezes the node
        - sets custom defaults (nodes, and sockets)

        """
        ng = self.id_data

        ng.freeze()
        if hasattr(self, "sv_init"):
            self.sv_init(context)
        self.set_color()
        ng.unfreeze()

        if not ng.limited_init:
            # print('applying default for', self.name)
            set_defaults_if_defined(self)

    def process_node(self, context):
        '''
        Doesn't work as intended, inherited functions can't be used for bpy.props
        update= ...
        Still this is called from updateNode
        '''
        if self.id_data.bl_idname == "SverchCustomTreeType":
            if self.id_data.is_frozen():
                return

            if data_structure.DEBUG_MODE:
                a = time.perf_counter()
                process_from_node(self)
                b = time.perf_counter()
                debug("Partial update from node %s in %s", self.name, round(b - a, 4))
            else:
                process_from_node(self)
        elif self.id_data.bl_idname == "SverchGroupTreeType":
            monad = self.id_data
            for instance in monad.instances:
                instance.process_node(context)
        else:
            pass

classes = [
    SverchCustomTree,
    VerticesSocket, MatrixSocket, StringsSocket,
    SvColorSocket, SvQuaternionSocket, SvDummySocket,
    SvLinkNewNodeInput,
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
