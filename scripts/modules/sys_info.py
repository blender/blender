# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# classes for extracting info from blenders internal classes


def write_sysinfo(filepath):
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

    with open(filepath, 'w', encoding="utf-8") as output:
        try:
            header = "= Blender %s System Information =\n" % bpy.app.version_string
            lilies = "%s\n\n" % ((len(header) - 1) * "=")
            output.write(lilies[:-1])
            output.write(header)
            output.write(lilies)

            def title(text):
                return "\n%s:\n%s" % (text, lilies)

            # build info
            output.write(title("Blender"))
            output.write(
                "version: %s, branch: %s, commit date: %s %s, hash: %s, type: %s\n" %
                (bpy.app.version_string,
                 prepr(bpy.app.build_branch),
                 prepr(bpy.app.build_commit_date),
                 prepr(bpy.app.build_commit_time),
                 prepr(bpy.app.build_hash),
                 prepr(bpy.app.build_type),
                 ))

            output.write("build date: %s, %s\n" % (prepr(bpy.app.build_date), prepr(bpy.app.build_time)))
            output.write("platform: %s\n" % prepr(platform.platform()))
            output.write("binary path: %s\n" % prepr(bpy.app.binary_path))
            output.write("build cflags: %s\n" % prepr(bpy.app.build_cflags))
            output.write("build cxxflags: %s\n" % prepr(bpy.app.build_cxxflags))
            output.write("build linkflags: %s\n" % prepr(bpy.app.build_linkflags))
            output.write("build system: %s\n" % prepr(bpy.app.build_system))

            # Windowing Environment (include when dynamically selectable).
            from _bpy import _ghost_backend
            ghost_backend = _ghost_backend()
            if ghost_backend not in {'NONE', 'DEFAULT'}:
                output.write("windowing environment: %s\n" % prepr(ghost_backend))
            del _ghost_backend, ghost_backend

            # Python info.
            output.write(title("Python"))
            output.write("version: %s\n" % (sys.version.replace("\n", " ")))
            output.write("file system encoding: %s:%s\n" % (
                sys.getfilesystemencoding(),
                sys.getfilesystemencodeerrors(),
            ))
            output.write("paths:\n")
            for p in sys.path:
                output.write("\t%r\n" % p)

            output.write(title("Python (External Binary)"))
            output.write("binary path: %s\n" % prepr(sys.executable))
            try:
                py_ver = prepr(subprocess.check_output([
                    sys.executable,
                    "--version",
                ]).strip())
            except BaseException as ex:
                py_ver = str(ex)
            output.write("version: %s\n" % py_ver)
            del py_ver

            output.write(title("Directories"))
            output.write("scripts:\n")
            for p in bpy.utils.script_paths():
                output.write("\t%r\n" % p)
            output.write("user scripts: %r\n" % (bpy.utils.script_path_user()))
            output.write("pref scripts:\n")
            for p in bpy.utils.script_paths_pref():
                output.write("\t%r\n" % p)
            output.write("datafiles: %r\n" % (bpy.utils.user_resource('DATAFILES')))
            output.write("config: %r\n" % (bpy.utils.user_resource('CONFIG')))
            output.write("scripts: %r\n" % (bpy.utils.user_resource('SCRIPTS')))
            output.write("extensions: %r\n" % (bpy.utils.user_resource('EXTENSIONS')))
            output.write("tempdir: %r\n" % (bpy.app.tempdir))

            output.write(title("FFmpeg"))
            ffmpeg = bpy.app.ffmpeg
            if ffmpeg.supported:
                for lib in ("avcodec", "avdevice", "avformat", "avutil", "swscale"):
                    output.write(
                        "%s:%s%r\n" % (lib, " " * (10 - len(lib)),
                                       getattr(ffmpeg, lib + "_version_string")))
            else:
                output.write("Blender was built without FFmpeg support\n")

            if bpy.app.build_options.sdl:
                output.write(title("SDL"))
                output.write("Version: %s\n" % bpy.app.sdl.version_string)
                output.write("Loading method: ")
                if bpy.app.build_options.sdl_dynload:
                    output.write("dynamically loaded by Blender (WITH_SDL_DYNLOAD=ON)\n")
                else:
                    output.write("linked (WITH_SDL_DYNLOAD=OFF)\n")
                if not bpy.app.sdl.available:
                    output.write("WARNING: Blender could not load SDL library\n")

            output.write(title("Other Libraries"))
            ocio = bpy.app.ocio
            output.write("OpenColorIO: ")
            if ocio.supported:
                if ocio.version_string == "fallback":
                    output.write("Blender was built with OpenColorIO, "
                                 "but it currently uses fallback color management.\n")
                else:
                    output.write("%s\n" % (ocio.version_string))
            else:
                output.write("Blender was built without OpenColorIO support\n")

            oiio = bpy.app.oiio
            output.write("OpenImageIO: ")
            if ocio.supported:
                output.write("%s\n" % (oiio.version_string))
            else:
                output.write("Blender was built without OpenImageIO support\n")

            output.write("OpenShadingLanguage: ")
            if bpy.app.build_options.cycles:
                if bpy.app.build_options.cycles_osl:
                    from _cycles import osl_version_string
                    output.write("%s\n" % (osl_version_string))
                else:
                    output.write("Blender was built without OpenShadingLanguage support in Cycles\n")
            else:
                output.write("Blender was built without Cycles support\n")

            opensubdiv = bpy.app.opensubdiv
            output.write("OpenSubdiv: ")
            if opensubdiv.supported:
                output.write("%s\n" % opensubdiv.version_string)
            else:
                output.write("Blender was built without OpenSubdiv support\n")

            openvdb = bpy.app.openvdb
            output.write("OpenVDB: ")
            if openvdb.supported:
                output.write("%s\n" % openvdb.version_string)
            else:
                output.write("Blender was built without OpenVDB support\n")

            alembic = bpy.app.alembic
            output.write("Alembic: ")
            if alembic.supported:
                output.write("%s\n" % alembic.version_string)
            else:
                output.write("Blender was built without Alembic support\n")

            usd = bpy.app.usd
            output.write("USD: ")
            if usd.supported:
                output.write("%s\n" % usd.version_string)
            else:
                output.write("Blender was built without USD support\n")

            if not bpy.app.build_options.sdl:
                output.write("SDL: Blender was built without SDL support\n")

            if bpy.app.background:
                output.write("\nGPU: missing, background mode\n")
            else:
                output.write(title("GPU"))
                output.write("renderer:\t%r\n" % gpu.platform.renderer_get())
                output.write("vendor:\t\t%r\n" % gpu.platform.vendor_get())
                output.write("version:\t%r\n" % gpu.platform.version_get())
                output.write("device type:\t%r\n" % gpu.platform.device_type_get())
                output.write("backend type:\t%r\n" % gpu.platform.backend_type_get())
                output.write("extensions:\n")

                glext = sorted(gpu.capabilities.extensions_get())

                for l in glext:
                    output.write("\t%s\n" % l)

                output.write(title("Implementation Dependent GPU Limits"))
                output.write("Maximum Batch Vertices:\t%d\n" % gpu.capabilities.max_batch_vertices_get())
                output.write("Maximum Batch Indices:\t%d\n" % gpu.capabilities.max_batch_indices_get())

                output.write("\nGLSL:\n")
                output.write("Maximum Varying Floats:\t%d\n" % gpu.capabilities.max_varying_floats_get())
                output.write("Maximum Vertex Attributes:\t%d\n" % gpu.capabilities.max_vertex_attribs_get())
                output.write("Maximum Vertex Uniform Components:\t%d\n" % gpu.capabilities.max_uniforms_vert_get())
                output.write("Maximum Fragment Uniform Components:\t%d\n" % gpu.capabilities.max_uniforms_frag_get())
                output.write("Maximum Vertex Image Units:\t%d\n" % gpu.capabilities.max_textures_vert_get())
                output.write("Maximum Fragment Image Units:\t%d\n" % gpu.capabilities.max_textures_frag_get())
                output.write("Maximum Pipeline Image Units:\t%d\n" % gpu.capabilities.max_textures_get())
                output.write("Maximum Image Units:\t%d\n" % gpu.capabilities.max_images_get())

                output.write("\nFeatures:\n")
                output.write("Compute Shader Support:               \t%d\n" %
                             gpu.capabilities.compute_shader_support_get())
                output.write("Image Load/Store Support:             \t%d\n" %
                             gpu.capabilities.shader_image_load_store_support_get())

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
                    output.write("%s (MISSING)\n" % (addon))
                else:
                    output.write("%s (version: %s, path: %s)\n" %
                                 (addon, addon_mod.bl_info.get('version', "UNKNOWN"), addon_mod.__file__))
        except BaseException as ex:
            output.write("ERROR: %s\n" % ex)
