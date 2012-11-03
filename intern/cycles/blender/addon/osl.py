#
# Copyright 2011, Blender Foundation.
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

# <pep8 compliant>

import bpy, _cycles, os, tempfile

# compile .osl file with given filepath to temporary .oso file
def osl_compile(input_path, report):
    output_file = tempfile.NamedTemporaryFile(mode='w', suffix=".oso", delete=False)
    output_path = output_file.name
    output_file.close()

    ok = _cycles.osl_compile(input_path, output_path)

    if ok:
        report({'INFO'}, "OSL shader compilation succeeded")

    return ok, output_path

# compile and update shader script node
def update_script_node(node, report):
    import os, shutil

    if node.mode == 'EXTERNAL':
        # compile external script file
        script_path = bpy.path.abspath(node.filepath)
        script_path_noext, script_ext = os.path.splitext(script_path)

        if script_ext == ".oso":
            # it's a .oso file, no need to compile
            ok, oso_path = True, script_path
            oso_file_remove = False
        elif script_ext == ".osl":
            # compile .osl file
            ok, oso_path = osl_compile(script_path, report)
            oso_file_remove = True

            if ok:
                # copy .oso from temporary path to .osl directory
                dst_path = script_path_noext + ".oso"
                try:
                    shutil.copy2(oso_path, dst_path)
                except:
                    report({'ERROR'}, "Failed to write .oso file next to external .osl file at " + dst_path)
        elif os.path.dirname(node.filepath) == "":
            # module in search path
            oso_path = node.filepath
            oso_file_remove = False
            ok = True
        else:
            # unknown
            report({'ERROR'}, "External shader script must have .osl or .oso extension, or be a module name")
            ok = False

        if ok:
            node.bytecode = ""
            node.bytecode_hash = ""

    elif node.mode == 'INTERNAL' and node.script:
        # internal script, we will store bytecode in the node
        script = node.script
        osl_path = bpy.path.abspath(script.filepath)

        if script.is_in_memory or script.is_dirty or script.is_modified or not os.path.exists(osl_path):
            # write text datablock contents to temporary file
            osl_file = tempfile.NamedTemporaryFile(mode='w', suffix=".osl", delete=True)
            osl_file.write(script.as_string())
            osl_file.flush()
            ok, oso_path = osl_compile(osl_file.name, report)
            oso_file_remove = False
            osl_file.close()
        else:
            # compile text datablock from disk directly
            ok, oso_path = osl_compile(osl_path, report)
            oso_file_remove = False

        if ok:
            # read bytecode
            try:
                oso = open(oso_path, 'r')
                node.bytecode = oso.read()
                oso.close()
            except:
                report({'ERROR'}, "Can't read OSO bytecode to store in node at " + oso_path)
                ok = False
    
    else:
        report({'WARNING'}, "No text or file specified in node, nothing to compile")
        return

    if ok:
        # now update node with new sockets
        ok = _cycles.osl_update_node(node.id_data.as_pointer(), node.as_pointer(), oso_path)

        if not ok:
            report({'ERROR'}, "OSL query failed to open " + oso_path)
    else:
        report({'ERROR'}, "OSL script compilation failed, see console for errors")

    # remove temporary oso file
    if oso_file_remove:
        try:
            os.remove(oso_path)
        except:
            pass

    return ok

