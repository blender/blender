
import bpy

def particle_panel_enabled(psys):
	return psys.point_cache.baked==False and psys.editable==False
	
def particle_panel_poll(context):
	psys = context.particle_system
	type = psys.settings.type
	return psys != None and (type=='EMITTER' or type=='REACTOR'or type=='HAIR')

class ParticleButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "particle"

	def poll(self, context):
		psys = context.particle_system
		type = psys.settings.type
		return psys != None and (type=='EMITTER' or type=='REACTOR'or type=='HAIR')

class PARTICLE_PT_particles(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_particles"
	__label__ = "ParticleSystem"

	def poll(self, context):
		return (context.particle_system != None)
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		#row = layout.row()
		#row.itemL(text="Particle system datablock")
		#row.itemL(text="Viewport")
		#row.itemL(text="Render")
		
		type = psys.settings.type
		
		if(type!='EMITTER' and type!='REACTOR' and type!='HAIR'):
			layout.itemL(text="No settings for fluid particles")
			return
		
		row = layout.row()
		row.enabled = particle_panel_enabled(psys)
		row.itemR(part, "type")
		row.itemR(psys, "seed")
		
		row = layout.row()
		if part.type=='HAIR':
			if psys.editable==True:
				row.itemO("PARTICLE_OT_editable_set", text="Free Edit")
			else:
				row.itemO("PARTICLE_OT_editable_set", text="Make Editable")
			subrow = row.row()
			subrow.enabled = particle_panel_enabled(psys)
			subrow.itemR(part, "hair_step")
		elif part.type=='REACTOR':
			row.itemR(psys, "reactor_target_object")
			row.itemR(psys, "reactor_target_particle_system", text="Particle System")
		
class PARTICLE_PT_emission(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_emission"
	__label__ = "Emission"
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys)
		
		row = layout.row()
		#col.itemL(text="TODO: Rate instead of amount")
		row.itemR(part, "amount")
		row.itemL(text="")
		
		split = layout.split()
		
		col = split.column(align=True)
		col.itemR(part, "start")
		col.itemR(part, "end")

		col = split.column(align=True)
		col.itemR(part, "lifetime")
		col.itemR(part, "random_lifetime", slider=True)
		
class PARTICLE_PT_cache(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_cache"
	__label__ = "Cache"
	
	def poll(self, context):
		psys = context.particle_system
		type = psys.settings.type
		return psys != None and (type=='EMITTER' or type== 'REACTOR')

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		cache = psys.point_cache
		
		#if cache.baked==True:
			#layout.itemO("PARTICLE_OT_free_bake", text="BAKE")
		#else:
		row = layout.row()
			#row.itemO("PARTICLE_OT_bake", text="BAKE")
		row.itemR(cache, "start_frame")
		row.itemR(cache, "end_frame")
			
			#layout.row().itemL(text="No simulation frames in disk cache.")
		
		
class PARTICLE_PT_initial(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_initial"
	__label__ = "Initial values"

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys)
		
		layout.row().itemL(text="Location from:")
		
		box = layout.box()
		row = box.row()
		row.itemR(part, "trand")
		
		col = row.column()
		col.row().itemR(part, "emit_from", expand=True)
		
		if part.emit_from=='FACE' or part.emit_from=='VOLUME':
			row = box.row()

			if part.distribution!='GRID':
				row.itemR(part, "even_distribution")
			else:
				row.itemL(text="")
				
			row.itemR(part, "distribution", expand=True)
			
			row = box.row()

			if part.distribution=='JIT':
				row.itemR(part, "userjit", text="Particles/Face")
				row.itemR(part, "jitter_factor", text="Jittering Amount", slider=True)
			elif part.distribution=='GRID':
				row.itemR(part, "grid_resolution")

		#layout.row().itemL(text="")
				
		layout.row().itemL(text="Velocity:")
		box = layout.box()
		row = box.row()
		col = row.column()
		col.itemR(part, "normal_factor")
		if part.emit_from=='PARTICLE':
			col.itemR(part, "particle_factor")
		else:
			col.itemR(part, "object_factor", slider=True)
		col.itemR(part, "random_factor")
		
		col = row.column(align=True)
		col.itemL(text="TODO:")
		col.itemL(text="Object aligned")
		col.itemL(text="direction: X, Y, Z")
		
		row = box.row()
		col = row.column(align=True)
		col.itemR(part, "tangent_factor")
		col.itemR(part, "tangent_phase", slider=True)
		
		col = row.column(align=True)
		if part.type=='REACTOR':
			col.itemR(part, "reactor_factor")
			col.itemR(part, "reaction_shape", slider=True)
		else:
			col.itemL(text="")
		
		layout.row().itemL(text="Rotation:")
		box = layout.box()
		box.row().itemR(part, "rotation_dynamic")
		
		row = box.row()
		col = row.column(align=True)
		col.itemR(part, "rotation_mode", text="")
		col.itemR(part, "random_rotation_factor", slider=True)
		col = row.column(align=True)
		col.itemR(part, "phase_factor", slider=True)
		col.itemR(part, "random_phase_factor", text="Random", slider=True)
		
		
		layout.row().itemL(text="Angular velocity:")

		box = layout.box()
		row = box.row()
		row.itemR(part, "angular_velocity_mode", expand=True)
		row.itemR(part, "angular_velocity_factor", text="")
		
class PARTICLE_PT_physics(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_physics"
	__label__ = "Physics"

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = layout.enabled = particle_panel_enabled(psys)
		
		layout.itemR(part, "effector_group")
		
		layout.itemL(text="General:")
		box = layout.box()
		row = box.row()
		col = row.column(align=True)
		col.itemR(part, "particle_size")
		col.itemR(part, "random_size", slider=True)
		col = row.column(align=True)
		col.itemR(part, "mass")
		col.itemR(part, "sizemass", text="Multiply mass with size")
		
		layout.row().itemL(text="")
		
		row = layout.row()
		row.itemL(text="Physics Type:")
		row.itemR(part, "physics_type", expand=True)
		
		if part.physics_type != 'NO':
			box = layout.box()
			row = box.row()
		
		if part.physics_type == 'NEWTON':
			row.itemR(part, "integrator")
			row = box.row()
			col = row.column(align=True)
			col.itemL(text="Forces:")
			col.itemR(part, "brownian_factor")
			col.itemR(part, "drag_factor", slider=True)
			col.itemR(part, "damp_factor", slider=True)
			
			row.column().itemR(part, "acceleration")
		elif part.physics_type == 'KEYED':
			row.itemR(psys, "keyed_first")
			if psys.keyed_first==True:
				row.itemR(psys, "timed_keys", text="Key timing")
			else:
				row.itemR(part, "keyed_time")
			
			row = box.row()
			row.itemL(text="Next key from object:")
			row.itemR(psys, "keyed_object", text="")
			row.itemR(psys, "keyed_particle_system")
		
		if part.physics_type=='NEWTON' or part.physics_type=='BOIDS':
			row = box.row()
			row.itemR(part, "size_deflect")
			row.itemR(part, "die_on_collision")
			row.itemR(part, "sticky")

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
		col = row.column()
		col.itemR(part, "emitter");
		col.itemR(part, "parent");
		col = row.column()
		col.itemR(part, "unborn");
		col.itemR(part, "died");
		
		row = layout.row()
		row.itemR(part, "ren_as", expand=True)
		
		row = layout.row(align=True)
		
		if part.ren_as == 'LINE':
			row.itemR(part, "line_length_tail")
			row.itemR(part, "line_length_head")
			row.itemR(part, "velocity_length")
		elif part.ren_as == 'PATH':
		
			if (part.type!='HAIR' and psys.point_cache.baked==False):
				box = layout.box()
				box.itemL(text="Baked or keyed particles needed for correct rendering.")
				return
				
			row.itemR(part, "hair_bspline")
			row.itemR(part, "render_step", text="Steps")
			
			row = layout.row()
			row.itemR(part, "abs_length")
			col = row.column(align=True)
			col.itemR(part, "absolute_length")
			col.itemR(part, "random_length", slider=True)
			
			#row = layout.row()
			#row.itemR(part, "timed_path")
			#col = row.column(align=True)
			#col.active = part.timed_path == True
			#col.itemR(part, "line_length_tail", text="Start")
			#col.itemR(part, "line_length_head", text="End")
			
			row = layout.row()
			col = row.column()
			col.itemR(part, "render_strand")
			
			subrow = col.row()
			subrow.active = part.render_strand == False
			subrow.itemR(part, "render_adaptive")
			col = row.column(align=True)
			subrow = col.row()
			subrow.active = part.render_adaptive or part.render_strand == True
			subrow.itemR(part, "adaptive_angle")
			subrow = col.row()
			subrow.active = part.render_adaptive == True and part.render_strand == False
			subrow.itemR(part, "adaptive_pix")
			
			if part.type=='HAIR' and part.render_strand==True and part.child_type=='FACES':
				layout.itemR(part, "enable_simplify")
				if part.enable_simplify==True:
					box = layout.box()
					row = box.row()
					row.itemR(part, "simplify_refsize")
					row.itemR(part, "simplify_rate")
					row.itemR(part, "simplify_transition")
					row = box.row()
					row.itemR(part, "viewport")
					subrow = row.row()
					subrow.active = part.viewport==True
					subrow.itemR(part, "simplify_viewport")
			

		elif part.ren_as == 'OBJECT':
			row.itemR(part, "dupli_object")
		elif part.ren_as == 'GROUP':
			split = layout.split()
			col = split.column()
			row = col.row()
			row.itemR(part, "whole_group")
			subcol = row.column()
			subcol.active = part.whole_group == False
			subcol.itemR(part, "rand_group")
			split.column().itemR(part, "dupli_group", text="")
		elif part.ren_as == 'BILLBOARD':
			row.itemL(text="Align:")
			row.itemR(part, "billboard_lock", text="Lock")
			row = layout.row()
			row.itemR(part, "billboard_align", expand=True)
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
	__label__ = "Draw"
	
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

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.row().itemR(part, "child_type", expand=True)
		
		if part.child_type=='NONE':
			return
		
		row = layout.row()
		
		col = row.column(align=True)
		col.itemR(part, "child_nbr", text="Draw")
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
		
		row = layout.row()
		row.itemR(part, "kink_amplitude")
		row.itemR(part, "kink_frequency")
		row.itemR(part, "kink_shape", slider=True)

				
		
class PARTICLE_PT_vertexgroups(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_vertexgroups"
	__label__ = "Vertexgroups"

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

