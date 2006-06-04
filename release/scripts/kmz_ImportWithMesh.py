#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'Google Map Model (KMZ/KML)'
Blender: 241
Group: 'Import'
Tip: 'Import Google earth models'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/py_import_kml-kmz.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.1.5, june, 04, 2006"

__bpydoc__ = """\
       To open .kmz file this script needs zipfile.py from the complete python 
"""
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2006: jm soler, jmsoler_at_free.fr
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# --------------------------------------------------------------------------

import Blender
from Blender  import Window
from Blender import Mathutils

def split(pathname):
         if pathname.find(Blender.sys.sep)!=-1:
             k0=pathname.split(Blender.sys.sep)
         else:
            if Blender.sys.sep=='/':
                k0=pathname.split('\\')
            else:
                k0=pathname.split('/') 

         directory=pathname.replace(k0[len(k0)-1],'')
         Name=k0[len(k0)-1]
         return directory, Name

def join(l0,l1): 
     return  l0+Blender.sys.sep+l1

def fonctionSELECT(nom):
    scan_FILE(nom)

def filtreFICHIER(nom):
     """
     Function  filtreFICHIER
     in  : string  nom , filename
     out : string  t   , if correct filecontaint 
     """
     if nom.upper().find('.KMZ')!=-1:
             from zipfile import ZipFile, ZIP_DEFLATED
             tz=ZipFile(nom,'r',ZIP_DEFLATED)
             t = tz.read(tz.namelist()[0])
             tz.close()
             return t
     elif nom.upper().find('.KML')!=-1:
             tz=open(nom,'r')
             t = tz.read()
             tz.close()
             return t
     else : 
        return None

OB = None #Blender.Object.New('Mesh')  # link mesh to an object
SC = None #Blender.Scene.GetCurrent()          # link object to current scene
#SC.link(OB)
ME=  None#OB.getData(mesh=1)


DOCUMENTORIGINE=[]
UPDATE_V=[]
UPDATE_F=[]
POS=0
NUMBER=0
PLACEMARK=0

eps=0.0000001
npoly=0
gt1=Blender.sys.time()

def cree_POLYGON(ME,TESSEL):
   global  OB, npoly, UPDATE_V, UPDATE_F, POS
   npoly+=1 
   for T in TESSEL: del T[-1]
   if npoly %100 == 1 : 
      print 'Pgon: ', npoly, 'verts:',[len(T) for T in TESSEL]
	
   if npoly %250 == 1 : 
      Blender.Window.RedrawAll()
      g2= Blender.sys.time()-gt1
      print int(g2/60),':',int(g2%60)      
	
   if len(TESSEL)==1 and len(TESSEL[0]) in [3,4] :
       if UPDATE_F==[]:
          POS=len(ME.verts)

       UPDATE_V.extend(TESSEL[0])

       if len(TESSEL[0])==3:
          UPDATE_F.append([POS,POS+1,POS+2])
          POS+=3
       else :
          UPDATE_F.append([POS,POS+1,POS+2,POS+3])
          POS+=4
   else :
      if UPDATE_V: ME.verts.extend(UPDATE_V)
      FACES=[]
      if UPDATE_F:
        for FE in UPDATE_F:
           if len(FE)==3:
              FACES.append([ME.verts[FE[0]],ME.verts[FE[1]],ME.verts[FE[2]]]) 
           else :
              FACES.append([ME.verts[FE[0]],ME.verts[FE[1]],ME.verts[FE[2]],ME.verts[FE[3]]])
      if FACES:
           ME.faces.extend(FACES)
           FACES=[]

      UPDATE_F=[]
      UPDATE_V=[]
      EDGES=[]


      for T in TESSEL: 
         ME.verts.extend(T)
         for t in xrange(len(T),1,-1):
            ME.verts[-t].sel=1
            EDGES.append([ME.verts[-t],ME.verts[-t+1]])
         ME.verts[-1].sel=1
         EDGES.append([ME.verts[-1],ME.verts[-len(T)]])
         ME.edges.extend(EDGES)
      ME.fill()
      if npoly %500 == 1 :
        ME.sel= True
        ME.remDoubles(0.0)

      ME.sel= False

def cree_FORME(v,TESSEL):
    if 1 :
       VE=[(v[0]-DOCUMENTORIGINE[0])* 85331.2,
           (v[1]-DOCUMENTORIGINE[1])* 110976.0,
           (v[2]-DOCUMENTORIGINE[2])  ]
       TESSEL.append(VE)
    #return TESSEL 

def active_FORME():
    global ME, UPDATE_V, UPDATE_F, POS, OB
    if len(UPDATE_V)>2 :
        #print UPDATE_V
        ME.verts.extend(UPDATE_V)
        FACES=[]
        #print UPDATE_F,  len(UPDATE_F)
        for FE in UPDATE_F:
           #print FE
           if len(FE)<4:
              FACES.append([ME.verts[FE[0]],ME.verts[FE[1]],ME.verts[FE[2]]]) 
           else :
              FACES.append([ME.verts[FE[0]],ME.verts[FE[1]],ME.verts[FE[2]],ME.verts[FE[3]]])
           #if len(ME.faces)%200==1 : print len(ME.faces) 
        if FACES:
              ME.faces.extend(FACES)
    UPDATE_V=[]
    UPDATE_F=[]
    POS=0
     
    if ME.verts:
       ME.sel= True
       ME.remDoubles(0.0)
     
def wash_DATA(ndata):
   if ndata!='':
       ndata=ndata.replace('\n',',')
       ndata=ndata.replace('\r','') 
       while ndata[-1]=='  ': 
         ndata=ndata.replace('  ',' ')
       while ndata[0]==' ':  
         ndata=ndata[1:]
       while ndata[-1]==' ': 
         ndata=ndata[:-1]	
       if ndata[0]==',':ndata.pop(0)
       if ndata[-1]==',':ndata.pop()
       ndata=ndata.replace(',,',',')    
       ndata=ndata.replace(' ',',')
       ndata=ndata.split(',')
       for n in ndata :
          if n=='' : ndata.remove(n)	
   return  ndata 

def collecte_ATTRIBUTS(data):
    data=data.replace('  ',' ')
    ELEM={'TYPE':data[1:data.find(' ')]}
    t1=len(data)
    t2=0
    ct=data.count('="')
    while ct>0:
        t0=data.find('="',t2)
        t2=data.find(' ',t2)+1
        id=data[t2:t0]
        t2=data.find('"',t0+2)
        if id!='d':
           exec "ELEM[id]=\"\"\"%s\"\"\""%(data[t0+2:t2].replace('\\','/'))
        else:
              exec "ELEM[id]=[%s,%s]"%(t0+2,t2) 
        ct=data.count('="',t2)
    return ELEM

def contruit_HIERARCHIE(t):
    global DOCUMENTORIGINE, OB , ME, SC 
    global NUMBER, PLACEMARK
    vv=[]
    TESSEL=[]
    # De select all
    for O in SC.getChildren(): O.sel= False
    OB = Blender.Object.New('Mesh')  
    SC.link(OB)
    ME= OB.getData(mesh=1)
    OB.sel= True

    t=t.replace('\t',' ')
    while t.find('  ')!=-1:
          t=t.replace('  ',' ')
    n0=0 
    t0=t1=0
    baliste=[]
    balisetype=['?','?','/','/','!','!']
    BALISES=['D',  #DECL_TEXTE',
             'D',  #DECL_TEXTE',
             'F',  #FERMANTE',
             'E',  #ELEM_VIDE',
             'd',  #DOC',
             'R',  #REMARQUES',
             'C',  #CONTENU',
             'O'   #OUVRANTE'
             ]

    TAGS = ['kml','Document','description','DocumentSource',
        'DocumentOrigin','visibility','LookAt',
       'Folder','name','heading','tilt','latitude',
       'longitude','range','Style','LineStyle','color',
       'Placemark','styleUrl','GeometryCollection',
       'Polygon','LinearRing','outerBoundaryIs',
       'altitudeMode','coordinates','LineString',
        'fill','PolyStyle','outline','innerBoundaryIs',
        'IconStyle', 'Icon', 'x','y' ,'w','href','h'
        ]

    STACK=[]
    latitude = float(t[t.find('<latitude>')+len('<latitude>'):t.find('</latitude>')])
    longitude = float(t[t.find('<longitude>')+len('<longitude>'):t.find('</longitude>')])
    DOCUMENTORIGINE=[longitude,latitude,0 ]

    GETMAT=0
    MATERIALS=[M.getName() for M in Blender.Material.Get()]
    while t1<len(t) and t0>-1:
      t0=t.find('<',t0)
      t1=t.find('>',t0)
      ouvrante=0
      if t0>-1 and t1>-1:
          if t[t0+1] in balisetype:
             b=balisetype.index(t[t0+1])
             if t[t0+2]=='-': 
               b=balisetype.index(t[t0+1])+1
             balise=BALISES[b]
             
             
             
             # FIXME - JMS WHY IS STACK[-1] None Sometimes?
             try:
               if b==2 and t[t0:t1].find(STACK[-1])>-1:
                   parent=STACK.pop()
             except:
               #print 'ERROR'
               #print b
               #print STACK
               #raise "Error"
               STACK.pop()  # Remove the None value
               
          elif t[t1-1] in balisetype:
            balise=BALISES[balisetype.index(t[t1-1])+1]
          else:
            t2=t.find(' ',t0) 
            if t2>t1: t2=t1
            ouvrante=1
            NOM=t[t0+1:t2]
            if t.find('</'+NOM)>-1:
               balise=BALISES[-1]
            else:
               balise=BALISES[-2]
          if balise=='E' or balise=='O':
             if NOM not in TAGS and NOM!='a':
               TAGS.append(NOM)
             if  balise=='O' and NOM in TAGS: 
                 STACK.append(NOM)
                 if not PLACEMARK :
                    if NOM.startswith('Style'):
                       proprietes=collecte_ATTRIBUTS(t[t0:t1+ouvrante])
                    elif NOM.startswith('PolyStyle'):
                       GETMAT=1
                    elif NOM.startswith('color') and GETMAT:
                       COLOR=t[t2+1:t.find('</color',t2)]
                       print COLOR
                       COLOR=[eval('0x'+COLOR[0:2]), eval('0x'+COLOR[2:4]), eval('0x'+COLOR[4:6]), eval('0x'+COLOR[6:])]
                       print COLOR 
                       if 'id' in proprietes.keys() and proprietes['id'] not in MATERIALS:
                          MAT=Blender.Material.New(proprietes['id'])
                          MAT.rgbCol = [COLOR[3]/255.0,COLOR[2]/255.0,COLOR[1]/255.0]  
                          MAT.setAlpha(COLOR[0]/255.0)
                          MATERIALS.append(MAT.getName())
                       GETMAT=0
                 if NOM.find('Polygon')>-1:
                    VAL=t[t2+2:t.find('</Polygon',t2)]
                    n=VAL.count('<outerBoundaryIs>')+VAL.count('<innerBoundaryIs>')
                 if NUMBER and NOM.find('Placemark')>-1 :
                    PLACEMARK=1
                    if t[t2:t.find('</Placemark',t2)].find('Polygon')>-1 and len(ME.verts)>0:
                       active_FORME()
                       OB.sel= False
                       #[O.select(0) for O in Blender.Object.Get()]
                       OB = Blender.Object.New('Mesh')  # link mesh to an object
                       SC = Blender.Scene.GetCurrent()          # link object to current scene
                       SC.link(OB)
                       ME=OB.getData(mesh=1)
                       OB.sel= True
                 if NOM.find('styleUrl')>-1:
                        material= t[t2+2:t.find('</styleUrl',t2)]
                        if material in MATERIALS :
                           ME.materials=[Blender.Material.Get(material)]
                 if NOM.find('coordinates')>-1:
                    VAL=t[t2+2:t.find('</coordinates',t2)]
                    if STACK[-2]=='DocumentOrigin' :
                       DOCUMENTORIGINE=[float(V) for V in  VAL.replace(' ','').replace('\n','').split(',')]
                    if STACK[-2]=='LinearRing'  :
                       n-=1
                       TESSEL.append([])
                       VAL=wash_DATA(VAL) 
                       vv=[[float(VAL[a+ii]) for ii in xrange(3)] for a in xrange(0,len(VAL),3)]
                       if vv: [cree_FORME(v,TESSEL[-1]) for v in vv]
                       del VAL
                       if n==0:
                         cree_POLYGON(ME,TESSEL)
                         TESSEL= []
             
             elif balise=='O' : 
                 STACK.append(None)
             D=[] 
      else:
         break
      t1+=1
      t0=t1 

def scan_FILE(nom):
  global NUMBER, PLACEMARK, SC, OB, ME

  dir,name=split(nom)
  name=name.split('.')
  result=0
  t=filtreFICHIER(nom)
  POLYGON_NUMBER=t.count('<Polygon')
  print 'Number of Polygons :  ', POLYGON_NUMBER
  if POLYGON_NUMBER==0 :
     name = "WARNING %t| Sorry, the script can\'t find any geometry in this file ."  # if no %xN int is set, indices start from 1
     result = Blender.Draw.PupMenu(name)
     print '#----------------------------------------------'
     print '#  Sorry the script can\'t find any geometry in this' 
     print '#  file .'
     print '#----------------------------------------------'
     Blender.Window.RedrawAll()
     return
  else:
    
    SC = Blender.Scene.GetCurrent()

    PLACEMARK_NUMBER=t.count('<Placemark>')
    print 'Number of Placemark   :  ', PLACEMARK_NUMBER
    if PLACEMARK_NUMBER!=POLYGON_NUMBER :
      NUMBER=1
      PLACEMARK=0
    if t!='false':
      contruit_HIERARCHIE(t)
      active_FORME()
  gt2=Blender.sys.time()-gt1
  print int(gt2/60),':',int(gt2%60) 

if __name__ == '__main__':
	Blender.Window.FileSelector (fonctionSELECT, 'SELECT a .KMZ FILE')