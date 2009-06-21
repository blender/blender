
import bpy

def particle_panel_enabled(psys):
	return psys.point_cache.baked==False and psys.editable==False
	
def particle_panel_poll(context):
	psys = context.particle_system
	if psys==None:	return False
	return psys.settings.type in ('EMITTER', 'REACTOR', 'HAIR')

class ParticleButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "particle"

	def poll(self, context):
		return particle_panel_poll(context)

class PARTICLE_PT_particles(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_particles"
	__label__ = "Particle System"

	def poll(self, context):
		return (context.particle_system or context.object)

	def draw(self, context):
		layout = self.layout
		ob = context.object
		psys = context.particle_system

		split = layout.split(percentage=0.65)

		if psys:
			split.template_ID(context, psys, "settings")

		if psys:
			#row = layout.row()
			#row.itemL(text="Viewport")
			#row.itemL(text="Render")
			
			part = psys.settings
			ptype = psys.settings.type
			
			if ptype not in ('EMITTER', 'REACTOR', 'HAIR'):
				layout.itemL(text="No settings for fluid particles")
				return
				
			split = layout.split(percentage=0.65)
			
			split.enabled = particle_panel_enabled(psys)
			split.itemR(part, "type")
			split.itemR(psys, "seed")
			
			split = layout.split(percentage=0.65)
			if part.type=='HAIR':
				if psys.editable==True:
					split.itemO("PARTICLE_OT_editable_set", text="Free Edit")
				else:
					split.itemO("PARTICLE_OT_editable_set", text="Make Editable")
				row = split.row()
				row.enabled = particle_panel_enabled(psys)
				row.itemR(part, "hair_step")
			elif part.type=='REACTOR':
				split.enabled = particle_panel_enabled(psys)
				split.itemR(psys, "reactor_target_object")
				split.itemR(psys, "reactor_target_particle_system", text="Particle System")
		
class PARTICLE_PT_emission(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_emission"
	__label__ = "Emission"
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys)
		
		row = layout.row()
		row.itemR(part, "amount")
		
		split = layout.split()
		
		col = split.column(align=True)
		col.itemR(part, "start")
		col.itemR(part, "end")

		col = split.column(align=True)
		col.itemR(part, "lifetime")
		col.itemR(part, "random_lifetime", slider=True)
		
		layout.row().itemL(text="Emit From:")
		
		row = layout.row()
		row.itemR(part, "emit_from", expand=True)
		row = layout.row()
		row.itemR(part, "trand")
		if part.distribution!='GRID':
			row.itemR(part, "even_distribution")
		
		if part.emit_from=='FACE' or part.emit_from=='VOLUME':
			row = layout.row()
			row.itemR(part, "distribution", expand=True)
			
			row = layout.row()

			if part.distribution=='JIT':
				row.itemR(part, "userjit", text="Particles/Face")
				row.itemR(part, "jitter_factor", text="Jittering Amount", slider=True)
			elif part.distribution=='GRID':
				row.itemR(part, "grid_resolution")

class PARTICLE_PT_cache(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_cache"
	__label__ = "Cache"
	__default_closed__ = True
	
	def poll(self, context):
		psys = context.particle_system
		if psys==None:	return False
		return psys.settings.type in ('EMITTER', 'REACTOR')

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		cache = psys.point_cache
		
		row = layout.row()
		row.itemR(cache, "name", text="")
		if cache.outdated:
			row.itemL(text="Cache is outdated.")
		else:
			row.itemL(text="")
		
		row = layout.row()
		
		if cache.baked == True:
			row.itemO("PTCACHE_OT_free_bake_particle_system", text="Free Bake")
		else:
			row.item_booleanO("PTCACHE_OT_cache_particle_system", "bake", True, text="Bake")
			
		row = layout.row()
		row.enabled = particle_panel_enabled(psys)
		row.itemO("PTCACHE_OT_bake_from_particles_cache", text="Current Cache to Bake")
		if cache.autocache == 0:
			row.itemO("PTCACHE_OT_cache_particle_system", text="Cache to Current Frame")
	
		row = layout.row()
		row.enabled = particle_panel_enabled(psys)
		#row.itemR(cache, "autocache")
		row.itemR(cache, "disk_cache")
		row.itemL(text=cache.info)
		
		# for particles these are figured out automatically
		#row.itemR(cache, "start_frame")
		#row.itemR(cache, "end_frame")

class PARTICLE_PT_initial(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_initial"
	__label__ = "Velocity"

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys)
				
		layout.row().itemL(text="Direction:")
	
		split = layout.split()
			
		sub = split.column()
		sub.itemR(part, "normal_factor")
		if part.emit_from=='PARTICLE':
			sub.itemR(part, "particle_factor")
		else:
			sub.itemR(part, "object_factor", slider=True)
		sub.itemR(part, "random_factor")
		sub.itemR(part, "tangent_factor")
		sub.itemR(part, "tangent_phase", slider=True)
		
		sub = split.column()
		sub.itemL(text="TODO:")
		sub.itemL(text="Object aligned")
		sub.itemL(text="direction: X, Y, Z")
		
		if part.type=='REACTOR':
			sub.itemR(part, "reactor_factor")
			sub.itemR(part, "reaction_shape", slider=True)
		else:
			sub.itemL(text="")
		
		layout.row().itemL(text="Rotation:")
		split = layout.split()
			
		sub = split.column()
		
		sub.itemR(part, "rotation_mode", text="Axis")
		split = layout.split()
			
		sub = split.column()
		sub.itemR(part, "rotation_dynamic")
		sub.itemR(part, "random_rotation_factor", slider=True)
		sub = split.column()
		sub.itemR(part, "phase_factor", slider=True)
		sub.itemR(part, "random_phase_factor", text="Random", slider=True)

		layout.row().itemL(text="Angular velocity:")
		layout.row().itemR(part, "angular_velocity_mode", expand=True)
		split = layout.split()
			
		sub = split.column()
		
		sub.itemR(part, "angular_velocity_factor", text="")
		
class PARTICLE_PT_physics(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_physics"
	__label__ = "Physics"

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = layout.enabled = particle_panel_enabled(psys)

		row = layout.row()
		row.itemR(part, "physics_type", expand=True)
		if part.physics_type != 'NO':
			layout.itemR(part, "effector_group")
		
			row = layout.row()
			col = row.column(align=True)
			col.itemR(part, "particle_size")
			col.itemR(part, "random_size", slider=True)
			col = row.column(align=True)
			col.itemR(part, "mass")
			col.itemR(part, "sizemass", text="Multiply mass with size")
							
			split = layout.split()
			
			sub = split.column()
			
		if part.physics_type == 'NEWTON':
			
			sub.itemL(text="Forces:")
			sub.itemR(part, "brownian_factor")
			sub.itemR(part, "drag_factor", slider=True)
			sub.itemR(part, "damp_factor", slider=True)
			sub.itemR(part, "integrator")
			sub = split.column()
			sub.itemR(part, "acceleration")
			
		elif part.physics_type == 'KEYED':
			sub.itemR(psys, "keyed_first")
			if psys.keyed_first==True:
				sub.itemR(psys, "timed_keys", text="Key timing")
			else:
				sub.itemR(part, "keyed_time")
			sub = split.column()
			sub.itemL(text="Next key from object:")
			sub.itemR(psys, "keyed_object", text="")
			sub.itemR(psys, "keyed_particle_system")
		
		if part.physics_type=='NEWTON' or part.physics_type=='BOIDS':

			sub.itemR(part, "size_deflect")
			sub.itemR(part, "die_on_collision")
			sub.itemR(part, "sticky")

class PARTICLE_PT_render(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_render"
	__label__ = "Render"
	
	def poll(self, context):
		return (context.particle_system != None)
		
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		row = layout.row()
		row.itemR(part, "material")
		row.itemR(psys, "parent");
		
		split = layout.split()
			
		sub = split.column()
		sub.itemR(part, "emitter");
		sub.itemR(part, "parent");
		sub = split.column()
		sub.itemR(part, "unborn");
		sub.itemR(part, "died");
		
		row = layout.row()
		row.itemR(part, "ren_as", expand=True)
		
		split = layout.split()
			
		sub = split.column()
		
		if part.ren_as == 'LINE':
			sub.itemR(part, "line_length_tail")
			sub.itemR(part, "line_length_head")
			sub = split.column()
			sub.itemR(part, "velocity_length")
		elif part.ren_as == 'PATH':
		
			if (part.type!='HAIR' and psys.point_cache.baked==False):
				box = layout.box()
				box.itemL(text="Baked or keyed particles needed for correct rendering.")
				return
				
			sub.itemR(part, "render_strand")
			colsub = sub.column()
			colsub.active = part.render_strand == False
			colsub.itemR(part, "render_adaptive")
			colsub = sub.column()
			colsub.active = part.render_adaptive or part.render_strand == True
			colsub.itemR(part, "adaptive_angle")
			colsub = sub.column()
			colsub.active = part.render_adaptive == True and part.render_strand == False
			colsub.itemR(part, "adaptive_pix")
			sub.itemR(part, "hair_bspline")
			sub.itemR(part, "render_step", text="Steps")
			sub = split.column()
			sub.itemL(text="Length:")
			sub.itemR(part, "abs_length", text="Absolute")
			sub.itemR(part, "absolute_length", text="Maximum")
			sub.itemR(part, "random_length", text="Random", slider=True)
			
			#row = layout.row()
			#row.itemR(part, "timed_path")
			#col = row.column(align=True)
			#col.active = part.timed_path == True
			#col.itemR(part, "line_length_tail", text="Start")
			#col.itemR(part, "line_length_head", text="End")
			
			row = layout.row()
			col = row.column()
			
			if part.type=='HAIR' and part.render_strand==True and part.child_type=='FACES':
				layout.itemR(part, "enable_simplify")
				if part.enable_simplify==True:
					row = layout.row()
					row.itemR(part, "simplify_refsize")
					row.itemR(part, "simplify_rate")
					row.itemR(part, "simplify_transition")
					row = layout.row()
					row.itemR(part, "viewport")
					subrow = row.row()
					subrow.active = part.viewport==True
					subrow.itemR(part, "simplify_viewport")
			

		elif part.ren_as == 'OBJECT':
			#sub = split.column()
			sub.itemR(part, "dupli_object")
		elif part.ren_as == 'GROUP':
			sub.itemR(part, "dupli_group")
			split = layout.split()
			sub = split.column()
			sub.itemR(part, "whole_group")
			sub = split.column()
			colsub = sub.column()
			colsub.active = part.whole_group == False
			colsub.itemR(part, "rand_group")
			
		elif part.ren_as == 'BILLBOARD':
			sub.itemL(text="Align:")
			
			row = layout.row()
			row.itemR(part, "billboard_align", expand=True)
			row.itemR(part, "billboard_lock", text="Lock")
			row = layout.row()
			row.itemR(part, "billboard_object")
		
			row = layout.row()
			col = row.column(align=True)
			col.itemL(text="Tilt:")
			col.itemR(part, "billboard_tilt", text="Angle", slider=True)
			col.itemR(part, "billboard_random_tilt", slider=True)
			col = row.column()
			col.itemR(part, "billboard_offset")
			
			row = layout.row()
			row.itemR(psys, "billboard_normal_uv")
			row = layout.row()
			row.itemR(psys, "billboard_time_index_uv")
			
			row = layout.row()
			row.itemL(text="Split uv's:")
			row.itemR(part, "billboard_uv_split", text="Number of splits")
			row = layout.row()
			row.itemR(psys, "billboard_split_uv")
			row = layout.row()
			row.itemL(text="Animate:")
			row.itemR(part, "billboard_animation", expand=True)
			row.itemL(text="Offset:")
			row.itemR(part, "billboard_split_offset", expand=True)
		
class PARTICLE_PT_draw(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_draw"
	__label__ = "Display"
	__default_closed__ = True
	
	def poll(self, context):
		return (context.particle_system != None)
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		row = layout.row()
		row.itemR(part, "draw_as", expand=True)
		
		if part.draw_as=='NONE' or (part.ren_as=='NONE' and part.draw_as=='RENDER'):
			return
			
		path = (part.ren_as=='PATH' and part.draw_as=='RENDER') or part.draw_as=='PATH'
			
		if path and part.type!='HAIR' and psys.point_cache.baked==False:
			box = layout.box()
			box.itemL(text="Baked or keyed particles needed for correct drawing.")
			return
		
		row = layout.row()
		row.itemR(part, "display", slider=True)
		if part.draw_as!='RENDER' or part.ren_as=='HALO':
			row.itemR(part, "draw_size")
		else:
			row.itemL(text="")
		
		row = layout.row()
		col = row.column()
		col.itemR(part, "show_size")
		col.itemR(part, "velocity")
		col.itemR(part, "num")
		if part.physics_type == 'BOIDS':
			col.itemR(part, "draw_health")
		
		col = row.column()
		if (path):
			box = col.box()				
			box.itemR(part, "draw_step")
		else:
			col.itemR(part, "material_color", text="Use material color")
			subcol = col.column()
			subcol.active = part.material_color==False
			#subcol.itemL(text="color")
			#subcol.itemL(text="Override material color")

class PARTICLE_PT_children(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_children"
	__label__ = "Children"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.row().itemR(part, "child_type", expand=True)
		
		if part.child_type=='NONE':
			return
		
		row = layout.row()
		
		col = row.column(align=True)
		col.itemR(part, "child_nbr", text="Display")
		col.itemR(part, "rendered_child_nbr", text="Render")
		
		col = row.column(align=True)
		
		if part.child_type=='FACES':
			col.itemR(part, "virtual_parents", slider=True)
		else:
			col.itemR(part, "child_radius", text="Radius")
			col.itemR(part, "child_roundness", text="Roundness", slider=True)
		
			col = row.column(align=True)
			col.itemR(part, "child_size", text="Size")
			col.itemR(part, "child_random_size", text="Random")
		
		layout.row().itemL(text="Effects:")
		
		row = layout.row()
		
		col = row.column(align=True)
		col.itemR(part, "clump_factor", slider=True)
		col.itemR(part, "clumppow", slider=True)
		
		col = row.column(align=True)
		col.itemR(part, "rough_endpoint")
		col.itemR(part, "rough_end_shape")

		row = layout.row()
		
		col = row.column(align=True)
		col.itemR(part, "rough1")
		col.itemR(part, "rough1_size")

		col = row.column(align=True)
		col.itemR(part, "rough2")
		col.itemR(part, "rough2_size")
		col.itemR(part, "rough2_thres", slider=True)
		
		layout.row().itemL(text="Kink:")
		layout.row().itemR(part, "kink", expand=True)
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(part, "kink_amplitude")
		sub.itemR(part, "kink_frequency")
		sub = split.column()
		sub.itemR(part, "kink_shape", slider=True)

class PARTICLE_PT_vertexgroups(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_vertexgroups"
	__label__ = "Vertexgroups"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.itemL(text="Nothing here yet.")

		#row = layout.row()
		#row.itemL(text="Vertex Group")
		#row.itemL(text="Negate")

		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_density")
		#row.itemR(psys, "vertex_group_density_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_velocity")
		#row.itemR(psys, "vertex_group_velocity_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_length")
		#row.itemR(psys, "vertex_group_length_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_clump")
		#row.itemR(psys, "vertex_group_clump_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_kink")
		#row.itemR(psys, "vertex_group_kink_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_roughness1")
		#row.itemR(psys, "vertex_group_roughness1_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_roughness2")
		#row.itemR(psys, "vertex_group_roughness2_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_roughness_end")
		#row.itemR(psys, "vertex_group_roughness_end_negate", text="")

		#row = layout.row()
		#row.itemR(psys, "vertex_group_size")
		#row.itemR(psys, "vertex_group_size_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_tangent")
		#row.itemR(psys, "vertex_group_tangent_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_rotation")
		#row.itemR(psys, "vertex_group_rotation_negate", text="")
		
		#row = layout.row()
		#row.itemR(psys, "vertex_group_field")
		#row.itemR(psys, "vertex_group_field_negate", text="")
		
bpy.types.register(PARTICLE_PT_particles)
bpy.types.register(PARTICLE_PT_cache)
bpy.types.register(PARTICLE_PT_emission)
bpy.types.register(PARTICLE_PT_initial)
bpy.types.register(PARTICLE_PT_physics)
bpy.types.register(PARTICLE_PT_render)
bpy.types.register(PARTICLE_PT_draw)
bpy.types.register(PARTICLE_PT_children)
bpy.types.register(PARTICLE_PT_vertexgroups)
