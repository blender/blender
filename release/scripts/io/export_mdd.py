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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

__author__ = "Bill L.Nieuwendorp"
__bpydoc__ = """\
This script Exports Lightwaves MotionDesigner format.

The .mdd format has become quite a popular Pipeline format<br>
for moving animations from package to package.

Be sure not to use modifiers that change the number or order of verts in the mesh
"""
#Please send any fixes,updates,bugs to Slow67_at_Gmail.com or cbarton_at_metavr.com
#Bill Niewuendorp
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

import bpy
import Mathutils
import math
import os

#import Blender
#from Blender import *
#import BPyMessages
try:
    from struct import pack
except:
    pack = None

def zero_file(filepath):
    '''
    If a file fails, this replaces it with 1 char, better not remove it?
    '''
    file = open(filepath, 'w')
    file.write('\n') # apparently macosx needs some data in a blank file?
    file.close()

def check_vertcount(mesh,vertcount):
    '''
    check and make sure the vertcount is consistent throughout the frame range
    '''
    if len(mesh.verts) != vertcount:
        raise Exception('Error, number of verts has changed during animation, cannot export')
        f.close()
        zero_file(filepath)
        return
    
    
def write(filename, sce, ob, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS):
    """
    Blender.Window.WaitCursor(1)

    mesh_orig = Mesh.New()
    mesh_orig.getFromObject(ob.name)
    """

    bpy.ops.object.mode_set(mode='OBJECT')

    orig_frame = sce.current_frame
    sce.set_frame(PREF_STARTFRAME)
    me = ob.create_mesh(True, 'PREVIEW')

    #Flip y and z
    mat_flip= Mathutils.Matrix(\
    [1.0, 0.0, 0.0, 0.0],\
    [0.0, 0.0, 1.0, 0.0],\
    [0.0, 1.0, 0.0, 0.0],\
    [0.0, 0.0, 0.0, 1.0],\
    )

    numverts = len(me.verts)

    numframes = PREF_ENDFRAME-PREF_STARTFRAME+1
    PREF_FPS= float(PREF_FPS)
    f = open(filename, 'wb') #no Errors yet:Safe to create file
    
    # Write the header
    f.write(pack(">2i", numframes, numverts))
    
    # Write the frame times (should we use the time IPO??)
    f.write( pack(">%df" % (numframes), *[frame/PREF_FPS for frame in range(numframes)]) ) # seconds
    
    #rest frame needed to keep frames in sync
    """
    Blender.Set('curframe', PREF_STARTFRAME)
    me_tmp.getFromObject(ob.name)
    """

    check_vertcount(me,numverts)
    me.transform(mat_flip * ob.matrix)
    f.write(pack(">%df" % (numverts*3), *[axis for v in me.verts for axis in v.co]))
        
    for frame in range(PREF_STARTFRAME,PREF_ENDFRAME+1):#in order to start at desired frame
        """
        Blender.Set('curframe', frame)
        me_tmp.getFromObject(ob.name)
        """

        sce.set_frame(frame)
        me = ob.create_mesh(True, 'PREVIEW')
        check_vertcount(me,numverts)
        me.transform(mat_flip * ob.matrix)
        
        # Write the vertex data
        f.write(pack(">%df" % (numverts*3), *[axis for v in me.verts for axis in v.co]))
    
    """
    me_tmp.verts= None
    """
    f.close()
    
    print ('MDD Exported: %s frames:%d\n'% (filename, numframes-1))
    """
    Blender.Window.WaitCursor(0)
    Blender.Set('curframe', orig_frame)
    """
    sce.set_frame(orig_frame)

from bpy.props import *

class ExportMDD(bpy.types.Operator):
    '''Animated mesh to MDD vertex keyframe file.'''
    bl_idname = "export.mdd"
    bl_label = "Export MDD"

    # get first scene to get min and max properties for frames, fps

    minframe = 1
    maxframe = 300000
    minfps = 1
    maxfps = 120

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    path = StringProperty(name="File Path", description="File path used for exporting the MDD file", maxlen= 1024, default= "tmp.mdd")
    fps = IntProperty(name="Frames Per Second", description="Number of frames/second", min=minfps, max=maxfps, default= 25)
    start_frame = IntProperty(name="Start Frame", description="Start frame for baking", min=minframe,max=maxframe,default=1)
    end_frame = IntProperty(name="End Frame", description="End frame for baking", min=minframe, max=maxframe, default= 250)

    def poll(self, context):
        ob = context.active_object
        return (ob and ob.type=='MESH')

    def execute(self, context):
        if not self.path:
            raise Exception("filename not set")
        write(self.path, context.scene, context.active_object,
            self.start_frame, self.end_frame, self.fps )
        return ('FINISHED',)
    
    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return ('RUNNING_MODAL',)

bpy.ops.add(ExportMDD)

# Add to a menu
import dynamic_menu

def menu_func(self, context):
    default_path = bpy.data.filename.replace(".blend", ".mdd")
    self.layout.item_stringO(ExportMDD.bl_idname, "path", default_path, text="Vertex Keyframe Animation (.mdd)...")

menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)

if __name__=='__main__':
    bpy.ops.export.mdd(path="/tmp/test.mdd")
