# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import bpy
import _cycles

from bpy.app.translations import pgettext_tip as tip_


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

    oso_file_remove = False

    if node.mode == 'EXTERNAL':
        # compile external script file
        script_path = bpy.path.abspath(node.filepath, library=node.id_data.library)
        script_path_noext, script_ext = os.path.splitext(script_path)

        if script_ext == ".oso":
            # it's a .oso file, no need to compile
            ok, oso_path = True, script_path
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
            osl_file.write("\n")
            osl_file.close()

            ok, oso_path = osl_compile(osl_file.name, report)
            os.remove(osl_file.name)
        else:
            # compile text datablock from disk directly
            ok, oso_path = osl_compile(osl_path, report)

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
        data = bpy.data.as_pointer()
        ok = _cycles.osl_update_node(data, node.id_data.as_pointer(), node.as_pointer(), oso_path)

        if not ok:
            report({'ERROR'}, tip_("OSL query failed to open %s") % oso_path)
    else:
        report({'ERROR'}, "OSL script compilation failed, see console for errors")

    # remove temporary oso file
    if oso_file_remove:
        try:
            os.remove(oso_path)
        except:
            pass

    return ok
