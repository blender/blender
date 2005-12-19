#!BPY
"""
Name: 'Empties to Armature'
Blender: 239
Group: 'Animation'
Tooltip: 'Create Armature from a parented-empties chain'
"""
__author__ = "Jean-Baptiste PERIN (jb_perin(at)yahoo.fr)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://perso.wanadoo.fr/jb.perin/",
"Documentation, http://perso.wanadoo.fr/jb.perin/BVH2ARM/doc/bvh2arm.html",
"Mocap tutorial, http://perso.wanadoo.fr/jb.perin/Mocap/MocapAnimation.html",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender") 

__version__ = "2.4"

__bpydoc__ = """ BVH2ARM.py 

Script for generating armature on BVH empties.

This script generates an armature upon an empty-made parented chain, 
and make the armature follow the hip bone of the chain. 
User only have to set up IKSolver contraints on every end effector bone.

Usage:<br>
 - Import a bvh in Blender (File->Import->BVH);<br>
 - Select the root empty of the hierarchical chain.<br>
 - Launch this script ;<br>
 - Set up variables:<br>
   "hipbonename": the name of the main bone (automatically set to the selected empty).<br>
   "startframe":  the first frame of your anim;<br>
   "endframe":  the last frame of your anim;<br>
   "decimation": the frequency (in number of frame) to which the armature's pos is updated;<br>
- Press "Create Armature".<br>
- Set IKSolver for every end effector bone (targeting)

Notes: <br>
- The start frame configuration is used as the rest pose for the armature.<br>
- If the armature already exists when script is launched, the current armature is re-used.
"""
# -------------------------------------------------------------------------- 
# BVH2ARM.py 
# -------------------------------------------------------------------------- 
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
# -------------------------------------------------------------------------- 



import Blender
from Blender import Mathutils
import math

dicEmptiesRestMatrix= {}
dicEmptiesInvRestMatrix= {}
dicBoneRestMatrix= {}
dicBone={}
dicEmptyChild={}
dicBoneRestInvEmpRest={}
dicEmpRestInvBoneRest={}
restFrame = 1

########################################################################
#
#          UTILITY FUNCTIONS FOR HANDLING BONES AND EMPTIES
#
########################################################################

def names(ob): return ob.getName()

#########
# Cette fonction renvoie la liste des empties attaches a root
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getTree(emp_list, root):
	empties=getAllEmpties()
	chlds = getChildren(root, empties)
	dicEmptyChild[root.getName()]=chlds
	for ch in chlds:
		emp_list.append(ch)
		getTree(emp_list,ch)

#########
# Cette fonction renvoie la liste des empties attaches a root
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getEmpties():
	global hipbonename
	emp_list = []
	root = Blender.Object.Get(hipbonename)
	emp_list.append(root)
	getTree(emp_list, root)
	return emp_list

#########
# Cette fonction renvoie la liste des empties
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getAllEmpties():
	emp_list = []
	objs = Blender.Object.Get()
	for o in objs:
		if o.getType()=="Empty":
			emp_list.append(o)
	return emp_list

#########
# Cette fonction renvoie la liste des empties
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getEmpty(name):
	p = None
	objs = Blender.Object.Get()
	for o in objs:
		if o.getType()=="Empty" and o.getName()==name:
			p = o
	return p

def getChild(emp, emp_list):
	return dicEmptyChild[emp.getName()]


#########
# Cette fonction fournit la liste des enfants d'un empty
# in  : emp (Object) un empty
#       emp_list (List of Object) la liste des empties
# out : children (List of Object) la liste des empties enfants de 'empty'
#########
def getChildren(emp, emp_list):
	children = []
	root_emp = getRootEmpties(emp_list)
	for em in emp_list:
		if (em.getName() != emp.getName()) and (em not in root_emp):
			if (em.getParent().getName() == emp.getName()):
				children.append(em)
	return children



#########
# Cette fonction renvoie la liste des empties n'ayant pas de parent
# in  : emp_list (List) une liste d'empties
# out : root (List) le (ou les) empty n'ayant pas de parent
#########
def getRootEmpties(emp_list):
	root = []
	for em in emp_list:
		if em.getParent() == None:
			root.append(em)
	return root


#########
# Cette fonction renvoie le bone de nom 'name' dans l'armature 'armature'
# in  : armature (Armature) l'armature dans laquelle cherchait le bone
#       name (String) le nom de l'os a chercher
# out : p (Bone) 
#########
#def getBone(armature, name):
#	return (dicBone[name])
	#p = None
	#bones = armature.getBones()
	#for i in bones:
	#	if i.getName() == name:
	#		p = i
	#		break
	#return p


def eraseIPO (objectname):
	object = Blender.Object.Get(objectname)
	lIpo = object.getIpo()
	if lIpo != None:
		nbCurves = lIpo.getNcurves()
 		for i in range(nbCurves):
			nbBezPoints = lIpo.getNBezPoints(i)
			for j in range(nbBezPoints):
				lIpo.delBezPoint(i)




def GetOrCreateIPO(name):

	ipos = Blender.Ipo.Get()
	if name in map(names,ipos):
		myipo = Blender.Ipo.Get(name)
		print name+' exists'
	else:
		myipo = Blender.Ipo.New('Object',name)
		print name+' was created'
	return myipo


def GetOrCreateCurve(ipo, curvename):
	curves = ipo.getCurves()
	if curvename in map(names,curves):
		mycurve = ipo.getCurve(curvename)
		print curvename+' exists'
	else:
		mycurve = ipo.addCurve(curvename)
		print curvename+' was created'
	return mycurve




########################################################################
#
# FUNCTIONS FOR COMPUTING POSITION AND ROTATION OF BONES 
#
########################################################################

def computeRootQuat2(empty, bone):

	M1=dicBoneRestInvEmpRest[bone.getName()].rotationPart()
	M2=dicEmpRestInvBoneRest[bone.getName()].rotationPart()
	emprot = empty.getMatrix('worldspace').rotationPart()
	emprot.transpose()
	mat = M1*emprot*M2
	mat.transpose()
	return (mat.toQuat())

	#emprest = dicEmptiesRestMatrix[empty.getName()].rotationPart()
	#invemprest= dicEmptiesRestMatrix[empty.getName()].rotationPart()
	##invemprest= emprest
	##invemprest.invert()
	##invemprest= dicEmptiesInvRestMatrix[empty.getName()].rotationPart()
	#emprot = empty.getMatrix('worldspace').rotationPart()
	#bonerest = dicBoneRestMatrix[bone.getName()].rotationPart()
	#invbonerest = dicBoneRestMatrix[bone.getName()].rotationPart()
	#invbonerest.invert()
	#T2=emprot*invemprest
	#T2.transpose()
	#mat = bonerest*invemprest*T2*emprest*invbonerest
	#mat.transpose()
	#return (mat.toQuat())




#########
# Cette fonction 
# in  : 
# out : 
#########
def computeRootPos(empty, bone):
	vec = computeScaledPos(empty.getMatrix('worldspace').translationPart()) - dicBoneRestMatrix[bone.getName()].translationPart()
	mat = dicBoneRestMatrix[bone.getName()].rotationPart()
	vec2 =  Mathutils.MatMultVec (mat, vec)
	return vec2


def computeRelativePos(empty,bone):
	vec = computeScaledPos(empty.getMatrix('worldspace').translationPart())	- dicBoneRestMatrix[bone.getName()].translationPart()
	rootempty = getEmpty(hipbonename)
	vec3 = computeScaledPos(rootempty.getMatrix('worldspace').translationPart())
	mat = dicBoneRestMatrix[bone.getName()].rotationPart()
	vec2 =  Mathutils.MatMultVec (mat, vec-vec3)
	return vec2


 #########
# Cette fonction 
# in  : 
# out : 
#########
def computeScaledPos(vec):
	global scalef
	vec2 = Mathutils.Vector([vec[0]*scalef, vec[1]*scalef, vec[2]*scalef])
	return vec2

########################################################################
#
#            FUNCTIONS FOR CREATING AND MOVING ARMATURES
#
########################################################################

#########
# Cette fonction 
# in  : 
# out : 
#########
def createBone (armature, empty, bone, empties):
	children = getChildren(empty, empties)
	if len(children) != 0:
	  for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		print "creating Bone %s"%(bonename)
		b=Blender.Armature.Editbone()
		b.head = (computeScaledPos(empty.getMatrix('worldspace').translationPart()))
		b.tail = (computeScaledPos(ch.getMatrix('worldspace').translationPart()))
		b.parent = bone
		# armature.makeEditable()  should already be editable????
		armature.bones[bonename] = b
		#armature.update()
##		#b.setParent(bone)
##		matrice = empty.getMatrix('worldspace')
##		invmatrice = empty.getMatrix('worldspace')
##		invmatrice.invert()
##		invmatricet=empty.getMatrix('worldspace')
##		invmatricet.invert()
##		invmatricet.transpose()
##		dicEmptiesRestMatrix[empty.getName()] = matrice
##		dicEmptiesInvRestMatrix[empty.getName()] = invmatrice
##		#armature.addBone(b)
##		#????armature.bones[b.name]=b
##		#invbonerest=b.getRestMatrix()
##		#invbonerest.invert()
##		invbonerest=Blender.Mathutils.Matrix(b.matrix)
##		invbonerest.resize4x4()
##		invbonerest[3][3]=1.0
##		invbonerest.invert()
##
##		#dicBoneRestMatrix[b.getName()] = b.matrix
##		tmpmat = b.matrix.resize4x4()
##		tmpmat[3][3] = 1.0
##		dicBoneRestMatrix[b.name] = tmpmat
##		
##		#dicBoneRestInvEmpRest[b.getName()]=b.getRestMatrix()*invmatrice*invmatricet
##		dicBoneRestInvEmpRest[b.name]=tmpmat*invmatrice*invmatricet
##		dicEmpRestInvBoneRest[b.name]=matrice*invbonerest
##		dicBone[b.name]=b
		createBone(armature, ch, b, empties)

#########
# Cette fonction 
# in  : 
# out : 
#########
def f_createBone (armData, empty, bone, empties):
	bones = armData.bones.values()
	def getBone(bonename):
		bone = None
		for b in bones:
			#print b.getName()
			if b.name == bonename:
				bone = b
		return bone

	children = getChildren(empty, empties)
	for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		#b=Blender.Armature.Bone.New(bonename)
		b=getBone(bonename)
		b.head = (computeScaledPos(empty.getMatrix('worldspace').translationPart()))
		b.tail = (computeScaledPos(ch.getMatrix('worldspace').translationPart()))
		b.parent = bone
		#b.setHead(empty.getMatrix('worldspace').translationPart())
		#b.setTail(ch.getMatrix('worldspace').translationPart())
		#b.setParent(bone)
##		matrice = empty.getMatrix('worldspace')
##		invmatrice = empty.getMatrix('worldspace')
##		invmatrice.invert()
##		invmatricet=empty.getMatrix('worldspace')
##		invmatricet.invert()
##		invmatricet.transpose()
##		dicEmptiesRestMatrix[empty.getName()] = matrice
##		dicEmptiesInvRestMatrix[empty.getName()] = invmatrice
##		#armature.addBone(b)
##		#invbonerest=b.getRestMatrix()
##		#invbonerest.invert()
##		invbonerest=Blender.Mathutils.Matrix(b.matrix)
##		invbonerest.resize4x4()
##		invbonerest[3][3]=1.0
##		invbonerest.invert()
##		#dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
##		tmpmat = b.matrix.resize4x4()
##		tmpmat[3][3] = 1.0
##		dicBoneRestMatrix[b.name] = tmpmat
##
##		dicBoneRestInvEmpRest[b.name]=tmpmat*invmatrice*invmatricet
##		dicEmpRestInvBoneRest[b.name]=matrice*invbonerest
		dicBone[b.name]=b
		#print "Ajout de ", b.getName(),"  au dictionnaire"
		f_createBone(armData, ch, b, empties)
	

#########
# Cette fonction fabrique une arma
# in  : 
# out : 
#########
def createArmature (armObj, rootEmpty, empties):
	armData=Blender.Armature.Armature('monArmature')
	children = getChildren(rootEmpty, empties)
	armObj.link(armData)
	armData.makeEditable()
	for ch in children:
		b=Blender.Armature.Editbone()
		bonename = rootEmpty.getName()[1:len(rootEmpty.getName())] + ch.getName()[1:len(ch.getName())]
		print "creating Bone %s"%(bonename)

		#print b, dir([b])
		b.head=(computeScaledPos(rootEmpty.getMatrix('worldspace').translationPart()))
		b.tail=(computeScaledPos(ch.getMatrix('worldspace').translationPart()))
		armData.bones[bonename] = b
		#armData.update()
		#armData.addBone(b)
		#matrice = ch.getMatrix('worldspace')
		#print dir (ch.matrix)
##		matrice = Blender.Mathutils.Matrix(ch.getMatrix('worldspace'))
##		matrice.resize4x4()
##		matrice[3][3]=1.0
##		#print matrice
##		#eval("invmatrice = Blender.Mathutils.Matrix(ch.getMatrix('worldspace'))" ) #??
##		invmatrice = Blender.Mathutils.Matrix(ch.getMatrix('worldspace')) #??
##		invmatrice.invert()
##		#invmatrice.resize4x4()
##		invmatricet= Blender.Mathutils.Matrix(ch.getMatrix('worldspace')) #??
##		invmatricet.invert()
##		invmatricet.transpose()
##		#invmatricet.resize4x4()
##		dicEmptiesRestMatrix[rootEmpty.getName()] = matrice
##		dicEmptiesInvRestMatrix[rootEmpty.getName()] = invmatrice
##		#invbonerest=b.getRestMatrix()
##		invbonerest=Blender.Mathutils.Matrix(b.matrix)
##		invbonerest.resize4x4()
##		invbonerest[3][3]=1.0
##		invbonerest.invert()
##		#dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
##		tmpmat = b.matrix.resize4x4()
##		tmpmat[3][3] = 1.0
##		dicBoneRestMatrix[b.name] = tmpmat
##		#print tmpmat
##		#print invmatrice
##		#print invmatricet
##		dicBoneRestInvEmpRest[b.name]=tmpmat*invmatrice*invmatricet
##		dicEmpRestInvBoneRest[b.name]=matrice*invbonerest
##		dicBone[b.name]=b
		createBone(armData, ch, b, empties)
	armData.update()
	return armData



#########
# Cette fonction fabrique une arma
# in  : 
# out : 
#########
def f_createArmature (rootEmpty, empties, armData):
	armData.makeEditable()
	bones = armData.bones.values()
	

	def getBone(bonename):
		bone = None
		for b in bones:
			#print b.getName()
			if b.name == bonename:
				bone = b
		return bone

	children = getChildren(rootEmpty, empties)
	for ch in children:
		b=getBone(rootEmpty.getName()[1:len(rootEmpty.getName())] + ch.getName()[1:len(ch.getName())])
##		matrice = Blender.Mathutils.Matrix(ch.getMatrix('worldspace'))
##		invmatrice = Blender.Mathutils.Matrix(ch.getMatrix('worldspace'))
##		invmatrice.invert()
##		invmatricet=Blender.Mathutils.Matrix(ch.getMatrix('worldspace'))
##		invmatricet.invert()
##		invmatricet.transpose()
##		dicEmptiesRestMatrix[rootEmpty.getName()] = matrice
##		dicEmptiesInvRestMatrix[rootEmpty.getName()] = invmatrice
##		print b.matrix
##		invbonerest=Blender.Mathutils.Matrix(b.matrix)
##		invbonerest.resize4x4()
##		invbonerest[3][3] = 1.0
##		invbonerest.invert()
##		#dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
##		#dicBoneRestMatrix[b.name] = b.matrix
##		tmpmat = b.matrix.resize4x4()
##		tmpmat[3][3] = 1.0
##		dicBoneRestMatrix[b.name] = tmpmat
##		
##		dicBoneRestInvEmpRest[b.name]=tmpmat*invmatrice*invmatricet
##		dicEmpRestInvBoneRest[b.name]=matrice*invbonerest
		dicBone[b.name]=b
		#print "Ajout de ", b.getName(),"  au dictionnaire"
		
		f_createBone(armData, ch, b, empties)



#########
# Cette fonction 
# in  : 
# out : 
#########
def moveBones(armature, empty, empties):
	children = dicEmptyChild[empty.getName()]
	for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		bone = dicBone[bonename]
		#bone.setLoc(computeRootPos(empty,bone))
		#???? what can replace bone.setLoc(computeRelativePos(empty,bone))
		#???? what can replace bone.setQuat(computeRootQuat2(empty,bone))
		chch = dicEmptyChild[ch.getName()]
		if len(chch) >= 1:
			moveBones(armature, ch, empties)


#########
# Cette fonction 
# in  : 
# out : 
#########
def moveArmature (armature, empties):
	root = Blender.Object.Get(hipbonename)
	children = dicEmptyChild[hipbonename]
	for ch in children:
		b=dicBone[hipbonename[1:len(hipbonename)] + ch.getName()[1:len(ch.getName())]]
		#b.setLoc(computeRootPos(root, b))
		#???? what can replace b.setLoc([0.0, 0.0, 0.0])
		#???? what can replace b.setQuat(computeRootQuat2(root, b))
		moveBones(armature, ch, empties)




########################################################################
#
#                  MAIN PROGRAM
#
########################################################################

def RemoveEmpties():

	global endframe, startframe,hipbonename

	lesEmpties = getEmpties()
	scn = Blender.Scene.getCurrent()
	#scn.link (armObj)
	for em in lesEmpties:
		eraseIPO (em.getName())
		scn.unlink(em)
	Blender.Redraw()


def Main():

	global endframe, startframe,hipbonename, framedecimation


	print "*****START*****"

	Blender.Set("curframe",restFrame)

	##-----------
	## Positionnement des empties
	##-----------
	em0 = Blender.Object.Get(hipbonename)

	Blender.Redraw()



	##-----------
	## Creation de l'armature et des os
	##-----------

	lesEmpties = getEmpties()
	#print dicEmptyChild	
	print "creating armature"
	#armData = createArmature(em0, lesEmpties)
	objects = Blender.Object.Get()
	if 'OBArmature' in map(names,objects):
		armObj = Blender.Object.Get('OBArmature')
		armData = armObj.getData()
		print 'OBArmature'+' exists'
		eraseIPO ('OBArmature')
		#print armData.getBones()
		f_createArmature(em0, lesEmpties, armData)
	else:
		armObj=Blender.Object.New('Armature', 'OBArmature')
		armData= createArmature(armObj, em0, lesEmpties)
		#armObj.link(armData)
		scn = Blender.Scene.getCurrent()
		scn.link (armObj)

		print 'OBArmature'+' was created'
	#return myobj
	armData.drawType = Blender.Armature.STICK 
	##-----------
	## Creation de l'ipo de l'armature
	##-----------
	lipo = GetOrCreateIPO('BVHIpo')
	armObj.setIpo(lipo)
	curvX =  GetOrCreateCurve(lipo, 'LocX')
	curvY =  GetOrCreateCurve(lipo, 'LocY')
	curvZ =  GetOrCreateCurve(lipo, 'LocZ')
	curvrX =  GetOrCreateCurve(lipo, 'RotX')
	curvrY =  GetOrCreateCurve(lipo, 'RotY')
	curvrZ =  GetOrCreateCurve(lipo, 'RotZ')

	print "animating armature"

	#armData.drawAxes(1)
	#armData.drawNames(1)

	

	Blender.Redraw()

	##-----------
	## Enregistrement de la position  de l'armature
	##-----------

	bones = armData.bones.values()
	#??? for bo in bones:
	#???	bo.setPose([Blender.Armature.Bone.ROT, Blender.Armature.Bone.LOC]) 

	curvX.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[0]*scalef))
	curvY.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[1]*scalef))
	curvZ.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[2]*scalef))
	curvX.setInterpolation('Linear')
	curvX.setExtrapolation('Constant')
	curvY.setInterpolation('Linear')
	curvY.setExtrapolation('Constant')
	curvZ.setInterpolation('Linear')
	curvZ.setExtrapolation('Constant')
	curvrX.setInterpolation('Linear')
	curvrX.setExtrapolation('Constant')
	curvrY.setInterpolation('Linear')
	curvrY.setExtrapolation('Constant')
	curvrZ.setInterpolation('Linear')
	curvrZ.setExtrapolation('Constant')

	Blender.Redraw()

	Blender.Set("curframe",startframe)
	while endframe >= Blender.Get("curframe"):

		##-----------
		## Positionnement des os
		##-----------

		#moveArmature(armData, lesEmpties)


		##-----------
		## Enregistrement de la position  de l'armature
		##-----------

		# ???? for bo in bones:
		# ????	bo.setPose([Blender.Armature.Bone.ROT, Blender.Armature.Bone.LOC]) 
		curvX.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[0])*scalef))
		curvY.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[1])*scalef))
		curvZ.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[2])*scalef))
		curvrX.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').rotationPart().toEuler()[0])*scalef/10))
		curvrY.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').rotationPart().toEuler()[1])*scalef/10))
		curvrZ.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').rotationPart().toEuler()[2])*scalef/10))

		##-----------
		## Passage a la frame suivante
		##-----------
		num_frame = Blender.Get("curframe")+framedecimation
		print num_frame
		Blender.Set("curframe", num_frame)

	curvX.Recalc()
	curvY.Recalc()
	curvZ.Recalc()
	curvrX.Recalc()
	curvrY.Recalc()
	curvrZ.Recalc()
	Blender.Set("curframe",startframe)
	Blender.Redraw()

	print "*****END*****"


########################################################################
#
#            GUI FUNCTIONS AND VARIABLES
#
########################################################################

EFrame = Blender.Draw.Create(5)
IFrame = Blender.Draw.Create(6)
SFrame2 = Blender.Draw.Create(5)
HBName = Blender.Draw.Create(0)
FrameDecimation = Blender.Draw.Create(5)
ScaleF =  Blender.Draw.Create(0)

Msg = ' '

def event (evt, val):
	if evt == Blender.Draw.ESCKEY:
		Blender.Draw.Exit()
	return

def button_event(evt):
	global EFrame, IFrame, SFrame2, HBName, Msg , FrameDecimation, ScaleF
	global endframe, startframe, insertionframe, hipbonename, framedecimation , scalef
	if evt==1:
		startframe = SFrame2.val
		insertionframe = 100 #IFrame.val
		endframe =  EFrame.val
		hipbonename = HBName.val
		framedecimation = FrameDecimation.val
		scalef= 1.0 #eval(str(ScaleF.val))
		#print "scalef = ", scalef
		if startframe>=endframe:
			Msg = 'Start frame must be lower than End frame'
		else:
			ob = getEmpty(hipbonename)
			if (ob!=None): 
				if ob.getParent()!=None:
					Msg = 'Empty '+hipbonename+ ' is not a root bone.'
				else:  
					if (0.0 > scalef):
						Msg = 'Scale factor must be greater than 0'
					else:
						#Blender.Draw.Exit()
						Main()
				#Main()
			else:
				Msg = 'Empty '+ hipbonename+ ' not found'
				
		#Blender.Draw.Redraw(1)
	elif evt==2:
		hipbonename = HBName.val
		ob = getEmpty(hipbonename)
		if (ob!=None): 
			if ob.getParent()!=None:
				Msg = 'Empty '+hipbonename+ ' is not a root bone.'
			else:  
				#Blender.Draw.Exit()
				RemoveEmpties()
		else:
			Msg = 'Empty '+ hipbonename+ ' not found'

	#else:
	#	print "evt = ",evt

def GUI():
	global EFrame, SFrame2, HBName, Msg , ScaleF, FrameDecimation
	Blender.BGL.glClearColor(0,0,1,1)
	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)
	Blender.BGL.glColor3f(1,1,1)
	Blender.BGL.glRasterPos2i(20,200)
	selobj = Blender.Object.GetSelected()
	if len(selobj) == 1 and type (selobj[0]) == Blender.Types.ObjectType:
		hipname = selobj[0].getName()
	else:
		hipname = '_Hips'
	Blender.Draw.Text ("BVH 2 ARMATURE v%s by %s"%(__version__, __author__), 'normal')
	HBName = Blender.Draw.String("HipBoneName: ", 0, 20, 175, 250, 20, hipname, 100)
	SFrame2 = Blender.Draw.Number("Startframe: ", 0, 20, 150, 250, 20, 1, 1,3000,"Start frame of anim")
	EFrame = Blender.Draw.Number("Endframe: ", 0, 20, 125, 250, 20, Blender.Get("endframe"), 1,3000,"Last frame of anim")
	#IFrame = Blender.Draw.Number("Insertionframe: ", 0, 20, 100, 250, 20, Blender.Get("staframe"), 1,3000,"")
	FrameDecimation = Blender.Draw.Number("FrameDecimation: ", 0, 20, 75, 250, 20,1, 1,10,'number of frame to skip between two action keys')
#	ScaleF = Blender.Draw.Number("Scale: ", 0, 20, 50, 250, 20, 1.0, 0.0, 10.0,  'Scale Factor')
	Blender.Draw.Toggle("Create Armature", 1, 20, 10, 100, 20, 0, "Create Armature")
	#Blender.Draw.Toggle("Remove Empties", 2, 200, 10, 100, 20, 0, "Remove Empties")
	Blender.BGL.glRasterPos2i(20,40)
	Blender.Draw.Text (Msg, 'normal')


Blender.Draw.Register(GUI, event, button_event)
