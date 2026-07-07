# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import bpy
import _cycles

from bpy.app.translations import pgettext_rpt as rpt_


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


def osl_param_ensure_property(ccam, param):
    import idprop

    if param.isoutput or param.isclosure:
        return None

    # Get metadata for the parameter to control UI display
    metadata = {meta.name: meta.value for meta in param.metadata}
    if 'label' not in metadata:
        metadata['label'] = param.name

    datatype = None
    if param.type.basetype == param.type.basetype.INT:
        datatype = int
    elif param.type.basetype == param.type.basetype.FLOAT:
        datatype = float
    elif param.type.basetype == param.type.basetype.STRING:
        datatype = str

    # OSl doesn't have boolean as a type, but we do
    if (datatype == int) and (metadata.get('widget') in ('boolean', 'checkBox')):
        datatype = bool
    default = param.value if isinstance(param.value, tuple) else [param.value]
    default = [datatype(v) for v in default]

    name = param.name
    if name in ccam:
        # If the parameter already exists, only reset its value if its type
        # or array length changed
        cur_data = ccam[name]
        if isinstance(cur_data, idprop.types.IDPropertyArray):
            cur_length = len(cur_data)
            cur_type = type(cur_data[0])
        else:
            cur_length = 1
            cur_type = type(cur_data)
        do_replace = datatype != cur_type or len(default) != cur_length
    else:
        # Parameter doesn't exist yet, so set it from the defaults
        do_replace = True

    if do_replace:
        ccam[name] = tuple(default) if len(default) > 1 else default[0]

    ui = ccam.id_properties_ui(name)
    ui.clear()
    ui.update(default=tuple(default) if len(default) > 1 else default[0])

    # Determine subtype (limited unit support for now)
    if param.type.vecsemantics == param.type.vecsemantics.COLOR:
        ui.update(subtype='COLOR')
    elif param.type.vecsemantics == param.type.vecsemantics.POINT:
        ui.update(subtype='TRANSLATION')
    elif param.type.vecsemantics == param.type.vecsemantics.NORMAL:
        ui.update(subtype='DIRECTION')
    elif datatype is str and metadata.get('widget') == 'filename':
        ui.update(subtype='FILE_PATH')
    elif datatype is float and metadata.get('unit') == 'radians':
        ui.update(subtype='ANGLE')
    elif datatype is float and metadata.get('unit') == 'm':
        ui.update(subtype='DISTANCE')
    elif datatype is float and metadata.get('unit') == 'mm':
        ui.update(subtype='DISTANCE_CAMERA')
    elif datatype is float and metadata.get('unit') in ('s', 'sec'):
        ui.update(subtype='TIME_ABSOLUTE')
    elif metadata.get('slider'):
        ui.update(subtype='FACTOR')
    elif datatype is int and metadata.get('widget') == 'mapper':
        options = metadata.get('options', "")
        options = options.split("|")
        option_items = []
        for option in options:
            if ":" not in option:
                continue
            item, index = option.split(":")
            # Ensure that the index can be converted to an integer
            try:
                int(index)
            except ValueError:
                continue
            option_items.append((str(index), bpy.path.display_name(item), ""))
        ui.update(items=option_items)

    # Map OSL metadata to Blender names
    option_map = {
        'help': 'description',
        'sensitivity': 'step', 'digits': 'precision',
        'min': 'min', 'max': 'max',
        'slidermin': 'soft_min', 'slidermax': 'soft_max',
    }
    if 'sensitivity' in metadata:
        # Blender divides this value by 100 by convention, so counteract that.
        metadata['sensitivity'] *= 100
    for option, value in metadata.items():
        if option in option_map:
            ui.update(**{option_map[option]: value})

    return name


def update_external_script(report, filepath, library):
    """compile and update OSL script"""
    import os
    import shutil

    oso_file_remove = False

    script_path = bpy.path.abspath(filepath, library=library)
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
                report({'ERROR'}, rpt_("Failed to write .oso file next to external .osl file at {:s}").format(dst_path))
    elif os.path.dirname(filepath) == "":
        # module in search path
        oso_path = filepath
        ok = True
    else:
        # unknown
        report({'ERROR'}, "External shader script must have .osl or .oso extension, or be a module name")
        ok = False

    return ok, oso_path, oso_file_remove


def update_internal_script(report, script):
    """compile and update shader script node"""
    import os
    import tempfile
    import pathlib
    import hashlib

    bytecode = None
    bytecode_hash = None

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
            bytecode_hash = md5.hexdigest()
        except:
            import traceback
            traceback.print_exc()

            report({'ERROR'}, "Cannot read OSO bytecode to store in node at {!r}".format(oso_path))
            ok = False

    return ok, oso_path, bytecode, bytecode_hash


def update_script_node(node, report):
    """compile and update shader script node"""
    import os
    import oslquery

    oso_file_remove = False

    if node.mode == 'EXTERNAL':
        # compile external script file
        ok, oso_path, oso_file_remove = update_external_script(report, node.filepath, node.id_data.library)
        if ok:
            # Clear old internal bytecode, and also trigger node update if it was already cleared.
            node.bytecode = ""
            node.bytecode_hash = ""

    elif node.mode == 'INTERNAL' and node.script:
        # internal script, we will store bytecode in the node
        ok, oso_path, bytecode, bytecode_hash = update_internal_script(report, node.script)
        if bytecode:
            node.bytecode = bytecode
            node.bytecode_hash = bytecode_hash

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
            report({'ERROR'}, rpt_("OSL query failed to open %s") % oso_path)
    else:
        report({'ERROR'}, "OSL script compilation failed, see console for errors")

    # remove temporary oso file
    if oso_file_remove:
        try:
            os.remove(oso_path)
        except:
            pass

    return ok


def update_custom_camera_shader(cam, report):
    """compile and update custom camera shader"""
    import os
    import oslquery

    oso_file_remove = False

    custom_props = cam.cycles_custom
    if cam.custom_mode == 'EXTERNAL':
        # compile external script file
        ok, oso_path, oso_file_remove = update_external_script(report, cam.custom_filepath, cam.library)

    elif cam.custom_mode == 'INTERNAL' and cam.custom_shader:
        # internal script, we will store bytecode in the node
        ok, oso_path, bytecode, bytecode_hash = update_internal_script(report, cam.custom_shader)
        if bytecode:
            cam.custom_bytecode = bytecode
            cam.custom_bytecode_hash = bytecode_hash
            cam.update_tag()

    else:
        report({'WARNING'}, "No text or file specified in node, nothing to compile")
        return

    if ok:
        if query := oslquery.OSLQuery(oso_path):
            # Ensure that all parameters have a matching property
            used_params = set()
            for param in query.parameters:
                if name := osl_param_ensure_property(custom_props, param):
                    used_params.add(name)

            # Clean up unused parameters
            for prop in list(custom_props.keys()):
                if prop not in used_params:
                    del custom_props[prop]
        else:
            ok = False
            report({'ERROR'}, rpt_("OSL query failed to open %s") % oso_path)
    else:
        report({'ERROR'}, "Custom Camera shader compilation failed, see console for errors")

    # remove temporary oso file
    if oso_file_remove:
        try:
            os.remove(oso_path)
        except:
            pass

    return ok
