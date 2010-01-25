 # ***** BEGIN GPL LICENSE BLOCK *****
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
 # along with this program; if not, write to the Free Software Foundation,
 # Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 #
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****

script_help_msg = '''
Usage,
run this script from blenders root path once you have compiled blender
    ./blender.bin -b -P /b/source/blender/python/sphinx_doc_gen.py

This will generate python files in "./source/blender/python/doc/sphinx-in"
Generate html docs  by running...
    
    sphinx-build source/blender/python/doc/sphinx-in source/blender/python/doc/sphinx-out
'''


import os
import inspect
import bpy
import rna_info
reload(rna_info)

def range_str(val):
    if val < -10000000:	return '-inf'
    if val >  10000000:	return 'inf'
    if type(val)==float:
        return '%g'  % val
    else:
        return str(val)

def write_indented_lines(ident, fn, text):
    if text is None:
        return
    for l in text.split("\n"):
        fn(ident + l.strip() + "\n")


def pymethod2sphinx(ident, fw, identifier, py_func):
    '''
    class method to sphinx
    '''
    arg_str = inspect.formatargspec(*inspect.getargspec(py_func))
    if arg_str.startswith("(self, "):
        arg_str = "(" + arg_str[7:]
        func_type = "method"
    elif arg_str.startswith("(cls, "):
        arg_str = "(" + arg_str[6:]
        func_type = "classmethod"
    else:
        func_type = "staticmethod"

    fw(ident + ".. %s:: %s%s\n\n" % (func_type, identifier, arg_str))
    if py_func.__doc__:
        write_indented_lines(ident + "   ", fw, py_func.__doc__)
        fw("\n")


def pyfunc2sphinx(ident, fw, identifier, py_func, is_class=True):
    '''
    function or class method to sphinx
    '''
    arg_str = inspect.formatargspec(*inspect.getargspec(py_func))

    if not is_class:
        func_type = "function"
        
        # ther rest are class methods
    elif arg_str.startswith("(self, "):
        arg_str = "(" + arg_str[7:]
        func_type = "method"
    elif arg_str.startswith("(cls, "):
        arg_str = "(" + arg_str[6:]
        func_type = "classmethod"
    else:
        func_type = "staticmethod"

    fw(ident + ".. %s:: %s%s\n\n" % (func_type, identifier, arg_str))
    if py_func.__doc__:
        write_indented_lines(ident + "   ", fw, py_func.__doc__.strip())
        fw("\n")

def py_c_func2sphinx(ident, fw, identifier, py_func, is_class=True):
    '''
    c defined function to sphinx.
    '''
    
    # dump the docstring, assume its formatted correctly
    if py_func.__doc__:
        for l in py_func.__doc__.split("\n"):
            fw(ident + l + "\n")
        fw("\n")
    else:
        fw(ident + ".. function:: %s()\n\n" % identifier)
        fw(ident + "   Undocumented function.\n\n" % identifier)


def pyprop2sphinx(ident, fw, identifier, py_prop):
    '''
    python property to sphinx
    '''
    fw(ident + ".. attribute:: %s\n\n" % identifier)
    write_indented_lines(ident + "   ", fw, py_prop.__doc__)
    if py_prop.fset is None:
        fw(ident + "   (readonly)\n\n")


def pymodule2sphinx(BASEPATH, module_name, module, title):
    import types
    # lame, python wont give some access
    MethodDescriptorType = type(dict.get)
    GetSetDescriptorType = type(int.real)
    

    filepath = os.path.join(BASEPATH, module_name + ".rst")
    
    file = open(filepath, "w")
    print(filepath)
    print(filepath)
    fw = file.write
    
    fw(title + "\n")
    fw(("=" * len(title)) + "\n\n")
    
    fw(".. module:: %s\n\n" % module_name)
    
    if module.__doc__:
        # Note, may contain sphinx syntax, dont mangle!
        fw(module.__doc__.strip())
        fw("\n\n")
    
    classes = []

    for attribute in dir(module):
        if not attribute.startswith("_"):
            value = getattr(module, attribute)

            value_type = type(value)

            if value_type == types.FunctionType:
                pyfunc2sphinx("", fw, attribute, value, is_class=False)
            elif value_type in (types.BuiltinMethodType, types.BuiltinFunctionType): # both the same at the moment but to be future proof
                # note: can't get args from these, so dump the string as is
                # this means any module used like this must have fully formatted docstrings.
                py_c_func2sphinx("", fw, attribute, value, is_class=False)
            elif value_type == type:
                classes.append((attribute, value))
            # TODO, more types...
    
    # write collected classes now
    for (attribute, value) in classes:
        # May need to be its own function
        fw(".. class:: %s\n\n" % attribute)
        if value.__doc__:
            for l in value.__doc__.split("\n"):
                fw("   %s\n" % l)
        fw("\n")

        for key, descr in value.__dict__.items():
            if key.startswith("__"):
                continue

            descr_type = type(descr)
            if descr_type in (MethodDescriptorType, ): # GetSetDescriptorType, GetSetDescriptorType's are not documented yet
                if descr.__doc__:
                    for l in descr.__doc__.split("\n"):
                        fw("   %s\n" % l)
                    fw("\n")
        fw("\n\n")

    file.close()


def rna2sphinx(BASEPATH):

    structs, funcs, ops, props = rna_info.BuildRNAInfo()

    try:
        os.mkdir(BASEPATH)
    except:
        pass

    # conf.py - empty for now
    filepath = os.path.join(BASEPATH, "conf.py")
    file = open(filepath, "w")
    fw = file.write
    
    fw("project = 'Blender 3D'\n")
    # fw("master_doc = 'index'\n")
    fw("copyright = u'Blender Foundation'\n")
    fw("version = '2.5'\n")
    fw("release = '2.5'\n")
    file.close()


    filepath = os.path.join(BASEPATH, "contents.rst")
    file = open(filepath, "w")
    fw = file.write
    
    fw("\n")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    #fw("   bpy.ops.*\n\n")
    #fw("   bpy.types.*\n\n")
    
    # py modules
    fw("   bpy.utils\n\n")
    fw("   bpy.app\n\n")
    
    # C modules
    fw("   bpy.props\n\n")
    
    fw("   Mathutils\n\n")

    file.close()

    # python modules
    from bpy import utils as module
    pymodule2sphinx(BASEPATH, "bpy.utils", module, "Blender Python Utilities")
    from bpy import app as module
    pymodule2sphinx(BASEPATH, "bpy.app", module, "Blender Python Application Constants")

    from bpy import props as module
    pymodule2sphinx(BASEPATH, "bpy.props", module, "Blender Python Property Definitions")
    
    import Mathutils as module
    pymodule2sphinx(BASEPATH, "Mathutils", module, "Module Mathutils")
    del module


    if 0:
        filepath = os.path.join(BASEPATH, "bpy.rst")
        file = open(filepath, "w")
        fw = file.write
        
        fw("\n")

        title = ":mod:`bpy` --- Blender Python Module"
        fw("%s\n%s\n\n" % (title, "=" * len(title)))
        fw(".. module:: bpy.types\n\n")
        file.close()

    def write_param(ident, fw, prop, is_return=False):
        if is_return:
            id_name = "return"
            id_type = "rtype"
            kwargs = {"as_ret": True, "class_fmt": ":class:`%s`"}
            identifier = ""
        else:
            id_name = "arg"
            id_type = "type"
            kwargs = {"as_arg": True, "class_fmt": ":class:`%s`"}
            identifier = " %s" % prop.identifier

        type_descr = prop.get_type_description(**kwargs)
        if prop.name or prop.description:
            fw(ident + ":%s%s: %s\n" % (id_name, identifier, ", ".join([val for val in (prop.name, prop.description) if val])))
        fw(ident + ":%s%s: %s\n" % (id_type, identifier, type_descr))

    def write_struct(struct):
        #if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #    return

        #if not struct.identifier == "Object":
        #    return

        filepath = os.path.join(BASEPATH, "bpy.types.%s.rst" % struct.identifier)
        file = open(filepath, "w")
        fw = file.write
        
        if struct.base: 
            title = "%s(%s)" % (struct.identifier, struct.base.identifier)
        else:
            title = struct.identifier

        fw("%s\n%s\n\n" % (title, "=" * len(title)))
        
        fw(".. module:: bpy.types\n\n")
        
        bases = struct.get_bases()
        if bases:
            if len(bases) > 1:
                fw("base classes --- ")
            else:
                fw("base class --- ")

            fw(", ".join([(":class:`%s`" % base.identifier) for base in reversed(bases)]))
            fw("\n\n")
        
        subclasses = [s for s in structs.values() if s.base is struct]
        
        if subclasses:
            fw("subclasses --- \n")
            fw(", ".join([(":class:`%s`" % s.identifier) for s in subclasses]))
            fw("\n\n")


        if struct.base:
            fw(".. class:: %s(%s)\n\n" % (struct.identifier, struct.base.identifier))
        else:
            fw(".. class:: %s\n\n" % struct.identifier)

        fw("   %s\n\n" % struct.description)

        for prop in struct.properties:
            fw("   .. attribute:: %s\n\n" % prop.identifier)
            if prop.description:
                fw("      %s\n\n" % prop.description)
            type_descr = prop.get_type_description(class_fmt=":class:`%s`")
            fw("      *type* %s\n\n" % type_descr)
        
        # python attributes
        py_properties = struct.get_py_properties()
        py_prop = None
        for identifier, py_prop in py_properties:
            pyprop2sphinx("   ", fw, identifier, py_prop)
        del py_properties, py_prop

        for func in struct.functions:
            args_str = ", ".join([prop.get_arg_default(force=False) for prop in func.args])

            fw("   .. method:: %s(%s)\n\n" % (func.identifier, args_str))
            fw("      %s\n\n" % func.description)
            
            for prop in func.args:
                write_param("      ", fw, prop)

            if len(func.return_values) == 1:
                write_param("      ", fw, func.return_values[0], is_return=True)
            elif func.return_values: # multiple return values
                fw("      :return (%s):\n" % ", ".join([prop.identifier for prop in func.return_values]))
                for prop in func.return_values:
                    type_descr = prop.get_type_description(as_ret=True, class_fmt=":class:`%s`")
                    descr = prop.description
                    if not descr:
                        descr = prop.name
                    fw("         `%s`, %s, %s\n\n" % (prop.identifier, descr, type_descr))

            fw("\n")


        # python methods
        py_funcs = struct.get_py_functions()
        py_func = None
        
        for identifier, py_func in py_funcs:
            pyfunc2sphinx("   ", fw, identifier, py_func, is_class=True)
        del py_funcs, py_func

        if struct.references:
            # use this otherwise it gets in the index for a normal heading.
            fw(".. rubric:: References\n\n")

            for ref in struct.references:
                ref_split = ref.split(".")
                if len(ref_split) > 2:
                    ref = ref_split[-2] + "." + ref_split[-1]
                fw("* :class:`%s`\n" % ref)
            fw("\n")


    for struct in structs.values():
        write_struct(struct)

    # oeprators
    def write_ops():
        fw = None
        
        last_mod = ''
        
        for op_key in sorted(ops.keys()):
            op = ops[op_key]
            
            if last_mod != op.module_name:
                filepath = os.path.join(BASEPATH, "bpy.ops.%s.rst" % op.module_name)
                file = open(filepath, "w")
                fw = file.write
                
                title = "%s Operators"  % (op.module_name[0].upper() + op.module_name[1:])
                fw("%s\n%s\n\n" % (title, "=" * len(title)))
                
                fw(".. module:: bpy.ops.%s\n\n" % op.module_name)
                last_mod = op.module_name
            
            args_str = ", ".join([prop.get_arg_default(force=True) for prop in op.args])
            fw(".. function:: %s(%s)\n\n" % (op.func_name, args_str))
            if op.description:
                fw("   %s\n\n" % op.description)
            for prop in op.args:
                write_param("   ", fw, prop)
            if op.args:
                fw("\n")

            location = op.get_location()
            if location != (None, None):
                fw("   *python operator source --- `%s:%d`* \n\n" % location)
    
    write_ops()

    file.close()

if __name__ == '__main__':
    if 'bpy' not in dir():
        print("\nError, this script must run from inside blender2.5")
        print(script_help_msg)
    else:
        # os.system("rm source/blender/python/doc/sphinx-in/*.rst")
        # os.system("rm -rf source/blender/python/doc/sphinx-out/*")
        rna2sphinx('source/blender/python/doc/sphinx-in')

    import sys
    sys.exit()
