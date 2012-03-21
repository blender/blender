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
 # Contributor(s): Campbell Barton, Luca Bonavita
 #
 # #**** END GPL LICENSE BLOCK #****

# <pep8 compliant>

SCRIPT_HELP_MSG = """

API dump in RST files
---------------------
  Run this script from blenders root path once you have compiled blender

    ./blender.bin --background -noaudio --python doc/python_api/sphinx_doc_gen.py

  This will generate python files in doc/python_api/sphinx-in/
  providing ./blender.bin is or links to the blender executable

  To choose sphinx-in directory:
    ./blender.bin --background --python doc/python_api/sphinx_doc_gen.py -- --output ../python_api

  For quick builds:
    ./blender.bin --background --python doc/python_api/sphinx_doc_gen.py -- --partial


Sphinx: HTML generation
-----------------------
  After you have built doc/python_api/sphinx-in (see above),
  generate html docs by running:

    cd doc/python_api
    sphinx-build sphinx-in sphinx-out

  This requires sphinx 1.0.7 to be installed.


Sphinx: PDF generation
----------------------
  After you have built doc/python_api/sphinx-in (see above),
  generate the pdf doc by running:

    sphinx-build -b latex doc/python_api/sphinx-in doc/python_api/sphinx-out
    cd doc/python_api/sphinx-out
    make

"""

try:
    import bpy  # blender module
except:
    print("\nERROR: this script must run from inside Blender")
    print(SCRIPT_HELP_MSG)
    import sys
    sys.exit()

import rna_info     # blender module

# import rpdb2; rpdb2.start_embedded_debugger('test')
import os
import sys
import inspect
import shutil
import logging

from platform import platform
PLATFORM = platform().split('-')[0].lower()    # 'linux', 'darwin', 'windows'

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))


def handle_args():
    '''
    Parse the args passed to Blender after "--", ignored by Blender
    '''
    import argparse

    # When --help is given, print the usage text
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        usage=SCRIPT_HELP_MSG
    )

    # optional arguments
    parser.add_argument("-p", "--partial",
                        dest="partial",
                        type=str,
                        default="",
                        help="Use a wildcard to only build specific module(s)\n"
                             "Example: --partial bmesh*\n",
                        required=False)

    parser.add_argument("-f", "--fullrebuild",
                        dest="full_rebuild",
                        default=False,
                        action='store_true',
                        help="Rewrite all rst files in sphinx-in/ "
                             "(default=False)",
                        required=False)

    parser.add_argument("-b", "--bpy",
                        dest="bpy",
                        default=False,
                        action='store_true',
                        help="Write the rst file of the bpy module "
                             "(default=False)",
                        required=False)

    parser.add_argument("-o", "--output",
                        dest="output_dir",
                        type=str,
                        default=SCRIPT_DIR,
                        help="Path of the API docs (default=<script dir>)",
                        required=False)

    parser.add_argument("-T", "--sphinx-theme",
                        dest="sphinx_theme",
                        type=str,
                        default='default',
                        help=
                        # see SPHINX_THEMES below
                        "Sphinx theme (default='default')\n"
                        "Available themes\n"
                        "----------------\n"
                        "(Blender Foundation) blender-org\n"    # naiad
                        "(Sphinx) agogo, basic, epub, haiku, nature, "
                        "scrolls, sphinxdoc, traditional\n",
#                        choices=['naiad', 'blender-org'] +      # bf
#                                ['agogo', 'basic', 'epub',
#                                 'haiku', 'nature', 'scrolls',
#                                 'sphinxdoc', 'traditional'],   # sphinx
                        required=False)

    parser.add_argument("-N", "--sphinx-named-output",
                        dest="sphinx_named_output",
                        default=False,
                        action='store_true',
                        help="Add the theme name to the html dir name.\n"
                             "Example: \"sphinx-out_haiku\" (default=False)",
                        required=False)

    parser.add_argument("-B", "--sphinx-build",
                        dest="sphinx_build",
                        default=False,
                        action='store_true',
                        help="Build the html docs by running:\n"
                             "sphinx-build SPHINX_IN SPHINX_OUT\n"
                             "(default=False; does not depend on -P)",
                        required=False)

    parser.add_argument("-P", "--sphinx-build-pdf",
                        dest="sphinx_build_pdf",
                        default=False,
                        action='store_true',
                        help="Build the pdf by running:\n"
                             "sphinx-build -b latex SPHINX_IN SPHINX_OUT_PDF\n"
                             "(default=False; does not depend on -B)",
                        required=False)

    parser.add_argument("-R", "--pack-reference",
                        dest="pack_reference",
                        default=False,
                        action='store_true',
                        help="Pack all necessary files in the deployed dir.\n"
                             "(default=False; use with -B and -P)",
                        required=False)

    parser.add_argument("-l", "--log",
                        dest="log",
                        default=False,
                        action='store_true',
                        help=(
                        "Log the output of the api dump and sphinx|latex "
                        "warnings and errors (default=False).\n"
                        "If given, save logs in:\n"
                        "* OUTPUT_DIR/.bpy.log\n"
                        "* OUTPUT_DIR/.sphinx-build.log\n"
                        "* OUTPUT_DIR/.sphinx-build_pdf.log\n"
                        "* OUTPUT_DIR/.latex_make.log",
                        ),
                        required=False)

    # parse only the args passed after '--'
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1:]  # get all args after "--"

    return parser.parse_args(argv)


ARGS = handle_args()

# ----------------------------------BPY-----------------------------------------

BPY_LOGGER = logging.getLogger('bpy')
BPY_LOGGER.setLevel(logging.DEBUG)

"""
# for quick rebuilds
rm -rf /b/doc/python_api/sphinx-* && \
./blender.bin -b -noaudio --factory-startup -P doc/python_api/sphinx_doc_gen.py && \
sphinx-build doc/python_api/sphinx-in doc/python_api/sphinx-out

or

./blender.bin -b -noaudio --factory-startup -P doc/python_api/sphinx_doc_gen.py -- -f -B
"""

# Switch for quick testing so doc-builds don't take so long
if not ARGS.partial:
    # full build
    FILTER_BPY_OPS = None
    FILTER_BPY_TYPES = None
    EXCLUDE_INFO_DOCS = False
    EXCLUDE_MODULES = ()

else:
    # can manually edit this too:
    FILTER_BPY_OPS = ("import.scene", )  # allow
    FILTER_BPY_TYPES = ("bpy_struct", "Operator", "ID")  # allow
    EXCLUDE_INFO_DOCS = True
    EXCLUDE_MODULES = (
        "aud",
        "bge",
        "bge.constraints",
        "bge.events",
        "bge.logic",
        "bge.render",
        "bge.texture",
        "bge.types",
        "bgl",
        "blf",
        "bmesh",
        "bmesh.types",
        "bmesh.utils",
        "bpy.app",
        "bpy.app.handlers",
        "bpy.context",
        "bpy.data",
        "bpy.ops",  # supports filtering
        "bpy.path",
        "bpy.props",
        "bpy.types",  # supports filtering
        "bpy.utils",
        "bpy_extras",
        "gpu",
        "mathutils",
        "mathutils.geometry",
        "mathutils.noise",
    )

    # ------
    # Filter
    #
    # TODO, support bpy.ops and bpy.types filtering
    import fnmatch
    m = None
    EXCLUDE_MODULES = tuple([m for m in EXCLUDE_MODULES if not fnmatch.fnmatchcase(m, ARGS.partial)])
    del m
    del fnmatch

    BPY_LOGGER.debug("Partial Doc Build, Skipping: %s\n" % "\n                             ".join(sorted(EXCLUDE_MODULES)))

    #
    # done filtering
    # --------------

try:
    __import__("aud")
except ImportError:
    BPY_LOGGER.debug("Warning: Built without 'aud' module, docs incomplete...")
    EXCLUDE_MODULES = EXCLUDE_MODULES + ("aud", )

# examples
EXAMPLES_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "examples"))
EXAMPLE_SET = set()
for f in os.listdir(EXAMPLES_DIR):
    if f.endswith(".py"):
        EXAMPLE_SET.add(os.path.splitext(f)[0])
EXAMPLE_SET_USED = set()

# rst files dir
RST_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "rst"))

# extra info, not api reference docs
# stored in ./rst/info_*
INFO_DOCS = (
    ("info_quickstart.rst", "Blender/Python Quickstart: new to blender/scripting and want to get your feet wet?"),
    ("info_overview.rst", "Blender/Python API Overview: a more complete explanation of python integration"),
    ("info_best_practice.rst", "Best Practice: Conventions to follow for writing good scripts"),
    ("info_tips_and_tricks.rst", "Tips and Tricks: Hints to help you while writing scripts for blender"),
    ("info_gotcha.rst", "Gotcha's: some of the problems you may come up against when writing scripts"),
    )

# only support for properties atm.
RNA_BLACKLIST = {
    # XXX messes up PDF!, really a bug but for now just workaround.
    "UserPreferencesSystem": {"language", }
    }


# --------------------configure compile time options----------------------------

# -------------------------------BLENDER----------------------------------------

blender_version_strings = [str(v) for v in bpy.app.version]

# converting bytes to strings, due to #30154
BLENDER_REVISION = str(bpy.app.build_revision, 'utf_8')
BLENDER_DATE = str(bpy.app.build_date, 'utf_8')

BLENDER_VERSION_DOTS = ".".join(blender_version_strings)    # '2.62.1'
if BLENDER_REVISION != "Unknown":
    BLENDER_VERSION_DOTS += " r" + BLENDER_REVISION         # '2.62.1 r44584'

BLENDER_VERSION_PATH = "_".join(blender_version_strings)    # '2_62_1'
if bpy.app.version_cycle == "release":
    BLENDER_VERSION_PATH = "%s%s_release" % ("_".join(blender_version_strings[:2]),
                                             bpy.app.version_char)   # '2_62_release'

# --------------------------DOWNLOADABLE FILES----------------------------------

REFERENCE_NAME = "blender_python_reference_%s" % BLENDER_VERSION_PATH
REFERENCE_PATH = os.path.join(ARGS.output_dir, REFERENCE_NAME)
BLENDER_PDF_FILENAME = "%s.pdf" % REFERENCE_NAME
BLENDER_ZIP_FILENAME = "%s.zip" % REFERENCE_NAME

# -------------------------------SPHINX-----------------------------------------

SPHINX_THEMES = {'bf': ['blender-org'],  # , 'naiad',
                 'sphinx': ['agogo',
                            'basic',
                            'default',
                            'epub',
                            'haiku',
                            'nature',
                            'scrolls',
                            'sphinxdoc',
                            'traditional']}

available_themes = SPHINX_THEMES['bf'] + SPHINX_THEMES['sphinx']
if ARGS.sphinx_theme not in available_themes:
    print ("Please choose a theme among: %s" % ', '.join(available_themes))
    sys.exit()

if ARGS.sphinx_theme in SPHINX_THEMES['bf']:
    SPHINX_THEME_DIR = os.path.join(ARGS.output_dir, ARGS.sphinx_theme)
    SPHINX_THEME_SVN_DIR = os.path.join(SCRIPT_DIR, ARGS.sphinx_theme)

SPHINX_IN = os.path.join(ARGS.output_dir, "sphinx-in")
SPHINX_IN_TMP = SPHINX_IN + "-tmp"
SPHINX_OUT = os.path.join(ARGS.output_dir, "sphinx-out")
if ARGS.sphinx_named_output:
    SPHINX_OUT += "_%s" % ARGS.sphinx_theme

# html build
if ARGS.sphinx_build:
    SPHINX_BUILD = ["sphinx-build", SPHINX_IN, SPHINX_OUT]

    if ARGS.log:
        SPHINX_BUILD_LOG = os.path.join(ARGS.output_dir, ".sphinx-build.log")
        SPHINX_BUILD = ["sphinx-build",
                        "-w", SPHINX_BUILD_LOG,
                        SPHINX_IN, SPHINX_OUT]

# pdf build
if ARGS.sphinx_build_pdf:
    SPHINX_OUT_PDF = os.path.join(ARGS.output_dir, "sphinx-out_pdf")
    SPHINX_BUILD_PDF = ["sphinx-build",
                        "-b", "latex",
                        SPHINX_IN, SPHINX_OUT_PDF]
    SPHINX_MAKE_PDF = ["make", "-C", SPHINX_OUT_PDF]
    SPHINX_MAKE_PDF_STDOUT = None

    if ARGS.log:
        SPHINX_BUILD_PDF_LOG = os.path.join(ARGS.output_dir, ".sphinx-build_pdf.log")
        SPHINX_BUILD_PDF = ["sphinx-build", "-b", "latex",
                            "-w", SPHINX_BUILD_PDF_LOG,
                            SPHINX_IN, SPHINX_OUT_PDF]

        sphinx_make_pdf_log = os.path.join(ARGS.output_dir, ".latex_make.log")
        SPHINX_MAKE_PDF_STDOUT = open(sphinx_make_pdf_log, "w", encoding="utf-8")

# --------------------------------API DUMP--------------------------------------

# lame, python wont give some access
ClassMethodDescriptorType = type(dict.__dict__['fromkeys'])
MethodDescriptorType = type(dict.get)
GetSetDescriptorType = type(int.real)
from types import MemberDescriptorType

_BPY_STRUCT_FAKE = "bpy_struct"
_BPY_PROP_COLLECTION_FAKE = "bpy_prop_collection"

if _BPY_PROP_COLLECTION_FAKE:
    _BPY_PROP_COLLECTION_ID = ":class:`%s`" % _BPY_PROP_COLLECTION_FAKE
else:
    _BPY_PROP_COLLECTION_ID = "collection"


def is_struct_seq(value):
    return isinstance(value, tuple) and type(tuple) != tuple and hasattr(value, "n_fields")

def undocumented_message(module_name, type_name, identifier):
    if str(type_name).startswith('<module'):
        preloadtitle = '%s.%s' % (module_name, identifier)
    else:
        preloadtitle = '%s.%s.%s' % (module_name, type_name, identifier)
    message = ("Undocumented (`contribute "
               "<http://wiki.blender.org/index.php/"
               "Dev:2.5/Py/API/Generating_API_Reference/Contribute"
               "?action=edit"
               "&section=new"
               "&preload=Dev:2.5/Py/API/Generating_API_Reference/Contribute/Howto-message"
               "&preloadtitle=%s>`_)\n\n" % preloadtitle)
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


def example_extract_docstring(filepath):
    file = open(filepath, "r", encoding="utf-8")
    line = file.readline()
    line_no = 0
    text = []
    if line.startswith('"""'):  # assume nothing here
        line_no += 1
    else:
        file.close()
        return "", 0

    for line in file.readlines():
        line_no += 1
        if line.startswith('"""'):
            break
        else:
            text.append(line.rstrip())

    line_no += 1
    file.close()
    return "\n".join(text), line_no


def title_string(text, heading_char, double=False):
    filler = len(text) * heading_char
    
    if double:
        return "%s\n%s\n%s\n\n" % (filler, text, filler)
    else:
        return "%s\n%s\n\n" % (text, filler)


def write_example_ref(ident, fw, example_id, ext="py"):
    if example_id in EXAMPLE_SET:

        # extract the comment
        filepath = os.path.join("..", "examples", "%s.%s" % (example_id, ext))
        filepath_full = os.path.join(os.path.dirname(fw.__self__.name), filepath)

        text, line_no = example_extract_docstring(filepath_full)

        for line in text.split("\n"):
            fw("%s\n" % (ident + line).rstrip())
        fw("\n")

        fw("%s.. literalinclude:: %s\n" % (ident, filepath))
        if line_no > 0:
            fw("%s   :lines: %d-\n" % (ident, line_no))
        fw("\n")
        EXAMPLE_SET_USED.add(example_id)
    else:
        if bpy.app.debug:
            BPY_LOGGER.debug("\tskipping example: " + example_id)

    # Support for numbered files bpy.types.Operator -> bpy.types.Operator.1.py
    i = 1
    while True:
        example_id_num = "%s.%d" % (example_id, i)
        if example_id_num in EXAMPLE_SET:
            write_example_ref(ident, fw, example_id_num, ext)
            i += 1
        else:
            break


def write_indented_lines(ident, fn, text, strip=True):
    '''
    Apply same indentation to all lines in a multilines text.
    '''
    if text is None:
        return

    lines = text.split("\n")

    # strip empty lines from the start/end
    while lines and not lines[0].strip():
        del lines[0]
    while lines and not lines[-1].strip():
        del lines[-1]

    if strip:
        # set indentation to <indent>
        ident_strip = 1000
        for l in lines:
            if l.strip():
                ident_strip = min(ident_strip, len(l) - len(l.lstrip()))
        for l in lines:
            fn(ident + l[ident_strip:] + "\n")
    else:
        # add <indent> number of blanks to the current indentation
        for l in lines:
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
        write_indented_lines(ident + "   ", fw, py_func.__doc__)
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
        fw("\n")
    elif type(descr) == MemberDescriptorType:  # same as above but use 'data'
        fw(ident + ".. data:: %s\n\n" % identifier)
        write_indented_lines(ident + "   ", fw, doc, False)
        fw("\n")
    elif type(descr) in (MethodDescriptorType, ClassMethodDescriptorType):
        write_indented_lines(ident, fw, doc, False)
        fw("\n")
    else:
        raise TypeError("type was not GetSetDescriptorType, MethodDescriptorType or ClassMethodDescriptorType")

    write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
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

    if is_class:
        write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    else:
        write_example_ref(ident + "   ", fw, module_name + "." + identifier)

    fw("\n")


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


def pymodule2sphinx(basepath, module_name, module, title):
    import types
    attribute_set = set()
    filepath = os.path.join(basepath, module_name + ".rst")

    module_all = getattr(module, "__all__", None)
    module_dir = sorted(dir(module))

    if module_all:
        module_dir = module_all

    file = open(filepath, "w", encoding="utf-8")

    fw = file.write

    fw(title_string("%s (%s)" % (title, module_name), "="))

    fw(".. module:: %s\n\n" % module_name)

    if module.__doc__:
        # Note, may contain sphinx syntax, dont mangle!
        fw(module.__doc__.strip())
        fw("\n\n")

    write_example_ref("", fw, module_name)

    # write submodules
    # we could also scan files but this ensures __all__ is used correctly
    if module_all is not None:
        submod_name = None
        submod = None
        submod_ls = []
        for submod_name in module_all:
            ns = {}
            exec_str = "from %s import %s as submod" % (module.__name__, submod_name)
            exec(exec_str, ns, ns)
            submod = ns["submod"]
            if type(submod) == types.ModuleType:
                submod_ls.append((submod_name, submod))

        del submod_name
        del submod

        if submod_ls:
            fw(".. toctree::\n")
            fw("   :maxdepth: 1\n\n")

            for submod_name, submod in submod_ls:
                submod_name_full = "%s.%s" % (module_name, submod_name)
                fw("   %s.rst\n\n" % submod_name_full)

                pymodule2sphinx(basepath, submod_name_full, submod, "%s submodule" % module_name)
        del submod_ls
    # done writing submodules!

    # write members of the module
    # only tested with PyStructs which are not exactly modules
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue
        # naughty, we also add getset's into PyStructs, this is not typical py but also not incorrect.

        # type_name is only used for examples and messages
        type_name = str(type(module)).strip("<>").split(" ", 1)[-1][1:-1]  # "<class 'bpy.app.handlers'>" --> bpy.app.handlers
        if type(descr) == types.GetSetDescriptorType:
            py_descr2sphinx("", fw, descr, module_name, type_name, key)
            attribute_set.add(key)
    descr_sorted = []
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue

        if type(descr) == MemberDescriptorType:
            if descr.__doc__:
                value = getattr(module, key, None)

                value_type = type(value)
                descr_sorted.append((key, descr, value, type(value)))
    # sort by the valye type
    descr_sorted.sort(key=lambda descr_data: str(descr_data[3]))
    for key, descr, value, value_type in descr_sorted:

        # must be documented as a submodule
        if is_struct_seq(value):
            continue

        type_name = value_type.__name__
        py_descr2sphinx("", fw, descr, module_name, type_name, key)

        attribute_set.add(key)

    del key, descr, descr_sorted

    classes = []
    submodules = []

    # use this list so we can sort by type
    module_dir_value_type = []

    for attribute in module_dir:
        if attribute.startswith("_"):
            continue

        if attribute in attribute_set:
            continue

        if attribute.startswith("n_"):  # annoying exception, needed for bpy.app
            continue

        # workaround for bpy.app documenting .index() and .count()
        if isinstance(module, tuple) and hasattr(tuple, attribute):
            continue

        value = getattr(module, attribute)

        module_dir_value_type.append((attribute, value, type(value)))

    # sort by str of each type
    # this way lists, functions etc are grouped.
    module_dir_value_type.sort(key=lambda triple: str(triple[2]))

    for attribute, value, value_type in module_dir_value_type:
        if value_type == types.FunctionType:
            pyfunc2sphinx("", fw, attribute, value, is_class=False)
        elif value_type in (types.BuiltinMethodType, types.BuiltinFunctionType):  # both the same at the moment but to be future proof
            # note: can't get args from these, so dump the string as is
            # this means any module used like this must have fully formatted docstrings.
            py_c_func2sphinx("", fw, module_name, None, attribute, value, is_class=False)
        elif value_type == type:
            classes.append((attribute, value))
        elif issubclass(value_type, types.ModuleType):
            submodules.append((attribute, value))
        elif value_type in (bool, int, float, str, tuple):
            # constant, not much fun we can do here except to list it.
            # TODO, figure out some way to document these!
            #fw(".. data:: %s\n\n" % attribute)
            write_indented_lines("   ", fw, "constant value %s" % repr(value), False)
            fw("\n")
        else:
            BPY_LOGGER.debug("\tnot documenting %s.%s of %r type" % (module_name, attribute, value_type.__name__))
            continue

        attribute_set.add(attribute)
        # TODO, more types...
    del module_dir_value_type

    # TODO, bpy_extras does this already, mathutils not.
    '''
    if submodules:
        fw("\n"
           "**********\n"
           "Submodules\n"
           "**********\n"
           "\n"
           )
        for attribute, submod in submodules:
            fw("* :mod:`%s.%s`\n" % (module_name, attribute))
        fw("\n")
    '''

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


def pycontext2sphinx(basepath):
    # Only use once. very irregular

    filepath = os.path.join(basepath, "bpy.context.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write
    fw(title_string("Context Access (bpy.context)", "="))
    fw(".. module:: bpy.context\n")
    fw("\n")
    fw("The context members available depend on the area of blender which is currently being accessed.\n")
    fw("\n")
    fw("Note that all context values are readonly, but may be modified through the data api or by running operators\n\n")

    # nasty, get strings directly from blender because there is no other way to get it
    import ctypes

    context_strings = (
        "screen_context_dir",
        "view3d_context_dir",
        "buttons_context_dir",
        "image_context_dir",
        "node_context_dir",
        "text_context_dir",
    )

    # Changes in blender will force errors here
    type_map = {
        "active_base": ("ObjectBase", False),
        "active_bone": ("Bone", False),
        "active_object": ("Object", False),
        "active_operator": ("Operator", False),
        "active_pose_bone": ("PoseBone", False),
        "armature": ("Armature", False),
        "bone": ("Bone", False),
        "brush": ("Brush", False),
        "camera": ("Camera", False),
        "cloth": ("ClothModifier", False),
        "collision": ("CollisionModifier", False),
        "curve": ("Curve", False),
        "dynamic_paint": ("DynamicPaintModifier", False),
        "edit_bone": ("EditBone", False),
        "edit_image": ("Image", False),
        "edit_object": ("Object", False),
        "edit_text": ("Text", False),
        "editable_bones": ("EditBone", True),
        "fluid": ("FluidSimulationModifier", False),
        "image_paint_object": ("Object", False),
        "lamp": ("Lamp", False),
        "lattice": ("Lattice", False),
        "material": ("Material", False),
        "material_slot": ("MaterialSlot", False),
        "mesh": ("Mesh", False),
        "meta_ball": ("MetaBall", False),
        "object": ("Object", False),
        "particle_edit_object": ("Object", False),
        "particle_system": ("ParticleSystem", False),
        "particle_system_editable": ("ParticleSystem", False),
        "pose_bone": ("PoseBone", False),
        "scene": ("Scene", False),
        "sculpt_object": ("Object", False),
        "selectable_bases": ("ObjectBase", True),
        "selectable_objects": ("Object", True),
        "selected_bases": ("ObjectBase", True),
        "selected_bones": ("Bone", True),
        "selected_editable_bases": ("ObjectBase", True),
        "selected_editable_bones": ("Bone", True),
        "selected_editable_objects": ("Object", True),
        "selected_editable_sequences": ("Sequence", True),
        "selected_nodes": ("Node", True),
        "selected_objects": ("Object", True),
        "selected_pose_bones": ("PoseBone", True),
        "selected_sequences": ("Sequence", True),
        "sequences": ("Sequence", True),
        "smoke": ("SmokeModifier", False),
        "soft_body": ("SoftBodyModifier", False),
        "speaker": ("Speaker", False),
        "texture": ("Texture", False),
        "texture_slot": ("MaterialTextureSlot", False),
        "texture_user": ("ID", False),
        "vertex_paint_object": ("Object", False),
        "visible_bases": ("ObjectBase", True),
        "visible_bones": ("Object", True),
        "visible_objects": ("Object", True),
        "visible_pose_bones": ("PoseBone", True),
        "weight_paint_object": ("Object", False),
        "world": ("World", False),
    }

    unique = set()
    blend_cdll = ctypes.CDLL("")
    for ctx_str in context_strings:
        subsection = "%s Context" % ctx_str.split("_")[0].title()
        fw("\n%s\n%s\n\n" % (subsection, (len(subsection) * '-')))

        attr = ctypes.addressof(getattr(blend_cdll, ctx_str))
        c_char_p_p = ctypes.POINTER(ctypes.c_char_p)
        char_array = c_char_p_p.from_address(attr)
        i = 0
        while char_array[i] is not None:
            member = ctypes.string_at(char_array[i]).decode(encoding="ascii")
            fw(".. data:: %s\n\n" % member)
            member_type, is_seq = type_map[member]
            fw("   :type: %s :class:`bpy.types.%s`\n\n" % ("sequence of " if is_seq else "", member_type))
            unique.add(member)
            i += 1

    # generate typemap...
    # for member in sorted(unique):
    #     print('        "%s": ("", False),' % member)
    if len(type_map) > len(unique):
        raise Exception("Some types are not used: %s" % str([member for member in type_map if member not in unique]))
    else:
        pass  # will have raised an error above

    file.close()


def pyrna_enum2sphinx(prop, use_empty_descriptions=False):
    """ write a bullet point list of enum + descrptons
    """

    if use_empty_descriptions:
        ok = True
    else:
        ok = False
        for identifier, name, description in prop.enum_items:
            if description:
                ok = True
                break

    if ok:
        return "".join(["* ``%s`` %s.\n" %
                        (identifier,
                         ", ".join(val for val in (name, description) if val),
                         )
                        for identifier, name, description in prop.enum_items
                        ])
    else:
        return ""


def pyrna2sphinx(basepath):
    """ bpy.types and bpy.ops
    """
    structs, funcs, ops, props = rna_info.BuildRNAInfo()
    if FILTER_BPY_TYPES is not None:
        structs = {k: v for k, v in structs.items() if k[1] in FILTER_BPY_TYPES}

    if FILTER_BPY_OPS is not None:
        ops = {k: v for k, v in ops.items() if v.module_name in FILTER_BPY_OPS}

    def write_param(ident, fw, prop, is_return=False):
        if is_return:
            id_name = "return"
            id_type = "rtype"
            kwargs = {"as_ret": True}
            identifier = ""
        else:
            id_name = "arg"
            id_type = "type"
            kwargs = {"as_arg": True}
            identifier = " %s" % prop.identifier

        kwargs["class_fmt"] = ":class:`%s`"

        kwargs["collection_id"] = _BPY_PROP_COLLECTION_ID

        type_descr = prop.get_type_description(**kwargs)

        enum_text = pyrna_enum2sphinx(prop)

        if prop.name or prop.description or enum_text:
            fw(ident + ":%s%s:\n\n" % (id_name, identifier))

            if prop.name or prop.description:
                fw(ident + "   " + ", ".join(val for val in (prop.name, prop.description) if val) + "\n\n")

            # special exception, cant use genric code here for enums
            if enum_text:
                write_indented_lines(ident + "   ", fw, enum_text)
                fw("\n")
            del enum_text
            # end enum exception

        fw(ident + ":%s%s: %s\n" % (id_type, identifier, type_descr))

    def write_struct(struct):
        #if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #    return

        #if not struct.identifier == "Object":
        #    return

        filepath = os.path.join(basepath, "bpy.types.%s.rst" % struct.identifier)
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        base_id = getattr(struct.base, "identifier", "")
        struct_id = struct.identifier

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            title = "%s(%s)" % (struct_id, base_id)
        else:
            title = struct_id

        fw(title_string(title, "="))

        fw(".. module:: bpy.types\n\n")

        # docs first?, ok
        write_example_ref("", fw, "bpy.types.%s" % struct_id)

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
            fw(".. class:: %s(%s)\n\n" % (struct_id, base_id))
        else:
            fw(".. class:: %s\n\n" % struct_id)

        fw("   %s\n\n" % struct.description)

        # properties sorted in alphabetical order
        sorted_struct_properties = struct.properties[:]
        sorted_struct_properties.sort(key=lambda prop: prop.identifier)

        # support blacklisting props
        struct_blacklist = RNA_BLACKLIST.get(struct_id, ())

        for prop in sorted_struct_properties:

            # support blacklisting props
            if prop.identifier in struct_blacklist:
                continue

            type_descr = prop.get_type_description(class_fmt=":class:`%s`", collection_id=_BPY_PROP_COLLECTION_ID)
            # readonly properties use "data" directive, variables properties use "attribute" directive
            if 'readonly' in type_descr:
                fw("   .. data:: %s\n\n" % prop.identifier)
            else:
                fw("   .. attribute:: %s\n\n" % prop.identifier)
            if prop.description:
                fw("      %s\n\n" % prop.description)

            # special exception, cant use genric code here for enums
            if prop.type == "enum":
                enum_text = pyrna_enum2sphinx(prop)
                if enum_text:
                    write_indented_lines("      ", fw, enum_text)
                    fw("\n")
                del enum_text
            # end enum exception

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
                    # TODO, pyrna_enum2sphinx for multiple return values... actually dont think we even use this but still!!!
                    type_descr = prop.get_type_description(as_ret=True, class_fmt=":class:`%s`", collection_id=_BPY_PROP_COLLECTION_ID)
                    descr = prop.description
                    if not descr:
                        descr = prop.name
                    fw("         `%s`, %s, %s\n\n" % (prop.identifier, descr, type_descr))

            write_example_ref("      ", fw, "bpy.types." + struct_id + "." + func.identifier)

            fw("\n")

        # python methods
        py_funcs = struct.get_py_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            pyfunc2sphinx("   ", fw, identifier, py_func, is_class=True)
        del py_funcs, py_func

        py_funcs = struct.get_py_c_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            py_c_func2sphinx("   ", fw, "bpy.types", struct_id, identifier, py_func, is_class=True)

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

        # docs last?, disable for now
        # write_example_ref("", fw, "bpy.types.%s" % struct_id)
        file.close()

    if "bpy.types" not in EXCLUDE_MODULES:
        for struct in structs.values():
            # TODO, rna_info should filter these out!
            if "_OT_" in struct.identifier:
                continue
            write_struct(struct)

        def fake_bpy_type(class_value, class_name, descr_str, use_subclasses=True):
            filepath = os.path.join(basepath, "bpy.types.%s.rst" % class_name)
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            fw(title_string(class_name, "="))

            fw(".. module:: bpy.types\n")
            fw("\n")

            if use_subclasses:
                subclass_ids = [s.identifier for s in structs.values() if s.base is None if not rna_info.rna_id_ignore(s.identifier)]
                if subclass_ids:
                    fw("subclasses --- \n" + ", ".join((":class:`%s`" % s) for s in sorted(subclass_ids)) + "\n\n")

            fw(".. class:: %s\n\n" % class_name)
            fw("   %s\n\n" % descr_str)
            fw("   .. note::\n\n")
            fw("      Note that bpy.types.%s is not actually available from within blender, it only exists for the purpose of documentation.\n\n" % class_name)

            descr_items = [(key, descr) for key, descr in sorted(class_value.__dict__.items()) if not key.startswith("__")]

            for key, descr in descr_items:
                if type(descr) == MethodDescriptorType:  # GetSetDescriptorType, GetSetDescriptorType's are not documented yet
                    py_descr2sphinx("   ", fw, descr, "bpy.types", class_name, key)

            for key, descr in descr_items:
                if type(descr) == GetSetDescriptorType:
                    py_descr2sphinx("   ", fw, descr, "bpy.types", class_name, key)
            file.close()

        # write fake classes
        if _BPY_STRUCT_FAKE:
            class_value = bpy.types.Struct.__bases__[0]
            fake_bpy_type(class_value, _BPY_STRUCT_FAKE, "built-in base class for all classes in bpy.types.", use_subclasses=True)

        if _BPY_PROP_COLLECTION_FAKE:
            class_value = bpy.data.objects.__class__
            fake_bpy_type(class_value, _BPY_PROP_COLLECTION_FAKE, "built-in class used for all collections.", use_subclasses=False)

    # operators
    def write_ops():
        API_BASEURL = "http://svn.blender.org/svnroot/bf-blender/trunk/blender/release/scripts"
        API_BASEURL_ADDON = "http://svn.blender.org/svnroot/bf-extensions/trunk/py/scripts"
        API_BASEURL_ADDON_CONTRIB = "http://svn.blender.org/svnroot/bf-extensions/contrib/py/scripts"

        op_modules = {}
        for op in ops.values():
            op_modules.setdefault(op.module_name, []).append(op)
        del op

        for op_module_name, ops_mod in op_modules.items():
            filepath = os.path.join(basepath, "bpy.ops.%s.rst" % op_module_name)
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            title = "%s Operators" % op_module_name.replace("_", " ").title()

            fw(title_string(title, "="))

            fw(".. module:: bpy.ops.%s\n\n" % op_module_name)

            ops_mod.sort(key=lambda op: op.func_name)

            for op in ops_mod:
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
                    if location[0].startswith("addons_contrib" + os.sep):
                        url_base = API_BASEURL_ADDON_CONTRIB
                    elif location[0].startswith("addons" + os.sep):
                        url_base = API_BASEURL_ADDON
                    else:
                        url_base = API_BASEURL

                    fw("   :file: `%s <%s/%s>`_:%d\n\n" % (location[0],
                                                           url_base,
                                                           location[0],
                                                           location[1]))

            file.close()

    if "bpy.ops" not in EXCLUDE_MODULES:
        write_ops()


def write_sphinx_conf_py(basepath):
    '''
    Write sphinx's conf.py
    '''
    filepath = os.path.join(basepath, "conf.py")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw("project = 'Blender'\n")
    # fw("master_doc = 'index'\n")
    fw("copyright = u'Blender Foundation'\n")
    fw("version = '%s - API'\n" % BLENDER_VERSION_DOTS)
    fw("release = '%s - API'\n" % BLENDER_VERSION_DOTS)

    if ARGS.sphinx_theme != 'default':
        fw("html_theme = '%s'\n" % ARGS.sphinx_theme)

    if ARGS.sphinx_theme in SPHINX_THEMES['bf']:
        fw("html_theme_path = ['../']\n")
        # copied with the theme, exclude else we get an error [#28873]
        fw("html_favicon = 'favicon.ico'\n")    # in <theme>/static/

    # not helpful since the source is generated, adds to upload size.
    fw("html_copy_source = False\n")
    fw("\n")

    # needed for latex, pdf gen
    fw("latex_documents = [ ('contents', 'contents.tex', 'Blender Index', 'Blender Foundation', 'manual'), ]\n")
    fw("latex_paper_size = 'a4paper'\n")
    file.close()


def write_rst_contents(basepath):
    '''
    Write the rst file of the main page, needed for sphinx (index.html)
    '''
    filepath = os.path.join(basepath, "contents.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw(title_string("Blender Documentation Contents", "%", double=True))
    fw("\n")
    fw("Welcome, this document is an API reference for Blender %s, built %s.\n" % (BLENDER_VERSION_DOTS, BLENDER_DATE))
    fw("\n")

    # fw("`A PDF version of this document is also available <%s>`_\n" % BLENDER_PDF_FILENAME)
    fw("`A compressed ZIP file of this site is available <%s>`_\n" % BLENDER_ZIP_FILENAME)

    fw("\n")

    if not EXCLUDE_INFO_DOCS:
        fw(title_string("Blender/Python Documentation", "=", double=True))

        fw(".. toctree::\n")
        fw("   :maxdepth: 1\n\n")
        for info, info_desc in INFO_DOCS:
            fw("   %s <%s>\n\n" % (info_desc, info))
        fw("\n")

    fw(title_string("Application Modules", "=", double=True))
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")

    app_modules = [
        "bpy.context",  # note: not actually a module
        "bpy.data",     # note: not actually a module
        "bpy.ops",
        "bpy.types",

        # py modules
        "bpy.utils",
        "bpy.path",
        "bpy.app",
        "bpy.app.handlers",

        # C modules
        "bpy.props"
    ]
    for mod in app_modules:
        if mod not in EXCLUDE_MODULES:
            fw("   %s\n\n" % mod)

    fw(title_string("Standalone Modules", "=", double=True))
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")

    standalone_modules = [
        # mathutils
        "mathutils", "mathutils.geometry", "mathutils.noise",
        # misc
        "bgl", "blf", "gpu", "aud", "bpy_extras",
        # bmesh
        "bmesh", "bmesh.types", "bmesh.utils"
    ]
    for mod in standalone_modules:
        if mod not in EXCLUDE_MODULES:
            fw("   %s\n\n" % mod)

    # game engine
    if "bge" not in EXCLUDE_MODULES:
        fw(title_string("Game Engine Modules", "=", double=True))
        fw(".. toctree::\n")
        fw("   :maxdepth: 1\n\n")
        fw("   bge.types.rst\n\n")
        fw("   bge.logic.rst\n\n")
        fw("   bge.render.rst\n\n")
        fw("   bge.texture.rst\n\n")
        fw("   bge.events.rst\n\n")
        fw("   bge.constraints.rst\n\n")

    # rna generated change log
    fw(title_string("API Info", "=", double=True))
    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n\n")
    fw("   change_log.rst\n\n")

    fw("\n")
    fw("\n")
    fw(".. note:: The Blender Python API has areas which are still in development.\n")
    fw("   \n")
    fw("   The following areas are subject to change.\n")
    fw("      * operator behavior, names and arguments\n")
    fw("      * mesh creation and editing functions\n")
    fw("   \n")
    fw("   These parts of the API are relatively stable and are unlikely to change significantly\n")
    fw("      * data API, access to attributes of blender data such as mesh verts, material color, timeline frames and scene objects\n")
    fw("      * user interface functions for defining buttons, creation of menus, headers, panels\n")
    fw("      * render engine integration\n")
    fw("      * modules: bgl, mathutils & game engine.\n")
    fw("\n")

    file.close()


def write_rst_bpy(basepath):
    '''
    Write rst file of bpy module (disabled by default)
    '''
    if ARGS.bpy:
        filepath = os.path.join(basepath, "bpy.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        fw("\n")

        title = ":mod:`bpy` --- Blender Python Module"

        fw(title_string(title, "="))

        fw(".. module:: bpy.types\n\n")
        file.close()


def write_rst_types_index(basepath):
    '''
    Write the rst file of bpy.types module (index)
    '''
    if "bpy.types" not in EXCLUDE_MODULES:
        filepath = os.path.join(basepath, "bpy.types.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Types (bpy.types)", "="))
        fw(".. toctree::\n")
        fw("   :glob:\n\n")
        fw("   bpy.types.*\n\n")
        file.close()


def write_rst_ops_index(basepath):
    '''
    Write the rst file of bpy.ops module (index)
    '''
    if "bpy.ops" not in EXCLUDE_MODULES:
        filepath = os.path.join(basepath, "bpy.ops.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Operators (bpy.ops)", "="))
        write_example_ref("", fw, "bpy.ops")
        fw(".. toctree::\n")
        fw("   :glob:\n\n")
        fw("   bpy.ops.*\n\n")
        file.close()


def write_rst_data(basepath):
    '''
    Write the rst file of bpy.data module
    '''
    if "bpy.data" not in EXCLUDE_MODULES:
        # not actually a module, only write this file so we
        # can reference in the TOC
        filepath = os.path.join(basepath, "bpy.data.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Data Access (bpy.data)", "="))
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


def write_rst_importable_modules(basepath):
    '''
    Write the rst files of importable modules
    '''
    importable_modules = {
        # python_modules
        "bpy.path"          : "Path Utilities",
        "bpy.utils"         : "Utilities",
        "bpy_extras"        : "Extra Utilities",

        # C_modules
        "aud"               : "Audio System",
        "blf"               : "Font Drawing",
        "bmesh"             : "BMesh Module",
        "bmesh.types"       : "BMesh Types",
        "bmesh.utils"       : "BMesh Utilities",
        "bpy.app"           : "Application Data",
        "bpy.app.handlers"  : "Application Handlers",
        "bpy.props"         : "Property Definitions",
        "mathutils"         : "Math Types & Utilities",
        "mathutils.geometry": "Geometry Utilities",
        "mathutils.noise"   : "Noise Utilities",
    }
    for mod_name, mod_descr in importable_modules.items():
        if mod_name not in EXCLUDE_MODULES:
            module = __import__(mod_name,
                                fromlist=[mod_name.rsplit(".", 1)[-1]])
            pymodule2sphinx(basepath, mod_name, module, mod_descr)


def copy_handwritten_rsts(basepath):

    # info docs
    if not EXCLUDE_INFO_DOCS:
        for info, info_desc in INFO_DOCS:
            shutil.copy2(os.path.join(RST_DIR, info), basepath)

    # TODO put this docs in blender's code and use import as per modules above
    handwritten_modules = [
        "bge.types",
        "bge.logic",
        "bge.render",
        "bge.texture",
        "bge.events",
        "bge.constraints",
        "bgl",  # "Blender OpenGl wrapper"
        "gpu",  # "GPU Shader Module"

        # includes...
        "include__bmesh",
    ]
    for mod_name in handwritten_modules:
        if mod_name not in EXCLUDE_MODULES:
            # copy2 keeps time/date stamps
            shutil.copy2(os.path.join(RST_DIR, "%s.rst" % mod_name), basepath)

    # changelog
    shutil.copy2(os.path.join(RST_DIR, "change_log.rst"), basepath)


def rna2sphinx(basepath):

    try:
        os.mkdir(basepath)
    except:
        pass

    # sphinx setup
    write_sphinx_conf_py(basepath)

    # main page
    write_rst_contents(basepath)

    # context
    if "bpy.context" not in EXCLUDE_MODULES:
        # one of a kind, context doc (uses ctypes to extract info!)
        # doesn't work on mac
        if PLATFORM != "darwin":
            pycontext2sphinx(basepath)

    # internal modules
    write_rst_bpy(basepath)                 # bpy, disabled by default
    write_rst_types_index(basepath)         # bpy.types
    write_rst_ops_index(basepath)           # bpy.ops
    pyrna2sphinx(basepath)                  # bpy.types.* and bpy.ops.*
    write_rst_data(basepath)                # bpy.data
    write_rst_importable_modules(basepath)

    # copy the other rsts
    copy_handwritten_rsts(basepath)


def align_sphinx_in_to_sphinx_in_tmp():
    '''
    Move changed files from SPHINX_IN_TMP to SPHINX_IN
    '''
    import filecmp

    sphinx_in_files = set(os.listdir(SPHINX_IN))
    sphinx_in_tmp_files = set(os.listdir(SPHINX_IN_TMP))

    # remove deprecated files that have been removed
    for f in sorted(sphinx_in_files):
        if f not in sphinx_in_tmp_files:
            BPY_LOGGER.debug("\tdeprecated: %s" % f)
            os.remove(os.path.join(SPHINX_IN, f))

    # freshen with new files.
    for f in sorted(sphinx_in_tmp_files):
        f_from = os.path.join(SPHINX_IN_TMP, f)
        f_to = os.path.join(SPHINX_IN, f)

        do_copy = True
        if f in sphinx_in_files:
            if filecmp.cmp(f_from, f_to):
                do_copy = False

        if do_copy:
            BPY_LOGGER.debug("\tupdating: %s" % f)
            shutil.copy(f_from, f_to)


def refactor_sphinx_log(sphinx_logfile):
    refactored_log = []
    with open(sphinx_logfile, "r", encoding="utf-8") as original_logfile:
        lines = set(original_logfile.readlines())
        for line in lines:
            if 'warning' in line.lower() or 'error' in line.lower():
                line = line.strip().split(None, 2)
                if len(line) == 3:
                    location, kind, msg = line
                    location = os.path.relpath(location, start=SPHINX_IN)
                    refactored_log.append((kind, location, msg))
    with open(sphinx_logfile, "w", encoding="utf-8") as refactored_logfile:
        for log in sorted(refactored_log):
            refactored_logfile.write("%-12s %s\n             %s\n" % log)


def main():

    # eventually, create the dirs
    for dir_path in [ARGS.output_dir, SPHINX_IN]:
        if not os.path.exists(dir_path):
            os.mkdir(dir_path)

    # eventually, log in files
    if ARGS.log:
        bpy_logfile = os.path.join(ARGS.output_dir, ".bpy.log")
        bpy_logfilehandler = logging.FileHandler(bpy_logfile, mode="w")
        bpy_logfilehandler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_logfilehandler)
    
        # using a FileHandler seems to disable the stdout, so we add a StreamHandler
        bpy_log_stdout_handler = logging.StreamHandler(stream=sys.stdout)
        bpy_log_stdout_handler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_log_stdout_handler)

    # in case of out-of-source build, copy the needed dirs
    if ARGS.output_dir != SCRIPT_DIR:
        # examples dir
        examples_dir_copy = os.path.join(ARGS.output_dir, "examples")
        if os.path.exists(examples_dir_copy):
            shutil.rmtree(examples_dir_copy, True)
        shutil.copytree(EXAMPLES_DIR,
                        examples_dir_copy,
                        ignore=shutil.ignore_patterns(*(".svn",)),
                        copy_function=shutil.copy)

        # eventually, copy the theme dir
        if ARGS.sphinx_theme in SPHINX_THEMES['bf']:
            if os.path.exists(SPHINX_THEME_DIR):
                shutil.rmtree(SPHINX_THEME_DIR, True)
            shutil.copytree(SPHINX_THEME_SVN_DIR,
                            SPHINX_THEME_DIR,
                            ignore=shutil.ignore_patterns(*(".svn",)),
                            copy_function=shutil.copy)

    # dump the api in rst files
    if os.path.exists(SPHINX_IN_TMP):
        shutil.rmtree(SPHINX_IN_TMP, True)

    rna2sphinx(SPHINX_IN_TMP)

    if ARGS.full_rebuild:
        # only for full updates
        shutil.rmtree(SPHINX_IN, True)
        shutil.copytree(SPHINX_IN_TMP,
                        SPHINX_IN,
                        copy_function=shutil.copy)
        if ARGS.sphinx_build and os.path.exists(SPHINX_OUT):
            shutil.rmtree(SPHINX_OUT, True)
        if ARGS.sphinx_build_pdf and os.path.exists(SPHINX_OUT_PDF):
            shutil.rmtree(SPHINX_OUT_PDF, True)
    else:
        # move changed files in SPHINX_IN
        align_sphinx_in_to_sphinx_in_tmp()

    # report which example files weren't used
    EXAMPLE_SET_UNUSED = EXAMPLE_SET - EXAMPLE_SET_USED
    if EXAMPLE_SET_UNUSED:
        BPY_LOGGER.debug("\nUnused examples found in '%s'..." % EXAMPLES_DIR)
        for f in sorted(EXAMPLE_SET_UNUSED):
            BPY_LOGGER.debug("    %s.py" % f)
        BPY_LOGGER.debug("  %d total\n" % len(EXAMPLE_SET_UNUSED))

    # eventually, build the html docs
    if ARGS.sphinx_build:
        import subprocess
        subprocess.call(SPHINX_BUILD)

        # sphinx-build log cleanup+sort
        if ARGS.log:
            if os.stat(SPHINX_BUILD_LOG).st_size:
                refactor_sphinx_log(SPHINX_BUILD_LOG)

    # eventually, build the pdf docs
    if ARGS.sphinx_build_pdf:
        import subprocess
        subprocess.call(SPHINX_BUILD_PDF)
        subprocess.call(SPHINX_MAKE_PDF, stdout=SPHINX_MAKE_PDF_STDOUT)

        # sphinx-build log cleanup+sort
        if ARGS.log:
            if os.stat(SPHINX_BUILD_PDF_LOG).st_size:
                refactor_sphinx_log(SPHINX_BUILD_PDF_LOG)

    # eventually, prepare the dir to be deployed online (REFERENCE_PATH)
    if ARGS.pack_reference:

        if ARGS.sphinx_build:
            # delete REFERENCE_PATH
            if os.path.exists(REFERENCE_PATH):
                shutil.rmtree(REFERENCE_PATH, True)

            # copy SPHINX_OUT to the REFERENCE_PATH
            ignores = ('.doctrees', 'objects.inv', '.buildinfo')
            shutil.copytree(SPHINX_OUT,
                            REFERENCE_PATH,
                            ignore=shutil.ignore_patterns(*ignores))
            shutil.copy(os.path.join(REFERENCE_PATH, "contents.html"),
                        os.path.join(REFERENCE_PATH, "index.html"))

            # zip REFERENCE_PATH
            basename = os.path.join(ARGS.output_dir, REFERENCE_NAME)
            tmp_path = shutil.make_archive(basename, 'zip',
                                           root_dir=ARGS.output_dir, 
                                           base_dir=REFERENCE_NAME)
            final_path = os.path.join(REFERENCE_PATH, BLENDER_ZIP_FILENAME)
            os.rename(tmp_path, final_path)

        if ARGS.sphinx_build_pdf:
            # copy the pdf to REFERENCE_PATH
            shutil.copy(os.path.join(SPHINX_OUT_PDF, "contents.pdf"),
                        os.path.join(REFERENCE_PATH, BLENDER_PDF_FILENAME))

    sys.exit()


if __name__ == '__main__':
    main()
