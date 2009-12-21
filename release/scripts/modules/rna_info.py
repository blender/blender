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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# classes for extracting info from blenders internal classes

import bpy

def range_str(val):
    if val < -10000000:	return '-inf'
    if val >  10000000:	return 'inf'
    if type(val)==float:
        return '%g'  % val
    else:
        return str(val)

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

    def getNestedProperties(self, ls = None):
        if not ls:
            ls = self.properties[:]

        if self.nested:
            self.nested.getNestedProperties(ls)

        return ls

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

    def build(self):
        rna_prop = self.bl_prop

        self.enum_items = []
        self.min = getattr(rna_prop, "hard_min", -1)
        self.max = getattr(rna_prop, "hard_max", -1)
        self.array_length = getattr(rna_prop, "array_length", 0)

        self.type = rna_prop.type.lower()
        fixed_type = getattr(rna_prop, "fixed_type", "")
        if fixed_type:
            self.fixed_type = GetInfoStructRNA(fixed_type) # valid for pointer/collections
        else:
            self.fixed_type = None
            
        if self.type == "enum":
            self.enum_items[:] = rna_prop.items.keys()

        self.srna = GetInfoStructRNA(rna_prop.srna) # valid for pointer/collections

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

        self.args = [] # todo
        self.return_value = None # todo

    def build(self):
        rna_prop = self.bl_prop
        pass

    def __repr__(self):
        txt = ''
        txt += ' * ' + self.identifier + '('

        for arg in self.args:
            txt += arg.identifier + ', '
        txt += '): ' + self.description
        return txt


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


def BuildRNAInfo():
    # Use for faster lookups
    # use rna_struct.identifier as the key for each dict
    rna_struct_dict =		{}  # store identifier:rna lookups
    rna_full_path_dict =	{}	# store the result of full_rna_struct_path(rna_struct)
    rna_children_dict =		{}	# store all rna_structs nested from here
    rna_references_dict =	{}	# store a list of rna path strings that reference this type
    rna_functions_dict =	{}	# store all functions directly in this type (not inherited)
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
        try:		return rna_struct.base.identifier
        except:	return '' # invalid id

    #structs = [(base_id(rna_struct), rna_struct.identifier, rna_struct) for rna_struct in bpy.doc.structs.values()]
    '''
    structs = []
    for rna_struct in bpy.doc.structs.values():
        structs.append( (base_id(rna_struct), rna_struct.identifier, rna_struct) )
    '''
    structs = []
    for rna_type_name in dir(bpy.types):
        rna_type = getattr(bpy.types, rna_type_name)

        try:		rna_struct = rna_type.bl_rna
        except:	rna_struct = None

        if rna_struct:
            #if not rna_type_name.startswith('__'):

            identifier = rna_struct.identifier

            if not rna_id_ignore(identifier):
                structs.append( (base_id(rna_struct), identifier, rna_struct) )

                # Simple lookup
                rna_struct_dict[identifier] = rna_struct

                # Store full rna path 'GameObjectSettings' -> 'Object.GameObjectSettings'
                rna_full_path_dict[identifier] = full_rna_struct_path(rna_struct)

                # Store a list of functions, remove inherited later
                rna_functions_dict[identifier]= list(rna_struct.functions)


                # fill in these later
                rna_children_dict[identifier]= []
                rna_references_dict[identifier]= []


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
                    if structs[i][1]==rna_base:
                        structs.insert(i+1, data) # insert after the item we depend on.
                        ok = True
                        break
                    i+=1

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
            rna_base_func_keys= []

        # rna_struct_path = full_rna_struct_path(rna_struct)
        rna_struct_path = rna_full_path_dict[identifier]

        for rna_prop_identifier, rna_prop in rna_struct.properties.items():

            if rna_prop_identifier=='RNA':					continue
            if rna_id_ignore(rna_prop_identifier):			continue
            if rna_prop_identifier in rna_base_prop_keys:	continue


            for rna_prop_ptr in (getattr(rna_prop, "fixed_type", None), getattr(rna_prop, "srna", None)):
                # Does this property point to me?
                if rna_prop_ptr:
                    rna_references_dict[rna_prop_ptr.identifier].append( "%s.%s" % (rna_struct_path, rna_prop_identifier) )

        for rna_func in rna_struct.functions:
            for rna_prop_identifier, rna_prop in rna_func.parameters.items():

                if rna_prop_identifier=='RNA':					continue
                if rna_id_ignore(rna_prop_identifier):			continue
                if rna_prop_identifier in rna_base_func_keys:	continue


                try:		rna_prop_ptr = rna_prop.fixed_type
                except:	rna_prop_ptr = None

                # Does this property point to me?
                if rna_prop_ptr:
                    rna_references_dict[rna_prop_ptr.identifier].append( "%s.%s" % (rna_struct_path, rna_func.identifier) )


        # Store nested children
        nested = rna_struct.nested
        if nested:
            rna_children_dict[nested.identifier].append(rna_struct)


        if rna_base:
            rna_funcs =			rna_functions_dict[identifier]
            if rna_funcs:
                # Remove inherited functions if we have any
                rna_base_funcs =	rna_functions_dict__copy[rna_base]
                rna_funcs[:] =		[f for f in rna_funcs if f not in rna_base_funcs]

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
        info_struct= GetInfoStructRNA(rna_struct)
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
        
    #for rna_info in InfoStructRNA.global_lookup.values():
    #    print(rna_info)

    return InfoStructRNA.global_lookup, InfoFunctionRNA.global_lookup, InfoPropertyRNA.global_lookup

