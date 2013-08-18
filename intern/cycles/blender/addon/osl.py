#
# Copyright 2011-2013 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License
#

# <pep8 compliant>

import bpy
import _cycles


def osl_compile(input_path, report):
    """compile .osl file with given filepath to temporary .oso file"""
    import tempfile
    output_file = tempfile.NamedTemporaryFile(mode='w', suffix=".oso", delete=False)
    output_path = output_file.name
    output_file.close()

    ok = _cycles.osl_compile(input_path, output_path)

    if ok:
        report({'INFO'}, "OSL shader compilation succeeded")

    return ok, output_path


def update_script_node(node, report):
    """compile and update shader script node"""
    import os
    import shutil
    import tempfile

    if node.mode == 'EXTERNAL':
        # compile external script file
        script_path = bpy.path.abspath(node.filepath, library=node.id_data.library)
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
        osl_path = bpy.path.abspath(script.filepath, library=script.library)

        if script.is_in_memory or script.is_dirty or script.is_modified or not os.path.exists(osl_path):
            # write text datablock contents to temporary file
            osl_file = tempfile.NamedTemporaryFile(mode='w', suffix=".osl", delete=False)
            osl_file.write(script.as_string())
            osl_file.close()

            ok, oso_path = osl_compile(osl_file.name, report)
            oso_file_remove = False
            os.remove(osl_file.name)
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
                import traceback
                traceback.print_exc()

                report({'ERROR'}, "Can't read OSO bytecode to store in node at %r" % oso_path)
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
