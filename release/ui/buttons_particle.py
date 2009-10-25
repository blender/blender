
import bpy

def particle_panel_enabled(psys):
	return psys.point_cache.baked==False and psys.edited==False
	
def particle_panel_poll(context):
	psys = context.particle_system
	if psys==None:	return False
	if psys.settings==None:  return False
	return psys.settings.type in ('EMITTER', 'REACTOR', 'HAIR')
	
def point_cache_ui(self, cache, enabled, particles, smoke):
	layout = self.layout
	layout.set_context_pointer("PointCache", cache)
	
	row = layout.row()
	row.template_list(cache, "point_cache_list", cache, "active_point_cache_index", rows=2 )
	col = row.column(align=True)
	col.itemO("ptcache.add_new", icon='ICON_ZOOMIN', text="")
	col.itemO("ptcache.remove", icon='ICON_ZOOMOUT', text="")
	
	row = layout.row()
	row.itemL(text="File Name:")
	if particles:
		row.itemR(cache, "external")
	
	if cache.external:
		split = layout.split(percentage=0.80)
		split.itemR(cache, "name", text="")
		split.itemR(cache, "index", text="")
		
		layout.itemL(text="File Path:")
		layout.itemR(cache, "filepath", text="")
		
		layout.itemL(text=cache.info)
	else:
		layout.itemR(cache, "name", text="")
		
		if not particles:
			row = layout.row()
			row.enabled = enabled
			row.itemR(cache, "start_frame")
			row.itemR(cache, "end_frame")
		
		row = layout.row()
	
		if cache.baked == True:
			row.itemO("ptcache.free_bake", text="Free Bake")
		else:
			row.item_booleanO("ptcache.bake", "bake", True, text="Bake")
	
		sub = row.row()
		sub.enabled = (cache.frames_skipped or cache.outdated) and enabled
		sub.itemO("ptcache.bake", "bake", False, text="Calculate to Current Frame")
		
		row = layout.row()
		row.enabled = enabled
		row.itemO("ptcache.bake_from_cache", text="Current Cache to Bake")
		row.itemR(cache, "step");
	
		if not smoke:
			row = layout.row()
			sub = row.row()
			sub.enabled = enabled
			sub.itemR(cache, "quick_cache")
			row.itemR(cache, "disk_cache")
	
		layout.itemL(text=cache.info)
		
		layout.itemS()
		
		row = layout.row()
		row.item_booleanO("ptcache.bake_all", "bake", True, text="Bake All Dynamics")
		row.itemO("ptcache.free_bake_all", text="Free All Bakes")
		layout.itemO("ptcache.bake_all", "bake", False, text="Update All Dynamics to current frame")
	

class ParticleButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "particle"

	def poll(self, context):
		return particle_panel_poll(context)

class PARTICLE_PT_particles(ParticleButtonsPanel):
	__show_header__ = False

	def poll(self, context):
		return (context.particle_system or context.object)

	def draw(self, context):
		layout = self.layout
		ob = context.object
		psys = context.particle_system

		if ob:
			row = layout.row()

			row.template_list(ob, "particle_systems", ob, "active_particle_system_index", rows=2)

			col = row.column(align=True)
			col.itemO("object.particle_system_add", icon='ICON_ZOOMIN', text="")
			col.itemO("object.particle_system_remove", icon='ICON_ZOOMOUT', text="")

		if psys and not psys.settings:
			split = layout.split(percentage=0.32)
			col = split.column()
			col.itemL(text="Name:")
			col.itemL(text="Settings:")
			
			col = split.column()
			col.itemR(psys, "name", text="")
			col.template_ID(psys, "settings", new="particle.new")
		elif psys:
			part = psys.settings
			
			split = layout.split(percentage=0.32)
			col = split.column()
			col.itemL(text="Name:")
			if part.type in ('EMITTER', 'REACTOR', 'HAIR'):
				col.itemL(text="Settings:")
				col.itemL(text="Type:")
			
			col = split.column()
			col.itemR(psys, "name", text="")
			if part.type in ('EMITTER', 'REACTOR', 'HAIR'):
				col.template_ID(psys, "settings", new="particle.new")
			
			#row = layout.row()
			#row.itemL(text="Viewport")
			#row.itemL(text="Render")
			
			if part:
				if part.type not in ('EMITTER', 'REACTOR', 'HAIR'):
					layout.itemL(text="No settings for fluid particles")
					return
				
				row=col.row()
				row.enabled = particle_panel_enabled(psys)
				row.itemR(part, "type", text="")
				row.itemR(psys, "seed")
				
				split = layout.split(percentage=0.65)
				if part.type=='HAIR':
					if psys.edited==True:
						split.itemO("particle.edited_clear", text="Free Edit")
					else:
						split.itemL(text="")
					row = split.row()
					row.enabled = particle_panel_enabled(psys)
					row.itemR(part, "hair_step")
					if psys.edited==True:
						if psys.global_hair:
							layout.itemO("particle.connect_hair")
							layout.itemL(text="Hair is disconnected.")
						else:
							layout.itemO("particle.disconnect_hair")
							layout.itemL(text="")
				elif part.type=='REACTOR':
					split.enabled = particle_panel_enabled(psys)
					split.itemR(psys, "reactor_target_object")
					split.itemR(psys, "reactor_target_particle_system", text="Particle System")
		
class PARTICLE_PT_emission(ParticleButtonsPanel):
	__label__ = "Emission"
	
	def poll(self, context):
		if particle_panel_poll(context):
			return not context.particle_system.point_cache.external
		else:
			return False
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys) and not psys.multiple_caches
		
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

class PARTICLE_PT_hair_dynamics(ParticleButtonsPanel):
	__label__ = "Hair dynamics"
	__default_closed__ = True
	
	def poll(self, context):
		psys = context.particle_system
		if psys==None:	return False
		if psys.settings==None:  return False
		return psys.settings.type == 'HAIR'
		
	def draw_header(self, context):
		#cloth = context.cloth.collision_settings
		
		#self.layout.active = cloth_panel_enabled(context.cloth)
		#self.layout.itemR(cloth, "enable_collision", text="")
		psys = context.particle_system
		self.layout.itemR(psys, "hair_dynamics", text="")
		
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		cloth = psys.cloth.settings
		
		layout.enabled = psys.hair_dynamics
		
		split = layout.split()
			
		col = split.column()
		col.itemL(text="Quality:")
		col.itemR(cloth, "quality", text="Steps",slider=True)
		col.itemL(text="Gravity:")
		col.itemR(cloth, "gravity", text="")
		
		col = split.column()
		col.itemL(text="Material:")
		sub = col.column(align=True)
		sub.itemR(cloth, "pin_stiffness", text="Stiffness")
		sub.itemR(cloth, "mass")
		col.itemL(text="Damping:")
		sub = col.column(align=True)
		sub.itemR(cloth, "spring_damping", text="Spring")
		sub.itemR(cloth, "air_damping", text="Air")
		
		layout.itemR(cloth, "internal_friction", slider="True")
				
class PARTICLE_PT_cache(ParticleButtonsPanel):
	__label__ = "Cache"
	__default_closed__ = True
	
	def poll(self, context):
		psys = context.particle_system
		if psys==None:	return False
		if psys.settings==None:  return False
		phystype = psys.settings.physics_type
		if phystype == 'NO' or phystype == 'KEYED':
			return False
		return psys.settings.type in ('EMITTER', 'REACTOR') or (psys.settings.type == 'HAIR' and psys.hair_dynamics)

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		
		point_cache_ui(self, psys.point_cache, particle_panel_enabled(psys), not psys.hair_dynamics, 0)

class PARTICLE_PT_initial(ParticleButtonsPanel):
	__label__ = "Velocity"
	
	def poll(self, context):
		if particle_panel_poll(context):
			psys = context.particle_system
			return psys.settings.physics_type != 'BOIDS' and not psys.point_cache.external
		else:
			return False

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
	__label__ = "Physics"
	
	def poll(self, context):
		if particle_panel_poll(context):
			return not context.particle_system.point_cache.external
		else:
			return False

	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.enabled = particle_panel_enabled(psys)

		row = layout.row()
		row.itemR(part, "physics_type", expand=True)
		if part.physics_type != 'NO':
			row = layout.row()
			col = row.column(align=True)
			col.itemR(part, "particle_size")
			col.itemR(part, "random_size", slider=True)
			col = row.column(align=True)
			col.itemR(part, "mass")
			col.itemR(part, "sizemass", text="Multiply mass with size")
			
		if part.physics_type == 'NEWTON':
			split = layout.split()
			sub = split.column()
			
			sub.itemL(text="Forces:")
			sub.itemR(part, "brownian_factor")
			sub.itemR(part, "drag_factor", slider=True)
			sub.itemR(part, "damp_factor", slider=True)
			sub.itemR(part, "integrator")
			sub = split.column()
			sub.itemR(part, "acceleration")
			
		elif part.physics_type == 'KEYED':
			split = layout.split()
			sub = split.column()
			
			row = layout.row()
			col = row.column()
			col.active = not psys.keyed_timing
			col.itemR(part, "keyed_loops", text="Loops")
			row.itemR(psys, "keyed_timing", text="Use Timing")
			
			layout.itemL(text="Keys:")
		elif part.physics_type=='BOIDS':
			boids = part.boids
			

			row = layout.row()
			row.itemR(boids, "allow_flight")
			row.itemR(boids, "allow_land")
			row.itemR(boids, "allow_climb")
			
			split = layout.split()
			
			sub = split.column()
			col = sub.column(align=True)
			col.active = boids.allow_flight
			col.itemR(boids, "air_max_speed")
			col.itemR(boids, "air_min_speed", slider="True")
			col.itemR(boids, "air_max_acc", slider="True")
			col.itemR(boids, "air_max_ave", slider="True")
			col.itemR(boids, "air_personal_space")
			row = col.row()
			row.active = (boids.allow_land or boids.allow_climb) and boids.allow_flight
			row.itemR(boids, "landing_smoothness")
			
			sub = split.column()
			col = sub.column(align=True)
			col.active = boids.allow_land or boids.allow_climb
			col.itemR(boids, "land_max_speed")
			col.itemR(boids, "land_jump_speed")
			col.itemR(boids, "land_max_acc", slider="True")
			col.itemR(boids, "land_max_ave", slider="True")
			col.itemR(boids, "land_personal_space")
			col.itemR(boids, "land_stick_force")
			
			row = layout.row()
			
			col = row.column(align=True)
			col.itemL(text="Battle:")
			col.itemR(boids, "health")
			col.itemR(boids, "strength")
			col.itemR(boids, "aggression")
			col.itemR(boids, "accuracy")
			col.itemR(boids, "range")
			
			col = row.column()
			col.itemL(text="Misc:")
			col.itemR(part, "gravity")
			col.itemR(boids, "banking", slider=True)
			col.itemR(boids, "height", slider=True)
			
		if part.physics_type=='NEWTON':
			sub.itemR(part, "size_deflect")
			sub.itemR(part, "die_on_collision")
		elif part.physics_type=='KEYED' or part.physics_type=='BOIDS':
			if part.physics_type=='BOIDS':
				layout.itemL(text="Relations:")
			
			row = layout.row()
			row.template_list(psys, "targets", psys, "active_particle_target_index")
			
			col = row.column()
			subrow = col.row()
			subcol = subrow.column(align=True)
			subcol.itemO("particle.new_target", icon='ICON_ZOOMIN', text="")
			subcol.itemO("particle.remove_target", icon='ICON_ZOOMOUT', text="")
			subrow = col.row()
			subcol = subrow.column(align=True)
			subcol.itemO("particle.target_move_up", icon='VICON_MOVE_UP', text="")
			subcol.itemO("particle.target_move_down", icon='VICON_MOVE_DOWN', text="")
			
			key = psys.active_particle_target
			if key:
				row = layout.row()
				if part.physics_type=='KEYED':
					col = row.column()
					#doesn't work yet
					#col.red_alert = key.valid
					col.itemR(key, "object", text="")
					col.itemR(key, "system", text="System")
					col = row.column();
					col.active = psys.keyed_timing
					col.itemR(key, "time")
					col.itemR(key, "duration")
				else:
					subrow = row.row()
					#doesn't work yet
					#subrow.red_alert = key.valid
					subrow.itemR(key, "object", text="")
					subrow.itemR(key, "system", text="System")
					
					layout.itemR(key, "mode", expand=True)

class PARTICLE_PT_boidbrain(ParticleButtonsPanel):
	__label__ = "Boid Brain"

	def poll(self, context):
		psys = context.particle_system
		if psys==None:	return False
		if psys.settings==None:  return False
		if psys.point_cache.external: return False
		return psys.settings.physics_type=='BOIDS'
	
	def draw(self, context):
		boids = context.particle_system.settings.boids
		layout = self.layout
		
		layout.enabled = particle_panel_enabled(psys)
		
		# Currently boids can only use the first state so these are commented out for now.
		#row = layout.row()
		#row.template_list(boids, "states", boids, "active_boid_state_index", compact="True")
		#col = row.row()
		#subrow = col.row(align=True)
		#subrow.itemO("boid.boidstate_add", icon='ICON_ZOOMIN', text="")
		#subrow.itemO("boid.boidstate_del", icon='ICON_ZOOMOUT', text="")
		#subrow = row.row(align=True)
		#subrow.itemO("boid.boidstate_move_up", icon='VICON_MOVE_UP', text="")
		#subrow.itemO("boid.boidstate_move_down", icon='VICON_MOVE_DOWN', text="")
		
		state = boids.active_boid_state
		
		#layout.itemR(state, "name", text="State name")
		
		row = layout.row()
		row.itemR(state, "ruleset_type")
		if state.ruleset_type=='FUZZY':
			row.itemR(state, "rule_fuzziness", slider=True)
		else:
			row.itemL(text="")
		
		row = layout.row()
		row.template_list(state, "rules", state, "active_boid_rule_index")
		
		col = row.column()
		subrow = col.row()
		subcol = subrow.column(align=True)
		subcol.item_menu_enumO("boid.boidrule_add", "type", icon='ICON_ZOOMIN', text="")
		subcol.itemO("boid.boidrule_del", icon='ICON_ZOOMOUT', text="")
		subrow = col.row()
		subcol = subrow.column(align=True)
		subcol.itemO("boid.boidrule_move_up", icon='VICON_MOVE_UP', text="")
		subcol.itemO("boid.boidrule_move_down", icon='VICON_MOVE_DOWN', text="")
		
		rule = state.active_boid_rule
		
		if rule:
			row = layout.row()
			row.itemR(rule, "name", text="")
			#somebody make nice icons for boids here please! -jahka
			row.itemR(rule, "in_air", icon='VICON_MOVE_UP', text="")
			row.itemR(rule, "on_land", icon='VICON_MOVE_DOWN', text="")
			
			row = layout.row()

			if rule.type == 'GOAL':
				row.itemR(rule, "object")
				row = layout.row()
				row.itemR(rule, "predict")
			elif rule.type == 'AVOID':
				row.itemR(rule, "object")
				row = layout.row()
				row.itemR(rule, "predict")
				row.itemR(rule, "fear_factor")
			elif rule.type == 'FOLLOW_PATH':
				row.itemL(text="Not yet functional.")
			elif rule.type == 'AVOID_COLLISION':
				row.itemR(rule, "boids")
				row.itemR(rule, "deflectors")
				row.itemR(rule, "look_ahead")
			elif rule.type == 'FOLLOW_LEADER':
				row.itemR(rule, "object", text="")
				row.itemR(rule, "distance")
				row = layout.row()
				row.itemR(rule, "line")
				subrow = row.row()
				subrow.active = rule.line
				subrow.itemR(rule, "queue_size")
			elif rule.type == 'AVERAGE_SPEED':
				row.itemR(rule, "speed", slider=True)
				row.itemR(rule, "wander", slider=True)
				row.itemR(rule, "level", slider=True)
			elif rule.type == 'FIGHT':
				row.itemR(rule, "distance")
				row.itemR(rule, "flee_distance")
		

class PARTICLE_PT_render(ParticleButtonsPanel):
	__label__ = "Render"
	
	def poll(self, context):
		psys = context.particle_system
		if psys==None: return False
		if psys.settings==None: return False
		return True;
		
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
		
			if (part.type!='HAIR' and part.physics_type!='KEYED' and psys.point_cache.baked==False):
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

			sub.itemL(text="Timing:")
			sub.itemR(part, "abs_path_time")
			sub.itemR(part, "path_start", text="Start", slider= not part.abs_path_time)
			sub.itemR(part, "path_end", text="End", slider= not part.abs_path_time)		
			sub.itemR(part, "random_length", text="Random", slider=True)
			
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
		if part.ren_as == 'HALO' or part.ren_as == 'LINE' or part.ren_as=='BILLBOARD':
			row = layout.row()
			col = row.column()
			col.itemR(part, "trail_count")
			if part.trail_count > 1:
				col.itemR(part, "abs_path_time", text="Length in frames")
				col = row.column()
				col.itemR(part, "path_end", text="Length", slider=not part.abs_path_time)
				col.itemR(part, "random_length", text="Random", slider=True)
			else:
				col = row.column()
				col.itemL(text="")
				
class PARTICLE_PT_draw(ParticleButtonsPanel):
	__label__ = "Display"
	__default_closed__ = True
	
	def poll(self, context):
		psys = context.particle_system
		if psys==None: return False
		if psys.settings==None: return False
		return True;
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		row = layout.row()
		row.itemR(part, "draw_as", expand=True)
		
		if part.draw_as=='NONE' or (part.ren_as=='NONE' and part.draw_as=='RENDER'):
			return
			
		path = (part.ren_as=='PATH' and part.draw_as=='RENDER') or part.draw_as=='PATH'
			
		if path and part.type!='HAIR' and part.physics_type!='KEYED' and psys.point_cache.baked==False:
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
		col.itemR(part, "material_color", text="Use material color")
		
		if (path):			
			col.itemR(part, "draw_step")
		else:
			subcol = col.column()
			subcol.active = part.material_color==False
			#subcol.itemL(text="color")
			#subcol.itemL(text="Override material color")

class PARTICLE_PT_children(ParticleButtonsPanel):
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
		
		row = layout.row()
		col = row.column(align=True)
		col.itemR(part, "child_length", slider=True)
		col.itemR(part, "child_length_thres", slider=True)
		
		col = row.column(align=True)
		col.itemL(text="Space reserved for")
		col.itemL(text="hair parting controls")
		
		layout.row().itemL(text="Kink:")
		layout.row().itemR(part, "kink", expand=True)
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(part, "kink_amplitude")
		sub.itemR(part, "kink_frequency")
		sub = split.column()
		sub.itemR(part, "kink_shape", slider=True)

class PARTICLE_PT_effectors(ParticleButtonsPanel):
	__label__ = "Effectors"
	__default_closed__ = True
	
	def draw(self, context):
		layout = self.layout

		psys = context.particle_system
		part = psys.settings
		
		layout.itemR(part, "effector_group")
		
		layout.itemR(part, "eweight_all", slider=True)
		
		layout.itemS()
		layout.itemR(part, "eweight_spherical", slider=True)
		layout.itemR(part, "eweight_vortex", slider=True)
		layout.itemR(part, "eweight_magnetic", slider=True)
		layout.itemR(part, "eweight_wind", slider=True)
		layout.itemR(part, "eweight_curveguide", slider=True)
		layout.itemR(part, "eweight_texture", slider=True)
		layout.itemR(part, "eweight_harmonic", slider=True)
		layout.itemR(part, "eweight_charge", slider=True)
		layout.itemR(part, "eweight_lennardjones", slider=True)
		
class PARTICLE_PT_vertexgroups(ParticleButtonsPanel):
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
bpy.types.register(PARTICLE_PT_hair_dynamics)
bpy.types.register(PARTICLE_PT_cache)
bpy.types.register(PARTICLE_PT_emission)
bpy.types.register(PARTICLE_PT_initial)
bpy.types.register(PARTICLE_PT_physics)
bpy.types.register(PARTICLE_PT_boidbrain)
bpy.types.register(PARTICLE_PT_render)
bpy.types.register(PARTICLE_PT_draw)
bpy.types.register(PARTICLE_PT_children)
bpy.types.register(PARTICLE_PT_effectors)
bpy.types.register(PARTICLE_PT_vertexgroups)
