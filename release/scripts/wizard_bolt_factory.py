#!BPY
# -*- coding: latin-1 -*-
"""
Name: 'Bolt Factory'
Blender: 248
Group: 'Wizards'
Tooltip: 'Create models of various types of screw fasteners.'
"""

__author__ = " Aaron Keith (Spudmn) "
__version__ = "2.02 2009/06/10"
__url__ = ["Author's site,http://sourceforge.net/projects/boltfactory/", "Blender,http://wiki.blender.org/index.php/Extensions:Py/Scripts/Manual/Misc/Bolt_Factory"]
__bpydoc__ = """\
Bolt_Factory.py 

Bolt Factory is a Python script for Blender 3D.

The script allows the user to create models of various types of screw fasteners.

For best results set the material to smooth and apply a Edge Split modifier
with default settings.


History:
 V2.02 10/06/09 by Aaron Keith

    -Added changes made by the Blender team.

 V2.01 26/05/09 by Aaron Keith

    -    Fixed normal's on Lock Nut

V2.00 22/05/09 by Aaron Keith

- Better error checking.
- Lock Nut and Hex Nut meshes added.
- Pre-sets for common metric bolts and nuts.
- Improved GUI.
- Meshes scaled to a smaller size
- Fixed bug when using crest and root percent other than 10%
- Can now create meshes in Edit Mode.  This will add to the 
  current mesh and align with the current view.

V1.00 01/04/08 by Aaron Keith

- This version is very much a work in progress.
- This is my first attempt to program in Python.  This version is
  unpolished and doesn't do much error checking.  Therefore 
  if the user sets strange variable the model created will be 
  as equally strange.

- To Do:
- Better error checking.
- More Head and Bit types. 
- Better documentation.


"""

# -------------------------------------------------------------------------- 
# Bolt_Factory.py 
# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
# 
# Copyright (C) 2009: Aaron Keith
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
# -------------------------------------------------------------------------- 




import Blender
from Blender import Draw, BGL,Mesh
from Blender import *
from math import *
from Blender import Mathutils
from Blender.Mathutils import *


#Global_Scale = 0.001    #1 blender unit = X mm
Global_Scale = 0.1    #1 blender unit = X mm
#Global_Scale = 1.0    #1 blender unit = X mm
Global_NutRad = 0.0
MAX_INPUT_NUMBER = 50

No_Event,On_Preset_Click,On_Apply_Click,On_Create_Click,On_Hex_Click, On_Cap_Click,On_Dome_Click,On_Pan_Click,On_Bit_None_Click,On_Bit_Allen_Click,On_Bit_Philips_Click,On_Exit_Click,On_Model_Bolt_Click,On_Model_Nut_Click,On_Hex_Nut_Click,On_Lock_Nut_Click,On_Test_Click = range(17)  # this is like a ENUM


Head_Type={'HEX' : [Draw.Create(1),On_Hex_Click,""],
           'CAP' : [Draw.Create(0),On_Cap_Click,""],
           'DOME': [Draw.Create(0),On_Dome_Click,""],
           'PAN' : [Draw.Create(0),On_Pan_Click,""]}


Bit_Type={'NONE' : [Draw.Create(1),On_Bit_None_Click,""],
           'ALLEN' : [Draw.Create(0),On_Bit_Allen_Click,""],
           'PHILLIPS': [Draw.Create(0),On_Bit_Philips_Click,""]}

Model_Type={'BOLT' : [Draw.Create(1),On_Model_Bolt_Click,"Bolt Settings"],
           'NUT' : [Draw.Create(0),On_Model_Nut_Click,"Nut Settings"]}

Nut_Type={'HEX' : [Draw.Create(1),On_Hex_Nut_Click,""],
           'LOCK' : [Draw.Create(0),On_Lock_Nut_Click,""]}


Phillips_Bit_Depth = Draw.Create(3.27)
Philips_Bit_Dia = Draw.Create(5.20)

Allen_Bit_Depth = Draw.Create(4.0)
Allen_Bit_Flat_Distance = Draw.Create(6.0)

Hex_Head_Height = Draw.Create(5.3)
Hex_Head_Flat_Distance = Draw.Create(13.0)

Cap_Head_Dia = Draw.Create(13.5)
Cap_Head_Height = Draw.Create(8.0)

Dome_Head_Dia = Draw.Create(16.0)

Pan_Head_Dia = Draw.Create(16.0)

Shank_Dia = Draw.Create(8.0)
Shank_Length = Draw.Create(0.0)

Thread_Length = Draw.Create(16.0)
Major_Dia = Draw.Create(8.0)
Minor_Dia = Draw.Create(6.917)
Pitch = Draw.Create(1.0)
Crest_Percent = Draw.Create(10)
Root_Percent = Draw.Create(10)

Hex_Nut_Height = Draw.Create(8.0)
Hex_Nut_Flat_Distance = Draw.Create(13.0)

Preset_Menu = Draw.Create(5)


##########################################################################################
##########################################################################################
##                    Miscellaneous Utilities
##########################################################################################
##########################################################################################

# Returns a list of verts rotated by the given matrix. Used by SpinDup
def Rot_Mesh(verts,matrix):
    return [list(Vector(v) * matrix) for v in verts]

# Returns a list of faces that has there index incremented by offset 
def Copy_Faces(faces,offset):        
    ret = []
    for f in faces:
        fsub = []
        for i in range(len(f)):
            fsub.append(f[i]+ offset)
        ret.append(fsub)
    return ret


# Much like Blenders built in SpinDup.
def SpinDup(VERTS,FACES,DEGREE,DIVISIONS,AXIS):
    verts=[]
    faces=[]
    
    if DIVISIONS == 0:
       DIVISIONS = 1  
  
    step = DEGREE/DIVISIONS # set step so pieces * step = degrees in arc
    
    for i in xrange(int(DIVISIONS)):
        rotmat = Mathutils.RotationMatrix(step*i, 4, AXIS) # 4x4 rotation matrix, 30d about the x axis.
        Rot = Rot_Mesh(VERTS,rotmat)
        faces.extend(Copy_Faces(FACES,len(verts)))    
        verts.extend(Rot)
    return verts,faces


# Returns a list of verts that have been moved up the z axis by DISTANCE
def Move_Verts_Up_Z(VERTS,DISTANCE):
    return [[v[0],v[1],v[2]+DISTANCE] for v in VERTS]


# Returns a list of verts and faces that has been mirrored in the AXIS 
def Mirror_Verts_Faces(VERTS,FACES,AXIS,FLIP_POINT =0):
    ret_vert = []
    ret_face = []
    offset = len(VERTS)    
    if AXIS == 'y':
        for v in VERTS:
            Delta = v[0] - FLIP_POINT
            ret_vert.append([FLIP_POINT-Delta,v[1],v[2]]) 
    if AXIS == 'x':
        for v in VERTS:
            Delta = v[1] - FLIP_POINT
            ret_vert.append([v[0],FLIP_POINT-Delta,v[2]]) 
    if AXIS == 'z':
        for v in VERTS:
            Delta = v[2] - FLIP_POINT
            ret_vert.append([v[0],v[1],FLIP_POINT-Delta]) 
            
    for f in FACES:
        fsub = []
        for i in range(len(f)):
            fsub.append(f[i]+ offset)
        fsub.reverse() # flip the order to make norm point out
        ret_face.append(fsub)
            
    return ret_vert,ret_face



# Returns a list of faces that 
# make up an array of 4 point polygon. 
def Build_Face_List_Quads(OFFSET,COLUM,ROW,FLIP = 0):
    Ret =[]
    RowStart = 0;
    for j in range(ROW):
        for i in range(COLUM):
            Res1 = RowStart + i;
            Res2 = RowStart + i + (COLUM +1)
            Res3 = RowStart + i + (COLUM +1) +1
            Res4 = RowStart+i+1
            if FLIP:
                Ret.append([OFFSET+Res1,OFFSET+Res2,OFFSET+Res3,OFFSET+Res4])
            else:
                Ret.append([OFFSET+Res4,OFFSET+Res3,OFFSET+Res2,OFFSET+Res1])
        RowStart += COLUM+1
    return Ret


# Returns a list of faces that makes up a fill pattern for a 
# circle
def Fill_Ring_Face(OFFSET,NUM,FACE_DOWN = 0):
    Ret =[]
    Face = [1,2,0]
    TempFace = [0,0,0]
    A = 0
    B = 1
    C = 2
    if NUM < 3:
        return None
    for i in range(NUM-2):
        if (i%2):
            TempFace[0] = Face[C];
            TempFace[1] = Face[C] + 1;
            TempFace[2] = Face[B];
            if FACE_DOWN:
                Ret.append([OFFSET+Face[2],OFFSET+Face[1],OFFSET+Face[0]])
            else:
                Ret.append([OFFSET+Face[0],OFFSET+Face[1],OFFSET+Face[2]])
        else:
            TempFace[0] =Face[C];
            if Face[C] == 0:
                TempFace[1] = NUM-1; 
            else:
                TempFace[1] = Face[C] - 1;
            TempFace[2] = Face[B];
            if FACE_DOWN:
                Ret.append([OFFSET+Face[0],OFFSET+Face[1],OFFSET+Face[2]])
            else:
                Ret.append([OFFSET+Face[2],OFFSET+Face[1],OFFSET+Face[0]])
        
        Face[0] = TempFace[0]
        Face[1] = TempFace[1]
        Face[2] = TempFace[2]
    return Ret
    

##########################################################################################
##########################################################################################
##                    Converter Functions For Bolt Factory 
##########################################################################################
##########################################################################################


def Flat_To_Radius(FLAT):
    h = (float(FLAT)/2)/cos(radians(30))
    return h

def Get_Phillips_Bit_Height(Bit_Dia):
    Flat_Width_half = (Bit_Dia*(0.5/1.82))/2.0
    Bit_Rad = Bit_Dia / 2.0
    x = Bit_Rad - Flat_Width_half
    y = tan(radians(60))*x
    return y 

##########################################################################################
##########################################################################################
##                    Error Checking
##########################################################################################
##########################################################################################


def Error_Check():

    #global Phillips_Bit_Depth 
    #global Philips_Bit_Dia 

    #global Allen_Bit_Depth 
    #global Allen_Bit_Flat_Distance 

    #global Hex_Head_Height 
    #global Hex_Head_Flat_Distance 

    #global Cap_Head_Dia 
    #global Cap_Head_Height 
    

    #global Dome_Head_Dia 

    #global Pan_Head_Dia 

    #global Shank_Dia 
    #global Shank_Length 

    global Thread_Length
    global Major_Dia 
    global Minor_Dia 
    global Pitch 
    global Hex_Nut_Flat_Distance
    global Model_Type
    #global Crest_Percent 
    #global Root_Percent 

    Error_Result = 0
    
    if Minor_Dia.val >= Major_Dia.val:
        error_txt = "Error%t|Major Dia must be larger than Minor Dia"
        Blender.Draw.PupMenu(error_txt)
        print error_txt
        Error_Result = TRUE  

    elif (Model_Type['BOLT'][0].val) and ((Pitch.val*7.0) > Thread_Length.val):
        error_txt =  "Error%t|Thread length must be at least 7 times the Pitch"
        Blender.Draw.PupMenu(error_txt)
        print error_txt
        Error_Result = TRUE  
    
    elif (Model_Type['NUT'][0].val) and (Hex_Nut_Flat_Distance.val < Major_Dia.val):
        error_txt =  "Error%t|Nut Flat Distance must be greater than Major Dia"
        Blender.Draw.PupMenu(error_txt)
        print error_txt
        Error_Result = TRUE  
    
    elif (Model_Type['NUT'][0].val) and ((Pitch.val * 2.5 )> Hex_Nut_Height.val):
        error_txt =  "Error%t|Nut Height must be greater than 2.5 * Pitch"
        Blender.Draw.PupMenu(error_txt)
        print error_txt
        Error_Result = TRUE  

    elif (Model_Type['BOLT'][0].val):
        Check_Head_Height = None
        Check_Bit_Height = None
        if (Bit_Type['ALLEN'][0].val):
            Check_Bit_Height = Allen_Bit_Depth.val
        if (Bit_Type['PHILLIPS'][0].val):
            Check_Bit_Height = Phillips_Bit_Depth.val
        if (Head_Type['HEX'][0].val):
            Check_Head_Height = Hex_Head_Height.val
        if (Head_Type['CAP'][0].val):
            Check_Head_Height = Cap_Head_Height.val
        
        if Check_Head_Height != None and Check_Bit_Height != None :
            if Check_Bit_Height  > Check_Head_Height:
                error_txt =  "Error%t|Bit Depth must not be greater that Head Height"
                Blender.Draw.PupMenu(error_txt)
                print error_txt
                Error_Result = TRUE
    
    
    return Error_Result 



##########################################################################################
##########################################################################################
##                    Create Allen Bit
##########################################################################################
##########################################################################################


def Allen_Fill(OFFSET,FLIP= 0):
    faces = []
    Lookup = [[19,1,0],
              [19,2,1],
              [19,3,2],
              [19,20,3],
              [20,4,3],
              [20,5,4],
              [20,6,5],
              [20,7,6],
              [20,8,7],
              [20,9,8],
              
              [20,21,9],
              
              [21,10,9],
              [21,11,10],
              [21,12,11],
              [21,13,12],
              [21,14,13],
              [21,15,14],
              
              [21,22,15],
              [22,16,15],
              [22,17,16],
              [22,18,17]
              ]
    for i in Lookup:
        if FLIP:
            faces.append([OFFSET+i[2],OFFSET+i[1],OFFSET+i[0]])
        else:
            faces.append([OFFSET+i[0],OFFSET+i[1],OFFSET+i[2]])
            
    return faces

def Allen_Bit_Dia(FLAT_DISTANCE):
    Flat_Radius = (float(FLAT_DISTANCE)/2.0)/cos(radians(30))
    return (Flat_Radius * 1.05) * 2.0
    
def Allen_Bit_Dia_To_Flat(DIA):
    Flat_Radius = (DIA/2.0)/1.05
    return (Flat_Radius * cos (radians(30)))* 2.0
    
    

def Create_Allen_Bit(FLAT_DISTANCE,HEIGHT):
    Div = 36
    verts = []
    faces = []
    
    Flat_Radius = (float(FLAT_DISTANCE)/2.0)/cos(radians(30))
    OUTTER_RADIUS = Flat_Radius * 1.05
    Outter_Radius_Height = Flat_Radius * (0.1/5.77)
    FaceStart_Outside = len(verts)
    Deg_Step = 360.0 /float(Div)
    
    for i in range((Div/2)+1):    # only do half and mirror later
        x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
        y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
        verts.append([x,y,0])
    
    FaceStart_Inside = len(verts)
        
    Deg_Step = 360.0 /float(6) 
    for i in range((6/2)+1): 
        x = sin(radians(i*Deg_Step))* Flat_Radius
        y = cos(radians(i*Deg_Step))* Flat_Radius
        verts.append([x,y,0-Outter_Radius_Height])     
     
    faces.extend(Allen_Fill(FaceStart_Outside,0))
    
    
    FaceStart_Bottom = len(verts)
    
    Deg_Step = 360.0 /float(6) 
    for i in range((6/2)+1): 
        x = sin(radians(i*Deg_Step))* Flat_Radius
        y = cos(radians(i*Deg_Step))* Flat_Radius
        verts.append([x,y,0-HEIGHT])     
        
    faces.extend(Build_Face_List_Quads(FaceStart_Inside,3,1,TRUE))
    faces.extend(Fill_Ring_Face(FaceStart_Bottom,4))
    
    
    M_Verts,M_Faces = Mirror_Verts_Faces(verts,faces,'y')
    verts.extend(M_Verts)
    faces.extend(M_Faces)
    
    return verts,faces,OUTTER_RADIUS * 2.0


##########################################################################################
##########################################################################################
##                    Create Phillips Bit
##########################################################################################
##########################################################################################


def Phillips_Fill(OFFSET,FLIP= 0):
    faces = []
    Lookup = [[0,1,10],
              [1,11,10],
              [1,2,11],
              [2,12,11],
              
              [2,3,12],
              [3,4,12],
              [4,5,12],
              [5,6,12],
              [6,7,12],
              
              [7,13,12],
              [7,8,13],
              [8,14,13],
              [8,9,14],
              
              
              [10,11,16,15],
              [11,12,16],
              [12,13,16],
              [13,14,17,16],
              [15,16,17,18]
              
              
              ]
    for i in Lookup:
        if FLIP:
            if len(i) == 3:
                faces.append([OFFSET+i[2],OFFSET+i[1],OFFSET+i[0]])
            else:    
                faces.append([OFFSET+i[3],OFFSET+i[2],OFFSET+i[1],OFFSET+i[0]])
        else:
            if len(i) == 3:
                faces.append([OFFSET+i[0],OFFSET+i[1],OFFSET+i[2]])
            else:
                faces.append([OFFSET+i[0],OFFSET+i[1],OFFSET+i[2],OFFSET+i[3]])
    return faces



def Create_Phillips_Bit(FLAT_DIA,FLAT_WIDTH,HEIGHT):
    Div = 36
    verts = []
    faces = []
    
    FLAT_RADIUS = FLAT_DIA * 0.5
    OUTTER_RADIUS = FLAT_RADIUS * 1.05
    
    Flat_Half = float(FLAT_WIDTH)/2.0
        
    FaceStart_Outside = len(verts)
    Deg_Step = 360.0 /float(Div)
    for i in range((Div/4)+1):    # only do half and mirror later
        x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
        y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
        verts.append([x,y,0])
    
        
    FaceStart_Inside = len(verts)
    verts.append([0,FLAT_RADIUS,0]) #10
    verts.append([Flat_Half,FLAT_RADIUS,0]) #11
    verts.append([Flat_Half,Flat_Half,0])     #12
    verts.append([FLAT_RADIUS,Flat_Half,0])    #13
    verts.append([FLAT_RADIUS,0,0])            #14

 
    verts.append([0,Flat_Half,0-HEIGHT])        #15
    verts.append([Flat_Half,Flat_Half,0-HEIGHT])    #16
    verts.append([Flat_Half,0,0-HEIGHT])            #17
    
    verts.append([0,0,0-HEIGHT])            #18
    
    faces.extend(Phillips_Fill(FaceStart_Outside,TRUE))

    Spin_Verts,Spin_Face = SpinDup(verts,faces,360,4,'z')
   
    return Spin_Verts,Spin_Face,OUTTER_RADIUS * 2
    

##########################################################################################
##########################################################################################
##                    Create Head Types
##########################################################################################
##########################################################################################

def Max_Pan_Bit_Dia(HEAD_DIA):
    HEAD_RADIUS = HEAD_DIA * 0.5
    XRad = HEAD_RADIUS * 1.976
    return (sin(radians(10))*XRad) * 2.0


def Create_Pan_Head(HOLE_DIA,HEAD_DIA,SHANK_DIA,HEIGHT,RAD1,RAD2,FACE_OFFSET):

    DIV = 36
    HOLE_RADIUS = HOLE_DIA * 0.5
    HEAD_RADIUS = HEAD_DIA * 0.5
    SHANK_RADIUS = SHANK_DIA * 0.5

    verts = []
    faces = []
    Row = 0
    BEVEL = HEIGHT * 0.01
    #Dome_Rad =  HEAD_RADIUS * (1.0/1.75)
    
    Dome_Rad = HEAD_RADIUS * 1.12
    RAD_Offset = HEAD_RADIUS * 0.96
    OtherRad = HEAD_RADIUS * 0.16
    OtherRad_X_Offset = HEAD_RADIUS * 0.84
    OtherRad_Z_Offset = HEAD_RADIUS * 0.504
    XRad = HEAD_RADIUS * 1.976
    ZRad = HEAD_RADIUS * 1.768
    EndRad = HEAD_RADIUS * 0.284
    EndZOffset = HEAD_RADIUS * 0.432
    HEIGHT = HEAD_RADIUS * 0.59
    
#    Dome_Rad =  5.6
#    RAD_Offset = 4.9
#    OtherRad = 0.8
#    OtherRad_X_Offset = 4.2
#    OtherRad_Z_Offset = 2.52
#    XRad = 9.88
#    ZRad = 8.84
#    EndRad = 1.42
#    EndZOffset = 2.16
#    HEIGHT = 2.95
    
    FaceStart = FACE_OFFSET

    z = cos(radians(10))*ZRad
    verts.append([HOLE_RADIUS,0.0,(0.0-ZRad)+z])
    Start_Height = 0 - ((0.0-ZRad)+z)
    Row += 1

    #for i in range(0,30,10):  was 0 to 30 more work needed to make this look good.
    for i in range(10,30,10):
        x = sin(radians(i))*XRad
        z = cos(radians(i))*ZRad
        verts.append([x,0.0,(0.0-ZRad)+z])
        Row += 1

    for i in range(20,140,10):
        x = sin(radians(i))*EndRad
        z = cos(radians(i))*EndRad
        if ((0.0 - EndZOffset)+z) < (0.0-HEIGHT):
            verts.append([(HEAD_RADIUS -EndRad)+x,0.0,0.0 - HEIGHT])
        else:
            verts.append([(HEAD_RADIUS -EndRad)+x,0.0,(0.0 - EndZOffset)+z])
        Row += 1
        
        
    verts.append([SHANK_RADIUS,0.0,(0.0-HEIGHT)])
    Row += 1
    
    verts.append([SHANK_RADIUS,0.0,(0.0-HEIGHT)-Start_Height])
    Row += 1


    sVerts,sFaces = SpinDup(verts,faces,360,DIV,'z')
    sVerts.extend(verts)        #add the start verts to the Spin verts to complete the loop
    
    faces.extend(Build_Face_List_Quads(FaceStart,Row-1,DIV))

    Global_Head_Height = HEIGHT ;

    
    return Move_Verts_Up_Z(sVerts,Start_Height),faces,HEIGHT



def Create_Dome_Head(HOLE_DIA,HEAD_DIA,SHANK_DIA,HEIGHT,RAD1,RAD2,FACE_OFFSET):
    DIV = 36
    HOLE_RADIUS = HOLE_DIA * 0.5
    HEAD_RADIUS = HEAD_DIA * 0.5
    SHANK_RADIUS = SHANK_DIA * 0.5
    
    verts = []
    faces = []
    Row = 0
    BEVEL = HEIGHT * 0.01
    #Dome_Rad =  HEAD_RADIUS * (1.0/1.75)
    
    Dome_Rad =  HEAD_RADIUS * 1.12
    #Head_Height = HEAD_RADIUS * 0.78
    RAD_Offset = HEAD_RADIUS * 0.98
    Dome_Height = HEAD_RADIUS * 0.64
    OtherRad = HEAD_RADIUS * 0.16
    OtherRad_X_Offset = HEAD_RADIUS * 0.84
    OtherRad_Z_Offset = HEAD_RADIUS * 0.504
    
    
#    Dome_Rad =  5.6
#    RAD_Offset = 4.9
#    Dome_Height = 3.2
#    OtherRad = 0.8
#    OtherRad_X_Offset = 4.2
#    OtherRad_Z_Offset = 2.52
#    
    
    FaceStart = FACE_OFFSET
    
    verts.append([HOLE_RADIUS,0.0,0.0])
    Row += 1


    for i in range(0,60,10):
        x = sin(radians(i))*Dome_Rad
        z = cos(radians(i))*Dome_Rad
        if ((0.0-RAD_Offset)+z) <= 0:
            verts.append([x,0.0,(0.0-RAD_Offset)+z])
            Row += 1


    for i in range(60,160,10):
        x = sin(radians(i))*OtherRad
        z = cos(radians(i))*OtherRad
        z = (0.0-OtherRad_Z_Offset)+z
        if z < (0.0-Dome_Height):
            z = (0.0-Dome_Height)
        verts.append([OtherRad_X_Offset+x,0.0,z])
        Row += 1
        
    verts.append([SHANK_RADIUS,0.0,(0.0-Dome_Height)])
    Row += 1


    sVerts,sFaces = SpinDup(verts,faces,360,DIV,'z')
    sVerts.extend(verts)        #add the start verts to the Spin verts to complete the loop
    
    faces.extend(Build_Face_List_Quads(FaceStart,Row-1,DIV))

    return sVerts,faces,Dome_Height



def Create_Cap_Head(HOLE_DIA,HEAD_DIA,SHANK_DIA,HEIGHT,RAD1,RAD2):
    DIV = 36
    
    HOLE_RADIUS = HOLE_DIA * 0.5
    HEAD_RADIUS = HEAD_DIA * 0.5
    SHANK_RADIUS = SHANK_DIA * 0.5
    
    verts = []
    faces = []
    Row = 0
    BEVEL = HEIGHT * 0.01
    
    
    FaceStart = len(verts)

    verts.append([HOLE_RADIUS,0.0,0.0])
    Row += 1

    #rad
    
    for i in range(0,100,10):
        x = sin(radians(i))*RAD1
        z = cos(radians(i))*RAD1
        verts.append([(HEAD_RADIUS-RAD1)+x,0.0,(0.0-RAD1)+z])
        Row += 1
    
    
    verts.append([HEAD_RADIUS,0.0,0.0-HEIGHT+BEVEL])
    Row += 1

    verts.append([HEAD_RADIUS-BEVEL,0.0,0.0-HEIGHT])
    Row += 1

    #rad2
   
    for i in range(0,100,10):
        x = sin(radians(i))*RAD2
        z = cos(radians(i))*RAD2
        verts.append([(SHANK_RADIUS+RAD2)-x,0.0,(0.0-HEIGHT-RAD2)+z])
        Row += 1
    

    sVerts,sFaces = SpinDup(verts,faces,360,DIV,'z')
    sVerts.extend(verts)        #add the start verts to the Spin verts to complete the loop
    

    faces.extend(Build_Face_List_Quads(FaceStart,Row-1,DIV))
    
    return sVerts,faces,HEIGHT+RAD2



def Create_Hex_Head(FLAT,HOLE_DIA,SHANK_DIA,HEIGHT):
    
    verts = []
    faces = []
    HOLE_RADIUS = HOLE_DIA * 0.5
    Half_Flat = FLAT/2
    TopBevelRadius = Half_Flat - (Half_Flat* (0.05/8))
    Undercut_Height = (Half_Flat* (0.05/8))
    Shank_Bevel = (Half_Flat* (0.05/8)) 
    Flat_Height = HEIGHT - Undercut_Height - Shank_Bevel
    #Undercut_Height = 5
    SHANK_RADIUS = SHANK_DIA/2
    Row = 0;

    verts.append([0.0,0.0,0.0])
    
    
    FaceStart = len(verts)
    #inner hole
    
    x = sin(radians(0))*HOLE_RADIUS
    y = cos(radians(0))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/6))*HOLE_RADIUS
    y = cos(radians(60/6))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/3))*HOLE_RADIUS
    y = cos(radians(60/3))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/2))*HOLE_RADIUS
    y = cos(radians(60/2))*HOLE_RADIUS
    verts.append([x,y,0.0])
    Row += 1
    
    #bevel
    
    x = sin(radians(0))*TopBevelRadius
    y = cos(radians(0))*TopBevelRadius
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/6))*TopBevelRadius
    y = cos(radians(60/6))*TopBevelRadius
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/3))*TopBevelRadius
    y = cos(radians(60/3))*TopBevelRadius
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/2))*TopBevelRadius
    y = cos(radians(60/2))*TopBevelRadius
    vec4 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    Row += 1
    
    #Flats
    
    x = tan(radians(0))*Half_Flat
    dvec = vec1 - Mathutils.Vector([x,Half_Flat,0.0])
    verts.append([x,Half_Flat,-dvec.length])
    
    
    x = tan(radians(60/6))*Half_Flat
    dvec = vec2 - Mathutils.Vector([x,Half_Flat,0.0])
    verts.append([x,Half_Flat,-dvec.length])
    

    x = tan(radians(60/3))*Half_Flat
    dvec = vec3 - Mathutils.Vector([x,Half_Flat,0.0])
    Lowest_Point = -dvec.length
    verts.append([x,Half_Flat,-dvec.length])
    

    x = tan(radians(60/2))*Half_Flat
    dvec = vec4 - Mathutils.Vector([x,Half_Flat,0.0])
    Lowest_Point = -dvec.length
    verts.append([x,Half_Flat,-dvec.length])
    Row += 1
    
    #down Bits Tri
    x = tan(radians(0))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    
    x = tan(radians(60/6))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])

    x = tan(radians(60/3))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    
    x = tan(radians(60/2))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    Row += 1

    #down Bits
    
    x = tan(radians(0))*Half_Flat
    verts.append([x,Half_Flat,-Flat_Height])
    
    x = tan(radians(60/6))*Half_Flat
    verts.append([x,Half_Flat,-Flat_Height])

    x = tan(radians(60/3))*Half_Flat
    verts.append([x,Half_Flat,-Flat_Height])
    
    x = tan(radians(60/2))*Half_Flat
    verts.append([x,Half_Flat,-Flat_Height])
    Row += 1
    
    
    #under cut 
       
    x = sin(radians(0))*Half_Flat
    y = cos(radians(0))*Half_Flat
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height])
    
    x = sin(radians(60/6))*Half_Flat
    y = cos(radians(60/6))*Half_Flat
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height])
    
    x = sin(radians(60/3))*Half_Flat
    y = cos(radians(60/3))*Half_Flat
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height])
    
    x = sin(radians(60/2))*Half_Flat
    y = cos(radians(60/2))*Half_Flat
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height])
    Row += 1
    
    #under cut down bit
    x = sin(radians(0))*Half_Flat
    y = cos(radians(0))*Half_Flat
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/6))*Half_Flat
    y = cos(radians(60/6))*Half_Flat
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/3))*Half_Flat
    y = cos(radians(60/3))*Half_Flat
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/2))*Half_Flat
    y = cos(radians(60/2))*Half_Flat
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    Row += 1
    
    #under cut to Shank BEVEAL
    x = sin(radians(0))*(SHANK_RADIUS+Shank_Bevel)
    y = cos(radians(0))*(SHANK_RADIUS+Shank_Bevel)
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/6))*(SHANK_RADIUS+Shank_Bevel)
    y = cos(radians(60/6))*(SHANK_RADIUS+Shank_Bevel)
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/3))*(SHANK_RADIUS+Shank_Bevel)
    y = cos(radians(60/3))*(SHANK_RADIUS+Shank_Bevel)
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    
    x = sin(radians(60/2))*(SHANK_RADIUS+Shank_Bevel)
    y = cos(radians(60/2))*(SHANK_RADIUS+Shank_Bevel)
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height])
    Row += 1
    
    #under cut to Shank BEVEAL
    x = sin(radians(0))*SHANK_RADIUS
    y = cos(radians(0))*SHANK_RADIUS
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height-Shank_Bevel])
    
    x = sin(radians(60/6))*SHANK_RADIUS
    y = cos(radians(60/6))*SHANK_RADIUS
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height-Shank_Bevel])
    
    x = sin(radians(60/3))*SHANK_RADIUS
    y = cos(radians(60/3))*SHANK_RADIUS
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height-Shank_Bevel])
    
    x = sin(radians(60/2))*SHANK_RADIUS
    y = cos(radians(60/2))*SHANK_RADIUS
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,-Flat_Height-Undercut_Height-Shank_Bevel])
    Row += 1
    
    
    #Global_Head_Height = 0 - (-HEIGHT-0.1)
    faces.extend(Build_Face_List_Quads(FaceStart,3,Row - 1))
       
    
    Mirror_Verts,Mirror_Faces = Mirror_Verts_Faces(verts,faces,'y')
    verts.extend(Mirror_Verts)
    faces.extend(Mirror_Faces)
    
    Spin_Verts,Spin_Faces = SpinDup(verts,faces,360,6,'z')
    
    return Spin_Verts,Spin_Faces,0 - (-HEIGHT)
   
    
##########################################################################################
##########################################################################################
##                    Create Bolt
##########################################################################################
##########################################################################################



def MakeBolt():
    global Phillips_Bit_Depth 
    global Philips_Bit_Dia 

    global Allen_Bit_Depth 
    global Allen_Bit_Flat_Distance 

    global Hex_Head_Height 
    global Hex_Head_Flat_Distance 

    global Cap_Head_Dia 
    global Cap_Head_Height 
    

    global Dome_Head_Dia 

    global Pan_Head_Dia 

    global Shank_Dia 
    global Shank_Length 

    global Thread_Length
    global Major_Dia 
    global Minor_Dia 
    global Pitch 
    global Crest_Percent 
    global Root_Percent 
    
    verts = []
    faces = []
    Bit_Verts = []
    Bit_Faces = []
    Bit_Dia = 0.001
    Head_Verts = []
    Head_Faces= []
    Head_Height = 0.0
    ReSized_Allen_Bit_Flat_Distance = Allen_Bit_Flat_Distance.val  # set default  
   
    
    Head_Height = Hex_Head_Height.val # will be changed by the Head Functions
    
    if Bit_Type['ALLEN'][0].val and Head_Type['PAN'][0].val:
        #need to size Allen bit if it is too big.
        if  Allen_Bit_Dia(Allen_Bit_Flat_Distance.val) > Max_Pan_Bit_Dia(Pan_Head_Dia.val):
            ReSized_Allen_Bit_Flat_Distance = Allen_Bit_Dia_To_Flat(Max_Pan_Bit_Dia(Pan_Head_Dia.val)) * 1.05
            print "Resized Allen Bit Flat Distance to ",ReSized_Allen_Bit_Flat_Distance 
 
    #bit Mesh
    if Bit_Type['ALLEN'][0].val:
        Bit_Verts,Bit_Faces,Bit_Dia = Create_Allen_Bit(ReSized_Allen_Bit_Flat_Distance,Allen_Bit_Depth.val)
    
    if Bit_Type['PHILLIPS'][0].val:
        Bit_Verts,Bit_Faces,Bit_Dia = Create_Phillips_Bit(Philips_Bit_Dia.val,Philips_Bit_Dia.val*(0.5/1.82),Phillips_Bit_Depth.val)
   
        
    #Head Mesh
    if Head_Type['HEX'][0].val:  
        Head_Verts,Head_Faces,Head_Height = Create_Hex_Head(Hex_Head_Flat_Distance.val,Bit_Dia,Shank_Dia.val,Hex_Head_Height.val)

    elif Head_Type['CAP'][0].val:  
        Head_Verts,Head_Faces,Head_Height = Create_Cap_Head(Bit_Dia,Cap_Head_Dia.val,Shank_Dia.val,Cap_Head_Height.val,Cap_Head_Dia.val*(1.0/19.0),Cap_Head_Dia.val*(1.0/19.0))
        
    elif Head_Type['DOME'][0].val:  
        Head_Verts,Head_Faces,Head_Height = Create_Dome_Head(Bit_Dia,Dome_Head_Dia.val,Shank_Dia.val,Hex_Head_Height.val,1,1,0)
    
    elif Head_Type['PAN'][0].val:  
        Head_Verts,Head_Faces,Head_Height = Create_Pan_Head(Bit_Dia,Pan_Head_Dia.val,Shank_Dia.val,Hex_Head_Height.val,1,1,0)


    Face_Start = len(verts)
    verts.extend(Move_Verts_Up_Z(Bit_Verts,Head_Height))
    faces.extend(Copy_Faces(Bit_Faces,Face_Start))

    Face_Start = len(verts)
    verts.extend(Move_Verts_Up_Z(Head_Verts,Head_Height))
    faces.extend(Copy_Faces(Head_Faces,Face_Start))

    Face_Start = len(verts)
    Thread_Verts,Thread_Faces,Thread_Height = Create_External_Thread(Shank_Dia.val,Shank_Length.val,Minor_Dia.val,Major_Dia.val,Pitch.val,Thread_Length.val,Crest_Percent.val,Root_Percent.val)

    verts.extend(Move_Verts_Up_Z(Thread_Verts,00))
    faces.extend(Copy_Faces(Thread_Faces,Face_Start))
    
    return Move_Verts_Up_Z(verts,Thread_Height),faces







##########################################################################################
##########################################################################################
##                    Create Internal Thread
##########################################################################################
##########################################################################################


def Create_Internal_Thread_Start_Verts(verts,INNER_RADIUS,OUTTER_RADIUS,PITCH,DIV,CREST_PERCENT,ROOT_PERCENT,Height_Offset):
    
    
    Ret_Row = 0;
    
    Height_Offset = Height_Offset + PITCH  #Move the offset up so that the verts start at 
                                           #at the correct place  (Height_Start)
    
    Half_Pitch = float(PITCH)/2
    Height_Start = Height_Offset - PITCH 
    Height_Step = float(PITCH)/float(DIV)
    Deg_Step = 360.0 /float(DIV)
    
    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0
    

    Rank = float(OUTTER_RADIUS - INNER_RADIUS)/float(DIV)
    for j in range(1):
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z])
        Height_Offset -= Crest_Height
        Ret_Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z ])
        Height_Offset -= Crest_to_Root_Height
        Ret_Row += 1
    
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
        Height_Offset -= Root_Height
        Ret_Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS

            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
        Height_Offset -= Root_to_Crest_Height
        Ret_Row += 1
   
    return Ret_Row,Height_Offset


def Create_Internal_Thread_End_Verts(verts,INNER_RADIUS,OUTTER_RADIUS,PITCH,DIV,CREST_PERCENT,ROOT_PERCENT,Height_Offset):
    
    
    Ret_Row = 0;
    
    Half_Pitch = float(PITCH)/2
    #Height_End = Height_Offset - PITCH - PITCH - PITCH- PITCH - PITCH- PITCH
    Height_End = Height_Offset - PITCH 
    #Height_End = -2.1
    Height_Step = float(PITCH)/float(DIV)
    Deg_Step = 360.0 /float(DIV)
    
    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0
   
    

    Rank = float(OUTTER_RADIUS - INNER_RADIUS)/float(DIV)
    
    Num = 0
    
    for j in range(2):
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z < Height_End:
                z = Height_End
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z])
        Height_Offset -= Crest_Height
        Ret_Row += 1
    
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z < Height_End:
                z = Height_End
            
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z ])
        Height_Offset -= Crest_to_Root_Height
        Ret_Row += 1
    
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z < Height_End:
                z = Height_End
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            if j == Num:
                x = sin(radians(i*Deg_Step))*(INNER_RADIUS + (i*Rank))
                y = cos(radians(i*Deg_Step))*(INNER_RADIUS + (i*Rank))
            if j > Num:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS)
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS )
                
            verts.append([x,y,z ])
        Height_Offset -= Root_Height
        Ret_Row += 1
    
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z < Height_End:
                z = Height_End
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS

            if j == Num:
                x = sin(radians(i*Deg_Step))*(INNER_RADIUS + (i*Rank))
                y = cos(radians(i*Deg_Step))*(INNER_RADIUS + (i*Rank))
            if j > Num:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS )
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS )
                
            verts.append([x,y,z ])
        Height_Offset -= Root_to_Crest_Height
        Ret_Row += 1

       
    return Ret_Row,Height_End  # send back Height End as this is the lowest point


def Create_Internal_Thread(INNER_DIA,OUTTER_DIA,PITCH,HEIGHT,CREST_PERCENT,ROOT_PERCENT,INTERNAL = 1):
    verts = []
    faces = []
    
    DIV = 36
    
    INNER_RADIUS = INNER_DIA/2
    OUTTER_RADIUS = OUTTER_DIA/2
    
    Half_Pitch = float(PITCH)/2
    Deg_Step = 360.0 /float(DIV)
    Height_Step = float(PITCH)/float(DIV)
            
    Num = int(round((HEIGHT- PITCH)/PITCH))  # less one pitch for the start and end that is 1/2 pitch high    
    
    Col = 0
    Row = 0
    
    
    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0
    
    Height_Offset = 0
    FaceStart = len(verts)
    
    Row_Inc,Height_Offset = Create_Internal_Thread_Start_Verts(verts,INNER_RADIUS,OUTTER_RADIUS,PITCH,DIV,CREST_PERCENT,ROOT_PERCENT,Height_Offset)
    Row += Row_Inc
    
    for j in range(Num):
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,Height_Offset - (Height_Step*i) ])
        Height_Offset -= Crest_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,Height_Offset - (Height_Step*i) ])
        Height_Offset -= Crest_to_Root_Height
        Row += 1
    
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            verts.append([x,y,Height_Offset - (Height_Step*i) ])
        Height_Offset -= Root_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            verts.append([x,y,Height_Offset - (Height_Step*i) ])
        Height_Offset -= Root_to_Crest_Height
        Row += 1
    

    Row_Inc,Height_Offset = Create_Internal_Thread_End_Verts(verts,INNER_RADIUS,OUTTER_RADIUS,PITCH,DIV,CREST_PERCENT,ROOT_PERCENT,Height_Offset)
    Row += Row_Inc
    
    faces.extend(Build_Face_List_Quads(FaceStart,DIV,Row -1,INTERNAL))
    
    return verts,faces,0 - Height_Offset



##########################################################################################
##########################################################################################
##                    Create External Thread
##########################################################################################
##########################################################################################



def Thread_Start3(verts,INNER_RADIUS,OUTTER_RADIUS,PITCH,DIV,CREST_PERCENT,ROOT_PERCENT,Height_Offset):
    
    
    Ret_Row = 0;
    
    Half_Pitch = float(PITCH)/2
    Height_Start = Height_Offset - PITCH
    Height_Step = float(PITCH)/float(DIV)
    Deg_Step = 360.0 /float(DIV)
    
    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0
   
#theard start

    Rank = float(OUTTER_RADIUS - INNER_RADIUS)/float(DIV)
    for j in range(4):
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z])
        Height_Offset -= Crest_Height
        Ret_Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z ])
        Height_Offset -= Crest_to_Root_Height
        Ret_Row += 1
    
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
        Height_Offset -= Root_Height
        Ret_Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS

            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
        Height_Offset -= Root_to_Crest_Height
        Ret_Row += 1
   
    return Ret_Row,Height_Offset


def Create_Shank_Verts(START_DIA,OUTTER_DIA,LENGTH,Z_LOCATION = 0):

    verts = []
    DIV = 36
    
    START_RADIUS = START_DIA/2
    OUTTER_RADIUS = OUTTER_DIA/2
    
    Opp = abs(START_RADIUS - OUTTER_RADIUS)
    Taper_Lentgh = Opp/tan(radians(31));
    
    if Taper_Lentgh > LENGTH:
        Taper_Lentgh = 0
    
    Stright_Length = LENGTH - Taper_Lentgh
    
    Deg_Step = 360.0 /float(DIV)
    
    Row = 0
    
    Lowest_Z_Vert = 0;    
    
    Height_Offset = Z_LOCATION


        #ring
    for i in range(DIV+1): 
        x = sin(radians(i*Deg_Step))*START_RADIUS
        y = cos(radians(i*Deg_Step))*START_RADIUS
        z =  Height_Offset - 0
        verts.append([x,y,z])
        Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Height_Offset -= Stright_Length
    Row += 1

    for i in range(DIV+1): 
        x = sin(radians(i*Deg_Step))*START_RADIUS
        y = cos(radians(i*Deg_Step))*START_RADIUS
        z =  Height_Offset - 0
        verts.append([x,y,z])
        Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Height_Offset -= Taper_Lentgh
    Row += 1


    return verts,Row,Height_Offset


def Create_Thread_Start_Verts(INNER_DIA,OUTTER_DIA,PITCH,CREST_PERCENT,ROOT_PERCENT,Z_LOCATION = 0):
    
    verts = []
    DIV = 36
    
    INNER_RADIUS = INNER_DIA/2
    OUTTER_RADIUS = OUTTER_DIA/2
    
    Half_Pitch = float(PITCH)/2
    Deg_Step = 360.0 /float(DIV)
    Height_Step = float(PITCH)/float(DIV)

    Row = 0
    
    Lowest_Z_Vert = 0;    
    
    Height_Offset = Z_LOCATION
        
    Height_Start = Height_Offset 
    
    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0

    Rank = float(OUTTER_RADIUS - INNER_RADIUS)/float(DIV)
    
    Height_Offset = Z_LOCATION + PITCH 
    Cut_off = Z_LOCATION
  
    
    for j in range(1):
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i)
            if z > Cut_off : z = Cut_off
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i)
            if z > Cut_off : z = Cut_off
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_to_Root_Height
        Row += 1
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i)
            if z > Cut_off : z = Cut_off 
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i)
            if z > Cut_off : z = Cut_off 
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_to_Crest_Height
        Row += 1
    
    
    for j in range(2):
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_Height
        Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            verts.append([x,y,z ])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_to_Root_Height
        Row += 1
    
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_Height
        Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i) 
            if z > Height_Start:
                z = Height_Start
            
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS

            if j == 0:
                x = sin(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
                y = cos(radians(i*Deg_Step))*(OUTTER_RADIUS - (i*Rank))
            verts.append([x,y,z ])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_to_Crest_Height
        Row += 1
        
   
    return verts,Row,Height_Offset



def Create_Thread_Verts(INNER_DIA,OUTTER_DIA,PITCH,HEIGHT,CREST_PERCENT,ROOT_PERCENT,Z_LOCATION = 0):
    verts = []
        
    DIV = 36
    
    INNER_RADIUS = INNER_DIA/2
    OUTTER_RADIUS = OUTTER_DIA/2
    
    Half_Pitch = float(PITCH)/2
    Deg_Step = 360.0 /float(DIV)
    Height_Step = float(PITCH)/float(DIV)

    NUM_OF_START_THREADS = 4.0
    NUM_OF_END_THREADS = 3.0
    Num = int((HEIGHT- ((NUM_OF_START_THREADS*PITCH) + (NUM_OF_END_THREADS*PITCH) ))/PITCH)
    Row = 0
    

    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0


    Height_Offset = Z_LOCATION
    
    Lowest_Z_Vert = 0;
    FaceStart = len(verts)
    
    
    for j in range(Num):
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i) 
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*OUTTER_RADIUS
            y = cos(radians(i*Deg_Step))*OUTTER_RADIUS
            z = Height_Offset - (Height_Step*i)
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_to_Root_Height
        Row += 1
    
        
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            z = Height_Offset - (Height_Step*i) 
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_Height
        Row += 1
    
        for i in range(DIV+1):
            x = sin(radians(i*Deg_Step))*INNER_RADIUS
            y = cos(radians(i*Deg_Step))*INNER_RADIUS
            z = Height_Offset - (Height_Step*i) 
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_to_Crest_Height
        Row += 1
    
    return verts,Row,Height_Offset



def Create_Thread_End_Verts(INNER_DIA,OUTTER_DIA,PITCH,CREST_PERCENT,ROOT_PERCENT,Z_LOCATION = 0):
    verts = []
        
    DIV = 36

    INNER_RADIUS = INNER_DIA/2
    OUTTER_RADIUS = OUTTER_DIA/2
    
    Half_Pitch = float(PITCH)/2
    Deg_Step = 360.0 /float(DIV)
    Height_Step = float(PITCH)/float(DIV)

    Crest_Height = float(PITCH) * float(CREST_PERCENT)/float(100)
    Root_Height = float(PITCH) * float(ROOT_PERCENT)/float(100)
    Root_to_Crest_Height = Crest_to_Root_Height = (float(PITCH) - (Crest_Height + Root_Height))/2.0
       
    Col = 0
    Row = 0
    
    Height_Offset = Z_LOCATION 
    
    Tapper_Height_Start = Height_Offset - PITCH - PITCH 
    
    Max_Height = Tapper_Height_Start - PITCH 
    
    Lowest_Z_Vert = 0;
    
    FaceStart = len(verts)
    for j in range(4):
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i)
            z = max(z,Max_Height)
            Tapper_Radius = OUTTER_RADIUS
            if z < Tapper_Height_Start:
                Tapper_Radius = OUTTER_RADIUS - (Tapper_Height_Start - z)

            x = sin(radians(i*Deg_Step))*(Tapper_Radius)
            y = cos(radians(i*Deg_Step))*(Tapper_Radius)
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_Height
        Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i)
            z = max(z,Max_Height)
            Tapper_Radius = OUTTER_RADIUS
            if z < Tapper_Height_Start:
                Tapper_Radius = OUTTER_RADIUS - (Tapper_Height_Start - z)

            x = sin(radians(i*Deg_Step))*(Tapper_Radius)
            y = cos(radians(i*Deg_Step))*(Tapper_Radius)
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Crest_to_Root_Height
        Row += 1
    
        
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i)
            z = max(z,Max_Height)
            Tapper_Radius = OUTTER_RADIUS - (Tapper_Height_Start - z)
            if Tapper_Radius > INNER_RADIUS:
               Tapper_Radius = INNER_RADIUS
            
            x = sin(radians(i*Deg_Step))*(Tapper_Radius)
            y = cos(radians(i*Deg_Step))*(Tapper_Radius)
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_Height
        Row += 1
    
        for i in range(DIV+1):
            z = Height_Offset - (Height_Step*i)
            z = max(z,Max_Height)
            Tapper_Radius = OUTTER_RADIUS - (Tapper_Height_Start - z)
            if Tapper_Radius > INNER_RADIUS:
               Tapper_Radius = INNER_RADIUS
            
            x = sin(radians(i*Deg_Step))*(Tapper_Radius)
            y = cos(radians(i*Deg_Step))*(Tapper_Radius)
            verts.append([x,y,z])
            Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Height_Offset -= Root_to_Crest_Height
        Row += 1
    
    return verts,Row,Height_Offset,Lowest_Z_Vert




def Create_External_Thread(SHANK_DIA,SHANK_LENGTH,INNER_DIA,OUTTER_DIA,PITCH,LENGTH,CREST_PERCENT,ROOT_PERCENT):
    
    verts = []
    faces = []

    DIV = 36
    
    Total_Row = 0
    Thread_Len = 0;
    
    Face_Start = len(verts)
    Offset = 0.0;
    
                                             
    Shank_Verts,Shank_Row,Offset = Create_Shank_Verts(SHANK_DIA,OUTTER_DIA,SHANK_LENGTH,Offset)
    Total_Row += Shank_Row

    Thread_Start_Verts,Thread_Start_Row,Offset = Create_Thread_Start_Verts(INNER_DIA,OUTTER_DIA,PITCH,CREST_PERCENT,ROOT_PERCENT,Offset)
    Total_Row += Thread_Start_Row
    
    
    Thread_Verts,Thread_Row,Offset = Create_Thread_Verts(INNER_DIA,OUTTER_DIA,PITCH,LENGTH,CREST_PERCENT,ROOT_PERCENT,Offset)
    Total_Row += Thread_Row
    
    
    Thread_End_Verts,Thread_End_Row,Offset,Lowest_Z_Vert = Create_Thread_End_Verts(INNER_DIA,OUTTER_DIA,PITCH,CREST_PERCENT,ROOT_PERCENT,Offset )
    Total_Row += Thread_End_Row       
    
    
    verts.extend(Shank_Verts)
    verts.extend(Thread_Start_Verts)
    verts.extend(Thread_Verts)
    verts.extend(Thread_End_Verts)
    
    faces.extend(Build_Face_List_Quads(Face_Start,DIV,Total_Row -1,0))
    faces.extend(Fill_Ring_Face(len(verts)-DIV,DIV,1))
    
    return verts,faces,0.0 - Lowest_Z_Vert
 

##########################################################################################
##########################################################################################
##                    Create Nut
##########################################################################################
##########################################################################################


def add_Hex_Nut(FLAT,HOLE_DIA,HEIGHT):
    global Global_Head_Height
    global Global_NutRad
    
    verts = []
    faces = []
    HOLE_RADIUS = HOLE_DIA * 0.5
    Half_Flat = FLAT/2
    Half_Height = HEIGHT/2
    TopBevelRadius = Half_Flat - 0.05
    
    Global_NutRad =  TopBevelRadius
    
    Row = 0;
    Lowest_Z_Vert = 0.0;

    verts.append([0.0,0.0,0.0])
    
    
    FaceStart = len(verts)
    #inner hole
    
    x = sin(radians(0))*HOLE_RADIUS
    y = cos(radians(0))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/6))*HOLE_RADIUS
    y = cos(radians(60/6))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/3))*HOLE_RADIUS
    y = cos(radians(60/3))*HOLE_RADIUS
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/2))*HOLE_RADIUS
    y = cos(radians(60/2))*HOLE_RADIUS
    verts.append([x,y,0.0])
    Row += 1
    
    #bevel
    
    x = sin(radians(0))*TopBevelRadius
    y = cos(radians(0))*TopBevelRadius
    vec1 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/6))*TopBevelRadius
    y = cos(radians(60/6))*TopBevelRadius
    vec2 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/3))*TopBevelRadius
    y = cos(radians(60/3))*TopBevelRadius
    vec3 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    
    
    x = sin(radians(60/2))*TopBevelRadius
    y = cos(radians(60/2))*TopBevelRadius
    vec4 = Mathutils.Vector([x,y,0.0])
    verts.append([x,y,0.0])
    Row += 1
    
    #Flats
    
    x = tan(radians(0))*Half_Flat
    dvec = vec1 - Mathutils.Vector([x,Half_Flat,0.0])
    verts.append([x,Half_Flat,-dvec.length])
    Lowest_Z_Vert = min(Lowest_Z_Vert,-dvec.length)
    
    
    x = tan(radians(60/6))*Half_Flat
    dvec = vec2 - Mathutils.Vector([x,Half_Flat,0.0])
    verts.append([x,Half_Flat,-dvec.length])
    Lowest_Z_Vert = min(Lowest_Z_Vert,-dvec.length)
    

    x = tan(radians(60/3))*Half_Flat
    dvec = vec3 - Mathutils.Vector([x,Half_Flat,0.0])
    Lowest_Point = -dvec.length
    verts.append([x,Half_Flat,-dvec.length])
    Lowest_Z_Vert = min(Lowest_Z_Vert,-dvec.length)

    x = tan(radians(60/2))*Half_Flat
    dvec = vec4 - Mathutils.Vector([x,Half_Flat,0.0])
    Lowest_Point = -dvec.length
    verts.append([x,Half_Flat,-dvec.length])
    Lowest_Z_Vert = min(Lowest_Z_Vert,-dvec.length)
    Row += 1
    
    #down Bits Tri
    x = tan(radians(0))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    
    
    x = tan(radians(60/6))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])

    x = tan(radians(60/3))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    
    x = tan(radians(60/2))*Half_Flat
    verts.append([x,Half_Flat,Lowest_Point])
    Lowest_Z_Vert = min(Lowest_Z_Vert,Lowest_Point)
    Row += 1

    #down Bits
    
    x = tan(radians(0))*Half_Flat
    verts.append([x,Half_Flat,-Half_Height])
    
    x = tan(radians(60/6))*Half_Flat
    verts.append([x,Half_Flat,-Half_Height])

    x = tan(radians(60/3))*Half_Flat
    verts.append([x,Half_Flat,-Half_Height])
    
    x = tan(radians(60/2))*Half_Flat
    verts.append([x,Half_Flat,-Half_Height])
    Lowest_Z_Vert = min(Lowest_Z_Vert,-Half_Height)
    Row += 1

    
    
    
    faces.extend(Build_Face_List_Quads(FaceStart,3,Row - 1))


    Global_Head_Height = HEIGHT
    
    Tvert,tface = Mirror_Verts_Faces(verts,faces,'z',Lowest_Z_Vert)
    verts.extend(Tvert)
    faces.extend(tface)
           
    
    Tvert,tface = Mirror_Verts_Faces(verts,faces,'y')
    verts.extend(Tvert)
    faces.extend(tface)
    
    S_verts,S_faces = SpinDup(verts,faces,360,6,'z')
    return S_verts,S_faces,TopBevelRadius


def add_Nylon_Head(OUTSIDE_RADIUS,Z_LOCATION = 0):
    DIV = 36
    verts = []
    faces = []
    Row = 0

    INNER_HOLE = OUTSIDE_RADIUS - (OUTSIDE_RADIUS * (1.25/4.75))
    EDGE_THICKNESS = (OUTSIDE_RADIUS * (0.4/4.75))
    RAD1 = (OUTSIDE_RADIUS * (0.5/4.75))
    OVER_ALL_HEIGTH = (OUTSIDE_RADIUS * (2.0/4.75))
    
    
    FaceStart = len(verts)

    Start_Height = 0 - 3
    Height_Offset = Z_LOCATION
    Lowest_Z_Vert = 0
    
    x = INNER_HOLE
    z = (Height_Offset - OVER_ALL_HEIGTH) + EDGE_THICKNESS
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1
    
    x = INNER_HOLE
    z = (Height_Offset - OVER_ALL_HEIGTH)
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1
    
    
    for i in range(180,80,-10):
        x = sin(radians(i))*RAD1
        z = cos(radians(i))*RAD1
        verts.append([(OUTSIDE_RADIUS-RAD1)+x,0.0,((Height_Offset - OVER_ALL_HEIGTH)+RAD1)+z])
        Lowest_Z_Vert = min(Lowest_Z_Vert,z)
        Row += 1
    
    
    x = OUTSIDE_RADIUS - 0
    z = Height_Offset 
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1

    sVerts,sFaces = SpinDup(verts,faces,360,DIV,'z')
    sVerts.extend(verts)        #add the start verts to the Spin verts to complete the loop
    
    faces.extend(Build_Face_List_Quads(FaceStart,Row-1,DIV,1))

    return Move_Verts_Up_Z(sVerts,0),faces,Lowest_Z_Vert



def add_Nylon_Part(OUTSIDE_RADIUS,Z_LOCATION = 0):
    DIV = 36
    verts = []
    faces = []
    Row = 0

    INNER_HOLE = OUTSIDE_RADIUS - (OUTSIDE_RADIUS * (1.5/4.75))
    EDGE_THICKNESS = (OUTSIDE_RADIUS * (0.4/4.75))
    RAD1 = (OUTSIDE_RADIUS * (0.5/4.75))
    OVER_ALL_HEIGTH = (OUTSIDE_RADIUS * (2.0/4.75))
    PART_THICKNESS = OVER_ALL_HEIGTH - EDGE_THICKNESS
    PART_INNER_HOLE = (OUTSIDE_RADIUS * (2.5/4.75))
    
    FaceStart = len(verts)

    Start_Height = 0 - 3
    Height_Offset = Z_LOCATION
    Lowest_Z_Vert = 0
    

    x = INNER_HOLE + EDGE_THICKNESS
    z = Height_Offset 
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1
    
    x = PART_INNER_HOLE
    z = Height_Offset
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1
    
    x = PART_INNER_HOLE
    z = Height_Offset - PART_THICKNESS
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1
    
    x = INNER_HOLE + EDGE_THICKNESS
    z = Height_Offset - PART_THICKNESS
    verts.append([x,0.0,z])
    Lowest_Z_Vert = min(Lowest_Z_Vert,z)
    Row += 1


    sVerts,sFaces = SpinDup(verts,faces,360,DIV,'z')
    sVerts.extend(verts)  #add the start verts to the Spin verts to complete the loop
    
    faces.extend(Build_Face_List_Quads(FaceStart,Row-1,DIV,1))

    return sVerts,faces,0 - Lowest_Z_Vert



def Nut_Mesh():

    verts = []
    faces = []
    Head_Verts = []
    Head_Faces= []

    Face_Start = len(verts)
    Thread_Verts,Thread_Faces,New_Nut_Height = Create_Internal_Thread(Minor_Dia.val,Major_Dia.val,Pitch.val,Hex_Nut_Height.val,Crest_Percent.val,Root_Percent.val,1)
    verts.extend(Thread_Verts)
    faces.extend(Copy_Faces(Thread_Faces,Face_Start))
    
    Face_Start = len(verts)
    Head_Verts,Head_Faces,Lock_Nut_Rad = add_Hex_Nut(Hex_Nut_Flat_Distance.val,Major_Dia.val,New_Nut_Height)
    verts.extend((Head_Verts))
    faces.extend(Copy_Faces(Head_Faces,Face_Start))
    
    LowZ = 0 - New_Nut_Height
    
    if Nut_Type['LOCK'][0].val:
        Face_Start = len(verts)
        Nylon_Head_Verts,Nylon_Head_faces,LowZ = add_Nylon_Head(Lock_Nut_Rad,0-New_Nut_Height)    
        verts.extend((Nylon_Head_Verts))
        faces.extend(Copy_Faces(Nylon_Head_faces,Face_Start))
    
        Face_Start = len(verts)
        Nylon_Verts,Nylon_faces,Temp_LowZ = add_Nylon_Part(Lock_Nut_Rad,0-New_Nut_Height)    
        verts.extend((Nylon_Verts))
        faces.extend(Copy_Faces(Nylon_faces,Face_Start))
    

    return Move_Verts_Up_Z(verts,0 - LowZ),faces



##################################################################################################


def Create_Nut():

    verts = []
    faces = []
    
  
    if Error_Check() :
        return

    
    verts, faces = Nut_Mesh()
    Add_Mesh_To_Scene('Nut', verts, faces)
    

##################################################################################################


def Create_Bolt():
    verts = []
    faces = []
    
  
    if Error_Check() :
        return
    
    verts, faces = MakeBolt()
    Add_Mesh_To_Scene('Bolt', verts, faces)



def Remove_Doubles_From_Mesh(verts,faces):
    Ret_verts = []
    Ret_faces = []
    
    is_editmode = Window.EditMode() # Store edit mode state
    if is_editmode: Window.EditMode(0) # Python must get a mesh in object mode.
    
    Temp_mesh = Mesh.New('MeshTemp')          # create a new mesh

    Temp_mesh.verts.extend(verts)          # add vertices to mesh
    Temp_mesh.faces.extend(faces)           # add faces to the mesh (also adds edges)
    
    scn = Scene.GetCurrent()          # link object to current scene
    Temp_Object = scn.objects.new(Temp_mesh, 'ObjectTemp')

    Temp_mesh.remDoubles(0.010)
    Temp_mesh.transform(Mathutils.Matrix([Global_Scale,0,0,0], [0,Global_Scale,0,0], [0,0,Global_Scale,0], [0,0,0, Global_Scale]))
    Ret_verts[:] = [v.co for v in Temp_mesh.verts]
    Ret_faces[:] = [ [v.index for v in f] for f in Temp_mesh.faces]

    #delete temp mesh
    scn.objects.unlink(Temp_Object)
    scn.update(0)
    
    if is_editmode: Window.EditMode(1)
    return Ret_verts,Ret_faces



def Add_Mesh_To_Scene(name, verts, faces):
 
    scn = Scene.GetCurrent()
    if scn.lib: return
    ob_act = scn.objects.active

    is_editmode = Window.EditMode()

    cursor = Window.GetCursorPos()
    quat = None
    
    if is_editmode or Blender.Get('add_view_align'): # Aligning seems odd for editmode, but blender does it, oh well
        try:    quat = Blender.Mathutils.Quaternion(Window.GetViewQuat())
        except:    pass
    

    # Exist editmode for non mesh types
    if ob_act and ob_act.type != 'Mesh' and is_editmode:
        EditMode(0)
   
    # We are in mesh editmode
    if Window.EditMode():
        me = ob_act.getData(mesh=1)
        
        if me.multires:
            error_txt = 'Error%t|Unable to complete action with multires enabled'
            Blender.Draw.PupMenu(error_txt)
            print error_txt
            return
        
        #Don't want to remove doubles and scale the existing
        # mesh so we need to get the verts and the faces from
        # a mesh that has been scaled. 
        verts,faces = Remove_Doubles_From_Mesh(verts, faces)
                
        # Add to existing mesh
        # must exit editmode to modify mesh
        Window.EditMode(0)
        
        me.sel = False
        
        vert_offset = len(me.verts)
        face_offset = len(me.faces)
        
        
        # transform the verts
        txmat = Blender.Mathutils.TranslationMatrix(Blender.Mathutils.Vector(cursor))
        if quat:
            mat = quat.toMatrix()
            mat.invert()
            mat.resize4x4()
            txmat = mat * txmat
        
        txmat = txmat * ob_act.matrixWorld.copy().invert()
        
        
        me.verts.extend(verts)
        # Transform the verts by the cursor and view rotation
        me.transform(txmat, selected_only=True)
        
        if vert_offset:
            me.faces.extend([[i+vert_offset for i in f] for f in faces])
        else:
            # Mesh with no data, unlikely
            me.faces.extend(faces)        
    else:
        
        # Object mode add new
        me = Mesh.New(name)
        me.verts.extend(verts)
        me.faces.extend(faces)
        
        
        me.sel = True
        
        # Object creation and location
        scn.objects.selected = []
        ob_act = scn.objects.new(me, name)
        
        me.remDoubles(0.010)
        me.transform(Mathutils.Matrix([Global_Scale,0,0,0], [0,Global_Scale,0,0], [0,0,Global_Scale,0], [0,0,0, Global_Scale]))

        scn.objects.active = ob_act
        
        if quat:
            mat = quat.toMatrix()
            mat.invert()
            mat.resize4x4()
            ob_act.setMatrix(mat)
        
        ob_act.loc = cursor
    
    me.calcNormals()
    
    if is_editmode or Blender.Get('add_editmode'):
        Window.EditMode(1)
        
    Blender.Redraw(-1)#Redraw all

##################################################################################################
    
    

def Load_Preset():

    global Preset_Menu
    global Shank_Dia
    global Shank_Length
    global Thread_Length
    global Major_Dia 
    global Minor_Dia
    global Pitch 
    global Crest_Percent 
    global Root_Percent 
    global Allen_Bit_Flat_Distance
    global Allen_Bit_Depth
    global Head_Height
    global Hex_Head_Flat_Distance 
    global Head_Dia 
    global Dome_Head_Dia
    global Pan_Head_Dia 
    global Philips_Bit_Dia 
    global Phillips_Bit_Depth 
    global Cap_Head_Height

    global Hex_Nut_Height
    global Hex_Nut_Flat_Distance


    if Preset_Menu.val == 1 : #M3
        Shank_Dia.val = 3.0
        #Pitch.val = 0.5    #Coarse
        Pitch.val = 0.35  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10 
        Major_Dia.val = 3.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 5.5
        Hex_Head_Height.val = 2.0
        Cap_Head_Dia.val = 5.5
        Cap_Head_Height.val = 3.0
        Allen_Bit_Flat_Distance.val = 2.5
        Allen_Bit_Depth.val = 1.5
        Pan_Head_Dia.val = 5.6
        Dome_Head_Dia.val = 5.6
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 2.4
        Hex_Nut_Flat_Distance.val = 5.5
        Thread_Length.val = 6
        Shank_Length.val = 0.0
        
    
    if Preset_Menu.val == 2 : #M4
        Shank_Dia.val = 4.0
        #Pitch.val = 0.7    #Coarse
        Pitch.val = 0.5  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10 
        Major_Dia.val = 4.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 7.0
        Hex_Head_Height.val = 2.8
        Cap_Head_Dia.val = 7.0
        Cap_Head_Height.val = 4.0
        Allen_Bit_Flat_Distance.val = 3.0
        Allen_Bit_Depth.val = 2.0
        Pan_Head_Dia.val = 8.0
        Dome_Head_Dia.val = 8.0
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 3.2
        Hex_Nut_Flat_Distance.val = 7.0
        Thread_Length.val = 8
        Shank_Length.val = 0.0
        
        
    if Preset_Menu.val == 3 : #M5
        Shank_Dia.val = 5.0
        #Pitch.val = 0.8 #Coarse
        Pitch.val = 0.5  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10 
        Major_Dia.val = 5.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 8.0
        Hex_Head_Height.val = 3.5
        Cap_Head_Dia.val = 8.5
        Cap_Head_Height.val = 5.0
        Allen_Bit_Flat_Distance.val = 4.0
        Allen_Bit_Depth.val = 2.5
        Pan_Head_Dia.val = 9.5
        Dome_Head_Dia.val = 9.5
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 4.0
        Hex_Nut_Flat_Distance.val = 8.0
        Thread_Length.val = 10
        Shank_Length.val = 0.0
        
        
    if Preset_Menu.val == 4 : #M6
        Shank_Dia.val = 6.0
        #Pitch.val = 1.0 #Coarse
        Pitch.val = 0.75  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10
        Major_Dia.val = 6.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 10.0
        Hex_Head_Height.val = 4.0
        Cap_Head_Dia.val = 10.0
        Cap_Head_Height.val = 6.0
        Allen_Bit_Flat_Distance.val = 5.0
        Allen_Bit_Depth.val = 3.0
        Pan_Head_Dia.val = 12.0
        Dome_Head_Dia.val = 12.0
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 5.0
        Hex_Nut_Flat_Distance.val = 10.0
        Thread_Length.val = 12
        Shank_Length.val = 0.0
        
        
    if Preset_Menu.val == 5 : #M8
        Shank_Dia.val = 8.0
        #Pitch.val = 1.25 #Coarse
        Pitch.val = 1.00  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10
        Major_Dia.val = 8.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 13.0
        Hex_Head_Height.val = 5.3
        Cap_Head_Dia.val = 13.5
        Cap_Head_Height.val = 8.0
        Allen_Bit_Flat_Distance.val = 6.0
        Allen_Bit_Depth.val = 4.0
        Pan_Head_Dia.val = 16.0
        Dome_Head_Dia.val = 16.0
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 6.5
        Hex_Nut_Flat_Distance.val = 13.0
        Thread_Length.val = 16
        Shank_Length.val = 0.0
    
    if Preset_Menu.val == 6 : #M10
        Shank_Dia.val = 10.0
        #Pitch.val = 1.5 #Coarse
        Pitch.val = 1.25  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10
        Major_Dia.val = 10.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 17.0
        Hex_Head_Height.val = 6.4
        Cap_Head_Dia.val = 16.0
        Cap_Head_Height.val = 10.0
        Allen_Bit_Flat_Distance.val = 8.0
        Allen_Bit_Depth.val = 5.0
        Pan_Head_Dia.val = 20.0
        Dome_Head_Dia.val = 20.0
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 8.0
        Hex_Nut_Flat_Distance.val = 17.0
        Thread_Length.val = 20
        Shank_Length.val = 0.0
    
    
    if Preset_Menu.val == 7 : #M12
        #Pitch.val = 1.75 #Coarse
        Pitch.val = 1.50  #Fine
        Crest_Percent.val = 10
        Root_Percent.val = 10
        Major_Dia.val = 12.0
        Minor_Dia.val = Major_Dia.val - (1.082532 * Pitch.val)
        Hex_Head_Flat_Distance.val = 19.0
        Hex_Head_Height.val = 7.5
        Cap_Head_Dia.val = 18.5
        Cap_Head_Height.val = 12.0
        Allen_Bit_Flat_Distance.val = 10.0
        Allen_Bit_Depth.val = 6.0
        Pan_Head_Dia.val = 24.0
        Dome_Head_Dia.val = 24.0
        Philips_Bit_Dia.val = Pan_Head_Dia.val*(1.82/5.6)
        Phillips_Bit_Depth.val = Get_Phillips_Bit_Height(Philips_Bit_Dia.val)
        Hex_Nut_Height.val = 10.0
        Hex_Nut_Flat_Distance.val = 19.0
        Shank_Dia.val = 12.0
        Shank_Length.val = 33.0
        Thread_Length.val = 32.0
        
##############################################################################################

def Test():
    verts = []
    faces = []
   
    if Error_Check() :
        return
    
    verts, faces = MakeBolt()
    
    Add_Mesh_To_Scene("TestBolt", verts,faces)

    Window.Redraw(-1)




def event(evt, val):    # the function to handle input events

  if evt == Draw.ESCKEY:
    Draw.Exit()                 # exit when user presses ESC
    return


def button_event(evt):  # the function to handle Draw Button events

    if evt == On_Exit_Click:
        Draw.Exit()                 # exit when user presses ESC
        return

    if evt == On_Test_Click:
        Test()
        Draw.Redraw(1)
    
    if evt == On_Preset_Click:
        Load_Preset()
        Draw.Redraw(1)

    if evt == On_Create_Click:
        if Model_Type['BOLT'][0].val:
            Create_Bolt()
        if Model_Type['NUT'][0].val:
            Create_Nut()
        Draw.Redraw(1)
    
    elif (evt in [On_Hex_Click, On_Cap_Click,On_Dome_Click,On_Pan_Click]):
        for k in Head_Type.iterkeys():
            if Head_Type[k][1]!=evt:
                Head_Type[k][0].val=0
            else:
                Head_Type[k][0].val=1    
        Draw.Redraw(1)
    
    elif (evt in [On_Bit_None_Click,On_Bit_Allen_Click,On_Bit_Philips_Click]):
        for k in Bit_Type.iterkeys():
            if Bit_Type[k][1]!=evt:
                Bit_Type[k][0].val=0
            else:
                Bit_Type[k][0].val=1
        Draw.Redraw(1)

    elif (evt in [On_Model_Bolt_Click,On_Model_Nut_Click]):
        for k in Model_Type.iterkeys():
            if Model_Type[k][1]!=evt:
                Model_Type[k][0].val=0
            else:
                Model_Type[k][0].val=1
        Draw.Redraw(1)

    elif (evt in [On_Hex_Nut_Click,On_Lock_Nut_Click]):
        for k in Nut_Type.iterkeys():
            if Nut_Type[k][1]!=evt:
                Nut_Type[k][0].val=0
            else:
                Nut_Type[k][0].val=1
        Draw.Redraw(1)
    
#####################################################################################
      

def Draw_Border(X1,Y1,X2,Y2): # X1,Y1 = Top Left X2,Y2 = Bottom Right
    INDENT = 3
    
    BGL.glColor3f(1.0,1.0,1.0)
    BGL.glBegin(BGL.GL_LINES)                
    BGL.glVertex2i(X1+INDENT,Y1-INDENT)     #top line
    BGL.glVertex2i(X2-INDENT,Y1-INDENT)
    
    BGL.glVertex2i(X1+INDENT,Y1-INDENT)     #left line
    BGL.glVertex2i(X1+INDENT,Y2+INDENT)
    BGL.glEnd()

    BGL.glColor3f(0.5,0.5,0.5)
    BGL.glBegin(BGL.GL_LINES)                
    BGL.glVertex2i(X2-INDENT,Y1-INDENT)     #Right line
    BGL.glVertex2i(X2-INDENT,Y2+INDENT)
    
    BGL.glVertex2i(X1+INDENT,Y2+INDENT)     #bottom line
    BGL.glVertex2i(X2-INDENT,Y2+INDENT)
    BGL.glEnd()
    
    


def Create_Tab(X1,Y1,X2,Y2,Title,Buttons): # X1,Y1 = Top Left X2,Y2 = Bottom Right

    BIT_BUTTON_WIDTH = 55
    BIT_BUTTON_HEIGHT = 18
    TITLE_HEIGHT = 15
    INDENT = 6
    BUTTON_GAP = 4

    BGL.glColor3f(0.75, 0.75, 0.75)
    BGL.glRecti(X1,Y1,X2,Y2)
    
    Draw_Border(X1,Y1,X2,Y2);
    
    BGL.glColor3f(0.0,0.0,0.0)
    BGL.glRasterPos2d(X1+INDENT,Y1 - TITLE_HEIGHT)
    Draw.Text(Title)
    
    Button_X = X1 + INDENT
    Button_Y = Y1 - TITLE_HEIGHT - BIT_BUTTON_HEIGHT - 8
    
    #Nut_Number_X = Nut_Button_X
    #Nut_Number_Y = Nut_Button_Y - 25
    if (Buttons != 0):
        key= Buttons.keys()
        for k in key:
            Buttons[k][0]= Draw.Toggle(k,Buttons[k][1],Button_X,Button_Y, BIT_BUTTON_WIDTH,BIT_BUTTON_HEIGHT,Buttons[k][0].val,Buttons[k][2])
            Button_X += BIT_BUTTON_WIDTH + BUTTON_GAP    
    
    
    
def Dispaly_Title_Bar(Y_POS,CONTROL_HEIGHT):
    CONTROL_WIDTH = 250
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS -CONTROL_HEIGHT,"Bolt Factory V2.02",Model_Type)
   
    
      
def Dispaly_Preset_Tab(Y_POS,CONTROL_HEIGHT):
    CONTROL_WIDTH = 250
    BUTTON_Y_OFFSET = 40
    
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Preset",0)
    
    name = "M3%x1|M4%x2|M5%x3|M6%x4|M8%x5|M10%x6|M12%x7"
    
    global Preset_Menu
    Preset_Menu = Draw.Menu(name,No_Event,9,Y_POS-BUTTON_Y_OFFSET,50,18, Preset_Menu.val, "Predefined metric screw sizes.")
    Draw.Button("Apply",On_Preset_Click,150,Y_POS-BUTTON_Y_OFFSET,55,18,"Apply the preset screw sizes.")
    

def Dispaly_Bit_Tab(Y_POS,CONTROL_HEIGHT):  
    
    CONTROL_WIDTH = 250
    NUMBER_HEIGHT = 18
    NUMBER_WIDTH = CONTROL_WIDTH  -3-3-3-3-3

    Bit_Number_X = 3+3+3
    Bit_Number_Y = Y_POS - 64
        
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Bit Type",Bit_Type)
    
    if Bit_Type['NONE'][0].val:
        DoNothing = 1;
        
    elif Bit_Type['ALLEN'][0].val:
        global Allen_Bit_Depth
        Allen_Bit_Depth = Draw.Number('Bit Depth: ',No_Event,Bit_Number_X,Bit_Number_Y,NUMBER_WIDTH, NUMBER_HEIGHT,Allen_Bit_Depth.val, 0,100, '')
        Bit_Number_Y -= NUMBER_HEIGHT
        global Allen_Bit_Flat_Distance
        Allen_Bit_Flat_Distance = Draw.Number('Flat Dist: ',No_Event,Bit_Number_X,Bit_Number_Y,NUMBER_WIDTH,NUMBER_HEIGHT,Allen_Bit_Flat_Distance.val, 0,100, '')
        Bit_Number_Y -= NUMBER_HEIGHT

    elif Bit_Type['PHILLIPS'][0].val:
        global Phillips_Bit_Depth
        Phillips_Bit_Depth = Draw.Number('Bit Depth: ',No_Event,Bit_Number_X,Bit_Number_Y,NUMBER_WIDTH, NUMBER_HEIGHT,Phillips_Bit_Depth.val, 0,100, '')
        Bit_Number_Y -= NUMBER_HEIGHT
        global Philips_Bit_Dia
        Philips_Bit_Dia = Draw.Number('Bit Dia: ',No_Event,Bit_Number_X,Bit_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Philips_Bit_Dia.val, 0,100, '')
        Bit_Number_Y -= NUMBER_HEIGHT



def Dispaly_Shank_Tab(Y_POS,CONTROL_HEIGHT):  
    
    CONTROL_WIDTH = 250
    NUMBER_HEIGHT = 18
    NUMBER_WIDTH = CONTROL_WIDTH  -3-3-3-3-3

    Number_X = 3+3+3
    Number_Y_Pos = Y_POS - 40
        
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Shank",0)
  
    global Shank_Length 
    Shank_Length = Draw.Number('Shank Length: ',No_Event,Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT, Shank_Length.val, 0,MAX_INPUT_NUMBER, 'some text tip')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    global Shank_Dia 
    Shank_Dia = Draw.Number('Shank Dia: ',No_Event,Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT, Shank_Dia.val, 0,MAX_INPUT_NUMBER, 'some text tip')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    

def Dispaly_Thread_Tab(Y_POS,CONTROL_HEIGHT):  
    
    CONTROL_WIDTH = 250
    NUMBER_HEIGHT = 18
    NUMBER_WIDTH = CONTROL_WIDTH  -3-3-3-3-3
    

    Number_X = 3+3+3
    Number_Y_Pos = Y_POS - 40
            
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Thread",0)
    
    global Thread_Length
    if Model_Type['BOLT'][0].val:
        Thread_Length = Draw.Number('Thread Length: ',No_Event, Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT, Thread_Length.val, 0,MAX_INPUT_NUMBER, '')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    global Major_Dia
    Major_Dia = Draw.Number('Major Dia: ',No_Event,Number_X,Number_Y_Pos, NUMBER_WIDTH,NUMBER_HEIGHT, Major_Dia.val, 0,MAX_INPUT_NUMBER, '')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    global Minor_Dia
    Minor_Dia = Draw.Number('Minor Dia: ',No_Event,Number_X,Number_Y_Pos, NUMBER_WIDTH,NUMBER_HEIGHT, Minor_Dia.val, 0,MAX_INPUT_NUMBER, '')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    global Pitch
    Pitch = Draw.Number('Pitch: ',No_Event,Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT, Pitch.val, 0.1,7.0, '')
    Number_Y_Pos -= NUMBER_HEIGHT
    
    global Crest_Percent
    Crest_Percent = Draw.Number('Crest %: ',No_Event,Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT,Crest_Percent.val, 1,90, '')
    Number_Y_Pos -= NUMBER_HEIGHT
  
    global Root_Percent
    Root_Percent = Draw.Number('Root %: ',No_Event,Number_X,Number_Y_Pos,NUMBER_WIDTH,NUMBER_HEIGHT,Root_Percent.val, 1,90, '')
    Number_Y_Pos -= NUMBER_HEIGHT
  

    
    
def Dispaly_Head_Tab(Y_POS,CONTROL_HEIGHT):  
    
    CONTROL_WIDTH = 250
    NUMBER_HEIGHT = 18
    NUMBER_WIDTH = CONTROL_WIDTH  -3-3-3-3-3

    Head_Number_X = 3+3+3
    Head_Number_Y = Y_POS - 64
        
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Head Type",Head_Type)
    
    if Head_Type['HEX'][0].val:  
        global Hex_Head_Height
        Hex_Head_Height = Draw.Number('Head Height: ',No_Event,Head_Number_X ,Head_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Hex_Head_Height.val, 0,100, '')
        Head_Number_Y -= NUMBER_HEIGHT
        global Hex_Head_Flat_Distance
        Hex_Head_Flat_Distance = Draw.Number('Head Hex Flat Distance ',No_Event,Head_Number_X,Head_Number_Y,NUMBER_WIDTH, NUMBER_HEIGHT,Hex_Head_Flat_Distance.val, 0,MAX_INPUT_NUMBER, '')
        Head_Number_Y -= NUMBER_HEIGHT
  
    elif Head_Type['CAP'][0].val:  
        global Cap_Head_Height
        Cap_Head_Height = Draw.Number('Head Height: ',No_Event, Head_Number_X,Head_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Cap_Head_Height.val, 0,100, '')
        Head_Number_Y -= NUMBER_HEIGHT
        global Cap_Head_Dia
        Cap_Head_Dia = Draw.Number('Head Dia ',No_Event,Head_Number_X,Head_Number_Y,NUMBER_WIDTH, NUMBER_HEIGHT,Cap_Head_Dia.val, 0,MAX_INPUT_NUMBER, '')
        Head_Number_Y -= NUMBER_HEIGHT
  
    elif Head_Type['DOME'][0].val:  
        global Dome_Head_Dia
        Dome_Head_Dia = Draw.Number(' Dome Head Dia ',No_Event,Head_Number_X,Head_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Dome_Head_Dia.val, 0,MAX_INPUT_NUMBER, '')
        Head_Number_Y -= NUMBER_HEIGHT
  
    elif Head_Type['PAN'][0].val:  
        global Pan_Head_Dia
        Pan_Head_Dia = Draw.Number('Pan Head Dia ',No_Event,Head_Number_X,Head_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Pan_Head_Dia.val, 0,MAX_INPUT_NUMBER, '')
        Head_Number_Y -= NUMBER_HEIGHT
  
    
    
    
def Dispaly_Nut_Tab(Y_POS,CONTROL_HEIGHT):  
    
    CONTROL_WIDTH = 250
    NUMBER_HEIGHT = 18
    NUMBER_WIDTH = CONTROL_WIDTH  -3-3-3-3-3

    Nut_Number_X = 3+3+3
    Nut_Number_Y = Y_POS - 64
   
    Create_Tab(3,Y_POS,CONTROL_WIDTH,Y_POS-CONTROL_HEIGHT,"Nut Type",Nut_Type)
        
    #if Nut_Type['HEX'][0].val:  
    global Hex_Nut_Height
    Hex_Nut_Height = Draw.Number('Nut Height: ',No_Event,Nut_Number_X ,Nut_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Hex_Nut_Height.val, 0,MAX_INPUT_NUMBER, '')
    Nut_Number_Y -= NUMBER_HEIGHT
    global Hex_Nut_Flat_Distance
    Hex_Nut_Flat_Distance = Draw.Number('Nut Flat Distance ',No_Event,Nut_Number_X,Nut_Number_Y, NUMBER_WIDTH, NUMBER_HEIGHT,Hex_Nut_Flat_Distance.val, 0,MAX_INPUT_NUMBER, '')
    Nut_Number_Y -= NUMBER_HEIGHT
  

def Dispaly_Bolt_Tab():    
       
    Dispaly_Shank_Tab(284,66)
    Dispaly_Head_Tab(374,90)
    Dispaly_Bit_Tab(464,90)
    

##########################################################################################

def gui():              # the function to draw the screen
    
    CONTROL_WIDTH = 250

    BGL.glClearColor(0.6, 0.6, 0.6, 1.0)
    BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
    
    BGL.glColor3f(0.75, 0.75, 0.75)
    BGL.glRecti(3,30,CONTROL_WIDTH,3)
    
    Dispaly_Title_Bar(514,50);
    
    if Model_Type['BOLT'][0].val:
        Dispaly_Bolt_Tab();
    
    if Model_Type['NUT'][0].val:
        Dispaly_Nut_Tab(464,246);

    Dispaly_Thread_Tab(218,138)

    Dispaly_Preset_Tab(80,50)
    
    Draw.PushButton("Create",On_Create_Click,6,8,55,18,"Create Bolt")
    Draw.Button("Exit",On_Exit_Click,6+55+4,8,55,18)
   
#    Draw.Button("Test",On_Test_Click,150,10,55,20)

Load_Preset()
Draw.Register(gui, event, button_event)  # registering the 3 callbacks
