#!BPY
"""
Name: 'Normalize/Scale Weight...'
Blender: 245
Group: 'WeightPaint'
Tooltip: 'Normalize the weight of the active weightgroup.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Normalize Weights

This Script is to be used only in weight paint mode,
It Normalizes the weights of the current group, to the desired peak
optionaly scaling groups that are shared by these verts so the
proportion of the veighting is unchanged.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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

from Blender import Scene, Draw, Object, Modifier
import BPyMesh
SMALL_NUM= 0.000001

def getArmatureGroups(ob, me):
	
	arm_obs = []
	
	arm = ob.parent
	if arm and arm.type == 'Armature' and ob.parentType == Object.ParentTypes.ARMATURE:
		arm_obs.append(arm)
	
	for m in ob.modifiers:
		if m.type== Modifier.Types.ARMATURE:
			arm = m[Modifier.Settings.OBJECT]
			if arm:
				arm_obs.append(arm)
	
	# convert to a dict and back, should be a set! :/ - python 2.3 dosnt like.
	return dict([ (bonename, None) for arm in arm_obs for bonename in arm.data.bones.keys() ]).keys()



def actWeightNormalize(me, ob, PREF_PEAKWEIGHT, PREF_ACTIVE_ONLY, PREF_ARMATURE_ONLY, PREF_KEEP_PROPORTION):
	
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	new_weight= max_weight= -1.0
	act_group= me.activeGroup
	
	if PREF_ACTIVE_ONLY:
		normalizeGroups = [act_group]
	else:
		normalizeGroups  = groupNames[:]
	
	if PREF_ARMATURE_ONLY:
		
		armature_groups = getArmatureGroups(ob, me)
		
		i = len(normalizeGroups)
		while i:
			i-=1
			if not normalizeGroups[i] in armature_groups:
				del normalizeGroups[i]
	
	
	for act_group in normalizeGroups:
		vWeightDictUsed=[False] * len(vWeightDict)
		
		for i, wd in enumerate(vWeightDict):
			try:
				new_weight= wd[act_group]
				if new_weight > max_weight:
					max_weight= new_weight
				vWeightDictUsed[i]=wd
			except:
				pass
				
		# These can be skipped for now, they complicate things when using multiple vgroups,
		'''
		if max_weight < SMALL_NUM or new_weight == -1:
			Draw.PupMenu('No verts to normalize. exiting.')
			#return
		
		if abs(max_weight-PREF_PEAKWEIGHT) < SMALL_NUM:
			Draw.PupMenu('Vert Weights are alredy normalized.')
			#return
		'''
		max_weight= max_weight/PREF_PEAKWEIGHT
		
		if PREF_KEEP_PROPORTION:
			# TODO, PROPORTIONAL WEIGHT SCALING.
			for wd in vWeightDictUsed:
				if wd: # not false.
					if len(wd) == 1:
						# Only 1 group for thsi vert. Simple
						wd[act_group] /= max_weight
					else:
						# More then 1 group. will need to scale all users evenly.
						if PREF_ARMATURE_ONLY:
							local_maxweight= max([v for k, v in wd.iteritems() if k in armature_groups]) / PREF_PEAKWEIGHT
							if local_maxweight > 0.0:
								# So groups that are not used in any bones are ignored.
								for weight in wd.iterkeys():
									if weight in armature_groups:
										wd[weight] /= local_maxweight
						else:
							local_maxweight= max(wd.itervalues()) / PREF_PEAKWEIGHT
							for weight in wd.iterkeys():
								wd[weight] /= local_maxweight
		
		else: # Simple, just scale the weights up. we alredy know this is in an armature group (if needed)
			for wd in vWeightDictUsed:
				if wd: # not false.
					wd[act_group] /= max_weight
		
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	

def main():
	scn= Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_PEAKWEIGHT= Draw.Create(1.0)
	PREF_ACTIVE_ONLY= Draw.Create(1)
	PREF_KEEP_PROPORTION= Draw.Create(1)
	PREF_ARMATURE_ONLY= Draw.Create(0)
	
	pup_block= [\
	('Peak Weight:', PREF_PEAKWEIGHT, 0.01, 1.0, 'Upper weight for normalizing.'),\
	('Active Only', PREF_ACTIVE_ONLY, 'Only Normalize groups that have matching bones in an armature (when an armature is used).'),\
	('Proportional', PREF_KEEP_PROPORTION, 'Scale other weights so verts (Keep weights with other groups in proportion).'),\
	('Armature Only', PREF_ARMATURE_ONLY, 'Only Normalize groups that have matching bones in an armature (when an armature is used).'),\
	]
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	actWeightNormalize(me, ob, PREF_PEAKWEIGHT.val, PREF_ACTIVE_ONLY.val, PREF_ARMATURE_ONLY.val, PREF_KEEP_PROPORTION.val)
	
if __name__=='__main__':
	main()