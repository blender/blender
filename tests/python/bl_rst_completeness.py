# SPDX-FileCopyrightText: 2012-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# run this script in the game engine.
# or on the command line with...
#  ./blender.bin --background --python tests/python/bl_rst_completeness.py
'''
filepath = "/src/blender/tests/python/bl_rst_completeness.py"
exec(compile(open(filepath).read(), filepath, 'exec'))
'''

import os

THIS_DIR = os.path.dirname(__file__)
RST_DIR = os.path.normpath(os.path.join(THIS_DIR, "..", "..", "doc", "python_api", "rst"))

import sys
sys.path.append(THIS_DIR)

import rst_to_doctree_mini


# (file, module)
modules = (
    ("bgl.rst", "bgl", True),
    ("gpu.rst", "gpu", False),
)


def is_directive_pydata(filepath, directive):
    if directive.type in {"function", "method", "class", "attribute", "data"}:
        return True
    elif directive.type in {"module", "note", "warning", "code-block", "hlist", "seealso"}:
        return False
    elif directive.type == "literalinclude":  # TODO
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

    for filename, modname, partial_ok in modules:
        filepath = os.path.join(RST_DIR, filename)
        if not os.path.exists(filepath):
            raise Exception("%r not found" % filepath)

        doctree = rst_to_doctree_mini.parse_rst_py(filepath)
        __import__(modname)
        mod = sys.modules[modname]

        module_validate(filepath, mod, modname, doctree, partial_ok)


if __name__ == "__main__":
    main()
