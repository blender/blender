#!BPY
# -*- coding: latin-1 -*-
"""
Name: 'Motion Capture  (.c3d)...'
Blender: 246
Group: 'Import'
Tooltip: 'Import a C3D Motion Capture file'
"""
__script__ = "C3D Motion Capture file import"
__author__ = " Jean-Baptiste PERIN, Roger D. Wickes (rogerwickes@yahoo.com)"
__version__ = "0.9"
__url__ = ["Communicate problems and errors, BlenderArtists.org, Python forum"]
__email__= ["rogerwickes@yahoo.com", "c3d script"]
__bpydoc__ = """\
c3d_import.py v0.8

Script loading Graphics Lab Motion Capture file,  
Usage:<br>
	- Run the script <br>
	- Choose the file to open<br>
	- Press Import C3D button<br>

Version History:
 0.4: PERIN Released under Blender Artistic Licence
 0.5: WICKES used marker names, fixed 2.45 depricated call
 0.6: WICKES creates armature for each subject
 0.7: WICKES constrains armature to follow the empties (markers). Verified for shake hands s
 0.8: WICKES resolved DEC support issue
 0.9: BARTON removed scene name change, whitespace edits. WICKES added IK layers
"""

#----------------------------------------------
# (c) Jean-Baptiste PERIN  december 2005, released under Blender Artistic Licence
#    for the Blender 2.40 Python Scripts Bundle.
#----------------------------------------------

######################################################
# This script imports a C3D file into blender. 
# Loader is based on MATLAB C3D loader from
# Alan Morris, Toronto, October 1998
# Jaap Harlaar, Amsterdam, april 2002
######################################################

import string
import Blender
from Blender import *
import bpy
import struct
import BPyMessages
Vector= Blender.Mathutils.Vector
Euler= Blender.Mathutils.Euler
Matrix= Blender.Mathutils.Matrix
RotationMatrix = Blender.Mathutils.RotationMatrix
TranslationMatrix= Blender.Mathutils.TranslationMatrix

#=================
# Global Variables, Constants, Defaults, and Shorthand References
#=================
# set senstitivity for displaying debug/console messages. 0=few, 100=max, including clicks at major steps
# debug(num,string) to conditionally display status/info in console window
DEBUG=Blender.Get('rt')

# marker sets known in the world
HUMAN_CMU= "HumanRTKm.mkr" # The Human Real-Time capture marker set used by CMU
HUMAN_CMU2="HumanRT.mkr" # found in another file, seems same as others in that series
MARKER_SETS = [ HUMAN_CMU, HUMAN_CMU2 ] # marker sets that this program supports (can make an armature for)
XYZ_LIMIT= 10000 #max value for coordinates if in integer format

# what layers to put stuff on in scene. 1 is selected, so everything goes there
# selecting only layer 2 shows only the armature moving, 12 shows only the empties
LAYERS_ARMOB= [1,2]
LAYERS_MARKER=[1,12]
LAYERS_IK=[1,11]
IK_PREFIX="ik_" # prefix in empty name: ik_prefix+subject prefix+bone name

CLEAN=True # Should program ignore markers at (0,0,0) and beyond the outer limits?

scn = Blender.Scene.GetCurrent()

BCS=Blender.Constraint.Settings # shorthand dictionary - define with brace, reference with bracket
trackto={"+x":BCS.TRACKX, "+y":BCS.TRACKY, "+z":BCS.TRACKZ, "-x":BCS.TRACKNEGX, "-y":BCS.TRACKNEGY, "-z":BCS.TRACKNEGZ}
trackup={"x":BCS.UPX, "y":BCS.UPY, "z":BCS.UPZ}

#=============================#
# Classes 
#=============================#
class Marker:
	def __init__(self, x, y, z):
		self.x=0.0
		self.y=0.0
		self.z=0.0

	def __repr__(self): #report on self, as in if just printed
		return str("[x = "+str(self.x) +" y = " + str(self.y)+" z = "+ str(self.z)+"]")

class ParameterGroup:
	def __init__(self, nom, description, parameter):
		self.name = nom
		self.description = description
		self.parameter = parameter

	def __repr__(self):
		return self.name, " ", self.description, " ", self.parameter

class Parameter:
	def __init__(self, name, datatype, dim, data, description):
		self.name = name
		self.datatype = datatype
		self.dim = dim
		self.data = data
		self.description = description
 
		def __repr__(self):
			return self.name, " ", self.description, " ", self.dim

class MyVector:
	def __init__(self, fx,fy,fz):
		self.x=fx
		self.y=fy
		self.z=fz

class Mybone:
	"information structure for bone generation and posing"
	def __init__(self, name,vec,par,head,tail,const):
		self.name=name      # name of this bone. must be unique within armature
		self.vec=vec         # edit bone vector it points
		self.parent=par     # name of parent bone to locate head and form a chain
		self.headMark=head  # list of 0+ markers where the head of this non-parented bone should be placed
		self.tailMark=tail  # list of 0+ markers where the tip should be placed
		self.const=const    # list of 0+ constraint tuples to control posing
		self.head=MyVector(0,0,0) #T-pose location
		self.tail=MyVector(0,0,0)
	def __repr__(self):
		return '[Mybone "%s"]' % self.name


#=============================#
# functions/modules 
#=============================#
def error(str):
	Draw.PupMenu('ERROR%t|'+str)
	return
def status(str):
	Draw.PupMenu('STATUS%t|'+str+"|Continue?")
	return
def debug(num,msg): #use log4j or just console here.
	if DEBUG >= num:
					print 'debug:', (' '*num), msg
	#TODO: if level 0, make a text file in Blender file to record major stuff
	return

def names(ob): return ob.name


#########
# Cette fonction renvoie la liste des empties
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getEmpty(name):
	obs = [ob for ob in scn.objects if ob.type=="Empty" and ob.name==name]
	if len(obs)==0:
		return None
	elif len(obs)==1:
		return obs[0]
	else:
		error("FATAL ERROR: %i empties %s in file" % (len(obs),ob[0]))
#########
# Cette fonction renvoie un empty 
# in  : objname : le nom de l'empty recherche
# out : myobj : l'empty cree ou retrouve
#########
def getOrCreateEmpty(objname):
	myobj= getEmpty(objname)
	if myobj==None: 
		myobj = scn.objects.new("Empty",objname)
		debug(50,'Marker/Empty created %s' %  myobj)
	return myobj

def getOrCreateCurve(ipo, curvename):
	"""
	Retrieve or create a Blender Ipo Curve named C{curvename} in the C{ipo} Ipo

	>>> import mylib 

	>>> lIpo = GetOrCreateIPO("Une IPO")
	>>> laCurve = getOrCreateCurve(lIpo, "RotX")

	Either an ipo curve named C{curvename} exists before the call then this curve is returned,
	Or such a curve doesn't exist before the call .. then it is created into the c{ipo} Ipo and returned 

	@type  ipo: Blender Ipo
	@param ipo: the Ipo in which the curve must be retrieved or created.
	@type  curvename: string
	@param curvename: name of the IPO.
	@rtype:   Blender Curve
	@return:  a Blender Curve named C{curvename} in the C{ipo} Ipo 
	"""
	try:
		mycurve = ipo.getCurve(curvename)
		if mycurve != None:
			pass
		else:
			mycurve = ipo.addCurve(curvename)
	except:
		mycurve = ipo.addCurve(curvename)
	return mycurve

def eraseIPO (objectname):
	object = Blender.Object.Get(objectname)
	lIpo = object.getIpo()
	if lIpo != None:
		nbCurves = lIpo.getNcurves()
		for i in range(nbCurves):
			nbBezPoints = lIpo.getNBezPoints(i)
			for j in range(nbBezPoints):
				lIpo.delBezPoint(i)

def comp_loc(emptyNameList):
	myloc=Vector(0,0,0)
	for emName in emptyNameList:
		myobj = Blender.Object.Get(emName)
		for i in range(3):
			myloc[i]= myloc[i]+(myobj.loc[i]/len(emptyNameList)) #take the average loc of all marks
	return myloc

def comp_len(head, tail): # computes the length of a bone
	headvec=comp_loc(head)
	tailvec=comp_loc(tail)
	netvec=headvec-tailvec
	return netvec.length

def createHumanCMU(): # human bone structure, makes a node set for CMU MoCap Lab
	# order of bones: "spine","chest","neck","head",...face toward you in front view
	# pose constraints are tuples of (type,target,influence,other-as-needed)
	# constraint stack order is important. for proper bone pointing and orinetation:
	#  IK, then TT +YZ in world space. then LR XZ to 0 in world space, this points the bone, twists it, but then
	# limits the rotation to the sidebar enpty with the Z facing it, and Y pointing along the bone.
	nodes=[]        # bonename, vector, parent, head targets, tail targets, constraint list
	for i in range(23): nodes.append(Mybone("name","vec","par",[],[],[]))
	nodes[0]= Mybone("root", "-Y","",["RBWT", "LBWT"],["RFWT", "LFWT", "RBWT", "LBWT"],[("LOC","RBWT",1.0),("LOC","LBWT",0.5),("IK","RFWT",1.0),("IK","LFWT",0.5),("TT","RBWT",1,"+YZ"),("LR","XZ",1)])
	nodes[1]= Mybone("spine","+Z","root",[],["STRN","T10"],[("IK","STRN",1.0),("IK","T10",0.5),("TT","STRN",1,"+YZ"),("LR","XZ",1)])
	nodes[2]= Mybone("chest","+Z","spine",[],["CLAV","C7"],[("IK","CLAV",1.0),("IK","C7",0.5),("TT","CLAV",1,"+YZ"),("LR","XZ",1)])
	nodes[3]= Mybone("neck", "+Z","chest",[],["RBHD","LBHD"],[("IK","RBHD",1.0),("IK","LBHD",0.5),("TT","LBHD",1,"+YZ"),("LR","XZ",1)])
	nodes[4]= Mybone("head" ,"-Y","neck",[],["RFHD","LFHD"],[("IK","RFHD",1.0),("IK","LFHD",0.5),("TT","LFHD",1,"+YZ"),("LR","XZ",1)])
	
	nodes[5]= Mybone("shoulder.R","-X","chest",[],["RSHO"],[("IK","RSHO",1.0)])
	nodes[6]= Mybone("toparm.R",  "-X","shoulder.R",[],["RELB"],[("IK","RELB",1.0),("TT","RUPA",1,"+YZ"),("LR","XZ",1)])
	nodes[7]= Mybone("lowarm.R",  "-X","toparm.R",[],["RWRA","RWRB"],[("IK","RWRA",1.0),("IK","RWRB",0.5),("TT","RFRM",1,"+YZ"),("LR","XZ",1)])
	nodes[8]= Mybone("hand.R",    "-X","lowarm.R",[],["RFIN"],[("IK","RFIN",1.0),("TT","RWRA",1,"+YZ"),("LR","XZ",1)])  #missing ,"RTHM"

	nodes[9]= Mybone("hip.R",   "-X","root",[],["RFWT","RBWT"],[("IK","RFWT",1.0),("IK","RBWT",0.5)])
	nodes[10]=Mybone("topleg.R","-Z","hip.R",[],["RKNE"],[("IK","RKNE",1),("TT","RTHI",1,"+YZ"),("LR","XZ",1)])
	nodes[11]=Mybone("lowleg.R","-Z","topleg.R",[],["RANK","RHEE"],[("IK","RHEE",1.0),("TT","RSHN",1,"+YZ"),("LR","XZ",1)])
	nodes[12]=Mybone("foot.R",  "-Y","lowleg.R",[],["RTOE","RMT5"],[("IK","RTOE",1.0),("IK","RMT5",0.2),("TT","RMT5",1,"+YZ")])
	nodes[13]=Mybone("toes.R",  "-Y","foot.R",[],["RTOE"],[("IK","RTOE",1.0)])
	
	nodes[14]=Mybone("shoulder.L","+X","chest",[],["LSHO"],[("IK","LSHO",1.0)])
	nodes[15]=Mybone("toparm.L",  "+X","shoulder.L",[],["LELB"],[("IK","LELB",1.0),("TT","LUPA",1,"+YZ"),("LR","XZ",1)])
	nodes[16]=Mybone("lowarm.L",  "+X","toparm.L",[],["LWRA","LWRB"],[("IK","LWRA",1.0),("IK","LWRB",0.5),("TT","LFRM",1,"+YZ"),("LR","XZ",1)])
	nodes[17]=Mybone("hand.L",    "+X","lowarm.L",[],["LFIN"],[("IK","LFIN",1.0),("TT","RWRA",1,"+YZ"),("LR","XZ",1)]) #missing ,"LTHM"
	
	nodes[18]=Mybone("hip.L",   "+X","root",[],["LFWT","LBWT"],[("IK","LFWT",1.0),("IK","LBWT",0.5)])
	nodes[19]=Mybone("topleg.L","-Z","hip.L",[],["LKNE"],[("IK","LKNE",1),("TT","LTHI",1,"+YZ"),("LR","XZ",1)])
	nodes[20]=Mybone("lowleg.L","-Z","topleg.L",[],["LANK","LHEE"],[("IK","LHEE",1.0),("TT","LSHN",1,"+YZ"),("LR","XZ",1)])
	nodes[21]=Mybone("foot.L",  "-Y","lowleg.L",[],["LTOE","LMT5"],[("IK","LTOE",1.0),("IK","LMT5",0.2),("TT","LMT5",1,"+YZ"),("LR","XZ",1)])
	nodes[22]=Mybone("toes.L",  "-Y","foot.L",[],["LTOE"],[("IK","LTOE",1.0)])
	return nodes

def createNodes(marker_set): # make a list of bone name, parent, edit head loc, edit tail loc, pose constraints
	#ultimately, I want to read in an XML file here that specifies the node trees for various marker sets
	if   marker_set==HUMAN_CMU:  nodes= createHumanCMU() #load up and verify the file has the CMU marker set
	elif marker_set==HUMAN_CMU2: nodes= createHumanCMU()
	else: nodes=[]
	return nodes
def findEntry(item,list):
	for i in range(len(list)):
		if item==list[i]: break
	debug(100,"findEtnry %s is %i in list of %i items" % (item,i,len(list)))
	return i
def makeNodes(prefix, markerList, empties, marker_set): #make sure the file has the nodes selected
	nodes= createNodes(marker_set)  # list has generic marker names; replace them with the actual object names created
	#each entry in markerlist has a corresponding entry in empties in the same order
	errList=[]
	for i in range(len(nodes)):
		node= nodes[i]
		debug(60,"Adapting node %s to prefix %s" % (node,prefix))

		#replace generic head markers with actual empty names
		for im in range(len(node.headMark)):
			marker= node.headMark[im]
			mark= prefix+marker
			imn= findEntry(mark,markerList)
			if imn < len(markerList):
				debug(90,"Adapating head marker %s to %s" % (marker,empties[imn].name))
				nodes[i].headMark[im]= empties[imn].name
			else: errList.append([node.name,"head location",mark,node,2])
 
		#replace generic tail markers with actual empty names
		for im in range(len(node.tailMark)):
			marker= node.tailMark[im]
			mark= prefix+marker
			imn= findEntry(mark,markerList)
			if imn < len(markerList):
				debug(90,"Adapating  marker %s to %s" % (marker,empties[imn].name))
				nodes[i].tailMark[im]= empties[imn].name
			else: errList.append([node.name,"tail location",mark,node,2])
			
		#replace generic constraint markers (if the constraint references a marker) with empty name
		for im in range(len(node.const)):
			const=node.const[im]
			if const[0] in ("LOC","IK","TT"):
				marker=const[1]
				mark= prefix+marker
				imn= findEntry(mark,markerList)
				if imn < len(markerList):
					debug(90,"Adapating %s constraint marker %s to %s" % (const[0],marker,empties[imn].name))
					if const[0] in ("IK","LR","LOC"):
						nodes[i].const[im]=(const[0], empties[imn].name, const[2])
					else: nodes[i].const[im]=(const[0], empties[imn].name, const[2], const[3])
				else: errList.append([node.name,const[0]+" constraint",mark,node,4])
			
	if errList!=[]: #we have issues. 
		for err in errList:
			debug(0,"Bone "+err[0]+" specifies "+err[2]+" as "+err[1]+"which was not specified in file.")
			#need a popup here to ignore/cleanup node tree, or add the marker(?) or abort
			usrOption= 1
			if usrOption==0: #ignore this marker (remove it)
				for node in nodes: #find the bone in error
					if node.name==err[0]:
						print "Before",node
						if err[3] in range(2,3):
							node[err[3]].remove(err[2]) #find the marker in error and remove it
						elif err[3]==4: #find the constraint and remove it
							for const in node.const:
								if const[1]==err[2]: node.const.remove(const)
						print "After",node
			elif usrOption==1: #add these markers as static empties, and user will automate them later
				#and the bones will be keyed to them, so it will all be good.
				#file may have just mis-named the empty, or the location can be derived based on other markers
				em= getOrCreateEmpty(err[2])
				em.layers= LAYERS_MARKER
			else: abort() #abend
			if DEBUG==100: status("Nodes Updated")
	return nodes #nodes may be updated

def makeBones(arm,nodes): 
	debug(20,"Making %i edit bones" % len(nodes))
	for node in nodes:
		bone= Blender.Armature.Editbone()
		bone.name= node.name
		arm.bones[bone.name]= bone #add it to the armature
		debug(50,"Bone added: %s" % bone)
		if bone.name <> node.name:
			debug(0,"ERROR: duplicate node % name specified" % node.name)
			node.name= bone.name #you may not get what you asked for
		if node.parent!="": #parent
			debug(60,"Bone parent: %s"%node.parent)
			bone.parent= arm.bones[node.parent]
			bone.options = [Armature.CONNECTED]
		#compute head = average of the reference empties
		if node.headMark==[]: # no head explicitly stated, must be tail of parent
			for parnode in nodes:
				if node.parent==parnode.name: break
			node.headMark= parnode.tailMark
			node.head= parnode.tail
		else: node.head= comp_loc(node.headMark) #node head is specified, probably only for root.

		bone.head= node.head
		debug(60,"%s bone head: (%0.2f, %0.2f, %0.2f)" % (bone.name,bone.head.x, bone.head.y, bone.head.z))
		mylen=comp_len(node.headMark,node.tailMark) # length of the bone as it was recorded for that person
		# for our T position, compute the bone length, add it to the head vector component to get the tail
		if node.vec[0]=="-": mylen=-mylen
		debug(80,"Bone vector %s length %0.2f" %(node.vec,mylen))
		node.tail= Vector(node.head)
		myvec=node.vec[1].lower()
		if   myvec=="x": node.tail.x+=mylen
		elif myvec=="y": node.tail.y+=mylen
		elif myvec=="z": node.tail.z+=mylen
		else:
			debug(0,"%s %s %s %s" % (node.vec,myvec,node.vec[0],node.vec[1]))
			error("ERROR IN BONE SPEC ")
		bone.tail= node.tail
		debug(60,"Bone tail: (%i,%i,%i)" %(bone.tail.x, bone.tail.y, bone.tail.z))
	#Armature created in the T postion, but with bone lengths to match the marker set and subject
		#when this is constrained to the markers, the recorded action will be relative to a know Rotation
		#so that all recorded actions should be interchangeable. wooot!
		#Only have to adjust starting object loc when matching up actions.
	return #arm #updated

def makeConstLoc(pbone,const):
	const_new= pbone.constraints.append(Constraint.Type.COPYLOC)
	const_new.name = const[0]+"-"+const[1]
	const_target=Blender.Object.Get(const[1])
	const_new[BCS.TARGET]= const_target
	const_new.influence = const[2]
	return
				
def makeConstLimRot(pbone,const):
	const_new= pbone.constraints.append(Constraint.Type.LIMITROT)
	const_new.name = const[0]+"-"+const[1]
	for axis in const[1]:
		if axis.lower()=="x": const_new[BCS.LIMIT] |= BCS.LIMIT_XROT #set
		if axis.lower()=="y": const_new[BCS.LIMIT] |= BCS.LIMIT_YROT #set
		if axis.lower()=="z": const_new[BCS.LIMIT] |= BCS.LIMIT_ZROT #set
	const_new[BCS.OWNERSPACE]= BCS.SPACE_LOCAL
	const_new.influence = const[2]
	# fyi, const[Constraint.Settings.LIMIT] &= ~Constraint.Settings.LIMIT_XROT #reset
	return
				
def makeConstIK(prefix,pbone,const):
	#Blender 246 only supports one IK Solver per bone, but we might want many,
	#  so we need to create a reference empty named after the bone
	#  that floats between the markers, so the bone can point to it as a singularity
	myob= getOrCreateEmpty(IK_PREFIX+prefix+pbone.name)
	myob.layers= LAYERS_IK
	# note that this empty gets all the IK constraints added on as location constraints
	myconst= myob.constraints.append(Constraint.Type.COPYLOC)
	myconst.name=const[0]+"-"+const[1]
	myconst[Constraint.Settings.TARGET]= Blender.Object.Get(const[1])
	myconst.influence = const[2]
	
	#point the bone once to the empty via IK
	success=False
	for myconst in pbone.constraints:
		if myconst.type == Constraint.Type.IKSOLVER: success=True
	if not(success): #add an IK constraint to the bone to point to the empty
		#print pbone
		myconst= pbone.constraints.append(Constraint.Type.IKSOLVER)
		myconst.name = const[1]
		myconst[BCS.TARGET]= myob
		myconst.influence = const[2]
		#const_new[Constraint.Settings.BONE]= ?
		myconst[BCS.CHAINLEN]= 1
		myconst[BCS.USETIP]= True
		myconst[BCS.STRETCH]= False
		return
					
def makeConstTT(pbone,const):
	myconst= pbone.constraints.append(Constraint.Type.TRACKTO)
	myconst.name=const[0]+"-"+const[1]
	debug(70,"%s %s" % (myconst,const[3]))
	myob= getEmpty(const[1])
	if myob!= None:
		myconst[BCS.TARGET]= myob
		myconst.influence = const[2]
		#const[3] is the Track and the thrird char is the Up indicator
		myconst[BCS.TRACK]= trackto[const[3][0:2].lower()]
		myconst[BCS.UP]=trackup[const[3][2].lower()]#up direction
		myconst[BCS.OWNERSPACE]= BCS.SPACE_LOCAL
		myconst[BCS.TARGETSPACE]= [BCS.SPACE_LOCAL]
		if const[3][1]==const[3][2]: debug(0,"WARNING: Track To axis and up axis should not be the same. Constraint is INACTIVE")
	else:   #marker not found. could be missing from this file, or an error in node spec
		error("TrackTo Constraint for %s |specifies unknown marker %s" % (pbone.name,const[1]))
	return

def makePoses(prefix,arm_ob,nodes): # pose this armature object based on node requirements
	#this is constraint-based posing, not hard-keyed posing.
	#we do constraint-based first so that user can adjust the constraints, possibly smooth/tweak motion
	#  add additional bones or referneces/constraints, before baking to hard keyframes

	pose= arm_ob.getPose()
	debug(0,"Posing %s %s" % (arm_ob, pose))
	for node in nodes:
		debug(30, "examining %s" %node)
		if len(node.const)>0: #constraints for this bone are desired
			pbone = pose.bones[node.name]
			debug(40,"Posing bone %s" %pbone)
			for const in node.const:
				debug(50,"Constraining %s by %s" %(pbone,const))
				if   const[0]=="LOC":makeConstLoc(pbone,const)
				elif const[0]=="IK": makeConstIK(prefix,pbone,const)
				elif const[0]=="LR": makeConstLimRot(pbone,const)  
				elif const[0]=="TT": makeConstTT(pbone,const)
				else: 
					error("FATAL: constraint %s not supported" %const[0])
					break
	debug(10, "Posing complete. Cycling pose and edit mode")
	pose.update()
	return

def make_arm(subject,prefix,markerList, emptyList,marker_set):
	debug(10,"**************************")
	debug(00, "**** Making Armature for %s..." % subject)
	debug(10, "**************************")
	# copied from bvh import bvh_node_dict2armature; trying to use similar process for further integtration down the road
	# Add the new armature,
	
	nodes= makeNodes(prefix, markerList, emptyList, marker_set) #assume everyone in file uses the same mocap suit
	# each person in the file may be different height, so each needs their own new armature to match marker location

##  obs= Blender.Object.Get()
##  success=False
##  for ob in obs:
##    if ob.name==subject:
##      success=True
##  if success:
##    menu="Human Armature already exists for this subject."
##    menu+="%t|Create another in this scene"
##    menu+="%l|Start a new scene"
##    menu+="%l|Use this armature"
##    menusel= Draw.PupMenu(menu)
	
	arm= Blender.Armature.New(subject) #make an armature.
	debug(10,"Created Armature %s" % arm)
	# Put us into editmode
	arm.makeEditable()
	arm.drawType = Armature.OCTAHEDRON
	makeBones(arm,nodes)
	scn = Blender.Scene.GetCurrent() #add it to the current scene. could create new scenes here as yaf
	arm_ob= scn.objects.new(arm) #instance it in the scene. this is the new way for 2.46 to instance objects
	arm_ob.name= subject #name it something like the person it represents
	arm_ob.layers= LAYERS_ARMOB
	debug(20,"Instanced Armature %s" % arm_ob)
	arm.update() #exit editmode. Arm must be instanced as an object before you can save changes or pose it
	Blender.Redraw() # show the world
	if DEBUG==100: status("T-Bones made.")

	makePoses(prefix,arm_ob,nodes) #constrain arm_ob with these markers
	
	scn.update(1) #make everyone behave themselves in the scene, and respect the new constraints
	return arm_ob

def setupAnim(StartFrame, EndFrame, VideoFrameRate):
	debug(100, 'VideoFrameRate is %i' %VideoFrameRate)
	if VideoFrameRate<1: VideoFrameRate=1
	if VideoFrameRate>120: VideoFrameRate=120
	# set up anim panel for them
	context=scn.getRenderingContext() 
	context.sFrame=StartFrame
	context.eFrame=EndFrame
	context.fps=int(VideoFrameRate)
	
	Blender.Set("curframe",StartFrame)
	Blender.Redraw()
	return

def makeCloud(Nmarkers,markerList,StartFrame,EndFrame,Markers):
	debug(10, "**************************")
	debug(00, "*** Making Cloud Formation")
	debug(10, "**************************")
	empties=[]
	ipos=[]
	curvesX=[]
	curvesY=[]
	curvesZ=[]
	debug(0, "%i Markers (empty cloud) will be put on layers %s" % (Nmarkers,LAYERS_MARKER))
	# Empty Cloud formation
	for i in range(Nmarkers):
		debug(100,"%i marker %s"%(i, markerList[i]))
		emptyname = markerList[i] # rdw: to use meaningful names from Points parameter
		em= getOrCreateEmpty(emptyname) #in this scene
		em.layers= LAYERS_MARKER
		#make a list of the actual empty
		empties.append(em)
		#assign it an ipo with the loc xyz curves
		lipo = Ipo.New("Object",em.name)
		ipos.append(lipo)
		curvesX.append(getOrCreateCurve(ipos[i],'LocX'))
		curvesY.append(getOrCreateCurve(ipos[i],'LocY'))
		curvesZ.append(getOrCreateCurve(ipos[i],'LocZ'))
		empties[i].setIpo(ipos[i])
	debug(30,"Cloud of %i empties created." % len(empties))
	NvideoFrames= EndFrame-StartFrame+1
	debug(10, "**************************")
	debug(00, "**** Calculating Marker Ipo Curves over %i Frames ..." % NvideoFrames)
	debug(10, "**************************")
	err= index=0 #number of errors, logical frame
	for frame in range(StartFrame,EndFrame+1):
		if   index==0:   start=sys.time()
		elif index==100:
			tmp=(NvideoFrames-100)*(sys.time()-start)/6000
			debug(0,"%i minutes process time estimated" % tmp)
		elif index >100: print index*100/(NvideoFrames-1),"% complete\r",
		for i in range(Nmarkers):
			if Markers[index][i].z < 0: Markers[index][i].z= -Markers[index][i].z
			success=True
			if CLEAN:   #check for good data
				# C3D marker decoding may have coordinates negative (improper sign bit decoding?)
				myX= abs(Markers[index][i].x)
				myY= abs(Markers[index][i].y)
				myZ= Markers[index][i].z
				if myX > 10000 or myY > 10000 or myZ > 10000: success=False
				if myX <.01 and myY <.01 and myZ <.01: success=False # discontinuity in marker tracking (lost marker)
			
			if success:
				curvesX[i].append((frame, Markers[index][i].x)) #2.46 knot method
				curvesY[i].append((frame, Markers[index][i].y))
				curvesZ[i].append((frame, Markers[index][i].z))
				if frame==StartFrame: debug(40, "%s loc frame %i: (%0.2f, %0.2f, %0.2f)" % (markerList[i],frame,Markers[index][i].x,Markers[index][i].y,Markers[index][i].z))
			else:
				err+=1 # some files have thousands...
				#debug(30,"Point ignored for marker:%s frame %i: (%i, %i, %i)" %       (markerList[i],frame,Markers[index][i].x,Markers[index][i].y,Markers[index][i].z))
		index += 1
	debug(70, "%i points ignored across all markers and frames. Recalculating..." % err)

	for i in range(Nmarkers):
		curvesX[i].Recalc()
		curvesY[i].Recalc()
		curvesZ[i].Recalc()
	Blender.Set('curframe', StartFrame)
	Blender.Redraw()
	if DEBUG==100: status("Clound formed")
	return empties

def getNumber(str, length):
		if length==2: # unsigned short
			return struct.unpack('H',str[0:2])[0], str[2:]
		sum = 0
		for i in range(length):
				#sum = (sum << 8) + ord(str[i]) for big endian
				sum = sum + ord(str[i])*(2**(8*i))
		return sum, str[length:]
def unpackFloat(chunk,proctype):
		#print proctype
		myvar=chunk[0:4]
		if   proctype==2: #DEC-VAX
			myvar=chunk[2:4]+chunk[0:2] #swap lo=hi word order pair
		return struct.unpack('f',myvar[0:4])[0]
		
def getFloat(chunk,proctype):
		return unpackFloat(chunk, proctype), chunk[4:]
def parseFloat(chunk,ptr,proctype):
	return unpackFloat(chunk[ptr:ptr+4], proctype), ptr+4

 
def load_c3d(FullFileName):
# Input:        FullFileName - file (including path) to be read
#
# Variable:
# Markers            3D-marker data [Nmarkers x NvideoFrames x Ndim(=3)]
# VideoFrameRate     Frames/sec
# AnalogSignals      Analog signals [Nsignals x NanalogSamples ]
# AnalogFrameRate    Samples/sec
# Event              Event(Nevents).time ..value  ..name
# ParameterGroup     ParameterGroup(Ngroups).Parameters(Nparameters).data ..etc.
# CameraInfo         MarkerRelated CameraInfo [Nmarkers x NvideoFrames]
# ResidualError      MarkerRelated ErrorInfo  [Nmarkers x NvideoFrames]

	Markers=[];
	VideoFrameRate=120;
	AnalogSignals=[];
	AnalogFrameRate=0;
	Event=[];
	ParameterGroups=[];
	CameraInfo=[];
	ResidualError=[];

	debug(10, "*********************")
	debug(10, "**** Opening File ***")
	debug(10, "*********************")

	#ind=findstr(FullFileName,'\');
	#if ind>0, FileName=FullFileName(ind(length(ind))+1:length(FullFileName)); else FileName=FullFileName; end
	debug(0, "FileName = " + FullFileName)
	fid=open(FullFileName,'rb'); # native format (PC-intel). ideasman says maybe rU
	content = fid.read();
	content_memory = content
	#Header  section
	NrecordFirstParameterblock, content = getNumber(content,1)     # Reading record number of parameter section

	key, content = getNumber(content,1)
	if key!=80:
		error('File: does not comply to the C3D format')
		fid.close()
		return
	#Paramter section
	content = content[512*(NrecordFirstParameterblock-1)+1:] # first word ignored
	#file format spec says that  3rd byte=NumberofParmaterRecords... but is ignored here.
	proctype,content =getNumber(content,1)
	proctype = proctype-83
	proctypes= ["unknown","(INTEL-PC)","(DEC-VAX)","(MIPS-SUN/SGI)"]
	
	if proctype in (1,2): debug(0, "Processor coding %s"%proctypes[proctype])
	elif proctype==3: debug(0,"Program untested with %s"%proctypes[proctype])
	else:
		debug(0, "INVALID processor type %i"%proctype)
		proctype=1
		debug(0,"OVERRIDE processor type %i"%proctype)

	#if proctype==2,
	#    fclose(fid);
	#    fid=fopen(FullFileName,'r','d'); % DEC VAX D floating point and VAX ordering
	#end
	debug(10, "***********************")
	debug(00, "**** Reading Header ***")
	debug(10, "***********************")

	# ###############################################
	# ##                                           ##
	# ##    read header                            ##
	# ##                                           ##
	# ###############################################

	#%NrecordFirstParameterblock=fread(fid,1,'int8');     % Reading record number of parameter section
	#%key1=fread(fid,1,'int8');                           % key = 80;

	content = content_memory
 #fseek(fid,2,'bof');
	content = content[2:]

	#
	Nmarkers, content=getNumber(content, 2)
	NanalogSamplesPerVideoFrame, content = getNumber(content, 2)
	StartFrame,  content = getNumber(content, 2)
	EndFrame,  content = getNumber(content, 2)
	MaxInterpolationGap,  content = getNumber(content, 2)

	Scale, content = getFloat(content,proctype)
	
	NrecordDataBlock,  content = getNumber(content, 2)
	NanalogFramesPerVideoFrame,  content = getNumber(content, 2)

	if NanalogFramesPerVideoFrame > 0:
		NanalogChannels=NanalogSamplesPerVideoFrame/NanalogFramesPerVideoFrame
	else:
		NanalogChannels=0

	VideoFrameRate, content = getFloat(content,proctype)

	AnalogFrameRate=VideoFrameRate*NanalogFramesPerVideoFrame
	NvideoFrames = EndFrame - StartFrame + 1

	debug(0, "Scale= %0.2f" %Scale)
	debug(0, "NanalogFramesPerVideoFrame= %i" %NanalogFramesPerVideoFrame)
	debug(0, "Video Frame Rate= %i" %VideoFrameRate)
	debug(0, "AnalogFrame Rate= %i"%AnalogFrameRate)
	debug(0, "# markers= %i" %Nmarkers)
	debug(0, "StartFrame= %i" %StartFrame)
	debug(0, "EndFrame= %i" %EndFrame)
	debug(0, "# Video Frames= %i" %NvideoFrames)
	
	if Scale>0:
		debug(0, "Marker data is in integer format")
		if Scale>(XYZ_LIMIT/32767):
			Scale=XYZ_LIMIT/32767.0
			debug(0, "OVERRIDE: Max coordinate is %i, Scale changed to %0.2f" % (XYZ_LIMIT,Scale))
	else: debug(0, "Marker data is in floating point format")
	if VideoFrameRate<1 or VideoFrameRate>120:
		VideoFrameRate= 120
		debug(0, "OVERRIDE Video Frame Rate= %i" %VideoFrameRate)
	if proctype not in (1,2): # Intel, DEC are known good
		debug(0, "OVERRIDE|Program not tested with this encoding. Set to Intel")
		proctype= 1

	debug(10, "***********************")
	debug(10, "**** Reading Events ...")
	debug(10, "***********************")

	content = content_memory
	content = content[298:] #bizarre .. ce devrait être 150 selon la doc rdw skips first 299 bytes?

	EventIndicator,  content = getNumber(content, 2)
	EventTime=[]
	EventValue=[]
	EventName=[]

	debug(0, "Event Indicator = %i" %EventIndicator)
	if EventIndicator==12345: #rdw: somehow, this original code seems fishy, but I cannot deny it.
		Nevents,  content = getNumber(content, 2)
		debug(0, "Nevents= %i" %Nevents)
		content = content[2:]
		if Nevents>0:
			for i in range(Nevents):
				letime, content = getFloat(content,proctype)
				EventTime.append(letime)
			content = content_memory
			content = content[188*2:]
			for i in range(Nevents):
				lavalue, content = getNumber(content, 1)
				EventValue.append(lavalue)
			content = content_memory
			content = content[198*2:]
			for i in range(Nevents):
				lenom = content[0:4]
				content = content[4:]
				EventName.append(lenom)

	debug(00, "***************************")
	debug(00, "**** Reading Parameters ...")
	debug(10, "***************************")
	subjects=[]  # a name would be nice, but human will do
	prefixes=[] # added on to mocap marker names, one for each subject
	marker_subjects = [] # hopefully will be specified in the file and known to this program
	markerList=[]
	ParameterGroups = []
	ParameterNumberIndex = []
	
	content = content_memory
	content = content[512*(NrecordFirstParameterblock-1):] 

	dat1, content = getNumber(content, 1)
	key2, content = getNumber(content, 1)

	NparameterRecords, content = getNumber(content, 1)
	debug(100, "NparameterRecords=%i"%NparameterRecords)
	proctype,content =getNumber(content,1)
	proctype = proctype-83                 # proctype: 1(INTEL-PC); 2(DEC-VAX); 3(MIPS-SUN/SGI)

	for i in range(NparameterRecords):
		leparam = ParameterGroup(None, None, [])
		ParameterGroups.append(leparam)
		ParameterNumberIndex.append(0)
	#
	Ncharacters, content = getNumber(content, 1)
	if Ncharacters>=128:
		Ncharacters = -(2**8)+(Ncharacters)
	GroupNumber, content = getNumber(content, 1)
	if GroupNumber>=128:
		GroupNumber = -(2**8)+(GroupNumber)
	debug(80,"GroupNumber = %i, Nchar=%i" %(GroupNumber,Ncharacters))

	while Ncharacters > 0:
		if GroupNumber<0:
			GroupNumber=abs(GroupNumber)
			GroupName = content[0:Ncharacters]
			content = content[Ncharacters:]
			#print "Group Number = ", GroupNumber
			ParameterGroups[GroupNumber].name = GroupName
			#print "ParameterGroupName =", GroupName
			offset, content = getNumber(content, 2)
			deschars, content = getNumber(content, 1)
			GroupDescription = content[0:deschars]
			content = content[deschars:]
			ParameterGroups[GroupNumber].description = GroupDescription
			#
			ParameterNumberIndex[GroupNumber]=0
			content = content[offset-3-deschars:]
		else:
			
			ParameterNumberIndex[GroupNumber]=ParameterNumberIndex[GroupNumber]+1
			ParameterNumber=ParameterNumberIndex[GroupNumber]
			#print "ParameterNumber=", ParameterNumber
			ParameterGroups[GroupNumber].parameter.append(Parameter(None, None, [], [], None))
			ParameterName = content[0:Ncharacters]
			content = content[Ncharacters:]
			#print "ParameterName = ",ParameterName 
			if len(ParameterName)>0:
				ParameterGroups[GroupNumber].parameter[ParameterNumber-1].name=ParameterName
			offset, content = getNumber(content, 2)
			filepos = len(content_memory)-len(content)
			nextrec = filepos+offset-2

			type, content=getNumber(content, 1)
			if type>=128:
				type = -(2**8)+type
			ParameterGroups[GroupNumber].parameter[ParameterNumber-1].type=type

			dimnum, content=getNumber(content, 1)
			if dimnum == 0:
				datalength = abs(type)
			else:
				mult=1
				dimension=[]
				for j in range (dimnum):
					ladim, content = getNumber(content, 1)
					dimension.append(ladim)
					mult=mult*dimension[j]
					ParameterGroups[GroupNumber].parameter[ParameterNumber-1].dim.append(dimension[j])
				datalength = abs(type)*mult

			#print "ParameterNumber = ", ParameterNumber, " Group Number = ", GroupNumber

			if type==-1:
				data = ""
				wordlength=dimension[0]
				if dimnum==2 and datalength>0:
					for j in range(dimension[1]):
						data=string.rstrip(content[0:wordlength])
						content = content[wordlength:]
						ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data.append(data)
				elif dimnum==1 and datalength>0:
						data=content[0:wordlength]
						ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data.append(data) # ???

				myParam=string.rstrip(ParameterName)
				myGroup=string.rstrip(GroupName)
				msg= "-%s-%s-" % (myGroup,myParam)
				if myGroup == "POINT":
					if myParam== "LABELS":
						# named in form of subject:marker.
						# the list "empties" is a corresponding list of actual empty object names that make up the cloud
						markerList= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sLABELS = %i %s" %(msg, len(markerList),markerList)) #list of logical markers from 0 to n corresponding to points
					elif myParam== "LABELS2": #more labels
						# named in form of subject:marker.
						# the list "empties" is a corresponding list of actual empty object names that make up the cloud
						momarkList= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						markerList+=momarkList
						debug(0, "%sLABELS2 = %i %s" %(msg, len(momarkList),momarkList)) #list of logical markers from 0 to n corresponding to points
					else: debug(70, "%s UNUSED = %s" %(msg,ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data))
				elif myGroup in ["SUBJECT", "SUBJECTS"]: #info about the actor
					if myParam in ["NAME", "NAMES"]:
						subjects= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sNames of Subjects = %s" %(msg, subjects)) # might be useful in naming armatures
						for i in range(len(subjects)):
							subjects[i]=subjects[i].rstrip()
							if subjects[i]=="": subjects[i]="Human"
					elif myParam == "LABEL_PREFIXES":
						prefixes = ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sMarker Prefixes = %s" %(msg, prefixes)) # to xlate marker name to that in file
						for i in range(len(prefixes)):
							prefixes[i]=prefixes[i].rstrip()
					elif myParam== "MARKER_SETS":
						marker_subjects= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sMarker Set = %s"%(msg, marker_subjects)) # marker set that each subject was wearing
					elif myParam== "MODEL_PARAM":
						action= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sModel Paramter = %s"%(msg,action)) # might be a good name for the blender scene
					elif myParam== "LABELS":
						# named in form of subject:marker.
						# the list "empties" is a corresponding list of actual empty object names that make up the cloud
						markerList= ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
						debug(0, "%sLABELS = %i %s"%(msg, len(markerList),markerList)) #list of logical markers from 0 to n corresponding to points
					else: debug(70, "%sUNUSED = %s"%(msg, ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data))
				else:
					debug(70, "%sUNUSED = %s"%(msg, ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data))
			elif type == 1:
				debug(100,"Block type %i is largely unsupported and untested."%type)
				data = []
				Nparameters=datalength/abs(type)
				debug(100, "Nparameters=%i"%Nparameters)
				for i in range(Nparameters):
					ladata,content = getNumber(content, 1)
					data.append(ladata)
				ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
				#print ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data

				#print "type boolean"
			elif type == 2 and datalength>0:
				debug(100,"Block type %i is largely unsupported and untested."%type)
				data = []
				Nparameters=datalength/abs(type)
				debug(100, "Nparameters=%i"%Nparameters)
				for i in range(Nparameters):
					ladata,content = getNumber(content, 2)
					data.append(ladata)
				#ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
				if dimnum>1:
					#???? print "arg je comprends pas"
					ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
					#???ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=reshape(data,dimension)
				else:
					ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
				#print ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
				#pass
				#print "type integer"
			elif type == 4 and datalength>0:
				debug(100,"Block type %i is largely unsupported and untested."%type)
				data = []
				Nparameters=datalength/abs(type)
				debug(100, "Nparameters=%i"%Nparameters)
				for i in range(Nparameters):
					ladata,content = getFloat(content,proctype)  
					data.append(ladata)
				if dimnum>1:
					ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
					#print "arg je comprends pas"
					#???ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=reshape(data,dimension)
				else:
					ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data=data
				#print ParameterGroups[GroupNumber].parameter[ParameterNumber-1].data
			else:
				debug(100,"Block type %i is largely unsupported and untested."%type)
				#print "error"
				pass
			deschars, content= getNumber(content, 1)
			if deschars>0:
				description = content[0:deschars]
				content = content[deschars:]
				ParameterGroups[GroupNumber].parameter[ParameterNumber-1].description=description
			
			content = content_memory
			content = content[nextrec:] 

		Ncharacters,content = getNumber(content, 1)
		if Ncharacters>=128:
			Ncharacters = -(2**8)+(Ncharacters)
		GroupNumber,content = getNumber(content, 1)
		if GroupNumber>=128:
			GroupNumber = -(2**8)+(GroupNumber)
		debug(80,"GroupNumber = %i, Nchar=%i" %(GroupNumber,Ncharacters))

	debug(00, "***************************")
	debug(00, "**** Examining Parameters ...")
	debug(10, "***************************")

	if len(subjects)==0: subjects=["Test"] #well, somebody got mocapped!
	for i in range(0, len(subjects)-len(prefixes)): prefixes.append("")
	for i in range(0, len(subjects)-len(marker_subjects)): marker_subjects.append(subjects[i])

	#make a markerlist if they didn't
	debug(0, "%i Markers specified, %i marker names supplied" %(Nmarkers,len(markerList)))
	if len(markerList)==0:
		debug(0, "File missing any POINT LABELS marker list. Making defaults")
		#I guess just make cloud of empty.xxx
	if len(markerList)<Nmarkers:
		for i in range(len(markerList),Nmarkers): markerList.append("mark."+str(i))
	#note that they may supply more markers than Nmarkers, extras are usually null or ignored
	#an idea here to winnow down the marker List is to go through the nodes and see if there are markers
	# in the list that are not used in constraining the armature, and discard them or set them debug(0,
	# so that later on in processing we don't bother saving their location, possibly speeding up processing
	# because we can just skip over their data block.
	# put this on TODO list since it gets pretty complicated going throuch each marker set and all constraints etc.
	
	## ###############################################
	## ##                                           ##
	## ##    Initalize Arrays and Allocate Memory
	## ##                                           ##
	## ###############################################
	##  Get the coordinate and analog data
	#

	content = content_memory
	content = content[(NrecordDataBlock-1)*512:] 
	debug(20,"Allocating memory for %i floats" %NvideoFrames*(Nmarkers*3+2))
	for i in range (NvideoFrames):
		Markers.append([])
		ResidualError.append([])
		CameraInfo.append([])
		for j in range (Nmarkers):
			Markers[i].append(Marker(0.0,0.0,0.0))
			ResidualError[i].append(0)
			CameraInfo[i].append(0)

	#print Markers
	#
	#if Scale < 0
	#    for i=1:NvideoFrames
	#        for j=1:Nmarkers
	#            Markers(i,j,1:3)=fread(fid,3,'float32')'; 
	#            a=fix(fread(fid,1,'float32'));  
	#            highbyte=fix(a/256);
	#            lowbyte=a-highbyte*256; 
	#            CameraInfo(i,j)=highbyte; 
	#            ResidualError(i,j)=lowbyte*abs(Scale); 
	#        end
	#        waitbar(i/NvideoFrames)
	#        for j=1:NanalogFramesPerVideoFrame,
	#            AnalogSignals(j+NanalogFramesPerVideoFrame*(i-1),1:NanalogChannels)=...
	#                fread(fid,NanalogChannels,'int16')';
	#        end
	#    end

	debug(10, "***************************")
	debug(00, "**** Reading DataBlock of %i Frames...." % NvideoFrames)
	debug(10, "***************************")
	residuals= NanalogFramesPerVideoFrame*NanalogChannels*2
	err=0 #keep track of errors or serious data issues
	ptr_read = 0

	if Scale < 0.0: # 3D Data - 4-byte Floating-point Format
		for i in range (NvideoFrames):
			if i==0: start=sys.time()
			elif i==10:
				tmp=(sys.time()-start)*NvideoFrames/600
				debug(0,"%i minutes remaining..." % tmp)
			else: print "%i percent complete. On Frame %i Points procesed: %i\r" % (i*100/NvideoFrames,i,i*Nmarkers),
			for j in range (Nmarkers):
	
				x,ptr_read = parseFloat(content, ptr_read, proctype)
				y,ptr_read = parseFloat(content, ptr_read, proctype)
				z,ptr_read = parseFloat(content, ptr_read, proctype)
				myx= x * -Scale
				myy= y * -Scale
				myz= z * -Scale

				if abs(myx)>XYZ_LIMIT or abs(myy)>XYZ_LIMIT or abs(myz)>XYZ_LIMIT:
					err+=1
					if err>100:
						debug(0, "Warning: 100 data points for markers seem way out there")
						debug(0, "data read: (%i, %i, %i)" %(x,y,z))
						debug(0, "Consider revising Scale %0.2f" % Scale)
						debug(0, "which now givs coordinates: (%i, %i, %i)" %(x*Scale,y*Scale,z*Scale))
						err=-0
					if abs(myx)>XYZ_LIMIT: myx= XYZ_LIMIT*myx/abs(myx) #preserve sign
					if abs(myy)>XYZ_LIMIT: myy= XYZ_LIMIT*myy/abs(myy) #preserve sign
					if abs(myz)>XYZ_LIMIT: myz= XYZ_LIMIT*myz/abs(myz) #preserve sign
				Markers[i][j].x = myx
				Markers[i][j].y = myy
				Markers[i][j].z = myz 

				a,ptr_read = parseFloat(content, ptr_read, proctype)
				a = int(a)
				highbyte = int(a/256)
				lowbyte=a-highbyte*256
				CameraInfo[i][j] = highbyte
				ResidualError[i][j] = lowbyte*abs(Scale)
				#Monitor marker location to ensure data block is being parsed properly
				if j==0: debug(90,"Frame %i loc of %s: (%i, %i, %i)" % (i,markerList[j],myx,myy,myz))
				if i==0: debug(50, "Initial loc of %s: (%i, %i, %i)" % (markerList[j],myx,myy,myz))

			ptr_read+=residuals #skip over the following  
			#for j in range (NanalogFramesPerVideoFrame):
			#  for k in range(NanalogChannels):
			#    val, content = getNumber(content, 2)
			#    AnalogSignals[j+NanalogFramesPerVideoFrame*(i)][k]=val #??? i-1
	#else
	#    for i=1:NvideoFrames
	#        for j=1:Nmarkers
	#            Markers(i,j,1:3)=fread(fid,3,'int16')'.*Scale;
	#            ResidualError(i,j)=fread(fid,1,'int8');
	#            CameraInfo(i,j)=fread(fid,1,'int8');
	#        end
	#        waitbar(i/NvideoFrames)
	#        for j=1:NanalogFramesPerVideoFrame,
	#            AnalogSignals(j+NanalogFramesPerVideoFrame*(i-1),1:NanalogChannels)=...
	#                fread(fid,NanalogChannels,'int16')';
	#        end
	#    end
	#end

	else: #Scale is positive, but should be <1 to scale down, like 0.05
		two16= -2**16
		if len(content) < NvideoFrames*(Nmarkers*(6+2)+residuals):
			error("%i bytes is not enough data for |%i frames|%i markers|%i residual" %(len(content),NvideoFrames,Nmarkers,residuals))
		#Note: I really tried to optimize this loop, since it was taking hours to process
		for i in range(NvideoFrames):
			if i==0: start=sys.time()
			elif i==10:
				tmp=(sys.time()-start)*NvideoFrames/600
				debug(0,"%i minutes remaining..." % tmp)
			else: print "%i percent complete. On Frame %i Points procesed: %i\r" % (i*100/NvideoFrames,i,i*Nmarkers),
				
			for j in range(Nmarkers):        
				#x, content = getNumber(content,2)
				# this is old skool signed int, not but not a short.
				x = ord(content[ptr_read+0]) + (ord(content[ptr_read+1])<<8)
				if x>32768: x+=two16
				y = ord(content[ptr_read+2]) + (ord(content[ptr_read+3])<<8)
				if y>32768: y+=two16
				z = ord(content[ptr_read+4]) + (ord(content[ptr_read+5])<<8)
				if z>32768: z+=two16
	
##        
##        x = ord(content[ptr_read]) + ord(content[ptr_read+1])*(2**8)
##        ptr_read+=2
##        if x > 32768:
##          x=-(2**16)+(x)
##        #y, content = getNumber(content,2)
##        y = ord(content[ptr_read]) + ord(content[ptr_read+1])*(2**8)
##        ptr_read+=2
##        if y > 32768:
##          y=-(2**16)+(y)
##        #z, content = getNumber(content,2)
##        z = ord(content[ptr_read]) + ord(content[ptr_read+1])*(2**8)
##        ptr_read+=2
##        if z > 32768:
##          z=-(2**16)+(z)
##
##        print "(%i=%i, %i=%i, %i=%i)" %(x,myx,y,myy,z,myz)

				# for integers, I changed Scale above to avoid getting impossible numbers       
				Markers[i][j].x = x*Scale
				Markers[i][j].y = y*Scale
				Markers[i][j].z = z*Scale

##        ResidualError[i][j], content = getNumber(content, 1)
##        CameraInfo[i][j], content = getNumber(content, 1)
				#try to improve performance by:
				ResidualError[i][j]= ord(content[ptr_read+6])
				CameraInfo[i][j]= ord(content[ptr_read+7])
				
				content= content[ptr_read+8:]
				ptr_read=0

				if j==0: debug(100,"Frame %i loc of %s: %s" % (i,markerList[j],Markers[i][j]))
				if i==0: debug(50, "Initial loc of %s: (%s)" % (markerList[j],Markers[i][j]))
				
			#for j in range (NanalogFramesPerVideoFrame):
			#  for k in range(NanalogChannels):
			#    val, content = getNumber(content, 2)
			#AnalogSignals(j+NanalogFramesPerVideoFrame*(i-1),1:NanalogChannels)=val
			ptr_read= residuals # skip over the above
	print "\ndone with file."
	fid.close()

	cloud= makeCloud(Nmarkers,markerList,StartFrame,EndFrame,Markers)

	setupAnim(StartFrame, EndFrame,VideoFrameRate)

	debug(10, "**************************")
	debug(00, "**** Making %i Armatures" % len(subjects))
	debug(10, "**************************")
	for i in range(len(subjects)):
		marker_set= marker_subjects[i]
		success=False
		if len(marker_set)>0:
				for trymark in MARKER_SETS: 
					if trymark[0:len(marker_set)]==marker_set:
						marker_set=trymark
						success=True
		if success:
			debug(0, "Armature for %s will be put on layers %s" % (subjects[i],LAYERS_ARMOB))
			debug(0, "  based on an markers beginning with %s" % prefixes[i])
			ob= make_arm(subjects[i],prefixes[i],markerList,cloud,marker_set)
		else:
			debug(00, "Presently, this program can automatically create a constrained armature for marker sets %s" % MARKER_SETS)
			debug(00, "%s uses an unknown marker set %s" % (subjects[i],marker_set))
			debug(10, "Have a nice day! If you figure out an armature node system for this cloud, please add it to the program.")

	debug(10, "**************************")
	debug(00, "**** Conclusion")
	minmax=[0,0,0,0,0,0]
	for i in range(NvideoFrames):
			for j in range(Nmarkers):
				if minmax[0]>Markers[i][j].x: minmax[0]=Markers[i][j].x
				if minmax[1]>Markers[i][j].y: minmax[1]=Markers[i][j].y
				if minmax[2]>Markers[i][j].z: minmax[2]=Markers[i][j].z
				if minmax[3]<Markers[i][j].x: minmax[3]=Markers[i][j].x
				if minmax[4]<Markers[i][j].y: minmax[4]=Markers[i][j].y
				if minmax[5]<Markers[i][j].z: minmax[5]=Markers[i][j].z
	debug(0,"Markers move in 3D space from (%i,%i,%i) to (%i,%i,%i). "%(minmax[0],minmax[1],minmax[2],minmax[3],minmax[4],minmax[5]))
	debug(0,"Set your 3D View Properties Clip End and zoom out your display.")
def my_callback(filename):
	# processing options UI goes here, eventually
	Window.WaitCursor(1) 
	t = sys.time() 
	load_c3d(filename)
	# Timing the script is a good way to be aware on any speed hits when scripting 
	debug(0, '%s file processed in %.2f sec.' % (filename,sys.time()-t))
	Window.WaitCursor(0)
				
def processFile():
	# select file and pass a handle to the processor
	Blender.Window.FileSelector(my_callback, "Import C3D") # makes a window a file selector and processes it
	#processing contiues while file is being worked
	
def main(): 
	# Display the GUI
	
	# Run the  function 
	processFile()

	#Close files, display stats, cleanup, advice on next steps

# This lets you import the script without running it 
if __name__ == '__main__': 
	debug(00, "------------------------------------")
	debug(00, '%s %s script began at %.0f' % (__script__,__version__,sys.time()))
	main() 
	
 

