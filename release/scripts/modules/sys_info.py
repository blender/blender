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

import bpy
import bgl

import sys


def cutPoint(text, length):
    """Returns position of the last space found before 'length' chars"""
    l = length
    c = text[l]
    while c != ' ':
        l -= 1
        if l == 0:
            return length  # no space found
        c = text[l]
    return l


def textWrap(text, length=70):
    lines = []
    while len(text) > 70:
        cpt = cutPoint(text, length)
        line, text = text[:cpt], text[cpt + 1:]
        lines.append(line)
    lines.append(text)
    return lines


def write_sysinfo(op):
    output_filename = "system-info.txt"

    output = bpy.data.texts.get(output_filename)
    if output:
        output.clear()
    else:
        output = bpy.data.texts.new(name=output_filename)

    header = "= Blender %s System Information =\n" % bpy.app.version_string
    lilies = "%s\n\n" % (len(header) * "=")
    firstlilies = "%s\n" % (len(header) * "=")
    output.write(firstlilies)
    output.write(header)
    output.write(lilies)

    # build info
    output.write("\nBlender:\n")
    output.write(lilies)
    if bpy.app.build_branch and bpy.app.build_branch != "Unknown":
        output.write("version %s, branch %r, commit date %r %r, hash %r, %r\n" %
            (bpy.app.version_string,
             bpy.app.build_branch,
             bpy.app.build_commit_date,
             bpy.app.build_commit_time,
             bpy.app.build_hash,
             bpy.app.build_type))
    else:
        output.write("version %s, revision %r. %r\n" %
            (bpy.app.version_string,
             bpy.app.build_change,
             bpy.app.build_type))

    output.write("build date: %r, %r\n" % (bpy.app.build_date, bpy.app.build_time))
    output.write("platform: %r\n" % (bpy.app.build_platform))
    output.write("binary path: %r\n" % (bpy.app.binary_path))
    output.write("build cflags: %r\n" % (bpy.app.build_cflags))
    output.write("build cxxflags: %r\n" % (bpy.app.build_cxxflags))
    output.write("build linkflags: %r\n" % (bpy.app.build_linkflags))
    output.write("build system: %r\n" % (bpy.app.build_system))

    # python info
    output.write("\nPython:\n")
    output.write(lilies)
    output.write("version: %s\n" % (sys.version))
    output.write("paths:\n")
    for p in sys.path:
        output.write("\t%r\n" % (p))

    output.write("\nDirectories:\n")
    output.write(lilies)
    output.write("scripts: %r\n" % (bpy.utils.script_paths()))
    output.write("user scripts: %r\n" % (bpy.utils.script_path_user()))
    output.write("pref scripts: %r\n" % (bpy.utils.script_path_pref()))
    output.write("datafiles: %r\n" % (bpy.utils.user_resource('DATAFILES')))
    output.write("config: %r\n" % (bpy.utils.user_resource('CONFIG')))
    output.write("scripts : %r\n" % (bpy.utils.user_resource('SCRIPTS')))
    output.write("autosave: %r\n" % (bpy.utils.user_resource('AUTOSAVE')))
    output.write("tempdir: %r\n" % (bpy.app.tempdir))

    output.write("\nFFmpeg:\n")
    output.write(lilies)
    ffmpeg = bpy.app.ffmpeg
    if ffmpeg.supported:
        for lib in ("avcodec", "avdevice", "avformat", "avutil", "swscale"):
            output.write("%r:%r%r\n" % (lib, " " * (10 - len(lib)),
                         getattr(ffmpeg, lib + "_version_string")))
    else:
        output.write("Blender was built without FFmpeg support\n")

    output.write("\nOther Libraries:\n")
    output.write(lilies)
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

    if bpy.app.background:
        output.write("\nOpenGL: missing, background mode\n")
    else:
        output.write("\nOpenGL\n")
        output.write(lilies)
        version = bgl.glGetString(bgl.GL_RENDERER);
        output.write("renderer:\t%r\n" % version)
        output.write("vendor:\t\t%r\n" % (bgl.glGetString(bgl.GL_VENDOR)))
        output.write("version:\t%r\n" % (bgl.glGetString(bgl.GL_VERSION)))
        output.write("extensions:\n")

        glext = bgl.glGetString(bgl.GL_EXTENSIONS)
        glext = textWrap(glext, 70)
        for l in glext:
            output.write("\t\t%r\n" % (l))

        output.write("\nImplementation Dependent OpenGL Limits:\n")
        output.write(lilies)
        limit = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGetIntegerv(bgl.GL_MAX_TEXTURE_UNITS, limit)
        output.write("Maximum Fixed Function Texture Units:\t%d\n" % limit[0])

        output.write("\nGLSL:\n")
        if version[0] > '1':
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

    output.current_line_index = 0

    op.report({'INFO'}, "System information generated in 'system-info.txt'")
