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

# Contributors: Bill L.Nieuwendorp

"""
This script Exports Lightwaves MotionDesigner format.

The .mdd format has become quite a popular Pipeline format<br>
for moving animations from package to package.

Be sure not to use modifiers that change the number or order of verts in the mesh
"""

import bpy
import mathutils
from struct import pack


def zero_file(filepath):
    '''
    If a file fails, this replaces it with 1 char, better not remove it?
    '''
    file = open(filepath, 'w')
    file.write('\n')  # apparently macosx needs some data in a blank file?
    file.close()


def check_vertcount(mesh, vertcount):
    '''
    check and make sure the vertcount is consistent throughout the frame range
    '''
    if len(mesh.vertices) != vertcount:
        raise Exception('Error, number of verts has changed during animation, cannot export')
        f.close()
        zero_file(filepath)
        return


def save(operator, context, filepath="", frame_start=1, frame_end=300, fps=25):
    """
    Blender.Window.WaitCursor(1)

    mesh_orig = Mesh.New()
    mesh_orig.getFromObject(obj.name)
    """

    scene = context.scene
    obj = context.object

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    orig_frame = scene.frame_current
    scene.frame_set(frame_start)
    me = obj.create_mesh(scene, True, 'PREVIEW')

    #Flip y and z
    mat_flip = mathutils.Matrix(((1.0, 0.0, 0.0, 0.0), \
                                  (0.0, 0.0, 1.0, 0.0), \
                                  (0.0, 1.0, 0.0, 0.0), \
                                  (0.0, 0.0, 0.0, 1.0), \
                                  ))

    numverts = len(me.vertices)

    numframes = frame_end - frame_start + 1
    fps = float(fps)
    f = open(filepath, 'wb')  # no Errors yet:Safe to create file

    # Write the header
    f.write(pack(">2i", numframes, numverts))

    # Write the frame times (should we use the time IPO??)
    f.write(pack(">%df" % (numframes), *[frame / fps for frame in range(numframes)]))  # seconds

    #rest frame needed to keep frames in sync
    """
    Blender.Set('curframe', frame_start)
    me_tmp.getFromObject(obj.name)
    """

    check_vertcount(me, numverts)
    me.transform(mat_flip * obj.matrix_world)
    f.write(pack(">%df" % (numverts * 3), *[axis for v in me.vertices for axis in v.co]))

    for frame in range(frame_start, frame_end + 1):  # in order to start at desired frame
        """
        Blender.Set('curframe', frame)
        me_tmp.getFromObject(obj.name)
        """

        scene.frame_set(frame)
        me = obj.create_mesh(scene, True, 'PREVIEW')
        check_vertcount(me, numverts)
        me.transform(mat_flip * obj.matrix_world)

        # Write the vertex data
        f.write(pack(">%df" % (numverts * 3), *[axis for v in me.vertices for axis in v.co]))

    """
    me_tmp.vertices= None
    """
    f.close()

    print('MDD Exported: %r frames:%d\n' % (filepath, numframes - 1))
    """
    Blender.Window.WaitCursor(0)
    Blender.Set('curframe', orig_frame)
    """
    scene.frame_set(orig_frame)

    return {'FINISHED'}
