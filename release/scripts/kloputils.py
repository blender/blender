#!BPY

""" Registration info for Blender menus:
Name: 'Kloputils 3'
Blender: 233
Group: 'Wizards'
Tip: 'Set of object alligning, modifing tools.'
"""

##################################################
#                                                #
#        KLOPUTILES v3.1 para Blender v2.33      #
#                   multilingue                  #
#                                                #
##################################################
#		Pulsa Alt+P para ejecutar el script
#	Improved:	use of Mathutils
#	Feature:	actualize objects on ApplyLRS
#	Improved:	Project onto planes
#	Feature:	mover/rotar/escalar objetos al azar
#	Feature:	acercar objs al activo (fijo, abs, rel)
#	Improved:	Fit/Embrace
#	TODO: subdividir solo aristas seleccionadas (NO FUNCIONA)
#	TODO: Elipses
#
##################################################
#
# $Id$
#
# --------------------------------------------------------------------------
# Kloputils by Carlos López (klopez)
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Carlos López, http://klopes.tk
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
from Blender.Mathutils import *
from Blender.NMesh import *
from Blender.BGL import *
from Blender.Draw import *
from Blender.sys import *
from Blender.Noise import random

from math import *

#path=dirname(progname)
#path=[dirname(progname),'.blender','bpydata']
path=Blender.Get("datadir")
sc=Blender.Scene.GetCurrent()
MId=Matrix([1,0],[0,1])
MId.resize4x4()

coo=['X','Y','Z']
l=PupMenu('Choose language%t|Espanol%x1|English%x2|Catala%x3')
Lang=['Espanol','English','Catala'][l-1]



#Buffer:
BfL=[0.,0.,0.]
BfR=[0.,0.,0.]
BfS=[1.,1.,1.]

pagina=Create(1)
creake=Create(1)
editake=Create(2)
moveke=Create(1)

numReal=Create(0.)
movekomo=Create(1)
menuLRS=Create(1)

ndiv=Create(3)
akeplano=Create(2)
akeplanoXYZ=Create(3)
enkedir=Create(3)

nLocX=Create(1.0)
nLocY=Create(2.0)
nLocZ=Create(3.0)
nRotX=Create(0.0)
nRotY=Create(0.0)
nRotZ=Create(0.0)
nSizX=Create(1.0)
nSizY=Create(1.0)
nSizZ=Create(1.0)
other=Create(1)

ang1=Create(0.0)
ang2=Create(30.0)
angulo=Create(30.0)
radio=Create(1.0)
pts=Create(10)
ctro=Create(0)
meTx=Create('Arco')

arcorden=Create(0)

CreaNew=Create(1)
Cuantos=Create(2)
toBuffer=Create(0)

menueqiX=Create(1)
sepX=Create(0.5)
aliX=Create(1)
menueqiY=Create(1)
sepY=Create(0.0)
aliY=Create(0)
menueqiZ=Create(2)
sepZ=Create(1.0)
aliZ=Create(0)

girX=Create(0)
incX=Create(0.0)
girY=Create(0)
incY=Create(0.0)
girZ=Create(1)
incZ=Create(5.0)

scaX=Create(1)
iScaX=Create(.5)
scaY=Create(1)
iScaY=Create(.5)
scaZ=Create(1)
iScaZ=Create(.5)

encX=Create(1)
encY=Create(0)
encZ=Create(0)
en2X=Create(1)
en2Y=Create(0)
en2Z=Create(0)


def CreaObjDeMalla(mm,o,nombre):
	oo=Blender.Object.New('Mesh')
	oo.link(mm)
	oo.setName(nombre)
	oo.LocX,oo.LocY,oo.LocZ=o.LocX,o.LocY,o.LocZ
	oo.RotX,oo.RotY,oo.RotZ=o.RotX,o.RotY,o.RotZ
	oo.SizeX,oo.SizeY,oo.SizeZ=o.SizeX,o.SizeY,o.SizeZ
	sc.link(oo)
	return oo

def rectang(x,y,l,h,r,g,b):
	glColor3f(r,g,b)
	glBegin(GL_POLYGON)
	glVertex2i(x, y)
	glVertex2i(l, y)
	glVertex2i(l, h)
	glVertex2i(x, h)
	glEnd()

def SituaL(objeto,coord,X):
	if coord==0:
		objeto.LocX=X
	elif coord==1:
		objeto.LocY=X
	elif coord==2:
		objeto.LocZ=X
def SituaR(objeto,coord,X):
	if coord==0:
		objeto.RotX=X
	elif coord==1:
		objeto.RotY=X
	elif coord==2:
		objeto.RotZ=X
def SituaS(objeto,coord,X):
	if coord==0:
		objeto.SizeX=X
	elif coord==1:
		objeto.SizeY=X
	elif coord==2:
		objeto.SizeZ=X

def mallaScan(o):
	return o.getType()=='Mesh'

def Sistema(MM,D):
	MM.invert()
	return MatMultVec(MM,D)

def GloVrt((x,y,z),M):	# Devuelve (vector) coords globs de v
	v=Vector([x,y,z,1])
	v=VecMultMat(v,M)
	v.resize3D()
	return v

def Extremo(lista,M=MId):	# Devuelve extremos y pto medio de lista de vectores
	o1=GloVrt(lista[0],M)
	o2=GloVrt(lista[0],M)
	for v in lista:
		va=Vector(list(v))
		va.resize4D()
		va=VecMultMat(va,M)
		for c in range(3):
			o1[c]=min(va[c],o1[c])
			o2[c]=max(va[c],o2[c])
	return [o1,(o1+o2)/2,o2]

def Media(lista):	#Media
	ctr=Vector([0,0,0])
	for c in range(3):
		for l in lista: ctr[c]=ctr[c]+l[c]
		ctr[c]=ctr[c]/len(lista)
	return ctr






def draw():
	global pagina, toBuffer
	glClearColor(0.4,0.2,0.2,0.0)
	glClear(GL_COLOR_BUFFER_BIT)

	pagina=Menu(menu0,98,20,370,170,28,pagina.val,"Selecciona pagina")
	toBuffer=Menu(menu4,80,200,370,20,15,toBuffer.val,"Copy values to the internal buffer")
	glColor3f(0.6,0.6,0.2)
	glRasterPos2i(198,388)
	Text("Copy",'small')

	if (pagina.val==1): draw1()
	elif (pagina.val==2): draw2()
	elif (pagina.val==3): draw3()
	elif (pagina.val==4): draw4()

	Button(menuExit,99,20,5,200,18)

def draw4():
	global moveke,o
	global aliX,aliY,aliZ,numReal,movekomo,menuLRS,menuPaste
	

	rectang(15,38,225,264,0.2,0.,0.)

	moveke=Menu(menu5,98,10,248,200,20,moveke.val,"Modifica la matriz de objetos seleccionados")
	if moveke.val==2:
		aliX=Toggle("X",0, 50,160,45,30,aliX.val,"Activa modif en X")
		aliY=Toggle("Y",0, 95,160,45,30,aliY.val,"Activa modif en Y")
		aliZ=Toggle("Z",0,140,160,45,30,aliZ.val,"Activa modif en Z")
		menuLRS=Menu("Loc%x1|Rot%x2|Size%x3",98,20,110,50,20,menuLRS.val)
	if moveke.val==1:
		movekomo=Menu(menu5a,98,20,225,200,20,movekomo.val,"Tipo de transformacion")
		m,M,tip= -100,100,"Cantidad"
	else:
		if menuLRS.val==1:
			m,M,tip= -1000, 1000, "Distancia"
		elif menuLRS.val==2:
			m,M,tip= 0, 360, "Angulo"
		elif menuLRS.val==3:
			m,M,tip= 0, 100, "Escala"
	if moveke.val==1 and movekomo.val==3:	tip="Multiplicador de "+tip
	if moveke.val==2:	tip=tip+" (maximo)"

	numReal=Number("",0,20,140,178,18,numReal.val,m,M,tip)
	glColor3f(0.6,0.6,0.2)
	glRasterPos2i(200,156)
	Text("Paste",'small')
	menuPaste=Menu("Paste...%t|LocX%x1|LocY%x2|LocZ%x3|\Loc\%x4|RotX%x5|RotY%x6|RotZ%x7|SizeX%x8|SizeY%x9|SizeZ%x10"
,88,202,140,20,12,0,"Paste what?")
	ok=Button("OK",50,80,110,80,20)


def draw3():
	global editake,ndiv,akeplano,akeplanoXYZ,enkedir,ok
	global other
	global getlocrotsiz,ok,nLocX,nLocY,nLocZ
	global nRotX,nRotY,nRotZ,nSizX,nSizY,nSizZ
	global Cloc,Ploc,Crot,Prot,Csiz,Psiz

	rectang(15,38,225,264,0.3,0.7,0.5)

	editake=Menu(menu3,98,10,248,143,20,editake.val,"Menu de edicion de mayas")
	if(editake.val==1):
		ndiv=Number(menu3a,98,20,200,150,18,ndiv.val,2,1000)
		ok=Button("OK",20,172,200,32,18)
	elif(editake.val==2):
		akeplano=Menu(menu3b,98,20,200,140,20,akeplano.val)
		if akeplano.val in [1,2]:
			akeplanoXYZ=Menu(menu3c,98,162,200,55,20,akeplanoXYZ.val) 
		enkedir=Menu(menu3d,98,20,100,140,20,enkedir.val)
		ok=Button("OK",40,162,100,55,20)
	elif(editake.val==3):
		Button("P",87,192,222,30,18,"Paste buffer matrix")
		nLocX=Number("LocX:",98,20,200,170,19,nLocX.val,-1000.,1000.)
		nLocY=Number("LocY:",98,20,180,170,19,nLocY.val,-1000.,1000.)
		nLocZ=Number("LocZ:",98,20,160,170,19,nLocZ.val,-1000.,1000.)
		nRotX=Number("RotX:",98,20,140,170,19,nRotX.val,-1000.,1000.)
		nRotY=Number("RotY:",98,20,120,170,19,nRotY.val,-1000.,1000.)
		nRotZ=Number("RotZ:",98,20,100,170,19,nRotZ.val,-1000.,1000.)
		nSizX=Number("SizeX:",98,20, 80,170,19,nSizX.val,-1000.,1000.)
		nSizY=Number("SizeY:",98,20, 60,170,19,nSizY.val,-1000.,1000.)
		nSizZ=Number("SizeZ:",98,20, 40,170,19,nSizZ.val,-1000.,1000.)
		other=Toggle("Actulize others",98,20,222,170,18,other.val)
		oklocrotsiz=Button("OK",30,192,40,30,179)

def draw2():
	global creake,ang1,ang2,angulo,radio,pts,ctro,meTx,arcorden

	rectang(15,65,224,257,0.7,0.5,0.65)

	creake=Menu(menu2,98,10,248,143,20,creake.val,"Tipo de objeto")
	if(creake.val==1):
		pts=Number(menu2a[0],98,20,115,80,18,pts.val,2,1000)
		ctro=Toggle(menu2a[1],98,102,115,50,18,ctro.val,"Cierra el arco con el centro")
		arcorden=Menu(menu2a[2]+"%t|1-2-3%x1|2-3-1%x2|3-1-2%x3",17,155,115,65,18,arcorden.val,"Orden de los vertices")
		meTx=String("Objeto: ",98,20,70,200,22,meTx.val,30,"Nombre del objeto a crear")
	if(creake.val==2):
		ang1=Slider(menu2a[4],10,20,220,200,18,ang1.val,-360,360,0,"Angulo inicial en grados")
		ang2=Slider(menu2a[5],11,20,200,200,18,ang2.val,ang1.val,ang1.val+360,0,"Angulo final en grados")
		angulo=Slider(menu2a[6],12,20,170,200,18,angulo.val,-360,360,0,"Angulo del arco en grados")
		radio=Slider(menu2a[7],13,20,140,200,18,radio.val,0,1,0)
		pts=Number(menu2a[8],14,20,115,80,18,pts.val,2,1000)
		ctro=Toggle(menu2a[9],15,102,115,50,18,ctro.val,"Cierra el arco con el centro")
		meTx=String(menu2a[10],16,20,70,200,22,meTx.val,30,"Nombre de la maya a sustituir")
	if(creake.val==3):
		pts=Number("Pts:",98,20,115,80,18,pts.val,2,1000)
		ok=Button("OK",18,102,115,120,36)
		meTx=String("Objeto: ",98,20,70,200,22,meTx.val,30,"Nombre del objeto a crear")
		
def draw1():
	global menueqiX,sepX,aliX,menueqiY,sepY,aliY,menueqiZ,sepZ,aliZ
	global girX,incX,girY,incY,girZ,incZ
	global encX,encY,encZ,en2X,en2Y,en2Z
	global scaX,scaY,scaZ,iScaX,iScaY,iScaZ
	global CreaNew,Cuantos,toBuffer

######################### ALINEACIONES #####################
	rectang(5,167,254,292,0.7,0.5,0.65)
	Button("C",81,194,270,25,18,"Copy")
	Button("P",82,220,270,25,18,"Paste buffer Loc")

	aliX=Toggle("X",0,10,270,30,18,aliX.val,"Activa alineacion X")
	menueqiX=Menu(menu1b,98,41,270,148,18,menueqiX.val,"Extremos de la separacion")  
	sepX=Number(menu1a[4],0,10,250,179,18,sepX.val,-1000,1000,"Distancia de separacion X")

	aliY=Toggle("Y",0,10,230,30,18,aliY.val,"Activa alineacion Y")
	menueqiY=Menu(menu1b,98,41,230,148,18,menueqiY.val,"Extremos de la separacion")  
	sepY=Number(menu1a[4],0,10,210,179,18,sepY.val,-1000,1000,"Distancia de separacion Y")

	aliZ=Toggle("Z",0,10,190,30,18,aliZ.val,"Activa alineacion Z")
	menueqiZ=Menu(menu1b,98,41,190,148,18,menueqiZ.val,"Extremos de la separacion")  
	sepZ=Number(menu1a[4],0,10,170,179,18,sepZ.val,-1000,1000,"Distancia de separacion Z")

########################## GIROS ##############################
	rectang(5,97,254,162,0.67,0.54,0.1)
	Button("C",83,194,140,25,18,"Copy")
	Button("P",84,220,140,25,18,"Paste buffer Rot")

	girX=Toggle("X",0,10,140,30,18,girX.val,"Incrementa valores RotX")
	incX=Number("X: ",0,42,140,147,18,incX.val,-180,180,"Incremento en grados")
	girY=Toggle("Y",0,10,120,30,18,girY.val,"Incrementa valores RotY")
	incY=Number("Y: ",0,42,120,147,18,incY.val,-180,180,"Incremento en grados")
	girZ=Toggle("Z",0,10,100,30,18,girZ.val,"Incrementa valores RotZ")
	incZ=Number("Z: ",0,42,100,147,18,incZ.val,-180,180,"Incremento en grados")

######################### ESCALADOS ##########################
	rectang(5,27,254,92,0.27,0.54,0.4)
	Button("C",85,194,70,25,18,"Copy")
	Button("P",86,220,70,25,18,"Paste buffer Sca")

	scaX=Toggle("X",0,10,70,30,18,scaX.val)
	iScaX=Number(menu1a[7],0,42,70,147,18,iScaX.val,-180,180,"Incremento de escala")
	scaY=Toggle("Y",0,10,50,30,18,scaY.val)
	iScaY=Number(menu1a[7],0,42,50,147,18,iScaY.val,-180,180,"Incremento de escala")
	scaZ=Toggle("Z",0,10,30,30,18,scaZ.val)
	iScaZ=Number(menu1a[7],0,42,30,147,18,iScaZ.val,-180,180,"Incremento de escala")
########################## ENCAJES ##########################
	rectang(5,317,254,363,0.1,0.5,0.6)

	encX=Toggle(menu1a[0]+"X",0,10,340,43,18,encX.val,"Desplaza el objeto")
	encY=Toggle(menu1a[0]+"Y",0,55,340,43,18,encY.val,"Desplaza el objeto")
	encZ=Toggle(menu1a[0]+"Z",0,100,340,43,18,encZ.val,"Desplaza el objeto")

	en2X=Toggle(menu1a[1]+"X",0,10,320,43,18,en2X.val,"Ajusta tamano")
	en2Y=Toggle(menu1a[1]+"Y",0,55,320,43,18,en2Y.val,"Ajusta tamano")
	en2Z=Toggle(menu1a[1]+"Z",0,100,320,43,18,en2Z.val,"Ajusta tamano")
######################################################
	CreaNew=Toggle(menu1a[8],98,10,295,129,18,CreaNew.val)
	if CreaNew.val:
		Cuantos=Number('',98,139,295,30,18,Cuantos.val,1,999,"Numero de copias")
	Button(menu1a[5],1,190,170,60,98)		#Alinea
	Button(menu1a[6],2,190,100,60,38)		#Gira
	Button(menu1a[9],3,190,30,29,38)		#Escala+
	Button(menu1a[10],7,220,30,30,38)		#Escala*
	Button(menu1a[2],6,147,320,45,38,"Encaja activo en otro u otros 2 (crea nuevo objeto)")		#Encaja
	Button(menu1a[3],5,195,320,55,38,"Encaja activo en otro u otros 2 (crea nuevo objeto)")		#Abarca

def HaceArco(pts,ang1,angul,ctro,R):
	me=New()
	for c in range(pts):
		alfa=(ang1+angul*c/(pts-1))*pi/180
		if (c): v1=v
		v=Vert()
		v.co[0]=R*cos(alfa)
		v.co[1]=R*sin(alfa)
		v.co[2]=0
		me.verts.append(v)
		if (c):
			f=Face()
			f.v=[v1]
			f.v.append(v)
			me.faces.append(f)
		if (c==0): v0=v
	if(ctro):
		v1=v
		v=Vert()
		me.verts.append(v)
		f=Face()
		f.v=[v1,v]
		me.faces.append(f)
		f=Face()
		f.v=[v,v0]
		me.faces.append(f)
	return me

def Exec_align(os,c,ali,eqi,sep):
	coor=os[0].loc[c]
	ademas=0
	flag=0
	for o in os:
		m=o.data     #Maya del objeto
		ctr=[0,0,0]
		if(eqi[c]==2):
			for d in range(3): ctr[d]=Extremo(m.verts,o.matrix)[1][d]
		elif(eqi[c]==3 or eqi[c]==6):
			ctr=Extremo(m.verts,o.matrix)[0]
		elif(eqi[c]==4):
			ctr=Extremo(m.verts,o.matrix)[2]
		elif(eqi[c]==5):
			ctr=Media(m.verts)
			ctr=GloVrt(ctr,o.matrix)

		if (eqi[c]>1): ademas=ctr[c]-o.loc[c]
		if (flag):	coor=coor-ademas
		else:	flag=1
		SituaL(o,c,coor)
		if(eqi[c]==6): ademas=Extremo(m.verts,o.matrix)[2][c]-o.loc[c]
		coor=coor+sep[c]+ademas

def event(evt,val):
	if (evt==ESCKEY and not val):
		print "Bye..."
		Exit()

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@22#

def bevent(evt):
	global me
	global BfL,BfR,BfS
	print "Evento:",evt
	os=Blender.Object.GetSelected()
	if os:
		oa=os[0]

	##########MENU BUFFER
	if evt==80:
		copi=toBuffer.val
		if copi==1:
			if len(os)==2:
				for c in range(3): BfL[c]=os[1].loc[c]-os[0].loc[c]
				print "Copiado al buffer Loc:",BfL
			else:
				PupMenu('ERROR%t|Selecciona 2 objetos|Just select 2 objects!')
		if copi==2:
			if len(os)==2:
				BfL=3*[sqrt((os[1].loc[0]-os[0].loc[0])**2+
(os[1].loc[1]-oa.loc[1])**2+
(os[1].loc[2]-oa.loc[2])**2)]
				print "Copiado al buffer Loc:",BfL
			else:
				PupMenu('ERROR%t|Selecciona 2 objetos|Just select 2 objects!')
		if copi==3:
			if len(os)==2:
				for c in range(3): BfR[c]=os[1].rot[c]-oa.rot[c]
				print "Copiado al buffer Rot:",BfR
			else:
				PupMenu('ERROR%t|Selecciona 2 objetos|Just select 2 objects!')
		if copi==4 and len(os):
			lista=[]
			for o in os: lista.append(o.loc)
			BfL=Media(lista)
			print "Copiado al buffer Loc:",BfL
			lista=[]
			for o in os: lista.append(o.rot)
			BfR=Media(lista)
			for c in range(3): BfR[c]*=180/pi
			print "Copiado al buffer Roc:",BfR
			lista=[]
			for o in os: lista.append(o.size)
			BfS=Media(lista)
			print "Copiado al buffer Siz:",BfS
		if copi==5:
			print"\n_Contenido del buffer_\n"
			for c in range(3): print "Loc",coo[c]," :",BfL[c]
			for c in range(3): print "Rot",coo[c]," :",BfR[c]
			for c in range(3): print "Size",coo[c],":",BfS[c]
			print "______________________"

	##########COPY
	if evt==81:
		X	=[sepX.val,sepY.val,sepZ.val]
		ali	=[aliX.val,aliY.val,aliZ.val]
		for c in range(3):
			if ali[c]:
				BfL[c]=X[c]
				print"Copiando al buffer Loc",coo[c],"el valor", BfL[c]

	if evt==83:
		X	=[incX.val,incY.val,incZ.val]
		ali	=[girX.val,girY.val,girZ.val]
		for c in range(3):
			if ali[c]:
				BfR[c]=X[c]
				print"Copiando al buffer Loc",coo[c],"el valor", BfR[c]

	if evt==85:
		X	=[iScaX.val,iScaY.val,iScaZ.val]
		ali	=[scaX.val,scaY.val,scaZ.val]
		for c in range(3):
			if ali[c]:
				BfS[c]=X[c]
				print"Copiando al buffer Loc",coo[c],"el valor", BfS[c]

	##########PASTE
	if evt==82:
		X	=[sepX.val,sepY.val,sepZ.val]
		ali	=[aliX.val,aliY.val,aliZ.val]
		for c in range(3):
			if ali[c]:
				X[c]=BfL[c]
				print"Pegando del buffer Loc",coo[c],"el valor", BfL[c]
		sepX.val,sepY.val,sepZ.val=X

	if evt==84:
		X	=[incX.val,incY.val,incZ.val]
		ali	=[girX.val,girY.val,girZ.val]
		for c in range(3):
			if ali[c]:
				X[c]=BfR[c]
				print"Pegando del buffer Rot",coo[c],"el valor", BfR[c]
		incX.val,incY.val,incZ.val=X

	if evt==86:
		X	=[iScaX.val,iScaY.val,iScaZ.val]
		ali	=[scaX.val,scaY.val,scaZ.val]
		for c in range(3):
			if ali[c]:
				X[c]=BfS[c]
				print"Pegando del buffer Size",coo[c],"el valor", BfS[c]
		iScaX.val,iScaY.val,iScaZ.val=X

	if evt==87:
		print"Pegando matriz a <AplicLocRotSize>"
		nLocX.val,nLocY.val,nLocZ.val=BfL
		nRotX.val,nRotY.val,nRotZ.val=BfR
		nSizX.val,nSizY.val,nSizZ.val=BfS

	if evt==88:
		print"Pegando num. real a boton"
		x=[BfL[0],BfL[1],BfL[2],
sqrt(BfL[0]**2+BfL[1]**2+BfL[2]**2),
BfR[0],BfR[1],BfR[2],
BfS[0],BfS[1],BfS[2]]
		numReal.val = x[menuPaste.val-1]

	if evt==50:
		if moveke.val==1:				#		MOVER HACIA EL O.ACTIVO
			oaV=Vector(list(oa.loc))
			for o in os[1:]:
				oV=Vector(list(o.loc))
				v=oV-oaV									#Calculo del vector final
				d=sqrt(DotVecs(v,v))
				v1=v*numReal.val
				if movekomo.val==1:
					v1 = oaV+v1/d
				elif movekomo.val==2:
					v1 = oV+v1/d
				elif movekomo.val==3:
					v1 = oaV+v1
				o.setLocation(v1)

		if moveke.val==2:					#	MOVER AL AZAR
			n=numReal.val
			if menuLRS.val==2:
				n=n*pi/180
			for o in os:
				v1=Vector([
aliX.val * (2*n*random() - n),
aliY.val * (2*n*random() - n),
aliZ.val * (2*n*random() - n)])

				if menuLRS.val==1:							#Uso del vector final
					oV=Vector(list(o.loc))
					o.setLocation(oV+v1)
				elif menuLRS.val==2:
					v=Vector(list(o.getEuler()))+v1
					o.setEuler(v)
				else:
					oV=Vector(list(o.size))
					o.setSize(oV+v1)

	if evt==40:				#					PROYECTA EN PLANOS
		n=Vector([0.,0.,0.])

		numObjs=-len(os)+1
		if akeplano.val==1:	#plano global
			n[akeplanoXYZ.val-1]=1.
			numObjs=-len(os)
		elif akeplano.val==2: n=oa.matrix[akeplanoXYZ.val-1] #plano local
		n.resize3D()

		d=Vector([0.,0.,0.])
		if enkedir.val<4: d[enkedir.val-1]=1.	#direccion global
		elif enkedir.val==4: d=n				#direc. ortog. al plano

		n1 = Vector([-n[2],n[0],n[1]])
		N1 = CrossVecs (n,n1)
		N2 = CrossVecs (n,N1)	
		n.normalize(), N1.normalize(), N2.normalize()
		#print"productos escalares (deben ser 0):",DotVecs(N1,n),DotVecs(N2,n),DotVecs(N2,N1)
		p=oa.loc
		N=Matrix(
	[N1[0],N1[1],N1[2],0],
	[N2[0],N2[1],N2[2],0],
	[ n[0], n[1], n[2],0],
	[ p[0], p[1], p[2],1])
		NI=CopyMat(N)
		NI.invert()

		dN=VecMultMat(d,NI.rotationPart())
		if dN[2]==0:
			PupMenu('Error%t|Operacion no permitida: la direccion esta en el plano%x1|Illegal operation: plane contains direction%x2')
			return
		print"Direccion absoluta:",d,"\nNormal al plano:",n,"\nDireccion relativa:",dN
		for o in filter(mallaScan,os[numObjs:]):
			me=Blender.NMesh.GetRawFromObject(o.name)
			M=o.matrix
			for v in me.verts:
				v0=Vector([v[0],v[1],v[2]])
				v0=GloVrt(v0,M)
				v0.resize4D()
				v0=VecMultMat(v0,NI)
				v[0] = v0[0] - v0[2]/dN[2] * dN[0]
				v[1] = v0[1] - v0[2]/dN[2] * dN[1]
				v[2] = 0
			oo=Blender.Object.New('Mesh')
			oo.setName(o.name+'Proy')
			oo.link(me)
			sc.link(oo)
			oo.setMatrix(N)

	if(evt>9 and evt<17):	#	HACE ARCO INTERACTIV.
		GetRaw(meTx.val)
		if(evt==11): angulo.val=ang2.val-ang1.val
		if(evt==12 or evt==10): ang2.val=(ang1.val+angulo.val)
		m=HaceArco(pts.val,ang1.val,angulo.val,ctro.val,radio.val)
		PutRaw(m,meTx.val)

	if evt in [17,18]: 	# HACE ARCO CON 3 PTS.
		o=oa
		m=o.getData()
		M=o.matrix
		if arcorden.val==1 or evt==18:
			p1,p2,p3=m.verts[0],m.verts[1],m.verts[2]
		elif arcorden.val==2:
			p1,p2,p3=m.verts[1],m.verts[2],m.verts[0]
		elif arcorden.val==3:
			p1,p2,p3=m.verts[2],m.verts[0],m.verts[1]

		p1=GloVrt(p1,M)
		p2=GloVrt(p2,M)
		p3=GloVrt(p3,M)

		print "Puntos:\n ",p1,p2,p3
		
		p21=p2-p1
		p31=p3-p1

		M1=CrossVecs(p21,p31)

		D=DotVecs(M1,p1)
		D2=DotVecs(p21,p1+p2)/2
		D3=DotVecs(p31,p1+p3)/2

		SS=Matrix(
		[M1[0],M1[1],M1[2]],
		[p21[0],p21[1],p21[2]],
		[p31[0],p31[1],p31[2]],
		)
		O=Sistema(SS,Vector([D,D2,D3]))
		
		v2=p2-O
		b3=p3-O
		vN=CrossVecs(v2,b3)
		v3=CrossVecs(v2,vN)

			#Terna ortogonal: v2,vN,v3
		R=sqrt(DotVecs(v2,v2))
		RN=sqrt(DotVecs(vN,vN))
		R3=sqrt(DotVecs(v3,v3))

		if evt==18: angul=2*pi
		else: angul=AngleBetweenVecs(v2,b3)

		print "Centro:",O
		print "Radio :",R,"RN:",RN,"R3:",R3
		print "Angulo:",angul
		M2=Matrix([v2[0]/R,v2[1]/R,v2[2]/R],[-v3[0]/R3,-v3[1]/R3,-v3[2]/R3],[vN[0]/RN,vN[1]/RN,vN[2]/RN])

		arco=HaceArco(pts.val,0,angul,ctro.val,1)
		oo=CreaObjDeMalla(arco,o,meTx.val+'.In')
		EU=M2.toEuler()
		EU=EU[0]*pi/180,EU[1]*pi/180,EU[2]*pi/180
		oo.RotX,oo.RotY,oo.RotZ=EU[0],EU[1],EU[2]
		oo.setLocation(O)
		oo.setSize(R,R,R)

		if evt==17:
			angul=angul-360
			arco=HaceArco(pts.val,0,angul,ctro.val,1)
			oo=CreaObjDeMalla(arco,o,meTx.val+'.Ex')
			oo.RotX,oo.RotY,oo.RotZ=EU[0],EU[1],EU[2]
			oo.setLocation(O)
			oo.setSize(R,R,R)

	if(evt==20): # SUBDIVIDE
		for o in filter(mallaScan,os):
			m=o.data
			mm=New()
			mm.name=m.name+'.Subdiv'
			for v in m.verts: mm.verts.append(v)
			N=ndiv.val
			NV=len(m.verts)-1
			for k1 in range(NV+1):
				v1=m.verts[k1]
				for k2 in range(NV-k1):
					v2=m.verts[NV-k2]
					for f in m.faces:
						if (v1 in f.v) and (v2 in f.v):	#	SI...
							dif=abs(f.v.index(v2)-f.v.index(v1))
							if dif==1 or dif==len(f.v)-1: #...entonces f contiene la arista v1-v2
								v=v1
								for K in range(N):
									cara=Face()
									cara.v.append(v)
									if K<N-1:
										v=Vert()
										for c in range(3): v.co[c]=(v1.co[c]*(N-K-1)+v2.co[c]*(K+1))/N
										mm.verts.append(v)
									elif K==N-1: v=v2
									cara.v.append(v)
									mm.faces.append(cara)
							break #para que no se repitan aristas comunes a varias caras
			CreaObjDeMalla(mm,o,o.name+'.Subdiv')

	if evt==30:						# APLICA LOC.ROT.SIZE
		for o in filter(mallaScan,os):
			M=o.matrix

			eu=Euler([nRotX.val,nRotY.val,nRotZ.val])
			Mr=eu.toMatrix()
			Mr.resize4x4()
			Mt=TranslationMatrix(Vector([nLocX.val,nLocY.val,nLocZ.val]))

			o.setMatrix(Mr*Mt)
			o.setSize(nSizX.val,nSizY.val,nSizZ.val)
			MI=o.getInverseMatrix()
			P=M*MI
			maya=o.getData()
			for v in maya.verts:
				w=list(VecMultMat(Vector([v[0],v[1],v[2],1]),P))
				for c in range(3):	v[c]=w[c]/o.size[c]

			maya.update()
			if other.val:
				P.invert()
				for oo in Blender.Object.Get():
					if oo.data.name==maya.name and o!=oo:
						N=oo.getMatrix()
						oo.setMatrix(P*N)
						oo.setSize(oo.SizeX*nSizX.val,oo.SizeY*nSizY.val,oo.SizeZ*nSizZ.val)

	if((evt==5 or evt==6) and len(os)):   #  ENCAJA-ABARCA
		enc=[encX.val,encY.val,encZ.val]
		en2=[en2X.val,en2Y.val,en2Z.val]
		me=GetRaw(oa.data.name)
		meVs=me.verts
		for v in meVs:
			w=GloVrt(v,oa.matrix)
			for c in range(3):
				v[c]=w[c]
		for c in range(3):
			if en2[c] or enc[c]:
				if (len(os)>1):
					n1=Extremo(os[1].data.verts,os[1].matrix)[0][c]
					m1=Extremo(os[1].data.verts,os[1].matrix)[2][c]
				if (len(os)>2):
					n2=Extremo(os[2].data.verts,os[2].matrix)[0][c]
					m2=Extremo(os[2].data.verts,os[2].matrix)[2][c]
				n0=Extremo(meVs)[0] [c]
				m0=Extremo(meVs)[2] [c]
				ancho=m0-n0
				pm=(m0+n0)/2
				if (len(os)==1): n1,m1=n0,m0
				if (len(os)<3): n2,m2=n1,m1
				if (n2<n1):
					t,s=n2,m2
					n2,n1,m2,m1=n1,t,m1,s
				print coo[c],n0,m0,n1,m1,n2,m2
			for v in meVs:
				A , factor = 0. , 1.
				if enc[c]:
					if evt==5:	pm2=(n2+m1)/2
					else:		pm2=(m2+n1)/2
					v[c]+= pm2-pm
				if en2[c] and ancho:
					if evt==5:	factor=(n2-m1)/ancho
					else:		factor=(m2-n1)/ancho
					v[c]=pm2+(v[c]-pm2)*factor
		PutRaw(me)

	elif (evt==1 and len(os)):
		ali=[aliX.val,aliY.val,aliZ.val]
		eqi=[menueqiX.val,menueqiY.val,menueqiZ.val]
		sep=[sepX.val,sepY.val,sepZ.val]
		if CreaNew.val:
			for o in filter(mallaScan,os):
				newos=[o]
				for i in range(Cuantos.val):
					newo=CreaObjDeMalla(o.getData(),o,o.name)
					newos.append(newo)
					for c in range(3):
						if (ali[c]):
							Exec_align(newos,c,ali,eqi,sep)
		else:
			for c in range(3):
				if (ali[c]):
					Exec_align(filter(mallaScan,os),c,ali,eqi,sep)
	elif (evt==2 and len(os)):
		gir=[girX.val,girY.val,girZ.val]
		inc=[incX.val,incY.val,incZ.val]
		if CreaNew.val:
			for o in filter(mallaScan,os):
				newos=[o]
				for i in range(Cuantos.val):
					newo=CreaObjDeMalla(o.getData(),o,o.name)
					newos.append(newo)
					for c in range(3):
						valor=o.rot[c]
						for oo in newos:
							if (gir[c]):
								SituaR(oo,c,valor)
								valor=valor+inc[c]*pi/180
		else:
			for c in range(3):
				if (gir[c]):
					valor=oa.rot[c]
					for o in os:
						SituaR(o,c,valor)
						valor=valor+inc[c]*pi/180
				
	elif evt in [3,7] and len(os):
		sca=[scaX.val,scaY.val,scaZ.val]
		inc=[iScaX.val,iScaY.val,iScaZ.val]
		if CreaNew.val:
			for o in filter(mallaScan,os):
				newos=[o]
				for i in range(Cuantos.val):
					newo=CreaObjDeMalla(o.getData(),o,o.name)
					newos.append(newo)
					for c in range(3):
						valor=o.size[c]
						for oo in newos:
							if (sca[c]):
								SituaS(oo,c,valor)
								if evt==3: valor=valor+inc[c]
								if evt==7: valor=valor*inc[c]
		else:
			for c in range(3):
				if (sca[c]):
					valor=oa.size[c]
					for o in os:
						SituaS(o,c,valor)
						if evt==3: valor=valor+inc[c]
						if evt==7: valor=valor*inc[c]

	elif (evt==99): Exit()
	Blender.Redraw()

#file=open(path+dirsep+'KUlang.txt','r')
#file=open(dirsep.join(path)+dirsep+'KUlang.txt','r')
file=open(join(path,'KUlang.txt'),'r')
fich=file.readlines()
print "\n\nKloputils",fich[0]
for i in range(len(fich)):
	if fich[i]==Lang+'\012': break
print "Lenguaje:",fich[i]
menuExit=fich[i+1]#Sale prog.
menu0=fich[i+2]#Pral.
J=int(fich[i+3])#Alinea:botones
menu1a=[]
for j in range(J): menu1a.append(fich[i+j+4])
i=i+J
menu1b=fich[i+4]#Alinea:menu separa
menu2=str(fich[i+5])#Crea:menu
J=int(fich[i+6])#Crea:botones
menu2a=[]
for j in range(J): menu2a.append(fich[i+j+7])
i=i+J
menu3=fich[i+7]#Modif:menu
menu3a=fich[i+8]#Modif:"partes
menu3b=fich[i+9]#Modif:menu plano
menu3c=fich[i+10]#Modf:"Actua...
menu3d=fich[i+11]#Modif:menu dire
menu3e=fich[i+12]#Modf:"Captura
menu4=fich[i+13]#Buffer
menu5=fich[i+14]#ModifObjs
menu5a=fich[i+15]

file.close

Register(draw,event,bevent)
