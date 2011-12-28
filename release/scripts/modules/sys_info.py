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
    "Returns position of the last space found before 'length' chars"
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

    header = '= Blender {} System Information =\n'.format(bpy.app.version_string)
    lilies = '{}\n\n'.format(len(header) * '=')
    firstlilies = '{}\n'.format(len(header) * '=')
    output.write(firstlilies)
    output.write(header)
    output.write(lilies)

    # build info
    output.write('\nBlender:\n')
    output.write(lilies)
    output.write('version {}, revision {}. {}\n'.format(bpy.app.version_string, bpy.app.build_revision, bpy.app.build_type))
    output.write('build date: {}, {}\n'.format(bpy.app.build_date, bpy.app.build_time))
    output.write('platform: {}\n'.format(bpy.app.build_platform))
    output.write('binary path: {}\n'.format(bpy.app.binary_path))
    output.write('build cflags: {}\n'.format(bpy.app.build_cflags))
    output.write('build cxxflags: {}\n'.format(bpy.app.build_cxxflags))
    output.write('build linkflags: {}\n'.format(bpy.app.build_linkflags))
    output.write('build system: {}\n'.format(bpy.app.build_system))

    # python info
    output.write('\nPython:\n')
    output.write(lilies)
    output.write('version: {}\n'.format(sys.version))
    output.write('paths:\n')
    for p in sys.path:
        output.write('\t{}\n'.format(p))

    output.write('\nDirectories:\n')
    output.write(lilies)
    output.write('scripts: {}\n'.format(bpy.utils.script_paths()))
    output.write('user scripts: {}\n'.format(bpy.utils.user_script_path()))
    output.write('datafiles: {}\n'.format(bpy.utils.user_resource('DATAFILES')))
    output.write('config: {}\n'.format(bpy.utils.user_resource('CONFIG')))
    output.write('scripts : {}\n'.format(bpy.utils.user_resource('SCRIPTS')))
    output.write('autosave: {}\n'.format(bpy.utils.user_resource('AUTOSAVE')))
    output.write('tempdir: {}\n'.format(bpy.app.tempdir))

    output.write('\nFFmpeg:\n')
    output.write(lilies)
    ffmpeg = bpy.app.ffmpeg
    if ffmpeg.supported:
        for lib in ['avcodec', 'avdevice', 'avformat', 'avutil', 'swscale']:
            output.write('{}:{}{}\n'.format(lib, " "*(10-len(lib)),
                         getattr(ffmpeg, lib + '_version_string')))
    else:
        output.write('Blender was built without FFmpeg support\n')

    if bpy.app.background:
        output.write('\nOpenGL: missing, background mode\n')
    else:
        output.write('\nOpenGL\n')
        output.write(lilies)
        output.write('renderer:\t{}\n'.format(bgl.glGetString(bgl.GL_RENDERER)))
        output.write('vendor:\t\t{}\n'.format(bgl.glGetString(bgl.GL_VENDOR)))
        output.write('version:\t{}\n'.format(bgl.glGetString(bgl.GL_VERSION)))
        output.write('extensions:\n')

        glext = bgl.glGetString(bgl.GL_EXTENSIONS)
        glext = textWrap(glext, 70)
        for l in glext:
            output.write('\t\t{}\n'.format(l))

    op.report({'INFO'}, "System information generated in 'system-info.txt'")
