#!BPY
# coding: utf-8
""" Registration info for Blender menus: <- these words are ignored
Name: 'Deformed mesh to Rvk'
Blender: 248
Group: 'Mesh'
Tip: 'Copy deform data (not surf. subdiv) of active obj to rvk of the 2nd selected obj'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "blenderartists.org",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rvk1versrvk2.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "2009/05/18"

__bpydoc__ = """\
"DEFORM to RVK2" copies deformed data (except all data with not exactly 
the same number of vertices like EDGESPLIT,DECIMATE,SUBSURF, BOOLEAN, 
BUILD, MIRROR, ARRAY) of the active object to the RVK (relative vertex 
key, now called Shapes key) of the other selected object.

It is presupposed that the second mesh object is built exactly like the first
one. In fact, it is better to use a true copy with at least one basic shape
key. If there is no other object selected, the script can create a copy.

The new version of this scrit (Blender 2.43) manages the modifier changes.
There are a lot of modifiers but only the ones which just deforms the shape
can be used : LATTICE, CURVE, WAVE, ARMATURE, CAST, DISPLACE, SMOOTH. 
SIMPLEDEFORM and SHRINKWRAP are not correctly seen before Blender 2.49, but
they can be copied too.	You can unset one or more of these modifiers from 
the script.

Usage:

Select the object that will receive the rvk info, then select the deformed
object, enter Edit Mode and run this script from the "Mesh->Scripts" menu of
the 3d View.  If the active object has subsurf turned on and nonzero subdiv
level, the script will ask if it should change that.  Before copying data to
the rvk it will also ask whether it should replace or add a new vertex group.


"""
#----------------------------------------------
# jm soler (c) 2004-2009 : 'Deformed mesh to Rvk'  
# released under GPL licence
#----------------------------------------------
"""
Ce programme est libre, vous pouvez le redistribuer et/ou
le modifier selon les termes de la Licence Publique Générale GNU
publiée par la Free Software Foundation (version 2 ou bien toute
autre version ultérieure choisie par vous).

Ce programme est distribué car potentiellement utile, mais SANS
AUCUNE GARANTIE, ni explicite ni implicite, y compris les garanties
de commercialisation ou d'adaptation dans un but spécifique.
Reportez-vous à la Licence Publique Générale GNU pour plus de détails.

Vous devez avoir reçu une copie de la Licence Publique Générale GNU
en même temps que ce programme ; si ce n'est pas le cas, écrivez à la
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
MA 02111-1307, États-Unis.


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""
#----------------------------------------------
# official Page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rvk1versrvk2.htm
# download the script :
# http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communicate upon problems or errors:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------

import Blender
from Blender import NMesh,Draw,Object,Modifier

DEBUG=0

def Value(t):
	exec "t=Modifier.Types.%s"%t
	return t

def copy_shapes(RVK1,RVK2):
	POSSMOD_list=['EDGESPLIT',
							'DECIMATE',
							'SUBSURF',
							'BOOLEAN',
							'BUILD',
							'MIRROR',
							'ARRAY',
							'BEVEL',
							'EXPLODE']

	AUTHMOD_list=['LATTICE',
			  'CURVE',
			  'WAVE',
			  'ARMATURE',
			  'SMOOTH',
			  'SIMPLEDEFORM',
			  'SHRINKWRAP',
			'CAST',
							'DISPLACE',								
							'MESHDEFORM']

	
	BMOD=[['Possible Modifiers'],
			['Allowed Modifiers']]
	# =============================================================
	# must be 2 meshes ============================================
	# =============================================================
	if RVK1.getType()=='Mesh' and RVK2.getType()=='Mesh':
		#MODIFIERS=0
		FRAME=Blender.Get('curframe')
		DATA2=RVK2.getData()
		if DEBUG: print DATA2.getKey()
		# ============================================================
		# at least the second must have a shape key ==================
		# ============================================================
		def select_modifier(RVK1, RVK2, DATA2):
			# ======================================================
			# in case of modifiers, use =============================
			# ======================================================
			if RVK1.modifiers:
				#MODIFIERS=1
				POSSMOD=[Value(t) for t in POSSMOD_list]
				AUTHMOD=[Value(t) for t in AUTHMOD_list]
				if DEBUG: print 'POSSMOD:',POSSMOD,'\nAUTHMOD:', AUTHMOD
				MODRVK1=RVK1.modifiers
				block = []
				# ===================================================
				# ===  Bloc Menu Modifiers ===1 doc =================
				# ===================================================
				m=0
				for mod in  MODRVK1:
					if DEBUG: print mod.type
					if mod.type in POSSMOD:
						BMOD[0].append([Draw.Create(0),mod.type,
																m,
																POSSMOD_list[POSSMOD.index(mod.type)],
																mod[Modifier.Settings.RENDER]==1,
																mod[Modifier.Settings.EDITMODE]==1
																])
					elif mod.type in AUTHMOD:
						BMOD[1].append([Draw.Create(1),
														mod.type,
																m,
																AUTHMOD_list[AUTHMOD.index(mod.type)],
																mod[Modifier.Settings.RENDER]==1,
																mod[Modifier.Settings.EDITMODE]==1
																])
					m+=1
				# ===================================================
				# ===  Bloc Menu Modifiers ===2 display =============
				# ===================================================
				block.append(BMOD[1][0])
				for	B in BMOD[1][1:]:
					block.append((B[3],B[0],""))
				block.append(BMOD[0][0])
				block.append("not alredy implemented")
				block.append("in this script.")
				for	B in BMOD[0][1:]:
					block.append((B[3],B[0],""))
				retval = Blender.Draw.PupBlock("MESH 2 RVK", block)
				# ===================================================
				# ===  unset Modifiers  =============================
				# ===================================================
				for	B in BMOD[0][1:]:
					if DEBUG: print B[2]
					MODRVK1[B[2]][Modifier.Settings.RENDER]=0
				for	B in BMOD[1]:
					if not B[1]:
						MODRVK1[B[2]][Modifier.Settings.RENDER]=0
				# ===================================================
				# ===  update Modifiers =============================
				# ===================================================
				#RVK1.makeDisplayList()
			# =======================================================
			# ===  get deformed mesh ================================
			# =======================================================
			RVK1NAME=Object.GetSelected()[0].getName()
			meshrvk1=NMesh.GetRawFromObject(RVK1NAME)
			if DEBUG: print len(meshrvk1.verts)
			# =======================================================
			# ===  get normal mesh for vertex group =================
			# =======================================================
			DATA1=RVK1.getData()
			# =======================================================
			# ===  get destination mesh  ============================
			# =======================================================
			DATA2=RVK2.getData()
			if DEBUG: print len(meshrvk1.verts)
			if DEBUG: print len(DATA2.verts)
			# ========================================================
			# ===== is there the same number of vertices =============
			# ========================================================
			if len(meshrvk1.verts)==len(DATA2.verts):
				name = "Do you want to replace or add vertex groups ? %t| YES %x1| NO ? %x2 "
				result = Draw.PupMenu(name)
				if result==1:
					# =====================================================
					# ===== Do we save vertex groups ?  ===================
					# =====================================================
					GROUPNAME2=DATA2.getVertGroupNames()
					if len(GROUPNAME2)!=0:
						for GROUP2 in GROUPNAME2:
							DATA2.removeVertGroup(GROUP2)
					GROUPNAME1=DATA1.getVertGroupNames()
					if len(GROUPNAME1)!=0:
						for GROUP1 in GROUPNAME1:
							DATA2.addVertGroup(GROUP1)
							DATA2.assignVertsToGroup(GROUP1,DATA1.getVertsFromGroup(GROUP1),1.0,'replace')
				# ========================================================
				# ===== now copy the vertices coords =====================
				# ========================================================
				for v in meshrvk1.verts:
					i= meshrvk1.verts.index(v)
					v1=DATA2.verts[i]
					for n in [0,1,2]:
						v1.co[n]=v.co[n]
				DATA2.update()
				DATA2.insertKey(FRAME,'relative')
				DATA2.update()
				RVK2.makeDisplayList()
				
				if RVK1.modifiers:
					# ===================================================
					# ===  unset Modifiers  =============================
					# ===================================================
					for	B in BMOD[0][1:]:
						MODRVK1[B[2]][Modifier.Settings.RENDER]|=B[-2]
					for	B in BMOD[1]:
						if not B[1]:
							MODRVK1[B[2]][Modifier.Settings.RENDER]|=B[-2]
							
			else:
				name = "Meshes Objects must have the same number of vertices %t|Ok. %x1"
				result = Draw.PupMenu(name)
				return				
		if DATA2.getKey():
			select_modifier(RVK1, RVK2, DATA2)
		else:
			name = "Second Object must have  at least a shape key %t| Ok. %x1| Add one ? %x2"
			result = Draw.PupMenu(name)
			if result :
				RVK2.insertShapeKey()
				DATA2=RVK2.getData()
				select_modifier(RVK1, RVK2, DATA2)
			else:
				return
	else:
		name = "Object must be Meshes %t| Ok. %x1"
		result = Draw.PupMenu(name)
		return

def deform2rvk():
	scn = Blender.Scene.GetCurrent()
	#	=================================================================
	# at leat 2 objects ===============================================
	#	=================================================================
	if len(scn.objects.selected) >1 :
		RVK1 = Object.GetSelected()[0]			
		RVK2=Object.GetSelected()[1]
		if RVK2.getType()=='Mesh' :			
			copy_shapes(RVK1,RVK2)			
	#	=================================================================
	# ... but if only one...===========================================
	#	=================================================================			
	elif len(scn.objects.selected)==1:
		name = "At least 2 Meshes  must be selected %t| Ok. %x1| Add one ? %x2"
		result = Draw.PupMenu(name)
		RVK1 = Object.GetSelected()[0]					
		if result and  RVK1.getType()=='Mesh' :
			Blender.Object.Duplicate(mesh=1)
			RVK2=scn.objects.active
			mod = RVK2.modifiers
			for m in mod :
				RVK2.modifiers.remove(m)			
			RVK2.LocX+=2.0
			copy_shapes(RVK1,RVK2)
			scn.objects.selected=[]
			RVK2.sel=1
			RVK1.sel=1		
		#	================================================================
		# ... and not a mesh...===========================================
		#	================================================================						
		elif  result:	
			name = "Selected object must be a mesh %t| Ok. %x1"
			result = Draw.PupMenu(name)			
			return
		else :
			return
	#	================================================================
	# ... if not object at all.  =====================================
	#	================================================================								
	else:
		name = "At least one Mesh object must be selected %t| Ok. %x1"
		result = Draw.PupMenu(name)
		return
	
	Blender.Redraw()

EDITMODE=Blender.Window.EditMode()
Blender.Window.EditMode(0)
END = 0
DEFAULT= Blender.Get('curframe')

while not END:
	deform2rvk()
	name = "Do you want to copy at a new frame %t| Yes ? %x1| No ? %x2"
	result = Draw.PupMenu(name)
	if result == 1:
		msg = "Frame :"
		inputresult = Draw.PupIntInput(msg, DEFAULT , Blender.Get('staframe'), Blender.Get('endframe'))
		print inputresult 
		if inputresult != None:
			Blender.Set('curframe',inputresult)
			Blender.Window.RedrawAll()
		else:
			END = 1  
	else:
			END = 1
			
Blender.Window.EditMode(EDITMODE)
