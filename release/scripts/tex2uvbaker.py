#!BPY

""" Registration info for Blender menus:
Name: 'Texture Baker'
Blender: 233
Group: 'UV'
Tooltip: 'Procedural to uvmapped texture baker'
"""

#---------------------------------------------
# Last release : 0.2.2 ,  2004/08/01 , 22h13
#---------------------------------------------
#---------------------------------------------
# (c) jm soler  07/2004 : 'Procedural Texture Baker'
#     Based on a Martin Theeth' Poirier's really
#     good idea :
#     it makes a rvk mesh with uv coords of the
#     original mesh.
#     released under Blender Artistic Licence
#
#  0.2.2 : if the uv mesh objet exists it used,
#          no creation of a new one. As the lamp and  
#          the camera
#  0.2.1 : This script automaticaly frame and shoot the
#  new uv mesh . The image file is saved ine the
#  /render folder.
#
#---------------------------------------------
#  On user-friendly side :
#---------------------------------------------
#- Tadje Vobovnik adds the Select Image Size Menu
#
#---------------------------------------------
# Official Page :
#   http://jmsoler.free.fr/util/blenderfile/py/text2uvbaker.py
# For problems and errors:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------

import Blender
from Blender import NMesh, Draw, Object, Scene, Camera

FRAME = 100
XYLIMIT = [0.0, 0.]
OBJPOS = 100.0

helpmsg = """
Texture Baker:

This script saves an uv texture layout of the chosen mesh, that can be used as
an uv map for it.  It is a way to export procedural textures from Blender as
normal image textures that can be edited with a 2d image manipulation program
or used with the mesh in games and other 3d applications.

Basic instructions:
- Enter face mode and define uv coordinates for your mesh;
- Define its materials and textures and set "Map Input" coordinates to UV;
- Run this script and check the console.
"""

def GET_newobject (TYPE):
   SCENE = Blender.Scene.getCurrent()
   OBJECT = Blender.Object.New(TYPE)
   SCENE.link(OBJECT)
   return OBJECT, SCENE

def SAVE_image (rc, name, FRAME):
   MYDIR = ''
   RENDERDIR = rc.getRenderPath()
   rc.setRenderPath(RENDERDIR + MYDIR)
   print "Render folder:", RENDERDIR + MYDIR
   IMAGETYPE = Blender.Scene.Render.PNG
   rc.setImageType(IMAGETYPE)
   NEWFRAME = FRAME
   OLDEFRAME = rc.endFrame()
   OLDSFRAME = rc.startFrame()
   rc.startFrame(NEWFRAME)
   rc.endFrame(NEWFRAME)
   rc.renderAnim()

   try:
      import nt
      os = nt

   except:
      import posix
      os = posix

   FILENAME = "%04d" % NEWFRAME
   FILENAME = FILENAME.replace (' ', '0')
   FILENAME = RENDERDIR + MYDIR + FILENAME + '.png'

   try:
      TRUE = os.stat(FILENAME)
      newfname = RENDERDIR + MYDIR + name
      if newfname.find('.png', -4) < 0: newfname += '.png'
      os.rename(FILENAME, newfname)
      print "Renamed to:", newfname

   except:
      pass

   rc.endFrame(OLDEFRAME)
   rc.startFrame(OLDSFRAME)
   rc.setRenderPath(RENDERDIR)

def SHOOT (XYlimit, frame, obj, name, FRAME):
   try:
      CAM = Blender.Object.Get('UVCAMERA')
      Cam = CAM.getData()
      SC = Blender.Scene.getCurrent()

   except:
      Cam = Blender.Camera.New()
      Cam.name = 'UVCamera'
      CAM, SC = GET_newobject('Camera')
      CAM.link(Cam)
      CAM.setName('UVCAMERA')
      Cam.lens = 30
      Cam.name = 'UVCamera'

   CAM.setLocation(obj.getLocation())
   CAM.LocX += XYlimit[0] / 2.0
   CAM.LocY += XYlimit[1] / 2.0
   CAM.LocZ += max (XYlimit[0], XYlimit[1])
   CAM.setEuler (0.0, 0.0, 0.0)

   try:
      LAMP = Blender.Object.Get('Eclairage')
      lampe = LAMP.getData()
      SC = Blender.Scene.getCurrent()

   except:
      lampe = Blender.Lamp.New()
      lampe.name = 'lumin'
      LAMP, SC = GET_newobject('Lamp')
      LAMP.link(lampe)
      LAMP.setName('Eclairage')

   LAMP.setLocation(obj.getLocation())
   LAMP.LocX += XYlimit[0] / 2.0
   LAMP.LocY += XYlimit[1] / 2.0
   LAMP.LocZ += max (XYlimit[0], XYlimit[1])
   LAMP.setEuler (0.0, 0.0, 0.0)
   context = SC.getRenderingContext()
   Camold = SC.getCurrentCamera()
   SC.setCurrentCamera(CAM)
   OLDy = context.imageSizeY()
   OLDx = context.imageSizeX()
   tres = Draw.PupMenu('TEXTURE OUT RESOLUTION : %t | 256 %x1 | 512 %x2 | 768 %x3 | 1024 %x4')

   if (tres) == 1: res = 256

   elif (tres) == 2: res = 512

   elif (tres) == 3: res = 768

   elif (tres) == 4: res = 1024

   else: res = 512

   context.imageSizeY(res)
   context.imageSizeX(res)
   SAVE_image (context, name, FRAME)
   context.imageSizeY(OLDy)
   context.imageSizeX(OLDx)
   SC.setCurrentCamera(Camold)
   Blender.Set ('curframe', frame)

def Mesh2UVCoord ():
   try:
      MESH3D = Object.GetSelected()[0]

      if MESH3D.getType() == 'Mesh':
         MESH = MESH3D.getData()
         MESH2 = Blender.NMesh.GetRaw()

         for f in MESH.faces:
            f1 = Blender.NMesh.Face()

            for v in f.v:
               v1 = Blender.NMesh.Vert (v.co[0], v.co[1], v.co[2])
               MESH2.verts.append(v1)
               f1.v.append(MESH2.verts[len(MESH2.verts) - 1])

            MESH2.faces.append(f1)
            f1.uv = f.uv[:]
            f1.col = f.col[:]
            f1.smooth = f.smooth
            f1.mode = f.mode
            f1.flag = f.flag
            f1.mat = f.mat

         MESH2.materials = MESH.materials[:]

         try:
            NewOBJECT=Blender.Object.Get('UVOBJECT')
            CurSCENE=Blender.Scene.getCurrent()            
         except:
            NewOBJECT, CurSCENE = GET_newobject('Mesh')

         NewOBJECT.link(MESH2)

         #NewOBJECT, CurSCENE = GET_newobject('Mesh')
         #NewOBJECT.link(MESH2)

         NewOBJECT.setLocation (OBJPOS, OBJPOS, 0.0)
         NewOBJECT.setEuler (0.0, 0.0, 0.0)

         MESH2.removeAllKeys()

         MESH2.update()
         MESH2.insertKey (1, 'absolute')
         MESH2.update()

         for f in MESH2.faces:
            for v in f.v:
               for n in [0, 1]:
                  v.co[n] = f.uv[f.v.index(v)][n]
                  exec "if v.co[%s] > XYLIMIT[%s]: XYLIMIT[%s] = v.co[%s]" % (n, n, n, n)

               v.co[2] = 0.0

         print XYLIMIT

         MESH2.update()
         MESH2.insertKey (FRAME, 'absolute')
         MESH2.update()
         imagename = 'uvtext'

         name = "CHANGE IMAGE NAME ? %t | Replace it | No replace | Script help"
         result = Draw.PupMenu(name)

         if result == 1:
            imagename = Draw.PupStrInput ('Image Name:', imagename, 32)

         if result != 3:
            SHOOT (XYLIMIT, FRAME, NewOBJECT, imagename, FRAME)
            Blender.Redraw()
         else:
            Draw.PupMenu("Ready%t|Please check console for instructions")
            print helpmsg

      else:
         name = "Error%t|Active object is not a mesh or has no UV coordinates"
         result = Draw.PupMenu(name)
         print 'problem : no object selected or not mesh'

   except:
      name = "Error%t|Active object is not a mesh or has no UV coordinates"
      result = Draw.PupMenu(name)
      print 'problem : no object selected or not mesh'

Mesh2UVCoord() 
