# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Function for extracting info from Blenders system information
# (sometimes useful to include in bug reports).
# Called by the operator `WM_OT_sysinfo`.

__all__ = (
    "write",
)


def write(output):
    # Writes into `output`, a file-like object.

    import sys
    import platform

    import subprocess

    import bpy
    import gpu

    # pretty repr
    def prepr(v):
        r = repr(v)
        vt = type(v)
        if vt is bytes:
            r = r[2:-1]
        elif vt is list or vt is tuple:
            r = r[1:-1]
        return r

    header = "= Blender {:s} System Information =\n".format(bpy.app.version_string)
    lilies = "{:s}\n\n".format((len(header) - 1) * "=")
    output.write(lilies[:-1])
    output.write(header)
    output.write(lilies)

    def title(text):
        return "\n{:s}:\n{:s}".format(text, lilies)

    # build info
    output.write(title("Blender"))
    output.write(
        "version: {:s}, branch: {:s}, commit date: {:s} {:s}, hash: {:s}, type: {:s}\n".format(
            bpy.app.version_string,
            prepr(bpy.app.build_branch),
            prepr(bpy.app.build_commit_date),
            prepr(bpy.app.build_commit_time),
            prepr(bpy.app.build_hash),
            prepr(bpy.app.build_type),
        )
    )

    output.write("build date: {:s}, {:s}\n".format(prepr(bpy.app.build_date), prepr(bpy.app.build_time)))
    output.write("platform: {:s}\n".format(prepr(platform.platform())))
    output.write("binary path: {:s}\n".format(prepr(bpy.app.binary_path)))
    output.write("build cflags: {:s}\n".format(prepr(bpy.app.build_cflags)))
    output.write("build cxxflags: {:s}\n".format(prepr(bpy.app.build_cxxflags)))
    output.write("build linkflags: {:s}\n".format(prepr(bpy.app.build_linkflags)))
    output.write("build system: {:s}\n".format(prepr(bpy.app.build_system)))

    # Windowing Environment (include when dynamically selectable).
    from _bpy import _ghost_backend
    ghost_backend = _ghost_backend()
    if ghost_backend not in {'NONE', 'DEFAULT'}:
        output.write("windowing environment: {:s}\n".format(prepr(ghost_backend)))
    del _ghost_backend, ghost_backend

    # Python info.
    output.write(title("Python"))
    output.write("version: {:s}\n".format(sys.version.replace("\n", " ")))
    output.write("file system encoding: {:s}:{:s}\n".format(
        sys.getfilesystemencoding(),
        sys.getfilesystemencodeerrors(),
    ))
    output.write("paths:\n")
    for p in sys.path:
        output.write("\t{!r}\n".format(p))

    output.write(title("Python (External Binary)"))
    output.write("binary path: {:s}\n".format(prepr(sys.executable)))
    try:
        py_ver = prepr(subprocess.check_output([
            sys.executable,
            "--version",
        ]).strip())
    except Exception as ex:
        py_ver = str(ex)
    output.write("version: {:s}\n".format(py_ver))
    del py_ver

    output.write(title("Directories"))
    output.write("scripts:\n")
    for p in bpy.utils.script_paths():
        output.write("\t{!r}\n".format(p))
    output.write("user scripts: {!r}\n".format(bpy.utils.script_path_user()))
    output.write("pref scripts:\n")
    for p in bpy.utils.script_paths_pref():
        output.write("\t{!r}\n".format(p))
    output.write("datafiles: {!r}\n".format(bpy.utils.user_resource('DATAFILES')))
    output.write("config: {!r}\n".format(bpy.utils.user_resource('CONFIG')))
    output.write("scripts: {!r}\n".format(bpy.utils.user_resource('SCRIPTS')))
    output.write("extensions: {!r}\n".format(bpy.utils.user_resource('EXTENSIONS')))
    output.write("tempdir: {!r}\n".format(bpy.app.tempdir))

    output.write(title("FFmpeg"))
    ffmpeg = bpy.app.ffmpeg
    if ffmpeg.supported:
        for lib in ("avcodec", "avdevice", "avformat", "avutil", "swscale"):
            output.write(
                "{:s}:{:s}{!r}\n".format(
                    lib,
                    " " * (10 - len(lib)),
                    getattr(ffmpeg, lib + "_version_string"),
                )
            )
    else:
        output.write("Blender was built without FFmpeg support\n")

    if bpy.app.build_options.sdl:
        output.write(title("SDL"))
        output.write("Version: {:s}\n".format(bpy.app.sdl.version_string))

    output.write(title("Other Libraries"))
    ocio = bpy.app.ocio
    output.write("OpenColorIO: ")
    if ocio.supported:
        if ocio.version_string == "fallback":
            output.write(
                "Blender was built with OpenColorIO, "
                "but it currently uses fallback color management.\n"
            )
        else:
            output.write("{:s}\n".format(ocio.version_string))
    else:
        output.write("Blender was built without OpenColorIO support\n")

    oiio = bpy.app.oiio
    output.write("OpenImageIO: ")
    if ocio.supported:
        output.write("{:s}\n".format(oiio.version_string))
    else:
        output.write("Blender was built without OpenImageIO support\n")

    output.write("OpenShadingLanguage: ")
    if bpy.app.build_options.cycles:
        if bpy.app.build_options.cycles_osl:
            from _cycles import osl_version_string
            output.write("{:s}\n".format(osl_version_string))
        else:
            output.write("Blender was built without OpenShadingLanguage support in Cycles\n")
    else:
        output.write("Blender was built without Cycles support\n")

    opensubdiv = bpy.app.opensubdiv
    output.write("OpenSubdiv: ")
    if opensubdiv.supported:
        output.write("{:s}\n".format(opensubdiv.version_string))
    else:
        output.write("Blender was built without OpenSubdiv support\n")

    openvdb = bpy.app.openvdb
    output.write("OpenVDB: ")
    if openvdb.supported:
        output.write("{:s}\n".format(openvdb.version_string))
    else:
        output.write("Blender was built without OpenVDB support\n")

    alembic = bpy.app.alembic
    output.write("Alembic: ")
    if alembic.supported:
        output.write("{:s}\n".format(alembic.version_string))
    else:
        output.write("Blender was built without Alembic support\n")

    usd = bpy.app.usd
    output.write("USD: ")
    if usd.supported:
        output.write("{:s}\n".format(usd.version_string))
    else:
        output.write("Blender was built without USD support\n")

    if not bpy.app.build_options.sdl:
        output.write("SDL: Blender was built without SDL support\n")

    if bpy.app.background:
        output.write("\nGPU: missing, background mode\n")
    else:
        output.write(title("GPU"))
        output.write("renderer:\t{!r}\n".format(gpu.platform.renderer_get()))
        output.write("vendor:\t\t{!r}\n".format(gpu.platform.vendor_get()))
        output.write("version:\t{!r}\n".format(gpu.platform.version_get()))
        output.write("device type:\t{!r}\n".format(gpu.platform.device_type_get()))
        output.write("backend type:\t{!r}\n".format(gpu.platform.backend_type_get()))
        output.write("extensions:\n")

        glext = sorted(gpu.capabilities.extensions_get())

        for line in glext:
            output.write("\t{:s}\n".format(line))

        output.write(title("Implementation Dependent GPU Limits"))
        output.write("Maximum Batch Vertices:\t{:d}\n".format(
            gpu.capabilities.max_batch_vertices_get(),
        ))
        output.write("Maximum Batch Indices:\t{:d}\n".format(
            gpu.capabilities.max_batch_indices_get(),
        ))

        output.write("\nGLSL:\n")
        output.write("Maximum Varying Floats:\t{:d}\n".format(
            gpu.capabilities.max_varying_floats_get(),
        ))
        output.write("Maximum Vertex Attributes:\t{:d}\n".format(
            gpu.capabilities.max_vertex_attribs_get(),
        ))
        output.write("Maximum Vertex Uniform Components:\t{:d}\n".format(
            gpu.capabilities.max_uniforms_vert_get(),
        ))
        output.write("Maximum Fragment Uniform Components:\t{:d}\n".format(
            gpu.capabilities.max_uniforms_frag_get(),
        ))
        output.write("Maximum Vertex Image Units:\t{:d}\n".format(
            gpu.capabilities.max_textures_vert_get(),
        ))
        output.write("Maximum Fragment Image Units:\t{:d}\n".format(
            gpu.capabilities.max_textures_frag_get(),
        ))
        output.write("Maximum Pipeline Image Units:\t{:d}\n".format(
            gpu.capabilities.max_textures_get(),
        ))
        output.write("Maximum Image Units:\t{:d}\n".format(
            gpu.capabilities.max_images_get(),
        ))

    if bpy.app.build_options.cycles:
        import cycles
        output.write(title("Cycles"))
        output.write(cycles.engine.system_info())

    import addon_utils
    addon_utils.modules()
    output.write(title("Enabled add-ons"))
    for addon in bpy.context.preferences.addons.keys():
        addon_mod = addon_utils.addons_fake_modules.get(addon, None)
        if addon_mod is None:
            output.write("{:s} (MISSING)\n".format(addon))
        else:
            output.write(
                "{:s} (version: {:s}, path: {!r})\n".format(
                    addon,
                    str(addon_mod.bl_info.get("version", "UNKNOWN")),
                    addon_mod.__file__,
                )
            )
