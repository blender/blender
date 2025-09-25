# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# classes for extracting info from blenders internal classes

__all__ = (
    "BuildRNAInfo",
    "InfoFunctionRNA",
    "InfoOperatorRNA",
    "InfoPropertyRNA",
    "InfoStructRNA",
    "rna_id_ignore",
)

import bpy

# use to strip python paths
script_paths = bpy.utils.script_paths()

_FAKE_STRUCT_SUBCLASS = True


def _get_direct_attr(rna_type, attr):
    props = getattr(rna_type, attr)
    base = rna_type.base

    if not base:
        return [prop for prop in props]
    else:
        props_base = getattr(base, attr).values()
        return [prop for prop in props if prop not in props_base]


def get_direct_properties(rna_type):
    return _get_direct_attr(rna_type, "properties")


def get_direct_functions(rna_type):
    return _get_direct_attr(rna_type, "functions")


def rna_id_ignore(rna_id):
    if rna_id == "rna_type":
        return True

    if "_OT_" in rna_id:
        return True
    if "_MT_" in rna_id:
        return True
    if "_PT_" in rna_id:
        return True
    if "_HT_" in rna_id:
        return True
    if "_KSI_" in rna_id:
        return True
    return False


def range_str(val):
    if val < -10000000:
        return "-inf"
    elif val > 10000000:
        return "inf"
    elif type(val) == float:
        return "{:g}".format(val)
    else:
        return str(val)


def float_as_string(f):
    val_str = "{:g}".format(f)
    # Ensure a `.0` suffix for whole numbers, excluding scientific notation such as `1e-05` or `1e+5`.
    if '.' not in val_str and 'e' not in val_str:
        val_str += '.0'
    return val_str


def get_py_class_from_rna(rna_type):
    """ Gets the Python type for a class which isn't necessarily added to ``bpy.types``.
    """
    identifier = rna_type.identifier
    py_class = getattr(bpy.types, identifier, None)
    if py_class is not None:
        return py_class

    def subclasses_recurse(cls):
        for c in cls.__subclasses__():
            # is_registered
            if "bl_rna" in c.__dict__:
                yield c
            yield from subclasses_recurse(c)

    base = rna_type.base
    while base is not None:
        py_class_base = getattr(bpy.types, base.identifier, None)
        if py_class_base is not None:
            for cls in subclasses_recurse(py_class_base):
                if cls.bl_rna.identifier == identifier:
                    return cls
        base = base.base
    raise Exception("can't find type")


class InfoStructRNA:
    __slots__ = (
        "bl_rna",
        "identifier",
        "name",
        "description",
        "base",
        "nested",
        "full_path",
        "functions",
        "children",
        "references",
        "properties",
        "py_class",
        "module_name",
    )

    global_lookup = {}

    def __init__(self, rna_type):
        self.bl_rna = rna_type

        self.identifier = rna_type.identifier
        self.name = rna_type.name
        self.description = rna_type.description.strip()

        # set later
        self.base = None
        self.nested = None
        self.full_path = ""

        self.functions = []
        self.children = []
        self.references = []
        self.properties = []

        self.py_class = get_py_class_from_rna(self.bl_rna)
        self.module_name = (
            self.py_class.__module__
            if (self.py_class and not hasattr(bpy.types, self.identifier)) else
            "bpy.types"
        )
        if self.module_name == "_bpy_types":
            self.module_name = "bpy.types"

    def build(self):
        rna_type = self.bl_rna
        parent_id = self.identifier
        self.properties[:] = [GetInfoPropertyRNA(rna_prop, parent_id)
                              for rna_prop in get_direct_properties(rna_type) if rna_prop.identifier != "rna_type"]
        self.functions[:] = [GetInfoFunctionRNA(rna_prop, parent_id)
                             for rna_prop in get_direct_functions(rna_type)]

    def get_bases(self):
        bases = []
        item = self

        while item:
            item = item.base
            if item:
                bases.append(item)

        return bases

    def get_nested_properties(self, ls=None):
        if not ls:
            ls = self.properties[:]

        if self.nested:
            self.nested.get_nested_properties(ls)

        return ls

    def _get_py_visible_attrs(self):
        attrs = []
        py_class = self.py_class

        for attr_str in dir(py_class):
            if attr_str.startswith("_"):
                continue
            attrs.append((attr_str, getattr(py_class, attr_str)))
        return attrs

    def get_py_properties(self):
        properties = []
        for identifier, attr in self._get_py_visible_attrs():
            if type(attr) is property:
                properties.append((identifier, attr))
        return properties

    def get_py_functions(self):
        import types
        functions = []
        for identifier, attr in self._get_py_visible_attrs():
            # Methods may be Python wrappers to C-API functions.
            ok = False
            if (attr_func := getattr(attr, "__func__", None)) is not None:
                if type(attr_func) == types.FunctionType:
                    ok = True
            else:
                if type(attr) in {types.FunctionType, types.MethodType}:
                    ok = True
            if ok:
                functions.append((identifier, attr))
        return functions

    def get_py_c_functions(self):
        import types
        functions = []
        for identifier, attr in self._get_py_visible_attrs():
            # Methods may be Python wrappers to C-API functions.
            ok = False
            if (attr_func := getattr(attr, "__func__", None)) is not None:
                if type(attr_func) == types.BuiltinFunctionType:
                    ok = True
            else:
                if type(attr) == types.BuiltinMethodType:
                    ok = True
                elif type(attr) == types.MethodDescriptorType:
                    # Without the `objclass` check, many inherited methods are included.
                    if attr.__objclass__ == self.py_class:
                        ok = True
            if ok:
                functions.append((identifier, attr))
        return functions

    def get_py_c_properties_getset(self):
        import types
        properties_getset = []
        for identifier, descr in self.py_class.__dict__.items():
            if type(descr) == types.GetSetDescriptorType:
                properties_getset.append((identifier, descr))
        return properties_getset

    def __str__(self):

        txt = ""
        txt += self.identifier
        if self.base:
            txt += "({:s})".format(self.base.identifier)
        txt += ": " + self.description + "\n"

        for prop in self.properties:
            txt += prop.__repr__() + "\n"

        for func in self.functions:
            txt += func.__repr__() + "\n"

        return txt


class InfoPropertyRNA:
    __slots__ = (
        "bl_prop",
        "srna",
        "identifier",
        "name",
        "description",
        "default_str",
        "default",
        "enum_items",
        "enum_pointer",
        "min",
        "max",
        "array_length",
        "array_dimensions",
        "collection_type",
        "type",
        "fixed_type",
        "subtype",
        "is_argument_optional",
        "is_enum_flag",
        "is_required",
        "is_readonly",
        "is_never_none",
        "is_path_supports_blend_relative",
        "is_path_supports_templates",
        "deprecated",
    )
    global_lookup = {}

    def __init__(self, rna_prop):
        self.bl_prop = rna_prop
        self.identifier = rna_prop.identifier
        self.name = rna_prop.name
        self.description = rna_prop.description.strip()
        self.default_str = "<UNKNOWN>"

    def build(self):
        rna_prop = self.bl_prop

        self.enum_items = []
        self.min = getattr(rna_prop, "hard_min", -1)
        self.max = getattr(rna_prop, "hard_max", -1)
        self.array_length = getattr(rna_prop, "array_length", 0)
        self.array_dimensions = getattr(rna_prop, "array_dimensions", ())[:]
        self.collection_type = GetInfoStructRNA(rna_prop.srna)
        self.subtype = getattr(rna_prop, "subtype", "")
        self.is_required = rna_prop.is_required
        self.is_readonly = rna_prop.is_readonly
        self.is_never_none = rna_prop.is_never_none
        self.is_argument_optional = rna_prop.is_argument_optional
        self.is_path_supports_blend_relative = rna_prop.is_path_supports_blend_relative
        self.is_path_supports_templates = rna_prop.is_path_supports_templates

        if rna_prop.is_deprecated:
            self.deprecated = (
                rna_prop.deprecated_note,
                tuple(rna_prop.deprecated_version),
                tuple(rna_prop.deprecated_removal_version),
            )
        else:
            self.deprecated = None

        self.type = rna_prop.type.lower()
        fixed_type = getattr(rna_prop, "fixed_type", "")
        if fixed_type:
            self.fixed_type = GetInfoStructRNA(fixed_type)  # valid for pointer/collections
        else:
            self.fixed_type = None

        self.enum_pointer = 0
        if self.type == "enum":
            # WARNING: don't convert to a tuple as this causes dynamically allocated enums to access freed memory
            # since freeing the iterator may free the memory used to store the internal `EnumPropertyItem` array.
            # To support this properly RNA would have to support owning the dynamically allocated memory.
            items = rna_prop.enum_items
            items_static = tuple(rna_prop.enum_items_static)
            self.enum_items[:] = [(item.identifier, item.name, item.description) for item in items]
            self.is_enum_flag = rna_prop.is_enum_flag
            # Prioritize static items as this is never going to be allocated data and is therefor
            # will be a stable match to compare against.
            item = (items_static or items)
            if item:
                self.enum_pointer = item[0].as_pointer()
            del items, items_static, item
        else:
            self.is_enum_flag = False

        self.default_str = ""  # fallback

        if self.array_length:
            self.default = tuple(getattr(rna_prop, "default_array", ()))
            if self.array_dimensions[1] != 0:  # Multi-dimensional array, convert default flat one accordingly.
                self.default_str = tuple(float_as_string(v) if self.type == "float" else str(v) for v in self.default)
                for dim in self.array_dimensions[::-1]:
                    if dim != 0:
                        self.default = tuple(zip(*((iter(self.default),) * dim)))
                        self.default_str = tuple(
                            "({:s})".format(", ".join(s for s in b)) for b in zip(*((iter(self.default_str),) * dim))
                        )
                self.default_str = self.default_str[0]
        elif self.type == "enum" and self.is_enum_flag:
            self.default = getattr(rna_prop, "default_flag", set())
        else:
            self.default = getattr(rna_prop, "default", None)

        if self.type == "pointer":
            # pointer has no default, just set as None
            self.default = None
            self.default_str = "None"
        elif self.type == "string":
            self.default_str = "\"{:s}\"".format(self.default)
        elif self.type == "enum":
            if self.is_enum_flag:
                # self.default_str = repr(self.default)  # repr or set()
                self.default_str = "{{{:s}}}".format(repr(list(sorted(self.default)))[1:-1])
            else:
                self.default_str = repr(self.default)
        elif self.array_length:
            if self.array_dimensions[1] == 0:  # single dimension array, we already took care of multi-dimensions ones.
                # special case for floats
                if self.type == "float" and len(self.default) > 0:
                    self.default_str = "({:s})".format(", ".join(float_as_string(f) for f in self.default))
                else:
                    self.default_str = str(self.default)
        else:
            if self.type == "float":
                self.default_str = float_as_string(self.default)
            else:
                self.default_str = str(self.default)

        self.srna = GetInfoStructRNA(rna_prop.srna)  # valid for pointer/collections

    def get_arg_default(self, force=True):
        default = self.default_str
        if default and (force or self.is_required is False):
            return "{:s}={:s}".format(self.identifier, default)
        return self.identifier

    def get_type_description(
            self, *,
            as_ret=False,
            as_arg=False,
            class_fmt="{:s}",
            mathutils_fmt="{:s}",
            literal_fmt="'{:s}'",
            collection_id="Collection",
            enum_descr_override=None,
    ):
        """
        :arg enum_descr_override: Optionally override items for enum.
           Otherwise expand the literal items.
        :type enum_descr_override: str | None
        """
        type_str = ""
        if self.fixed_type is None:
            type_str += self.type
            if self.type == "string" and self.subtype == "BYTE_STRING":
                type_str = "byte string"
            if self.array_length:
                if self.array_dimensions[1] != 0:
                    dimension_str = " of {:s} items".format(
                        " * ".join(str(d) for d in self.array_dimensions if d != 0)
                    )
                    type_str += " multi-dimensional array" + dimension_str
                else:
                    dimension_str = " of {:d} items".format(self.array_length)
                    type_str += " array" + dimension_str

                # Describe mathutils types; logic mirrors pyrna_math_object_from_array
                if self.type == "float":
                    if self.subtype == "MATRIX":
                        if self.array_length in {9, 16}:
                            type_str = (mathutils_fmt.format("Matrix")) + dimension_str
                    elif self.subtype in {"COLOR", "COLOR_GAMMA"}:
                        if self.array_length == 3:
                            type_str = (mathutils_fmt.format("Color")) + dimension_str
                    elif self.subtype in {"EULER", "QUATERNION"}:
                        if self.array_length == 3:
                            type_str = (mathutils_fmt.format("Euler")) + " rotation" + dimension_str
                        elif self.array_length == 4:
                            type_str = (mathutils_fmt.format("Quaternion")) + " rotation" + dimension_str
                    elif self.subtype in {
                            'COORDINATES', 'TRANSLATION', 'DIRECTION', 'VELOCITY',
                            'ACCELERATION', 'XYZ', 'XYZ_LENGTH',
                    }:
                        if 2 <= self.array_length <= 4:
                            type_str = (mathutils_fmt.format("Vector")) + dimension_str

            if self.type in {"float", "int"}:
                type_str += " in [{:s}, {:s}]".format(range_str(self.min), range_str(self.max))
            elif self.type == "enum":
                enum_descr = enum_descr_override
                if not enum_descr:
                    if self.is_enum_flag:
                        enum_descr = "{{{:s}}}".format(", ".join((literal_fmt.format(s[0])) for s in self.enum_items))
                    else:
                        enum_descr = "[{:s}]".format(", ".join((literal_fmt.format(s[0])) for s in self.enum_items))
                if self.is_enum_flag:
                    type_str += " set in {:s}".format(enum_descr)
                else:
                    type_str += " in {:s}".format(enum_descr)
                del enum_descr

            if not (as_arg or as_ret):
                # write default property, ignore function args for this.
                match self.type:
                    case "pointer":
                        pass
                    case "enum":
                        # Empty enums typically only occur for enums which are dynamically generated.
                        # In that case showing a default isn't helpful.
                        if self.default_str:
                            type_str += ", default {:s}".format(literal_fmt.format(self.default_str))
                    case _:
                        type_str += ", default {:s}".format(self.default_str)

        else:
            if self.type == "collection":
                if self.collection_type:
                    collection_str = (
                        class_fmt.format(self.collection_type.identifier) +
                        " {:s} of ".format(collection_id)
                    )
                else:
                    collection_str = "{:s} of ".format(collection_id)
            else:
                collection_str = ""

            type_str += collection_str + (class_fmt.format(self.fixed_type.identifier))

        # setup qualifiers for this value.
        type_info = []
        if as_ret:
            pass
        elif as_arg:
            if not self.is_required:
                type_info.append("optional")
            if self.is_argument_optional:
                type_info.append("optional for registration")
        else:  # readonly is only useful for self's, not args
            if self.is_readonly:
                type_info.append("readonly")

        if self.is_never_none:
            type_info.append("never None")

        if self.is_path_supports_blend_relative:
            type_info.append("blend relative ``//`` prefix supported")

        if self.is_path_supports_templates:
            type_info.append(
                "Supports `template expressions "
                "<https://docs.blender.org/manual/en/{:d}.{:d}/files/file_paths.html#path-templates>`_".format(
                    *bpy.app.version[:2],
                ),
            )

        if type_info:
            type_str += ", ({:s})".format(", ".join(type_info))

        return type_str

    def __str__(self):
        txt = ""
        txt += " * " + self.identifier + ": " + self.description

        return txt


class InfoFunctionRNA:
    __slots__ = (
        "bl_func",
        "identifier",
        "description",
        "args",
        "return_values",
        "is_classmethod",
    )
    global_lookup = {}

    def __init__(self, rna_func):
        self.bl_func = rna_func
        self.identifier = rna_func.identifier
        # self.name = rna_func.name # functions have no name!
        self.description = rna_func.description.strip()
        self.is_classmethod = not rna_func.use_self

        self.args = []
        self.return_values = ()

    def build(self):
        rna_func = self.bl_func
        parent_id = rna_func
        self.return_values = []

        for rna_prop in rna_func.parameters.values():
            prop = GetInfoPropertyRNA(rna_prop, parent_id)
            if rna_prop.is_output:
                self.return_values.append(prop)
            else:
                self.args.append(prop)

        self.return_values = tuple(self.return_values)

    def __str__(self):
        txt = ''
        txt += ' * ' + self.identifier + '('

        for arg in self.args:
            txt += arg.identifier + ', '
        txt += '): ' + self.description
        return txt


class InfoOperatorRNA:
    __slots__ = (
        "bl_op",
        "identifier",
        "name",
        "module_name",
        "func_name",
        "description",
        "args",
    )
    global_lookup = {}

    def __init__(self, rna_op):
        self.bl_op = rna_op
        self.identifier = rna_op.identifier

        mod, name = self.identifier.split("_OT_", 1)
        self.module_name = mod.lower()
        self.func_name = name

        # self.name = rna_func.name # functions have no name!
        self.description = rna_op.description.strip()

        self.args = []

    def build(self):
        rna_op = self.bl_op
        parent_id = self.identifier
        for rna_id, rna_prop in rna_op.properties.items():
            if rna_id == "rna_type":
                continue

            prop = GetInfoPropertyRNA(rna_prop, parent_id)
            self.args.append(prop)

    def get_location(self):
        try:
            op_class = getattr(bpy.types, self.identifier)
        except AttributeError:
            # defined in C.
            return None, None
        op_func = getattr(op_class, "execute", None)
        if op_func is None:
            op_func = getattr(op_class, "invoke", None)
        if op_func is None:
            op_func = getattr(op_class, "poll", None)

        if op_func:
            op_code = op_func.__code__
            source_path = op_code.co_filename

            # clear the prefix
            for p in script_paths:
                source_path = source_path.split(p)[-1]

            if source_path[0] in "/\\":
                source_path = source_path[1:]

            return source_path, op_code.co_firstlineno
        else:
            return None, None


def _GetInfoRNA(bl_rna, cls, parent_id=""):

    if bl_rna is None:
        return None

    key = parent_id, bl_rna.identifier
    try:
        return cls.global_lookup[key]
    except KeyError:
        instance = cls.global_lookup[key] = cls(bl_rna)
        return instance


def GetInfoStructRNA(bl_rna):
    return _GetInfoRNA(bl_rna, InfoStructRNA)


def GetInfoPropertyRNA(bl_rna, parent_id):
    return _GetInfoRNA(bl_rna, InfoPropertyRNA, parent_id)


def GetInfoFunctionRNA(bl_rna, parent_id):
    return _GetInfoRNA(bl_rna, InfoFunctionRNA, parent_id)


def GetInfoOperatorRNA(bl_rna):
    return _GetInfoRNA(bl_rna, InfoOperatorRNA)


def BuildRNAInfo():

    # needed on successive calls to prevent stale data access
    for cls in (InfoStructRNA, InfoFunctionRNA, InfoOperatorRNA, InfoPropertyRNA):
        cls.global_lookup.clear()
    del cls

    # Use for faster lookups
    # use rna_struct.identifier as the key for each dict
    rna_struct_dict = {}  # store identifier:rna lookups
    rna_full_path_dict = {}  # store the result of full_rna_struct_path(rna_struct)
    rna_children_dict = {}  # store all rna_structs nested from here
    rna_references_dict = {}  # store a list of rna path strings that reference this type
    # rna_functions_dict = {}  # store all functions directly in this type (not inherited)

    def full_rna_struct_path(rna_struct):
        """
        Needed when referencing one struct from another
        """
        nested = rna_struct.nested
        if nested:
            return "{:s}.{:s}".format(full_rna_struct_path(nested), rna_struct.identifier)
        else:
            return rna_struct.identifier

    # def write_func(rna_func, ident):
    def base_id(rna_struct):
        try:
            return rna_struct.base.identifier
        except AttributeError:
            return ""  # invalid id

    # structs = [(base_id(rna_struct), rna_struct.identifier, rna_struct) for rna_struct in bpy.doc.structs.values()]
    '''
    structs = []
    for rna_struct in bpy.doc.structs.values():
        structs.append( (base_id(rna_struct), rna_struct.identifier, rna_struct) )
    '''
    structs = []

    def _bpy_types_iterator():
        # Don't report when these types are ignored.
        suppress_warning = {
            "GeometrySet",
            "InlineShaderNodes",
            "bpy_func",
            "bpy_prop",
            "bpy_prop_array",
            "bpy_prop_collection",
            "bpy_prop_collection_idprop",
            "bpy_struct",
            "bpy_struct_meta_idprop",
        }

        names_unique = set()
        rna_type_list = []
        for rna_type_name in dir(bpy.types):
            names_unique.add(rna_type_name)
            rna_type = getattr(bpy.types, rna_type_name)
            rna_struct = getattr(rna_type, "bl_rna", None)
            if rna_struct is not None:
                rna_type_list.append(rna_type)
                yield (rna_type_name, rna_struct)
            elif rna_type_name.startswith("_"):
                # Ignore "__dir__", "__getattr__" .. etc.
                pass
            elif rna_type_name in suppress_warning:
                pass
            else:
                print("rna_info.BuildRNAInfo(..): ignoring type", repr(rna_type_name))

        # Now, there are some sub-classes in add-ons we also want to include.
        # Cycles for example. These are referenced from the Scene, but not part of
        # bpy.types module.
        # Include all sub-classes we didn't already get from 'bpy.types'.
        i = 0
        while i < len(rna_type_list):
            rna_type = rna_type_list[i]
            for rna_sub_type in rna_type.__subclasses__():
                rna_sub_struct = getattr(rna_sub_type, "bl_rna", None)
                if rna_sub_struct is not None:
                    rna_sub_type_name = rna_sub_struct.identifier
                    if rna_sub_type_name not in names_unique:
                        names_unique.add(rna_sub_type_name)
                        rna_type_list.append(rna_sub_type)
                        # The bl_idname may not match the class name in the file.
                        # Always use the 'bl_idname' because using the Python
                        # class name causes confusion - having two names for the same thing.
                        # Since having two names for the same thing is trickier to support
                        # without a significant benefit.
                        yield (rna_sub_type_name, rna_sub_struct)
            i += 1

    for (_rna_type_name, rna_struct) in _bpy_types_iterator():
        # if not _rna_type_name.startswith('__'):

        identifier = rna_struct.identifier

        if not rna_id_ignore(identifier):
            structs.append((base_id(rna_struct), identifier, rna_struct))

            # Simple lookup
            rna_struct_dict[identifier] = rna_struct

            # Store full rna path 'GameObjectSettings' -> 'Object.GameObjectSettings'
            rna_full_path_dict[identifier] = full_rna_struct_path(rna_struct)

            # Store a list of functions, remove inherited later
            # NOT USED YET
            # rna_functions_dict[identifier] = get_direct_functions(rna_struct)

            # fill in these later
            rna_children_dict[identifier] = []
            rna_references_dict[identifier] = []

    del _bpy_types_iterator

    structs.sort()  # not needed but speeds up sort below, setting items without an inheritance first

    # Arrange so classes are always defined in the correct order
    deps_ok = False
    while deps_ok is False:
        deps_ok = True
        rna_done = set()

        for i, (rna_base, identifier, rna_struct) in enumerate(structs):

            rna_done.add(identifier)

            if rna_base and rna_base not in rna_done:
                deps_ok = False
                data = structs.pop(i)
                ok = False
                while i < len(structs):
                    if structs[i][1] == rna_base:
                        structs.insert(i + 1, data)  # insert after the item we depend on.
                        ok = True
                        break
                    i += 1

                if not ok:
                    print("Dependency \"{:s}\" could not be found for \"{:s}\"".format(identifier, rna_base))

                break

    # Done ordering structs

    # precalculate vars to avoid a lot of looping
    for (rna_base, identifier, rna_struct) in structs:

        # rna_struct_path = full_rna_struct_path(rna_struct)
        rna_struct_path = rna_full_path_dict[identifier]

        for rna_prop in get_direct_properties(rna_struct):
            rna_prop_identifier = rna_prop.identifier

            if rna_prop_identifier == 'RNA' or rna_id_ignore(rna_prop_identifier):
                continue

            for rna_prop_ptr in (getattr(rna_prop, "fixed_type", None), getattr(rna_prop, "srna", None)):
                # Does this property point to me?
                if rna_prop_ptr and rna_prop_ptr.identifier in rna_references_dict:
                    rna_references_dict[rna_prop_ptr.identifier].append(
                        "{:s}.{:s}".format(rna_struct_path, rna_prop_identifier))

        for rna_func in get_direct_functions(rna_struct):
            for rna_prop_identifier, rna_prop in rna_func.parameters.items():

                if rna_prop_identifier == 'RNA' or rna_id_ignore(rna_prop_identifier):
                    continue

                rna_prop_ptr = getattr(rna_prop, "fixed_type", None)

                # Does this property point to me?
                if rna_prop_ptr and rna_prop_ptr.identifier in rna_references_dict:
                    rna_references_dict[rna_prop_ptr.identifier].append(
                        "{:s}.{:s}".format(rna_struct_path, rna_func.identifier))

        # Store nested children
        nested = rna_struct.nested
        if nested:
            rna_children_dict[nested.identifier].append(rna_struct)

    # Sort the refs, just reads nicer
    for rna_refs in rna_references_dict.values():
        rna_refs.sort()

    info_structs = []
    for (rna_base, identifier, rna_struct) in structs:
        # if rna_struct.nested:
        #     continue

        # write_struct(rna_struct, '')
        info_struct = GetInfoStructRNA(rna_struct)
        if rna_base:
            info_struct.base = GetInfoStructRNA(rna_struct_dict[rna_base])
        info_struct.nested = GetInfoStructRNA(rna_struct.nested)
        info_struct.children[:] = rna_children_dict[identifier]
        info_struct.references[:] = rna_references_dict[identifier]
        info_struct.full_path = rna_full_path_dict[identifier]

        info_structs.append(info_struct)

    for rna_info_prop in InfoPropertyRNA.global_lookup.values():
        rna_info_prop.build()

    for rna_info_prop in InfoFunctionRNA.global_lookup.values():
        rna_info_prop.build()

    done_keys = set()
    new_keys = set(InfoStructRNA.global_lookup.keys())
    while new_keys:
        for rna_key in new_keys:
            rna_info = InfoStructRNA.global_lookup[rna_key]
            rna_info.build()
            for prop in rna_info.properties:
                prop.build()
            for func in rna_info.functions:
                func.build()
                for prop in func.args:
                    prop.build()
                for prop in func.return_values:
                    prop.build()
        done_keys |= new_keys
        new_keys = set(InfoStructRNA.global_lookup.keys()) - done_keys

    # there are too many invalid defaults, unless we intend to fix, leave this off
    if 0:
        for rna_info in InfoStructRNA.global_lookup.values():
            for prop in rna_info.properties:
                # ERROR CHECK
                default = prop.default
                if type(default) in {float, int}:
                    if default < prop.min or default > prop.max:
                        print("\t {:s}.{:s}, {:s} not in [{:s} - {:s}]".format(
                            rna_info.identifier, prop.identifier, default, prop.min, prop.max,
                        ))

    # now for operators
    op_mods = dir(bpy.ops)

    for op_mod_name in sorted(op_mods):
        if op_mod_name.startswith('__'):
            continue

        op_mod = getattr(bpy.ops, op_mod_name)
        operators = dir(op_mod)
        for op in sorted(operators):
            try:
                rna_prop = getattr(op_mod, op).get_rna_type()
            except AttributeError:
                rna_prop = None
            except TypeError:
                rna_prop = None

            if rna_prop:
                GetInfoOperatorRNA(rna_prop)

    for rna_info in InfoOperatorRNA.global_lookup.values():
        rna_info.build()
        for rna_prop in rna_info.args:
            rna_prop.build()

    # for rna_info in InfoStructRNA.global_lookup.values():
    #     print(rna_info)
    return (
        InfoStructRNA.global_lookup,
        InfoFunctionRNA.global_lookup,
        InfoOperatorRNA.global_lookup,
        InfoPropertyRNA.global_lookup,
    )


def main():
    struct = BuildRNAInfo()[0]
    data = []
    for _struct_id, v in sorted(struct.items()):
        struct_id_str = v.identifier  # "".join(sid for sid in struct_id if struct_id)

        for base in v.get_bases():
            struct_id_str = base.identifier + "|" + struct_id_str

        props = [(prop.identifier, prop) for prop in v.properties]
        for _prop_id, prop in sorted(props):
            # if prop.type == "boolean":
            #     continue
            prop_type = prop.type
            if prop.array_length > 0:
                prop_type += "[{:d}]".format(prop.array_length)

            data.append(
                "{:s}.{:s} -> {:s}:    {:s}{:s}    {:s}".format(
                    struct_id_str,
                    prop.identifier,
                    prop.identifier,
                    prop_type,
                    ", (read-only)" if prop.is_readonly else "", prop.description,
                ))
        data.sort()

    if bpy.app.background:
        import sys
        sys.stderr.write("\n".join(data))
        sys.stderr.write("\n\nEOF\n")
    else:
        text = bpy.data.texts.new(name="api.py")
        text.from_string("\n".join(data))


if __name__ == "__main__":
    main()
