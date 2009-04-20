import Blender

def getObjectArmature(ob):
	'''
	This returns the first armature the mesh uses.
	remember there can be more then 1 armature but most people dont do that.
	'''
	if ob.type != 'Mesh':
		return None
	
	arm = ob.parent
	if arm and arm.type == 'Armature' and ob.parentType == Blender.Object.ParentTypes.ARMATURE:
		return arm
	
	for m in ob.modifiers:
		if m.type== Blender.Modifier.Types.ARMATURE:
			arm = m[Blender.Modifier.Settings.OBJECT]
			if arm:
				return arm
	
	return None


def getDerivedObjects(ob, PARTICLES= True):
	'''
	Takes an objects and returnes a list of (ob, maxrix4x4) pairs
	that are derived from this object -
	This will include the object its self if it would be rendered.
	all dupli's for eg are not rendered themselves.
	
	currently supports
	* dupligroups
	* dupliverts
	* dupliframes
	* static particles as a mesh
	
	it is possible this function will return an empty list.
	'''
	
	ob_mtx_pairs = ob.DupObjects
	effects= ob.effects
	
	# Ignore self if were a dupli* or our parent is a duplivert.
	if ob.enableDupFrames or ob.enableDupGroup or ob.enableDupVerts:
		pass
	else:
		parent= ob.parent
		if parent and parent.enableDupVerts:
			pass
		else:
			if effects and (not effects[0].flag & Blender.Effect.Flags.EMESH):
				# Particles mesh wont render
				pass
			else:
				ob_mtx_pairs.append((ob, ob.matrixWorld))
	
	
	if PARTICLES:
		type_vec= type(Blender.Mathutils.Vector())
		type_tp= type((0,0))
		type_ls= type([])
		
		# TODO, particles per child object.
		# TODO Support materials
		me= Blender.Mesh.New()
		for eff in effects:
			par= eff.getParticlesLoc()
			
			if par:
				type_par= type(par[0])
				
				if type_par == type_vec:
					# point particles
					me.verts.extend(par)
					
				elif type_par == type_tp:
					# edge pairs
					start_index= len(me.verts)
					me.verts.extend([v for p in par for v in p])
					me.edges.extend( [(i, i+1) for i in xrange(start_index, start_index + len(par) - 1 )] )
					
				elif type_par == type_ls:
					# lines of edges
					start_index= len(me.verts)
					me.verts.extend([v for line in par for v in line])
					
					edges= []
					for line in par:
						edges.extend( [(i,i+1) for i in xrange(start_index, start_index+len(line)-1) ] )
						start_index+= len(line)
						
					me.edges.extend(edges)
		
		if me.verts:
			# If we have verts, then add the mesh
			ob_par = Blender.Object.New('Mesh')
			ob_par.link( me ) 
			
			LOOSE= Blender.Mesh.EdgeFlags.LOOSE
			for ed in me.edges:
				ed.flag |= LOOSE
			
			# Particle's are in worldspace so an identity matrix is fine.
			ob_mtx_pairs.append( (ob_par, Blender.Mathutils.Matrix()) )
	
	return ob_mtx_pairs


