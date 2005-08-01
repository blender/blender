#!BPY

""" Registration info for Blender menus:
Name: 'Texture Baker'
Blender: 237
Group: 'UV'
Tooltip: 'Procedural to uvmapped texture baker'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Official Page, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_mesh3d2uv2d_en.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.2.8 2005/7/20"

__bpydoc__ = """\
Texture Baker "bakes" Blender procedural materials (including textures): it saves them as 2d uv-mapped images.

This script saves an uv texture layout of the chosen mesh, that can be used as
an uv map for it.  It is a way to export procedurals from Blender as normal
image textures that can be edited with a 2d image manipulation program or used
with the mesh in games and other 3d applications.

Usage:

a) Enter face mode and define uv coordinates for your mesh (do not forget to choose a development shape);<br>
b) Define its materials and textures;<br>
c) Run this script and check the console.

Global variables:

a) FRAME (integer): the last frame of the animation, autodocumented.<br>
b) LIMIT (integer): 0 or 1, uvcoords may exceed limits 0.0 to 1.0, this variable obliges the script to do a complete framing of the uvcoord.

Notes:<br>
   This script was based on a suggestion by Martin (Theeth) Poirier.
"""

#---------------------------------------------
# Last release : 0.2.8 ,  2005/07/20 , 17h10
#---------------------------------------------
#---------------------------------------------
# (c) jm soler  07/2004 : 'Procedural Texture Baker'
#     Based on a Martin 'Theeth' Poirier's really
#     good idea : makes a rvk mesh with uv coords
#     of the original mesh.
#
#     Released under Blender Artistic Licence
#
#   0.2.8
#       -- added the forgotten image property in face
#       data. a little longer but better.
#       ( a remove double in the resulting mesh may be
#       useful .) 
#       -- the data.update() function problem is 
#       corrected too
#       -- no more layers problem . CAM and MESH are
#       localised in layer 20 . This layer is
#       the active one for the image rendering .
#       -- mesh creation is cleaner, loop in double was
#       removed and the abskey is set in frame 1 
#       only  . This solves an other deform problem  .
#       -- if user does not want an autosaved image,
#       the "no replace" option leaves the render 
#       window on the screen 
#
#   0.2.7 
#      -- minor correction on line 147: "!=-1" added 
#
#   0.2.6
#       -- Creation of LAMP object is removed and replaced
#       by the use of the shadeless option  in material object
#
#       -- helpmsg corrected : the aim of the script
#       is to bake any type of textures so we have not
#       to mapinput its textures on UV .
#
#       --'pers' camera was replaced by an 'ortho' one. 
#
#   0.2.5
#       -- if a image file  with the same name exits the
#       system returns an error 
#
#   0.2.4
#       -- a LIMIT variable is added to unlock the uvcoords
#       autoframing
#
#
#   0.2.3 :  
#        Great thanks for Apollux  who sees a lot of these 
#        problems
#
#        --Everytime you run the script a new set 
#        of objects is created. File size and memory 
#        consumption can go  pretty high if you are 
#        not aware of that . 
#        Now it ONLY creates 3 objects: a flattened 
#         mesh, a camera and a lamp.
#        --all the 3 objects was placed on layer 1, but if 
#        that layer was not visible while you used the script 
#        all you will get a is an empty render. 
#        Now the layer is tst and activated befor the shoot 
#        --The flattened mesh was really flattend only after 
#        frame 100 (if you playbacked the animation, you can 
#        actually see the mesh becoming flat on the first 100 
#        frames). No more.
#        -- When the script is run, it changes temporary to 
#        the new cammera, set the render output to a square 
#        (i.e. 1024 x 1024 or else), does the render, and then 
#        resets the render output and the active camera to the 
#        original one. But if no original camera was found
#        this produce an error.    
#
#  0.2.2 : 
#        if the uv mesh objet exists it used,
#        no creation of a new one. As the lamp and  
#        the camera
#  0.2.1 : 
#        This script automaticaly frame and shoot the
#        new uv mesh . The image file is saved ine the
#        /render folder.
#
#---------------------------------------------
#  On user-friendly side :
#---------------------------------------------
#- Tadje Vobovnik adds the Select Image Size Menu
#
#---------------------------------------------
# Official Page :
#  http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_mesh3d2uv2d_en.htm
# For problems and errors:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------

import Blender
from Blender import NMesh, Draw, Object, Scene, Camera

#----------------------------------- 
# Last release : 0.2.5 ,  2005/05/22 , 20h00
#----------------------------------- 
# la fonction Blender.sys.dirname pose un
# probleme lorsque la memoire est trop encombree
# ---
# It seems that the Blender.sys.dirname function
# poses a problem when the memory is too much encumbered 
#----------------------------------- 
try:
  import nt
  os = nt
  os.sep='\\'
except:
  import posix
  os = posix
  os.sep='/'
DIRNAME=Blender.Get('filename')
#----------------------------------- 
#  decoupage de la chaine en fragment
#  de façon a isoler le nom du fichier
#  du repertoire
# ---
# split string in fragments to isolate
# the file name from the path name
#----------------------------------- 

if DIRNAME.find(os.sep)!=-1:
    k0=DIRNAME.split(os.sep)
else:
    k0=DIRNAME.split('/')   
DIRNAME=DIRNAME.replace(k0[-1],'')
#----------------------------------- 
# Last release : 0.2.5 ,  2005/05/22 , end
#----------------------------------- 

#----------------------------------- 
# Last release : 0.2.4 ,  2005/05/22 , 15h00
#----------------------------------- 
FRAME = Blender.Get('endframe')
#----------------------------------- 
# Last release : 0.2.4 ,  2005/05/22 , end
#----------------------------------- 

#----------------------------------- 
# Last release : 0.2.4 ,  2005/05/18 , 11h00
#
# Si  LIMIT == 0 le script n'essaye pas de realiser
# un nouveau cadrage pour que l'image presente toute les
# coordonnées uv.
# ---
# if  LIMIT == 0 the script do not try to make
# a new framing with all the uvcoord  in only one
# shoot...
#-----------------------------------
LIMIT=0
#----------------------------------- 
# Last release : 0.2.4 ,  2005/05/18 , END
#----------------------------------- 

XYLIMIT = [0.0, 0.0,1.0,1.0]    
OBJPOS = 100.0
DEBUG=1
RENDERLAYER=20
SCENELAYERS=[]

helpmsg = """
Texture Baker:

This script saves an uv texture layout of the chosen mesh, that can be used as
an uv map for it.  It is a way to export procedural textures from Blender as
normal image textures that can be edited with a 2d image manipulation program
or used with the mesh in games and other 3d applications.

Basic instructions:
- Enter face mode and define uv coordinates for your mesh (do not forget to
  choose a development shape);
- Define its materials and textures ;
- Run this script and check the console.

"""

def GET_newobject (TYPE,NAME):
   """
# ---------------------------
# Function  GET_newobject 
# 
#  IN : TYPE   string , object type ('Mesh','Empty',...)
#       NAME   string , name object  
#  OUT: OBJECT  Blender objetc described in teh string TYPE
#       SCENE   Blender current scene object
# ---------------------------   
    Return and object and the current scene
   """
   SCENE = Blender.Scene.getCurrent()
   OBJECT = Blender.Object.New(TYPE,NAME)
   SCENE.link(OBJECT)
   return OBJECT, SCENE

def RenameImage(RDIR, MYDIR, FILENAME, name):
    """
# ---------------------------
# Function  RenameImage
#
#  IN : RDIR      string , current render directory
#       MYDIR     string , new render dir for this shoot
#       FILENAME  string , last rendered image filename 
#       name      string , new name for this image 
#  OUT:  nothing
# ---------------------------
     Rename the file pointed by the string name
     recall the  function if the file yet exists
    """
    newfname = RDIR + MYDIR + name
    if newfname.find('.png', -4) < 0 : newfname += '.png'
    if not Blender.sys.exists(newfname):
       os.rename(FILENAME, newfname)
    else:
        name = Draw.PupStrInput ('ReName Image, please :', name, 32)
        RenameImage(RDIR, MYDIR, FILENAME, name)

def SAVE_image (rc, name, FRAME, result):
   """  
# ---------------------------
# Function  SAVE_image
#
#  IN : rc    current render context object
#       name  string , image name
#       FRAME  integer, last numbre of the curent animation  
#  OUT: nothing  
# ---------------------------   
   """
   rc.enableExtensions(1)   
   MYDIR = ''
   RENDERDIR = rc.getRenderPath().replace('\\','/')
   if RENDERDIR.find('//')==0 : 
      print 'filename', Blender.Get('filename'),'/n', Blender.sys.dirname(Blender.Get('filename')) 
      RDIR=RENDERDIR.replace('//',DIRNAME)
   else:
        RDIR=RENDERDIR[:]
   if DEBUG : print  'RDIR : ', RDIR
    
   HOMEDIR=Blender.Get('homedir')
   if DEBUG : print  'HOMEDIR', HOMEDIR
   rc.setRenderPath(RENDERDIR + MYDIR)
   if DEBUG : print "Render folder:", RENDERDIR + MYDIR
   IMAGETYPE = Blender.Scene.Render.PNG
   if DEBUG : print  'IMAGETYPE : ',IMAGETYPE
   rc.setImageType(IMAGETYPE)
   NEWFRAME = FRAME
   OLDEFRAME = rc.endFrame()
   OLDSFRAME = rc.startFrame()
   rc.startFrame(NEWFRAME)
   rc.endFrame(NEWFRAME)
   rc.renderAnim()
   if result!=2 and not KEEPRENDERWINDOW:
      Blender.Scene.Render.CloseRenderWindow()
      FILENAME = "%04d" % NEWFRAME
      FILENAME = FILENAME.replace (' ', '0')
      FILENAME = RDIR + MYDIR + FILENAME + '.png'
      RenameImage(RDIR, MYDIR, FILENAME, name)
	
   rc.endFrame(OLDEFRAME)
   rc.startFrame(OLDSFRAME)
   rc.setRenderPath(RENDERDIR)

def SHOOT (XYlimit, frame, obj, name, FRAME, result):
   """
# ---------------------------
# Function  SHOOT
#
#  IN : XYlimit  list of 4 floats, smallest and biggest 
#                uvcoords
#       frame    current frame
#       obj      for object location
#       name     image name
#       FRAME    the last animation's frame 
#  OUT:  nothing 
# ---------------------------   
      render and save the baked textures picture 
   """
   try:
      CAM = Blender.Object.Get('UVCAMERA')
      Cam = CAM.getData()
      SC = Blender.Scene.getCurrent()
   except:
      Cam = Blender.Camera.New()
      Cam.name = 'UVCamera'
      CAM, SC = GET_newobject('Camera','UVCAMERA')
      CAM.link(Cam)
      CAM.setName('UVCAMERA')
      CAM.layers=[RENDERLAYER]
      Cam.lens = 30
      Cam.name = 'UVCamera'

   Cam.setType('ortho')
   Cam.setScale(1.0)

   CAM.setLocation(obj.getLocation())
   CAM.LocX += XYlimit[2] * 0.500
   CAM.LocY += XYlimit[3] * 0.500
   CAM.LocZ += max (XYlimit[2], XYlimit[3])
   CAM.setEuler (0.0, 0.0, 0.0)

   context = SC.getRenderingContext()
   Camold = SC.getCurrentCamera()
   SC.setCurrentCamera(CAM)

   OLDy = context.imageSizeY()
   OLDx = context.imageSizeX()

   tres = Draw.PupMenu('TEXTURE OUT RESOLUTION : %t | 256 %x1 | 512 %x2 | 768 %x3 | 1024 %x4 | 2048 %x5 ')

   if (tres) == 1: res = 256
   elif (tres) == 2: res = 512
   elif (tres) == 3: res = 768
   elif (tres) == 4: res = 1024
   elif (tres) == 5: res = 2048
   else: res = 512

   SCENELAYERS=SC.layers
   SC.layers = [20]
   context.imageSizeY(res)
   context.imageSizeX(res)
   SAVE_image (context, name, FRAME, result)
   context.imageSizeY(OLDy)
   context.imageSizeX(OLDx)
   SC.layers = SCENELAYERS
   if Camold : SC.setCurrentCamera(Camold)

   Blender.Set ('curframe', frame)


#----------------------------------- 
# release : 0.2.6 ,  2005/05/29 , 00h00
#----------------------------------- 
def PROV_Shadeless(MATList):
    """
# ---------------------------
# Function  PROV_Shadeless
#
#  IN : MATList  a list of the mesh's materials
#  OUT: SHADEDict  a dictionnary  of the materials' shadeles value
# ---------------------------   
    """
    SHADEDict={}
    for mat in MATList:
       SHADEDict[mat.name]=mat.mode
       mat.mode |= Blender.Material.Modes.SHADELESS
    return SHADEDict
#----------------------------------- 
# Last release : 0.2.6 ,  2005/05/29 , end
#----------------------------------- 

#----------------------------------- 
# release : 0.2.6 ,  2005/05/29 , 00h00
#----------------------------------- 
def REST_Shadeless(SHADEDict):
    """
# ---------------------------
# Function  REST_Shadeless
#
#  IN : SHADEDict   a dictionnary  of the materials' shadeles value
#  OUT : nothing
# ---------------------------   
    """
    for m in SHADEDict.keys():
       mat=Blender.Material.Get(m)
       mat.mode=SHADEDict[m]
#----------------------------------- 
# release : 0.2.6 ,  2005/05/29 , end
#----------------------------------- 

def Mesh2UVCoord (LIMIT):
   """
# ---------------------------
# Function  Mesh2UVCoord
#
#  IN : LIMIT  integer, create or not a new framing for uvcoords 
#  OUT:  nothing
# ---------------------------   
   """
   global PUTRAW, FRAME, SCENELAYERS

   try:
      MESH3D = Object.GetSelected()[0]
      if MESH3D.getType() == 'Mesh':
         MESH = MESH3D.getData()

         try:
            NewOBJECT=Blender.Object.Get('UVOBJECT')
            CurSCENE=Blender.Scene.getCurrent()            
         except:
            NewOBJECT, CurSCENE = GET_newobject('Mesh','UVOBJECT')
         MESH2 = NewOBJECT.getData() 
         NewOBJECT.layers=[RENDERLAYER]

         MESH2.faces=[]
         for f in MESH.faces:
            f1 = Blender.NMesh.Face()
            for v in f.v:
               v1 = Blender.NMesh.Vert (0.0, 0.0, 0.0)
               for n in [0,1]:
                  v1.co[n] = f.uv[f.v.index(v)][n]
                  exec "if v1.co[%s] > XYLIMIT[%s]: XYLIMIT[%s] = v1.co[%s]" % (n, n+2, n+2, n)
                  exec "if v1.co[%s] < XYLIMIT[%s]: XYLIMIT[%s] = v1.co[%s]" % (n, n, n, n)
               v1.co[2] = 0.0
               MESH2.verts.append(v1)
               f1.v.append(MESH2.verts[len(MESH2.verts) - 1])

            MESH2.faces.append(f1)
            f1.uv = f.uv[:]
            f1.col = f.col[:]
            f1.smooth = f.smooth
            f1.mode = f.mode
            f1.flag = f.flag
            f1.mat = f.mat
            #----------------------------------- 
            # release : 0.2.8 ,  2005/07/19 , end
            #----------------------------------- 
            try:
              f1.image=f.image
            except :
              pass

         MESH2.materials = MESH.materials[:]
         NewOBJECT.setLocation (OBJPOS, OBJPOS, 0.0)
         NewOBJECT.setEuler (0.0, 0.0, 0.0)

         MESH2.removeAllKeys()
         MESH2.update()
         MESH2.insertKey (1, 'absolute')
         MESH2.update()

         imagename = 'uvtext'

         name = "CHANGE IMAGE NAME ? %t | Replace it | No replacing | Script help"
         result = Draw.PupMenu(name)

         if result == 1:
            imagename = Draw.PupStrInput ('Image Name:', imagename, 32)

         if result != 3:
            #----------------------------------- 
            # release : 0.2.6 ,  2005/05/29 , 00h00
            #----------------------------------- 
            SHADEDict=PROV_Shadeless(MESH2.materials)
            #----------------------------------- 
            # release : 0.2.6 ,  2005/05/29 , end
            #----------------------------------- 

            if LIMIT :
                 SHOOT(XYLIMIT, FRAME, NewOBJECT, imagename, FRAME,result)
            else :
                 SHOOT([0.0,0.0,1.0,1.0], FRAME, NewOBJECT, imagename, FRAME, result)
            #----------------------------------- 
            # release : 0.2.6,  2005/05/29 , 00h00
            #----------------------------------- 
            REST_Shadeless(SHADEDict)
            #----------------------------------- 
            # release : 0.2.6 ,  2005/05/29 , end
            #----------------------------------- 

            Blender.Redraw()

         else:
            Blender.ShowHelp('tex2uvbaker.py')
            #Draw.PupMenu("Ready%t|Please check console for instructions")
            if DEBUG: print helpmsg

      else:
         name = "ERROR: active object is not a mesh or has no UV coordinates"
         result = Draw.PupMenu(name)
         print 'problem : no object selected or not mesh'

   except:
      name = "ERROR: active object is not a mesh or has no UV coordinates"
      result = Draw.PupMenu(name)
      print 'problem : no object selected or not mesh'

Mesh2UVCoord(LIMIT)
