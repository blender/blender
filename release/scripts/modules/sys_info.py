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

# classes for extracting info from blenders internal classes


def write_sysinfo(filepath):
    import sys

    import subprocess

    import bpy
    import bgl

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
            output.write("platform: %s\n" % prepr(bpy.app.build_platform))
            output.write("binary path: %s\n" % prepr(bpy.app.binary_path))
            output.write("build cflags: %s\n" % prepr(bpy.app.build_cflags))
            output.write("build cxxflags: %s\n" % prepr(bpy.app.build_cxxflags))
            output.write("build linkflags: %s\n" % prepr(bpy.app.build_linkflags))
            output.write("build system: %s\n" % prepr(bpy.app.build_system))

            # python info
            output.write(title("Python"))
            output.write("version: %s\n" % (sys.version))
            output.write("paths:\n")
            for p in sys.path:
                output.write("\t%r\n" % p)

            output.write(title("Python (External Binary)"))
            output.write("binary path: %s\n" % prepr(bpy.app.binary_path_python))
            try:
                py_ver = prepr(subprocess.check_output([
                    bpy.app.binary_path_python,
                    "--version",
                ]).strip())
            except Exception as e:
                py_ver = str(e)
            output.write("version: %s\n" % py_ver)
            del py_ver

            output.write(title("Directories"))
            output.write("scripts:\n")
            for p in bpy.utils.script_paths():
                output.write("\t%r\n" % p)
            output.write("user scripts: %r\n" % (bpy.utils.script_path_user()))
            output.write("pref scripts: %r\n" % (bpy.utils.script_path_pref()))
            output.write("datafiles: %r\n" % (bpy.utils.user_resource('DATAFILES')))
            output.write("config: %r\n" % (bpy.utils.user_resource('CONFIG')))
            output.write("scripts : %r\n" % (bpy.utils.user_resource('SCRIPTS')))
            output.write("autosave: %r\n" % (bpy.utils.user_resource('AUTOSAVE')))
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
                    output.write("Blender was built with OpenColorIO, " +
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

            if not bpy.app.build_options.sdl:
                output.write("SDL: Blender was built without SDL support\n")

            if bpy.app.background:
                output.write("\nOpenGL: missing, background mode\n")
            else:
                output.write(title("OpenGL"))
                version = bgl.glGetString(bgl.GL_RENDERER)
                output.write("renderer:\t%r\n" % version)
                output.write("vendor:\t\t%r\n" % (bgl.glGetString(bgl.GL_VENDOR)))
                output.write("version:\t%r\n" % (bgl.glGetString(bgl.GL_VERSION)))
                output.write("extensions:\n")

                limit = bgl.Buffer(bgl.GL_INT, 1)
                bgl.glGetIntegerv(bgl.GL_NUM_EXTENSIONS, limit)

                glext = []
                for i in range(limit[0]):
                    glext.append(bgl.glGetStringi(bgl.GL_EXTENSIONS, i))

                glext = sorted(glext)

                for l in glext:
                    output.write("\t%s\n" % l)

                output.write(title("Implementation Dependent OpenGL Limits"))
                bgl.glGetIntegerv(bgl.GL_MAX_ELEMENTS_VERTICES, limit)
                output.write("Maximum DrawElements Vertices:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_ELEMENTS_INDICES, limit)
                output.write("Maximum DrawElements Indices:\t%d\n" % limit[0])

                output.write("\nGLSL:\n")
                bgl.glGetIntegerv(bgl.GL_MAX_VARYING_FLOATS, limit)
                output.write("Maximum Varying Floats:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_VERTEX_ATTRIBS, limit)
                output.write("Maximum Vertex Attributes:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_VERTEX_UNIFORM_COMPONENTS, limit)
                output.write("Maximum Vertex Uniform Components:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, limit)
                output.write("Maximum Fragment Uniform Components:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, limit)
                output.write("Maximum Vertex Image Units:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_TEXTURE_IMAGE_UNITS, limit)
                output.write("Maximum Fragment Image Units:\t%d\n" % limit[0])
                bgl.glGetIntegerv(bgl.GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, limit)
                output.write("Maximum Pipeline Image Units:\t%d\n" % limit[0])

            if bpy.app.build_options.cycles:
                import cycles
                output.write(title("Cycles"))
                output.write(cycles.engine.system_info())

            import addon_utils
            addon_utils.modules()
            output.write(title("Enabled add-ons"))
            for addon in bpy.context.user_preferences.addons.keys():
                addon_mod = addon_utils.addons_fake_modules.get(addon, None)
                if addon_mod is None:
                    output.write("%s (MISSING)\n" % (addon))
                else:
                    output.write("%s (version: %s, path: %s)\n" %
                                 (addon, addon_mod.bl_info.get('version', "UNKNOWN"), addon_mod.__file__))
        except Exception as e:
            output.write("ERROR: %s\n" % e)
