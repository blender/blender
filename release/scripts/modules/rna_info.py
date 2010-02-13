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

# <pep8 compliant>

# classes for extracting info from blenders internal classes

import bpy

# use to strip python paths
script_paths = bpy.utils.script_paths()


def range_str(val):
    if val < -10000000:
        return '-inf'
    elif val > 10000000:
        return 'inf'
    elif type(val) == float:
        return '%g' % val
    else:
        return str(val)


def float_as_string(f):
    val_str = "%g" % f
    if '.' not in val_str and '-' not in val_str: # value could be 1e-05
        val_str += '.0'
    return val_str


class InfoStructRNA:
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

    def build(self):
        rna_type = self.bl_rna
        parent_id = self.identifier
        self.properties[:] = [GetInfoPropertyRNA(rna_prop, parent_id) for rna_id, rna_prop in rna_type.properties.items() if rna_id != "rna_type"]
        self.functions[:] = [GetInfoFunctionRNA(rna_prop, parent_id) for rna_prop in rna_type.functions.values()]

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
        py_class = getattr(bpy.types, self.identifier)
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
            if type(attr) in (types.FunctionType, types.MethodType):
                functions.append((identifier, attr))
        return functions

    def __repr__(self):

        txt = ''
        txt += self.identifier
        if self.base:
            txt += '(%s)' % self.base.identifier
        txt += ': ' + self.description + '\n'

        for prop in self.properties:
            txt += prop.__repr__() + '\n'

        for func in self.functions:
            txt += func.__repr__() + '\n'

        return txt


class InfoPropertyRNA:
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
        self.collection_type = GetInfoStructRNA(rna_prop.srna)
        self.is_required = rna_prop.is_required
        self.is_readonly = rna_prop.is_readonly
        self.is_never_none = rna_prop.is_never_none

        self.type = rna_prop.type.lower()
        fixed_type = getattr(rna_prop, "fixed_type", "")
        if fixed_type:
            self.fixed_type = GetInfoStructRNA(fixed_type) # valid for pointer/collections
        else:
            self.fixed_type = None

        if self.type == "enum":
            self.enum_items[:] = rna_prop.items.keys()

        if self.array_length:
            self.default = tuple(getattr(rna_prop, "default_array", ()))
            self.default_str = ''
            # special case for floats
            if len(self.default) > 0:
                if type(self.default[0]) is float:
                    self.default_str = "(%s)" % ", ".join([float_as_string(f) for f in self.default])
            if not self.default_str:
                self.default_str = str(self.default)
        else:
            self.default = getattr(rna_prop, "default", "")
            if type(self.default) is float:
                self.default_str = float_as_string(self.default)
            else:
                self.default_str = str(self.default)

        self.srna = GetInfoStructRNA(rna_prop.srna) # valid for pointer/collections

    def get_default_string(self):
        # pointer has no default, just set as None
        if self.type == "pointer":
            return "None"
        elif self.type == "string":
            return '"' + self.default_str + '"'
        elif self.type == "enum":
            if self.default_str:
                return "'" + self.default_str + "'"
            else:
                return ""

        return self.default_str

    def get_arg_default(self, force=True):
        default = self.get_default_string()
        if default and (force or self.is_required == False):
            return "%s=%s" % (self.identifier, default)
        return self.identifier

    def get_type_description(self, as_ret=False, as_arg=False, class_fmt="%s"):
        type_str = ""
        if self.fixed_type is None:
            type_str += self.type
            if self.array_length:
                type_str += " array of %d items" % (self.array_length)

            if self.type in ("float", "int"):
                type_str += " in [%s, %s]" % (range_str(self.min), range_str(self.max))
            elif self.type == "enum":
                type_str += " in [%s]" % ', '.join([("'%s'" % s) for s in self.enum_items])
        else:
            if self.type == "collection":
                if self.collection_type:
                    collection_str = (class_fmt % self.collection_type.identifier) + " collection of "
                else:
                    collection_str = "Collection of "
            else:
                collection_str = ""

            type_str += collection_str + (class_fmt % self.fixed_type.identifier)

        if as_ret:
            pass
        elif as_arg:
            if not self.is_required:
                type_str += ", (optional)"
        else: # readonly is only useful for selfs, not args
            if self.is_readonly:
                type_str += ", (readonly)"

        if self.is_never_none:
            type_str += ", (never None)"

        return type_str

    def __repr__(self):
        txt = ''
        txt += ' * ' + self.identifier + ': ' + self.description

        return txt


class InfoFunctionRNA:
    global_lookup = {}

    def __init__(self, rna_func):
        self.bl_func = rna_func
        self.identifier = rna_func.identifier
        # self.name = rna_func.name # functions have no name!
        self.description = rna_func.description.strip()

        self.args = []
        self.return_values = ()

    def build(self):
        rna_func = self.bl_func
        parent_id = rna_func
        self.return_values = []

        for rna_prop in rna_func.parameters.values():
            prop = GetInfoPropertyRNA(rna_prop, parent_id)
            if rna_prop.use_output:
                self.return_values.append(prop)
            else:
                self.args.append(prop)

        self.return_values = tuple(self.return_values)

    def __repr__(self):
        txt = ''
        txt += ' * ' + self.identifier + '('

        for arg in self.args:
            txt += arg.identifier + ', '
        txt += '): ' + self.description
        return txt


class InfoOperatorRNA:
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
        op_class = getattr(bpy.types, self.identifier)
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


def _GetInfoRNA(bl_rna, cls, parent_id=''):

    if bl_rna == None:
        return None

    key = parent_id, bl_rna.identifier
    try:
        return cls.global_lookup[key]
    except:
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
    # Use for faster lookups
    # use rna_struct.identifier as the key for each dict
    rna_struct_dict = {}  # store identifier:rna lookups
    rna_full_path_dict = {}	# store the result of full_rna_struct_path(rna_struct)
    rna_children_dict = {}	# store all rna_structs nested from here
    rna_references_dict = {}	# store a list of rna path strings that reference this type
    rna_functions_dict = {}	# store all functions directly in this type (not inherited)
    rna_words = set()

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
        return False

    def full_rna_struct_path(rna_struct):
        '''
        Needed when referencing one struct from another
        '''
        nested = rna_struct.nested
        if nested:
            return "%s.%s" % (full_rna_struct_path(nested), rna_struct.identifier)
        else:
            return rna_struct.identifier

    # def write_func(rna_func, ident):
    def base_id(rna_struct):
        try:
            return rna_struct.base.identifier
        except:
            return "" # invalid id

    #structs = [(base_id(rna_struct), rna_struct.identifier, rna_struct) for rna_struct in bpy.doc.structs.values()]
    '''
    structs = []
    for rna_struct in bpy.doc.structs.values():
        structs.append( (base_id(rna_struct), rna_struct.identifier, rna_struct) )
    '''
    structs = []
    for rna_type_name in dir(bpy.types):
        rna_type = getattr(bpy.types, rna_type_name)

        rna_struct = getattr(rna_type, "bl_rna", None)

        if rna_struct:
            #if not rna_type_name.startswith('__'):

            identifier = rna_struct.identifier

            if not rna_id_ignore(identifier):
                structs.append((base_id(rna_struct), identifier, rna_struct))

                # Simple lookup
                rna_struct_dict[identifier] = rna_struct

                # Store full rna path 'GameObjectSettings' -> 'Object.GameObjectSettings'
                rna_full_path_dict[identifier] = full_rna_struct_path(rna_struct)

                # Store a list of functions, remove inherited later
                rna_functions_dict[identifier] = list(rna_struct.functions)


                # fill in these later
                rna_children_dict[identifier] = []
                rna_references_dict[identifier] = []


        else:
            print("Ignoring", rna_type_name)


    # Sucks but we need to copy this so we can check original parent functions
    rna_functions_dict__copy = {}
    for key, val in rna_functions_dict.items():
        rna_functions_dict__copy[key] = val[:]


    structs.sort() # not needed but speeds up sort below, setting items without an inheritance first

    # Arrange so classes are always defined in the correct order
    deps_ok = False
    while deps_ok == False:
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
                        structs.insert(i + 1, data) # insert after the item we depend on.
                        ok = True
                        break
                    i += 1

                if not ok:
                    print('Dependancy "%s" could not be found for "%s"' % (identifier, rna_base))

                break

    # Done ordering structs


    # precalc vars to avoid a lot of looping
    for (rna_base, identifier, rna_struct) in structs:

        if rna_base:
            rna_base_prop_keys = rna_struct_dict[rna_base].properties.keys() # could cache
            rna_base_func_keys = [f.identifier for f in rna_struct_dict[rna_base].functions]
        else:
            rna_base_prop_keys = []
            rna_base_func_keys = []

        # rna_struct_path = full_rna_struct_path(rna_struct)
        rna_struct_path = rna_full_path_dict[identifier]

        for rna_prop_identifier, rna_prop in rna_struct.properties.items():

            if rna_prop_identifier == 'RNA' or \
                    rna_id_ignore(rna_prop_identifier) or \
                    rna_prop_identifier in rna_base_prop_keys:
                continue


            for rna_prop_ptr in (getattr(rna_prop, "fixed_type", None), getattr(rna_prop, "srna", None)):
                # Does this property point to me?
                if rna_prop_ptr:
                    rna_references_dict[rna_prop_ptr.identifier].append("%s.%s" % (rna_struct_path, rna_prop_identifier))

        for rna_func in rna_struct.functions:
            for rna_prop_identifier, rna_prop in rna_func.parameters.items():

                if rna_prop_identifier == 'RNA' or \
                        rna_id_ignore(rna_prop_identifier) or \
                        rna_prop_identifier in rna_base_func_keys:
                    continue

                try:
                    rna_prop_ptr = rna_prop.fixed_type
                except:
                    rna_prop_ptr = None

                # Does this property point to me?
                if rna_prop_ptr:
                    rna_references_dict[rna_prop_ptr.identifier].append("%s.%s" % (rna_struct_path, rna_func.identifier))


        # Store nested children
        nested = rna_struct.nested
        if nested:
            rna_children_dict[nested.identifier].append(rna_struct)


        if rna_base:
            rna_funcs = rna_functions_dict[identifier]
            if rna_funcs:
                # Remove inherited functions if we have any
                rna_base_funcs = rna_functions_dict__copy[rna_base]
                rna_funcs[:] = [f for f in rna_funcs if f not in rna_base_funcs]

    rna_functions_dict__copy.clear()
    del rna_functions_dict__copy

    # Sort the refs, just reads nicer
    for rna_refs in rna_references_dict.values():
        rna_refs.sort()


    info_structs = []
    for (rna_base, identifier, rna_struct) in structs:
        #if rna_struct.nested:
        #    continue

        #write_struct(rna_struct, '')
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

    for rna_info in InfoStructRNA.global_lookup.values():
        rna_info.build()
        for prop in rna_info.properties:
            prop.build()
        for func in rna_info.functions:
            func.build()
            for prop in func.args:
                prop.build()
            for prop in func.return_values:
                prop.build()

    # now for operators
    op_mods = dir(bpy.ops)

    for op_mod_name in sorted(op_mods):
        if op_mod_name.startswith('__') or op_mod_name in ("add", "remove"):
            continue

        op_mod = getattr(bpy.ops, op_mod_name)
        operators = dir(op_mod)
        for op in sorted(operators):
            try:
                rna_prop = getattr(op_mod, op).get_rna()
            except AttributeError:
                rna_prop = None
            except TypeError:
                rna_prop = None

            if rna_prop:
                GetInfoOperatorRNA(rna_prop.bl_rna)

    for rna_info in InfoOperatorRNA.global_lookup.values():
        rna_info.build()
        for rna_prop in rna_info.args:
            rna_prop.build()


    #for rna_info in InfoStructRNA.global_lookup.values():
    #    print(rna_info)

    return InfoStructRNA.global_lookup, InfoFunctionRNA.global_lookup, InfoOperatorRNA.global_lookup, InfoPropertyRNA.global_lookup
