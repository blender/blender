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


def shader_param_type_default(param, is_bool):
    if param.isclosure:
        return 'NodeSocketShader', None
    elif param.type.vecsemantics == param.type.vecsemantics.COLOR:
        return 'NodeSocketColor', (param.value[0], param.value[1], param.value[2], 1.0)
    elif param.type.vecsemantics in [
        param.type.vecsemantics.POINT,
        param.type.vecsemantics.VECTOR,
        param.type.vecsemantics.NORMAL,
    ]:
        return 'NodeSocketVector', param.value
    elif param.type.aggregate == param.type.aggregate.SCALAR:
        if param.type.basetype == param.type.basetype.INT:
            if is_bool:
                return 'NodeSocketBool', bool(param.value)
            else:
                return 'NodeSocketInt', int(param.value)
        elif param.type.basetype == param.type.basetype.FLOAT:
            return 'NodeSocketFloat', float(param.value)
        elif param.type.basetype == param.type.basetype.STRING:
            return 'NodeSocketString', str(param.value)

    return None, None


def shader_param_ensure(node, param):
    # Skip unsupported types
    if param.varlenarray or param.isstruct or param.type.arraylen > 1:
        return None

    metadata = {meta.name: meta.value for meta in param.metadata}

    is_bool = metadata.get('widget') in ['boolean', 'checkBox']
    hide_value = (param.value is None) or (metadata.get('widget') == 'null')
    label = metadata.get('label', param.name)

    socket_type, default = shader_param_type_default(param, is_bool)
    if not socket_type:
        return None

    sockets = node.outputs if param.isoutput else node.inputs
    if param.name in sockets:
        sock = sockets[param.name]

        if sock.bl_idname != socket_type:
            # Type doesn't match, delete the socket and recreate it below
            sockets.remove(sock)
        else:
            # Update properties if needed
            if sock.name != label:
                sock.name = label
            if not param.isoutput and sock.hide_value != hide_value:
                sock.hide_value = hide_value

            # We have a matching socket, no need to create one
            return sock

    sock = sockets.new(type=socket_type, name=label, identifier=param.name)
    if default is not None:
        sock.default_value = default
    sock.hide_value = hide_value
    return sock


def update_script_node(node, report):
    """compile and update shader script node"""
    import os
    import shutil
    import tempfile
    import hashlib
    import pathlib
    import oslquery

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
                bytecode = pathlib.Path(oso_path).read_text()
                md5 = hashlib.md5(usedforsecurity=False)
                md5.update(bytecode.encode())

                node.bytecode = bytecode
                node.bytecode_hash = md5.hexdigest()
            except:
                import traceback
                traceback.print_exc()

                report({'ERROR'}, "Can't read OSO bytecode to store in node at %r" % oso_path)
                ok = False

    else:
        report({'WARNING'}, "No text or file specified in node, nothing to compile")
        return

    if ok:
        if query := oslquery.OSLQuery(oso_path):
            # Ensure that all parameters have a matching socket
            used_sockets = set()
            for param in query.parameters:
                if sock := shader_param_ensure(node, param):
                    used_sockets.add(sock)

            # Remove unused sockets
            for sockets in (node.inputs, node.outputs):
                for identifier in [sock.identifier for sock in sockets]:
                    if sockets[identifier] not in used_sockets:
                        sockets.remove(sockets[identifier])
        else:
            ok = False
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
