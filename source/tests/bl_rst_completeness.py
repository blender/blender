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

# run this script in the game engine.
# or on the command line with...
#  ./blender.bin --background -noaudio --python source/tests/bl_rst_completeness.py

# Paste this into the bge and run on an always actuator.
'''
filepath = "/dsk/data/src/blender/blender/source/tests/bl_rst_completeness.py"
exec(compile(open(filepath).read(), filepath, 'exec'))
'''

import os

THIS_DIR = os.path.dirname(__file__)
RST_DIR = os.path.normpath(os.path.join(THIS_DIR, "..", "..", "doc", "python_api", "rst"))

import sys
sys.path.append(THIS_DIR)

import rst_to_doctree_mini

try:
    import bge
except:
    bge = None

# (file, module)
modules = (
    ("bge.constraints.rst", "bge.constraints", False),
    ("bge.events.rst", "bge.events", False),
    ("bge.logic.rst", "bge.logic", False),
    ("bge.render.rst", "bge.render", False),
    ("bge.texture.rst", "bge.texture", False),
    ("bge.types.rst", "bge.types", False),

    ("bgl.rst", "bgl", True),
    ("gpu.rst", "gpu", False),
)


def is_directive_pydata(filepath, directive):
    if directive.type in {"function", "method", "class", "attribute", "data"}:
        return True
    elif directive.type in {"module", "note", "warning", "code-block", "hlist", "seealso"}:
        return False
    elif directive.type in {"literalinclude"}:  # TODO
        return False
    else:
        print(directive_to_str(filepath, directive), end=" ")
        print("unknown directive type %r" % directive.type)
        return False


def directive_to_str(filepath, directive):
    return "%s:%d:%d:" % (filepath, directive.line + 1, directive.indent)


def directive_members_dict(filepath, directive_members):
    return {directive.value_strip: directive for directive in directive_members
            if is_directive_pydata(filepath, directive)}


def module_validate(filepath, mod, mod_name, doctree, partial_ok):
    # RST member missing from MODULE ???
    for directive in doctree:
        # print(directive.type)
        if is_directive_pydata(filepath, directive):
            attr = directive.value_strip
            has_attr = hasattr(mod, attr)
            ok = False
            if not has_attr:
                # so we can have glNormal docs cover glNormal3f
                if partial_ok:
                    for s in dir(mod):
                        if s.startswith(attr):
                            ok = True
                            break

                if not ok:
                    print(directive_to_str(filepath, directive), end=" ")
                    print("rst contains non existing member %r" % attr)

            # if its a class, scan down the class...
            # print(directive.type)
            if has_attr:
                if directive.type == "class":
                    cls = getattr(mod, attr)
                    # print("directive:      ", directive)
                    for directive_child in directive.members:
                        # print("directive_child: ", directive_child)
                        if is_directive_pydata(filepath, directive_child):
                            attr_child = directive_child.value_strip
                            if attr_child not in cls.__dict__:
                                attr_id = "%s.%s" % (attr, attr_child)
                                print(directive_to_str(filepath, directive_child), end=" ")
                                print("rst contains non existing class member %r" % attr_id)

    # MODULE member missing from RST ???
    doctree_dict = directive_members_dict(filepath, doctree)
    for attr in dir(mod):
        if attr.startswith("_"):
            continue

        directive = doctree_dict.get(attr)
        if directive is None:
            print("module contains undocumented member %r from %r" % ("%s.%s" % (mod_name, attr), filepath))
        else:
            if directive.type == "class":
                directive_dict = directive_members_dict(filepath, directive.members)
                cls = getattr(mod, attr)
                for attr_child in cls.__dict__.keys():
                    if attr_child.startswith("_"):
                        continue
                    if attr_child not in directive_dict:
                        attr_id = "%s.%s.%s" % (mod_name, attr, attr_child), filepath
                        print("module contains undocumented member %r from %r" % attr_id)


def main():

    if bge is None:
        print("Skipping BGE modules!")

    for filename, modname, partial_ok in modules:
        if bge is None and modname.startswith("bge"):
            continue

        filepath = os.path.join(RST_DIR, filename)
        if not os.path.exists(filepath):
            raise Exception("%r not found" % filepath)

        doctree = rst_to_doctree_mini.parse_rst_py(filepath)
        __import__(modname)
        mod = sys.modules[modname]

        module_validate(filepath, mod, modname, doctree, partial_ok)


if __name__ == "__main__":
    main()
