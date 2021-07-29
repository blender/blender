import importlib
import importlib.abc
import importlib.util
import keyword
import sys
import inspect
from itertools import chain

import bpy
import bpy.types
from .. import nodes
from bpy.props import FloatProperty, IntProperty, StringProperty, FloatVectorProperty
from bpy.types import Node

import sverchok
from sverchok.node_tree import SverchCustomTreeNode

class SvBaseTypeP:
    def __init__(self, **kwargs):
        self.kwargs = kwargs

    def add(self, key, value):
        self.kwargs[key] = value

    def get_prop(self):
        return self.prop_func(**self.kwargs)

class FloatP(SvBaseTypeP):
    prop_func = FloatProperty

class IntP(SvBaseTypeP):
    prop_func = IntProperty

class VectorP(SvBaseTypeP):
    prop_func = FloatVectorProperty



class SvBaseType:
    def __init__(self, name=None):
        self.name = name

class SvValueType(SvBaseType):
    pass

class Vertex(SvBaseType):
    bl_idname = "VerticesSocket"

class Int(SvBaseType):
    bl_idname = "SvIntSocket"

class uInt(SvBaseType):
    bl_idname = "SvUnsignedIntSocket"

class Float(SvBaseType):
    bl_idname = "SvFloatSocket"

class Matrix(SvBaseType):
    bl_idname = "MatrixSocket"

class Face():
    bl_idname = "StringsSocket"

class Edge():
    bl_idname = "StringsSocket"

# to allow Int instead Int() syntax
socket_types = set(SvBaseType.__subclasses__())

Node = "node"


# get different types of parameters to the node

class SocketGetter:
    def __init__(self, index):
        self.index = index

    def get_value(self, node):
        return node.inputs[self.index].sv_get()

class PropertyGetter:
    def __init__(self, name):
        self.name = name

    def get_value(self, node):
        return getattr(node, self.name)

class NodeGetter:

    @staticmethod
    def get_value(node):
        return node

def get_signature(func):
    """
    annotate the function with meta data from the signature
    """
    sig = inspect.signature(func)
    func._inputs_template = []
    func._parameters = []
    func._sv_properties = {}
    func._outputs_template = []
    if not hasattr(func, "label"):
        func.label = func.__name__

    for name, parameter in sig.parameters.items():
        annotation = parameter.annotation
        print(name, parameter, annotation)
        if isinstance(annotation, SvBaseType): # Socket type parameter
            func._parameters.append(SocketGetter(len(func._inputs_template)))
            if parameter.default == inspect.Signature.empty or parameter.default is None:
                socket_settings = dict()
            else:
                socket_settings = {"default_value": parameter.default}

            if annotation.name:
                socket_name = annotation.name
            else:
                socket_name = name

            func._inputs_template.append((socket_name, annotation.bl_idname, socket_settings))
        elif isinstance(annotation, SvBaseTypeP):
            func._parameters.append(PropertyGetter(name))
            if not (parameter.default == inspect.Signature.empty or parameter.default is None):
                annotation.add("default", parameter.default)
            func._sv_properties[name] = annotation.get_prop()
        elif annotation == "Node":
            func._parameters.append(NodeGetter())
        elif isinstance(annotation, type) and issubclass(annotation, SvBaseType): # Socket used with Int syntax instead of Int()
            func._parameters.append(SocketGetter(len(func._inputs_template)))
            if parameter.default == inspect.Signature.empty or parameter.default is None:
                socket_settings = {}
            else:
                socket_settings = {"default_value": parameter.default}
            socket_name = name
            func._inputs_template.append((socket_name, annotation.bl_idname, socket_settings))


        else:
            raise SyntaxError

    print(sig.return_annotation)
    for s_type in sig.return_annotation:
        socket_type = s_type.bl_idname
        if isinstance(s_type, SvBaseType):
            name = s_type.name
        else:
            name = s_type.__name__
        func._outputs_template.append((name, socket_type))


def class_factory(func):
    cls_dict = {}
    module_name = func.__module__.split(".")[-1]
    cls_name = "SvScriptMK3_{}".format((module_name, func.__name__))

    cls_dict["bl_idname"] = cls_name

    # draw etc
    cls_dict["bl_label"] = func.label if hasattr(func, "label") else func.__name__

    supported_overides = ["process", "draw_buttons", "update"] # etc?
    for name in supported_overides:
        value = getattr(func, name, None)
        if value:
            cls_dict[name] = value

    cls_dict["input_template"] = func._inputs_template
    cls_dict["output_template"] = func._outputs_template
    for name, prop in func._sv_properties.items():
        cls_dict[name] = prop

    cls_dict["func"] = staticmethod(func)

    bases = (SvScriptBase, SverchCustomTreeNode, bpy.types.Node)

    cls = type(cls_name, bases, cls_dict)

    old_cls = getattr(bpy.types, cls_name, None)
    if old_cls:
        bpy.utils.unregister_class(old_cls)
    bpy.utils.register_class(cls)

    func.cls = cls
    return cls



class SvScriptBase:
    """Base class for Script nodes"""

    module = StringProperty()
    func = None



    def draw_buttons(self, context, layout):
        func = self.func
        if func:
            if hasattr(func, "draw_buttons"):
                func.draw_buttons(self, context, layout)
            elif func._sv_properties:
                for prop in func._sv_properties.keys():
                    #print(prop)
                    layout.prop(self, prop)

    def process(self):
        func = self.func
        if not func:
            return

        param = tuple(p.get_value(self) for p in func._parameters)
        print(param, len(param))
        results = func(*param)

        for s, data in zip(self.outputs, results):
            if s.is_linked:
                s.sv_set(data)

    def sv_init(self, context):
        func = self.func
        if not func:
            return
        for socket_name, socket_bl_idname, prop_data in self.input_template:
            s = self.inputs.new(socket_bl_idname, socket_name)
            for name, value in prop_data.items():
                setattr(s, name, value)

        for socket_name, socket_bl_idname in self.output_template:
            self.outputs.new(socket_bl_idname, socket_name)



def node_script(*args, **values):
    def real_node_func(func):
        def annotate(func):
            for key, value in values.items():
                setattr(func, key, value)
        annotate(func)
        print("annotating")
        get_signature(func)
        print("got sig")
        module_name = func.__module__.split(".")[-1]
        print("module:", module_name)
        _func_lookup[module_name] = func
        return func
    if args and callable(args[0]):
        return real_node_func(args[0])
    else:
        print(args, values)
        return real_node_func

_script_modules = {}
_name_lookup = {}
_func_lookup = {}

def make_valid_identifier(name):
    """Create a valid python identifier from name for use a a part of class name"""
    if not name[0].isalpha():
        name = "Sv" + name
    return "".join(ch for ch in name if ch.isalnum() or ch == "_")

def load_script(text):
    """
    Will load the blender text file as a module in nodes.script
    """
    #global _script_modules
    #global _name_lookup

    if text.endswith(".py"):
        name = text.rstrip(".py")
    else:
        name = text

    if not name.isidentifier() or keyword.iskeyword(name):
        print("bad text name: {}".format(text))
        return

    name = make_valid_identifier(name)
    _name_lookup[name] = text

    if name in _script_modules:
        print("reloading")
        mod = _script_modules[name]
        importlib.reload(mod)
    else:
        mod = importlib.import_module("sverchok.nodes.script.{}".format(name))
        _script_modules[name] = mod

    func = _func_lookup[name]
    setattr(mod, "_func", func)
    if func._sv_properties:
        cls = class_factory(func)
        setattr(mod, "_class", cls)
    else:
        setattr(mod, "_class", None)



class SvFinder(importlib.abc.MetaPathFinder):

    def find_spec(self, fullname, path, target=None):
        if fullname.startswith("sverchok.nodes.script."):
            name = fullname.split(".")[-1]
            text_name = _name_lookup.get(name, "")
            if text_name in bpy.data.texts:
                return importlib.util.spec_from_loader(fullname, SvLoader(text_name))
            else:
                print("couldn't find file")

        elif fullname == "sverchok.nodes.script":
            # load Module, right now uses real but empty module, will perhaps change
            pass

        return None

socket_type_names = (t.__name__ for t in  chain(SvBaseType.__subclasses__(),
                                                SvBaseTypeP.__subclasses__()))

standard_header = """
from sverchok.utils.loadscript import (node_script, Node, {})
""".format("{}".format(", ".join(socket_type_names)))

standard_footer = """

def unregister():
    if _class:
        bpy.utils.unregister_class(_class)
"""


def script_preprocessor(text):
    lines = []
    inserted_header = False
    # try to be clever not upset line number reporting in exceptions
    for line in text.lines:
        if "@node_func" in line.body and not inserted_header:
            lines.append(standard_header)
            inserted_header = True

        if not line.body.strip() and not inserted_header:
            lines.append(inserted_header)
            inserted_header = True
            continue

        lines.append(line.body)

    lines.append(standard_footer)
    return "\n".join(lines)

class SvLoader(importlib.abc.SourceLoader):

    def __init__(self, text):
        self._text = text

    def get_data(self, path):
        # here we should insert things and preprocss the file to make it valid
        # this will upset line numbers...
        source = "".join((standard_header,
                          bpy.data.texts[self._text].as_string(),
                          standard_footer))
        return source

    def get_filename(self, fullname):
        return "<bpy.data.texts[{}]>".format(self._text)


def register():
    sys.meta_path.append(SvFinder())


def unregister():
    for mod in _script_modules.values():
        mod.unregister()

    for finder in sys.meta_path[:]:
        if isinstance(finder, SvFinder):
            sys.meta_path.remove(finder)
