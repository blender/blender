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

import os


def blend_list(path):
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for filename in filenames:
            if filename.lower().endswith(".blend"):
                filepath = os.path.join(dirpath, filename)
                yield filepath


def generate(dirpath, random_order, **kwargs):
    files = list(blend_list(dirpath))
    if random_order:
        import random
        random.shuffle(files)
    else:
        files.sort()

    config = []
    for f in files:
        defaults = kwargs.copy()
        defaults["file"] = f
        config.append(defaults)

    return config, dirpath


def as_string(dirpath, random_order, exit, **kwargs):
    """ Config loader is in demo_mode.py
    """
    cfg, dirpath = generate(dirpath, random_order, **kwargs)

    # hint for reader, can be used if files are not found.
    cfg_str = [
        "# generated file\n",
        "\n",
        "# edit the search path so other systems may find the files below\n",
        "# based on name only if the absolute paths cant be found\n",
        "# Use '//' for current blend file path.\n",
        "\n",
        "search_path = %r\n" % dirpath,
        "\n",
        "exit = %r\n" % exit,
        "\n",
        ]

    # All these work but use nicest formatting!
    if 0:  # works but not nice to edit.
        cfg_str += ["config = %r" % cfg]
    elif 0:
        import pprint
        cfg_str += ["config = %s" % pprint.pformat(cfg, indent=0, width=120)]
    elif 0:
        cfg_str += [("config = %r" % cfg).replace("{", "\n    {")]
    else:
        import pprint

        def dict_as_kw(d):
            return "dict(%s)" % ", ".join(("%s=%s" % (k, pprint.pformat(v))) for k, v in sorted(d.items()))
        ident = "    "
        cfg_str += ["config = [\n"]
        for cfg_item in cfg:
            cfg_str += ["%s%s,\n" % (ident, dict_as_kw(cfg_item))]
        cfg_str += ["%s]\n\n" % ident]

    return "".join(cfg_str), dirpath
