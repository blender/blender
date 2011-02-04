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

# <pep8 compliant>

script_help_msg = '''
Usage:

For HTML generation
-------------------
- Run this script from blenders root path once you have compiled blender

    ./blender.bin -b -P doc/python_api/sphinx_doc_gen.py

  This will generate python files in doc/python_api/sphinx-in/,
  assuming that ./blender.bin is or links to the blender executable

- Generate html docs by running...

    sphinx-build doc/python_api/sphinx-in doc/python_api/sphinx-out

  assuming that you have sphinx 0.6.7 installed

For PDF generation
------------------
- After you have built doc/python_api/sphinx-in (see above), run:

    sphinx-build -b latex doc/python_api/sphinx-in doc/python_api/sphinx-out
    cd doc/python_api/sphinx-out
    make
'''

# import rpdb2; rpdb2.start_embedded_debugger('test')

import os
import inspect
import bpy
import rna_info

# lame, python wont give some access
ClassMethodDescriptorType = type(dict.__dict__['fromkeys'])
MethodDescriptorType = type(dict.get)
GetSetDescriptorType = type(int.real)

EXAMPLE_SET = set()
EXAMPLE_SET_USED = set()

_BPY_STRUCT_FAKE = "bpy_struct"
_BPY_FULL_REBUILD = False


def undocumented_message(module_name, type_name, identifier):
    if str(type_name).startswith('<module'):
        preloadtitle = '%s.%s' % (module_name, identifier)
    else:
        preloadtitle = '%s.%s.%s' % (module_name, type_name, identifier)
    message = "Undocumented (`contribute "\
        "<http://wiki.blender.org/index.php/Dev:2.5/Py/API/Documentation/Contribute"\
        "?action=edit&section=new&preload=Dev:2.5/Py/API/Documentation/Contribute/Howto-message"\
        "&preloadtitle=%s>`_)\n\n" % preloadtitle
    return message


def range_str(val):
    '''
    Converts values to strings for the range directive.
    (unused function it seems)
    '''
    if val < -10000000:
        return '-inf'
    elif val > 10000000:
        return 'inf'
    elif type(val) == float:
        return '%g' % val
    else:
        return str(val)


def write_example_ref(ident, fw, example_id, ext="py"):
    if example_id in EXAMPLE_SET:
        fw("%s.. literalinclude:: ../examples/%s.%s\n\n" % (ident, example_id, ext))
        EXAMPLE_SET_USED.add(example_id)
    else:
        if bpy.app.debug:
            print("\tskipping example:", example_id)


def write_indented_lines(ident, fn, text, strip=True):
    '''
    Apply same indentation to all lines in a multilines text.
    '''
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


def py_descr2sphinx(ident, fw, descr, module_name, type_name, identifier):
    if identifier.startswith("_"):
        return

    doc = descr.__doc__
    if not doc:
        doc = undocumented_message(module_name, type_name, identifier)

    if type(descr) == GetSetDescriptorType:
        fw(ident + ".. attribute:: %s\n\n" % identifier)
        write_indented_lines(ident + "   ", fw, doc, False)
    elif type(descr) in (MethodDescriptorType, ClassMethodDescriptorType):
        write_indented_lines(ident, fw, doc, False)
    else:
        raise TypeError("type was not GetSetDescriptorType, MethodDescriptorType or ClassMethodDescriptorType")

    write_example_ref(ident, fw, module_name + "." + type_name + "." + identifier)
    fw("\n")


def py_c_func2sphinx(ident, fw, module_name, type_name, identifier, py_func, is_class=True):
    '''
    c defined function to sphinx.
    '''

    # dump the docstring, assume its formatted correctly
    if py_func.__doc__:
        write_indented_lines(ident, fw, py_func.__doc__, False)
        fw("\n")
    else:
        fw(ident + ".. function:: %s()\n\n" % identifier)
        fw(ident + "   " + undocumented_message(module_name, type_name, identifier))


def pyprop2sphinx(ident, fw, identifier, py_prop):
    '''
    python property to sphinx
    '''
    # readonly properties use "data" directive, variables use "attribute" directive
    if py_prop.fset is None:
        fw(ident + ".. data:: %s\n\n" % identifier)
    else:
        fw(ident + ".. attribute:: %s\n\n" % identifier)
    write_indented_lines(ident + "   ", fw, py_prop.__doc__)
    if py_prop.fset is None:
        fw(ident + "   (readonly)\n\n")


def pymodule2sphinx(BASEPATH, module_name, module, title):
    import types
    attribute_set = set()
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
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue
        # naughty, we also add getset's into PyStructs, this is not typical py but also not incorrect.
        if type(descr) == types.GetSetDescriptorType:  # 'bpy_app_type' name is only used for examples and messages
            py_descr2sphinx("", fw, descr, module_name, "bpy_app_type", key)
            attribute_set.add(key)
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue

        if type(descr) == types.MemberDescriptorType:
            if descr.__doc__:
                fw(".. data:: %s\n\n" % key)
                write_indented_lines("   ", fw, descr.__doc__, False)
                attribute_set.add(key)
                fw("\n")
    del key, descr

    classes = []

    for attribute in sorted(dir(module)):
        if not attribute.startswith("_"):

            if attribute in attribute_set:
                continue

            if attribute.startswith("n_"):  # annoying exception, needed for bpy.app
                continue

            value = getattr(module, attribute)

            value_type = type(value)

            if value_type == types.FunctionType:
                pyfunc2sphinx("", fw, attribute, value, is_class=False)
            elif value_type in (types.BuiltinMethodType, types.BuiltinFunctionType):  # both the same at the moment but to be future proof
                # note: can't get args from these, so dump the string as is
                # this means any module used like this must have fully formatted docstrings.
                py_c_func2sphinx("", fw, module_name, module, attribute, value, is_class=False)
            elif value_type == type:
                classes.append((attribute, value))
            elif value_type in (bool, int, float, str, tuple):
                # constant, not much fun we can do here except to list it.
                # TODO, figure out some way to document these!
                fw(".. data:: %s\n\n" % attribute)
                write_indented_lines("   ", fw, "constant value %s" % repr(value), False)
                fw("\n")
            else:
                print("\tnot documenting %s.%s" % (module_name, attribute))
                continue

            attribute_set.add(attribute)
            # TODO, more types...

    # write collected classes now
    for (type_name, value) in classes:
        # May need to be its own function
        fw(".. class:: %s\n\n" % type_name)
        if value.__doc__:
            write_indented_lines("   ", fw, value.__doc__, False)
            fw("\n")
        write_example_ref("   ", fw, module_name + "." + type_name)

        descr_items = [(key, descr) for key, descr in sorted(value.__dict__.items()) if not key.startswith("__")]

        for key, descr in descr_items:
            if type(descr) == ClassMethodDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

        for key, descr in descr_items:
            if type(descr) == MethodDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

        for key, descr in descr_items:
            if type(descr) == GetSetDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

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

    # for use with files
    version_string_fp = "_".join(str(v) for v in bpy.app.version)

    fw("project = 'Blender'\n")
    # fw("master_doc = 'index'\n")
    fw("copyright = u'Blender Foundation'\n")
    fw("version = '%s - UNSTABLE API'\n" % version_string)
    fw("release = '%s - UNSTABLE API'\n" % version_string)
    fw("html_theme = 'blender-org'\n")
    fw("html_theme_path = ['../']\n")
    fw("html_favicon = 'favicon.ico'\n")
    # not helpful since the source us generated, adds to upload size.
    fw("html_copy_source = False\n")
    fw("\n")
    # needed for latex, pdf gen
    fw("latex_documents = [ ('contents', 'contents.tex', 'Blender Index', 'Blender Foundation', 'manual'), ]\n")
    fw("latex_paper_size = 'a4paper'\n")
    file.close()

    # main page needed for sphinx (index.html)
    filepath = os.path.join(BASEPATH, "contents.rst")
    file = open(filepath, "w")
    fw = file.write

    fw("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    fw(" Blender Documentation contents\n")
    fw("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    fw("\n")
    fw("This document is an API reference for Blender %s. built %s.\n" % (version_string, bpy.app.build_date))
    fw("\n")
    fw("An introduction to Blender and Python can be found at <http://wiki.blender.org/index.php/Dev:2.5/Py/API/Intro>\n")
    fw("\n")
    fw("`A PDF version of this document is also available <blender_python_reference_%s.pdf>`__\n" % version_string_fp)
    fw("\n")
    fw(".. warning:: The Python API in Blender is **UNSTABLE**, It should only be used for testing, any script written now may break in future releases.\n")
    fw("   \n")
    fw("   The following areas are subject to change.\n")
    fw("      * operator names and arguments\n")
    fw("      * render api\n")
    fw("      * function calls with the data api (any function calls with values accessed from bpy.data), including functions for importing and exporting meshes\n")
    fw("      * class registration (Operator, Panels, Menus, Headers)\n")
    fw("      * modules: bpy.props, blf)\n")
    fw("      * members in the bpy.context have to be reviewed\n")
    fw("      * python defined modal operators, especially drawing callbacks are highly experemental\n")
    fw("   \n")
    fw("   These parts of the API are relatively stable and are unlikely to change significantly\n")
    fw("      * data API, access to attributes of blender data such as mesh verts, material color, timeline frames and scene objects\n")
    fw("      * user interface functions for defining buttons, creation of menus, headers, panels\n")
    fw("      * modules: bgl and mathutils\n")
    fw("      * game engine modules\n")
    fw("\n")

    fw("===================\n")
    fw("Application Modules\n")
    fw("===================\n")
    fw("\n")
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")
    fw("   bpy.data.rst\n\n")  # note: not actually a module
    fw("   bpy.ops.rst\n\n")
    fw("   bpy.types.rst\n\n")

    # py modules
    fw("   bpy.utils.rst\n\n")
    fw("   bpy.path.rst\n\n")
    fw("   bpy.app.rst\n\n")

    # C modules
    fw("   bpy.props.rst\n\n")

    fw("==================\n")
    fw("Standalone Modules\n")
    fw("==================\n")
    fw("\n")
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")

    fw("   mathutils.rst\n\n")
    fw("   mathutils.geometry.rst\n\n")
    # XXX TODO
    #fw("   bgl.rst\n\n")
    fw("   blf.rst\n\n")
    fw("   aud.rst\n\n")

    # game engine
    fw("===================\n")
    fw("Game Engine Modules\n")
    fw("===================\n")
    fw("\n")
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")
    fw("   bge.types.rst\n\n")
    fw("   bge.logic.rst\n\n")
    fw("   bge.render.rst\n\n")
    fw("   bge.events.rst\n\n")

    file.close()

    # internal modules
    filepath = os.path.join(BASEPATH, "bpy.ops.rst")
    file = open(filepath, "w")
    fw = file.write
    fw("Operators (bpy.ops)\n")
    fw("===================\n\n")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    fw("   bpy.ops.*\n\n")
    file.close()

    filepath = os.path.join(BASEPATH, "bpy.types.rst")
    file = open(filepath, "w")
    fw = file.write
    fw("Types (bpy.types)\n")
    fw("=================\n\n")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    fw("   bpy.types.*\n\n")
    file.close()

    # not actually a module, only write this file so we
    # can reference in the TOC
    filepath = os.path.join(BASEPATH, "bpy.data.rst")
    file = open(filepath, "w")
    fw = file.write
    fw("Data Access (bpy.data)\n")
    fw("======================\n\n")
    fw(".. module:: bpy\n")
    fw("\n")
    fw("This module is used for all blender/python access.\n")
    fw("\n")
    fw(".. data:: data\n")
    fw("\n")
    fw("   Access to blenders internal data\n")
    fw("\n")
    fw("   :type: :class:`bpy.types.BlendData`\n")
    fw("\n")
    fw(".. literalinclude:: ../examples/bpy.data.py\n")
    file.close()

    EXAMPLE_SET_USED.add("bpy.data")

    # python modules
    from bpy import utils as module
    pymodule2sphinx(BASEPATH, "bpy.utils", module, "Utilities (bpy.utils)")

    from bpy import path as module
    pymodule2sphinx(BASEPATH, "bpy.path", module, "Path Utilities (bpy.path)")

    # C modules
    from bpy import app as module
    pymodule2sphinx(BASEPATH, "bpy.app", module, "Application Data (bpy.app)")

    from bpy import props as module
    pymodule2sphinx(BASEPATH, "bpy.props", module, "Property Definitions (bpy.props)")

    import mathutils as module
    pymodule2sphinx(BASEPATH, "mathutils", module, "Math Types & Utilities (mathutils)")
    del module

    import mathutils.geometry as module
    pymodule2sphinx(BASEPATH, "mathutils.geometry", module, "Geometry Utilities (mathutils.geometry)")
    del module

    import blf as module
    pymodule2sphinx(BASEPATH, "blf", module, "Font Drawing (blf)")
    del module

    # XXX TODO
    #import bgl as module
    #pymodule2sphinx(BASEPATH, "bgl", module, "Blender OpenGl wrapper (bgl)")
    #del module

    import aud as module
    pymodule2sphinx(BASEPATH, "aud", module, "Audio System (aud)")
    del module

    ## game engine
    import shutil
    # copy2 keeps time/date stamps
    shutil.copy2(os.path.join(BASEPATH, "..", "rst", "bge.types.rst"), BASEPATH)
    shutil.copy2(os.path.join(BASEPATH, "..", "rst", "bge.logic.rst"), BASEPATH)
    shutil.copy2(os.path.join(BASEPATH, "..", "rst", "bge.render.rst"), BASEPATH)
    shutil.copy2(os.path.join(BASEPATH, "..", "rst", "bge.events.rst"), BASEPATH)

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
            fw(ident + ":%s%s: %s\n" % (id_name, identifier, ", ".join(val for val in (prop.name, prop.description) if val)))
        fw(ident + ":%s%s: %s\n" % (id_type, identifier, type_descr))

    def write_struct(struct):
        #if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #    return

        #if not struct.identifier == "Object":
        #    return

        filepath = os.path.join(BASEPATH, "bpy.types.%s.rst" % struct.identifier)
        file = open(filepath, "w")
        fw = file.write

        base_id = getattr(struct.base, "identifier", "")

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            title = "%s(%s)" % (struct.identifier, base_id)
        else:
            title = struct.identifier

        fw("%s\n%s\n\n" % (title, "=" * len(title)))

        fw(".. module:: bpy.types\n\n")

        base_ids = [base.identifier for base in struct.get_bases()]

        if _BPY_STRUCT_FAKE:
            base_ids.append(_BPY_STRUCT_FAKE)

        base_ids.reverse()

        if base_ids:
            if len(base_ids) > 1:
                fw("base classes --- ")
            else:
                fw("base class --- ")

            fw(", ".join((":class:`%s`" % base_id) for base_id in base_ids))
            fw("\n\n")

        subclass_ids = [s.identifier for s in structs.values() if s.base is struct if not rna_info.rna_id_ignore(s.identifier)]
        if subclass_ids:
            fw("subclasses --- \n" + ", ".join((":class:`%s`" % s) for s in subclass_ids) + "\n\n")

        base_id = getattr(struct.base, "identifier", "")

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            fw(".. class:: %s(%s)\n\n" % (struct.identifier, base_id))
        else:
            fw(".. class:: %s\n\n" % struct.identifier)

        fw("   %s\n\n" % struct.description)

        # properties sorted in alphabetical order
        sorted_struct_properties = struct.properties[:]
        sorted_struct_properties.sort(key=lambda prop: prop.identifier)

        for prop in sorted_struct_properties:
            type_descr = prop.get_type_description(class_fmt=":class:`%s`")
            # readonly properties use "data" directive, variables properties use "attribute" directive
            if 'readonly' in type_descr:
                fw("   .. data:: %s\n\n" % prop.identifier)
            else:
                fw("   .. attribute:: %s\n\n" % prop.identifier)
            if prop.description:
                fw("      %s\n\n" % prop.description)
            fw("      :type: %s\n\n" % type_descr)

        # python attributes
        py_properties = struct.get_py_properties()
        py_prop = None
        for identifier, py_prop in py_properties:
            pyprop2sphinx("   ", fw, identifier, py_prop)
        del py_properties, py_prop

        for func in struct.functions:
            args_str = ", ".join(prop.get_arg_default(force=False) for prop in func.args)

            fw("   .. %s:: %s(%s)\n\n" % ("classmethod" if func.is_classmethod else "method", func.identifier, args_str))
            fw("      %s\n\n" % func.description)

            for prop in func.args:
                write_param("      ", fw, prop)

            if len(func.return_values) == 1:
                write_param("      ", fw, func.return_values[0], is_return=True)
            elif func.return_values:  # multiple return values
                fw("      :return (%s):\n" % ", ".join(prop.identifier for prop in func.return_values))
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

        lines = []

        if struct.base or _BPY_STRUCT_FAKE:
            bases = list(reversed(struct.get_bases()))

            # props
            lines[:] = []

            if _BPY_STRUCT_FAKE:
                descr_items = [(key, descr) for key, descr in sorted(bpy.types.Struct.__bases__[0].__dict__.items()) if not key.startswith("__")]

            if _BPY_STRUCT_FAKE:
                for key, descr in descr_items:
                    if type(descr) == GetSetDescriptorType:
                        lines.append("   * :class:`%s.%s`\n" % (_BPY_STRUCT_FAKE, key))

            for base in bases:
                for prop in base.properties:
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, prop.identifier))

                for identifier, py_prop in base.get_py_properties():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))

                for identifier, py_prop in base.get_py_properties():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))

            if lines:
                fw(".. rubric:: Inherited Properties\n\n")

                fw(".. hlist::\n")
                fw("   :columns: 2\n\n")

                for line in lines:
                    fw(line)
                fw("\n")

            # funcs
            lines[:] = []

            if _BPY_STRUCT_FAKE:
                for key, descr in descr_items:
                    if type(descr) == MethodDescriptorType:
                        lines.append("   * :class:`%s.%s`\n" % (_BPY_STRUCT_FAKE, key))

            for base in bases:
                for func in base.functions:
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, func.identifier))
                for identifier, py_func in base.get_py_functions():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))

            if lines:
                fw(".. rubric:: Inherited Functions\n\n")

                fw(".. hlist::\n")
                fw("   :columns: 2\n\n")

                for line in lines:
                    fw(line)
                fw("\n")

            lines[:] = []

        if struct.references:
            # use this otherwise it gets in the index for a normal heading.
            fw(".. rubric:: References\n\n")

            fw(".. hlist::\n")
            fw("   :columns: 2\n\n")

            for ref in struct.references:
                ref_split = ref.split(".")
                if len(ref_split) > 2:
                    ref = ref_split[-2] + "." + ref_split[-1]
                fw("   * :class:`%s`\n" % ref)
            fw("\n")

    for struct in structs.values():
        # TODO, rna_info should filter these out!
        if "_OT_" in struct.identifier:
            continue
        write_struct(struct)

    # special case, bpy_struct
    if _BPY_STRUCT_FAKE:
        filepath = os.path.join(BASEPATH, "bpy.types.%s.rst" % _BPY_STRUCT_FAKE)
        file = open(filepath, "w")
        fw = file.write

        fw("%s\n" % _BPY_STRUCT_FAKE)
        fw("=" * len(_BPY_STRUCT_FAKE) + "\n")
        fw("\n")
        fw(".. module:: bpy.types\n")
        fw("\n")

        subclass_ids = [s.identifier for s in structs.values() if s.base is None if not rna_info.rna_id_ignore(s.identifier)]
        if subclass_ids:
            fw("subclasses --- \n" + ", ".join((":class:`%s`" % s) for s in sorted(subclass_ids)) + "\n\n")

        fw(".. class:: %s\n\n" % _BPY_STRUCT_FAKE)
        fw("   built-in base class for all classes in bpy.types.\n\n")
        fw("   .. note::\n\n")
        fw("      Note that bpy.types.%s is not actually available from within blender, it only exists for the purpose of documentation.\n\n" % _BPY_STRUCT_FAKE)

        descr_items = [(key, descr) for key, descr in sorted(bpy.types.Struct.__bases__[0].__dict__.items()) if not key.startswith("__")]

        for key, descr in descr_items:
            if type(descr) == MethodDescriptorType:  # GetSetDescriptorType, GetSetDescriptorType's are not documented yet
                py_descr2sphinx("   ", fw, descr, "bpy.types", _BPY_STRUCT_FAKE, key)

        for key, descr in descr_items:
            if type(descr) == GetSetDescriptorType:
                py_descr2sphinx("   ", fw, descr, "bpy.types", _BPY_STRUCT_FAKE, key)

    # operators
    def write_ops():
        API_BASEURL = "https://svn.blender.org/svnroot/bf-blender/trunk/blender/release/scripts"
        fw = None
        last_mod = ''

        for op_key in sorted(ops.keys()):
            op = ops[op_key]

            if last_mod != op.module_name:
                filepath = os.path.join(BASEPATH, "bpy.ops.%s.rst" % op.module_name)
                file = open(filepath, "w")
                fw = file.write

                title = "%s Operators" % (op.module_name[0].upper() + op.module_name[1:])
                fw("%s\n%s\n\n" % (title, "=" * len(title)))

                fw(".. module:: bpy.ops.%s\n\n" % op.module_name)
                last_mod = op.module_name

            args_str = ", ".join(prop.get_arg_default(force=True) for prop in op.args)
            fw(".. function:: %s(%s)\n\n" % (op.func_name, args_str))

            # if the description isn't valid, we output the standard warning
            # with a link to the wiki so that people can help
            if not op.description or op.description == "(undocumented operator)":
                operator_description = undocumented_message('bpy.ops', op.module_name, op.func_name)
            else:
                operator_description = op.description

            fw("   %s\n\n" % operator_description)
            for prop in op.args:
                write_param("   ", fw, prop)
            if op.args:
                fw("\n")

            location = op.get_location()
            if location != (None, None):
                fw("   :file: `%s <%s/%s>`_:%d\n\n" % (location[0], API_BASEURL, location[0], location[1]))

    write_ops()

    file.close()


def main():
    import bpy
    if 'bpy' not in dir():
        print("\nError, this script must run from inside blender2.5")
        print(script_help_msg)
    else:
        import shutil

        script_dir = os.path.dirname(__file__)
        path_in = os.path.join(script_dir, "sphinx-in")
        path_out = os.path.join(script_dir, "sphinx-out")
        path_examples = os.path.join(script_dir, "examples")
        # only for partial updates
        path_in_tmp = path_in + "-tmp"

        if not os.path.exists(path_in):
            os.mkdir(path_in)

        for f in os.listdir(path_examples):
            if f.endswith(".py"):
                EXAMPLE_SET.add(os.path.splitext(f)[0])

        # only for full updates
        if _BPY_FULL_REBUILD:
            shutil.rmtree(path_in, True)
            shutil.rmtree(path_out, True)
        else:
            # write here, then move
            shutil.rmtree(path_in_tmp, True)

        rna2sphinx(path_in_tmp)

        if not _BPY_FULL_REBUILD:
            import filecmp

            # now move changed files from 'path_in_tmp' --> 'path_in'
            file_list_path_in = set(os.listdir(path_in))
            file_list_path_in_tmp = set(os.listdir(path_in_tmp))

            # remove deprecated files that have been removed.
            for f in sorted(file_list_path_in):
                if f not in file_list_path_in_tmp:
                    print("\tdeprecated: %s" % f)
                    os.remove(os.path.join(path_in, f))

            # freshen with new files.
            for f in sorted(file_list_path_in_tmp):
                f_from = os.path.join(path_in_tmp, f)
                f_to = os.path.join(path_in, f)

                do_copy = True
                if f in file_list_path_in:
                    if filecmp.cmp(f_from, f_to):
                        do_copy = False

                if do_copy:
                    print("\tupdating: %s" % f)
                    shutil.copy(f_from, f_to)
                '''else:
                    print("\tkeeping: %s" % f) # eh, not that useful'''

        EXAMPLE_SET_UNUSED = EXAMPLE_SET - EXAMPLE_SET_USED
        if EXAMPLE_SET_UNUSED:
            print("\nUnused examples found in '%s'..." % path_examples)
            for f in EXAMPLE_SET_UNUSED:
                print("    %s.py" % f)
            print("  %d total\n" % len(EXAMPLE_SET_UNUSED))

    import sys
    sys.exit()

if __name__ == '__main__':
    main()
