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
 # Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 #
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****

script_help_msg = '''
Usage,
run this script from blenders root path once you have compiled blender
    ./blender.bin -b -P /b/source/blender/python/doc/sphinx_doc_gen.py

This will generate python files in "./source/blender/python/doc/sphinx-in"
Generate html docs  by running...
    
    sphinx-build source/blender/python/doc/sphinx-in source/blender/python/doc/sphinx-out


For PDF generation

    sphinx-build -b latex source/blender/python/doc/sphinx-in source/blender/python/doc/sphinx-out
    cd source/blender/python/doc/sphinx-out
    make
'''


import os
import inspect
import bpy
import rna_info
reload(rna_info)

EXAMPLE_SET = set()
EXAMPLE_SET_USED = set()

def range_str(val):
    if val < -10000000:	return '-inf'
    if val >  10000000:	return 'inf'
    if type(val)==float:
        return '%g'  % val
    else:
        return str(val)


def write_example_ref(ident, fw, example_id, ext=".py"):
    if example_id in EXAMPLE_SET:
        fw("%s.. literalinclude:: ../examples/%s%s\n\n" % (ident, example_id, ext))
        EXAMPLE_SET_USED.add(example_id)
    else:
        if bpy.app.debug:
            print("\tskipping example:", example_id)


def write_indented_lines(ident, fn, text, strip=True):
    if text is None:
        return
    for l in text.split("\n"):
        if strip:
            fn(ident + l.strip() + "\n")
        else:
            fn(ident + l + "\n")


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
        write_indented_lines(ident, fw, py_func.__doc__, False)
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

    fw = file.write
    
    fw(title + "\n")
    fw(("=" * len(title)) + "\n\n")
    
    fw(".. module:: %s\n\n" % module_name)
    
    if module.__doc__:
        # Note, may contain sphinx syntax, dont mangle!
        fw(module.__doc__.strip())
        fw("\n\n")
        
    write_example_ref("", fw, module_name)
    
    # write members of the module
    # only tested with PyStructs which are not exactly modules
    for attribute, descr in sorted(type(module).__dict__.items()):
        if type(descr) == types.MemberDescriptorType:
            if descr.__doc__:
                fw(".. data:: %s\n\n" % attribute)
                write_indented_lines("   ", fw, descr.__doc__, False)
                fw("\n")
    
    
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
            write_indented_lines("   ", fw, value.__doc__, False)
            fw("\n")
        write_example_ref("   ", fw, module_name + "." + attribute)

        for key in sorted(value.__dict__.keys()):
            if key.startswith("__"):
                continue
            descr = value.__dict__[key]
            if type(descr) == GetSetDescriptorType:
                if descr.__doc__:
                    fw("   .. attribute:: %s\n\n" % key)
                    write_indented_lines("   ", fw, descr.__doc__, False)
                    write_example_ref("   ", fw, module_name + "." + attribute + "." + key)
                    fw("\n")

        for key in sorted(value.__dict__.keys()):
            if key.startswith("__"):
                continue
            descr = value.__dict__[key]
            if type(descr) == MethodDescriptorType: # GetSetDescriptorType, GetSetDescriptorType's are not documented yet
                if descr.__doc__:
                    write_indented_lines("   ", fw, descr.__doc__, False)
                    write_example_ref("   ", fw, module_name + "." + attribute + "." + key)
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
    
    
    version_string = bpy.app.version_string.split("(")[0]
    if bpy.app.build_revision != "Unknown":
        version_string = version_string + " r" + bpy.app.build_revision
    
    fw("project = 'Blender 3D'\n")
    # fw("master_doc = 'index'\n")
    fw("copyright = u'Blender Foundation'\n")
    fw("version = '%s'\n" % version_string)
    fw("release = '%s'\n" % version_string)
    fw("\n")
    # needed for latex, pdf gen
    fw("latex_documents = [ ('contents', 'contents.tex', 'Blender Index', 'Blender Foundation', 'manual'), ]\n")
    fw("latex_paper_size = 'a4paper'\n")
    file.close()


    filepath = os.path.join(BASEPATH, "contents.rst")
    file = open(filepath, "w")
    fw = file.write
    
    fw("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    fw(" Blender Documentation contents\n")
    fw("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    fw("\n")
    fw("This document is an API reference for Blender %s. built %s.\n" % (version_string, bpy.app.build_date))
    fw("\n")
    fw("An introduction to blender and python can be found at <http://wiki.blender.org/index.php/Dev:2.5/Py/API/Intro>\n")
    fw("\n")
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")
    fw("   bpy.ops.rst\n\n")
    fw("   bpy.types.rst\n\n")
    
    # py modules
    fw("   bpy.utils.rst\n\n")
    fw("   bpy.app.rst\n\n")
    
    # C modules
    fw("   bpy.props.rst\n\n")
    
    fw("   Mathutils.rst\n\n")
    fw("   BLF.rst\n\n")

    file.close()



    # internal modules
    filepath = os.path.join(BASEPATH, "bpy.ops.rst")
    file = open(filepath, "w")
    fw = file.write
    fw("Blender Operators (bpy.ops)\n")
    fw("===========================\n\n")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    fw("   bpy.ops.*\n\n")
    file.close()

    filepath = os.path.join(BASEPATH, "bpy.types.rst")
    file = open(filepath, "w")
    fw = file.write
    fw("Blender Types (bpy.types)\n")
    fw("=========================\n\n")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    fw("   bpy.types.*\n\n")
    file.close()



    # python modules
    from bpy import utils as module
    pymodule2sphinx(BASEPATH, "bpy.utils", module, "Utilities (bpy.utils)")

    # C modules
    from bpy import app as module
    pymodule2sphinx(BASEPATH, "bpy.app", module, "Application Data (bpy.app)")

    from bpy import props as module
    pymodule2sphinx(BASEPATH, "bpy.props", module, "Property Definitions (bpy.props)")
    
    import Mathutils as module
    pymodule2sphinx(BASEPATH, "Mathutils", module, "Math Types & Utilities (Mathutils)")
    del module

    import BLF as module
    pymodule2sphinx(BASEPATH, "blf", module, "Blender Font Drawing (blf)")
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
        # TODO, rna_info should filter these out!
        if "_OT_" in struct.identifier:
            continue
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
        import shutil

        path_in = 'source/blender/python/doc/sphinx-in'
        path_out = 'source/blender/python/doc/sphinx-in'
        path_examples = 'source/blender/python/doc/examples'

        shutil.rmtree(path_in, True)
        shutil.rmtree(path_out, True)
        
        for f in os.listdir(path_examples):
            if f.endswith(".py"):
                EXAMPLE_SET.add(os.path.splitext(f)[0])
        
        rna2sphinx(path_in)

        # for fast module testing
        # os.system("rm source/blender/python/doc/sphinx-in/bpy.types.*.rst")
        # os.system("rm source/blender/python/doc/sphinx-in/bpy.ops.*.rst")
        
        EXAMPLE_SET_UNUSED = EXAMPLE_SET - EXAMPLE_SET_USED
        if EXAMPLE_SET_UNUSED:
            print("\nUnused examples found in '%s'..." % path_examples)
            for f in EXAMPLE_SET_UNUSED:
                print("    %s.py" % f)
            print("  %d total\n" % len(EXAMPLE_SET_UNUSED))

    import sys
    sys.exit()
