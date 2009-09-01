
import bpy

from buttons_particle import point_cache_ui

def smoke_panel_enabled_low(smd):
	if smd.smoke_type == 'TYPE_DOMAIN':
		return smd.domain.point_cache.baked==False
	return True

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "physics"

	def poll(self, context):
		ob = context.object
		rd = context.scene.render_data
		return (ob and ob.type == 'MESH') and (not rd.use_game_engine)
		
class PHYSICS_PT_smoke(PhysicButtonsPanel):
	__label__ = "Smoke"

	def draw(self, context):
		layout = self.layout
		
		md = context.smoke
		ob = context.object

		split = layout.split()
		split.operator_context = 'EXEC_DEFAULT'

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("object.modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
			
		else:
			# add modifier
			split.item_enumO("object.modifier_add", "type", 'SMOKE', text="Add")
			split.itemL()

		if md:
		
			# layout.enabled = smoke_panel_enabled(md)
			layout.itemR(md, "smoke_type", expand=True)
		
			if md.smoke_type == 'TYPE_DOMAIN':
				
				domain = md.domain_settings
				
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Resolution:")
				col.itemR(domain, "maxres", text="Divisions")
				
				col = split.column()
				col.itemL(text="Behavior:")
				col.itemR(domain, "alpha")
				col.itemR(domain, "beta")
				col.itemR(domain, "dissolve_smoke", text="Dissolve")
				sub = col.column()
				sub.active = domain.dissolve_smoke
				sub.itemR(domain, "dissolve_speed", text="Speed")
				sub.itemR(domain, "dissolve_smoke_log", text="Slow")
				
			elif md.smoke_type == 'TYPE_FLOW':
				
				flow = md.flow_settings
				
				split = layout.split()
				
				col = split.column()
				col.itemR(flow, "outflow")
				col.itemL(text="Particle System:")
				col.item_pointerR(flow, "psys", ob, "particle_systems", text="")
				
				if md.flow_settings.outflow:				
					col = split.column()
				else:
					col = split.column()
					col.itemL(text="Behavior:")
					col.itemR(flow, "temperature")
					col.itemR(flow, "density")
					
			#elif md.smoke_type == 'TYPE_COLL':
			#	layout.itemS()
					
class PHYSICS_PT_smoke_groups(PhysicButtonsPanel):
	__label__ = "Smoke Groups"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.smoke
		return md and (md.smoke_type == 'TYPE_DOMAIN')

	def draw(self, context):
		layout = self.layout
		
		group = context.smoke.domain_settings
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Flow Group:")
		col.itemR(group, "fluid_group", text="")
				
		#col.itemL(text="Effector Group:")
		#col.itemR(group, "eff_group", text="")
				
		col = split.column()
		col.itemL(text="Collision Group:")
		col.itemR(group, "coll_group", text="")
		
class PHYSICS_PT_smoke_cache(PhysicButtonsPanel):
	__label__ = "Smoke Cache"
	__default_closed__ = True

	def poll(self, context):
		md = context.smoke
		return md and (md.smoke_type == 'TYPE_DOMAIN')

	def draw(self, context):
		layout = self.layout

		md = context.smoke.domain_settings
		cache = md.point_cache
			
		point_cache_ui(self, cache, cache.baked==False, 0, 1)
					
class PHYSICS_PT_smoke_highres(PhysicButtonsPanel):
	__label__ = "Smoke High Resolution"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.smoke
		return md and (md.smoke_type == 'TYPE_DOMAIN')

	def draw_header(self, context):	
		high = context.smoke.domain_settings
	
		self.layout.itemR(high, "highres", text="")
		
	def draw(self, context):
		layout = self.layout
		
		md = context.smoke.domain_settings

		split = layout.split()
			
		col = split.column()
		col.itemL(text="Resolution:")
		col.itemR(md, "amplify", text="Divisions")
			
		col = split.column()
		col.itemL(text="Noise Method:")
		col.row().itemR(md, "noise_type", text="")
		col.itemR(md, "strength")
		col.itemR(md, "show_highres")
		
class PHYSICS_PT_smoke_cache_highres(PhysicButtonsPanel):
	__label__ = "Smoke Cache"
	__default_closed__ = True

	def poll(self, context):
		return (context.smoke)

	def draw(self, context):
		layout = self.layout

		md = context.smoke

		cache = md.point_cache
			
		layout.set_context_pointer("PointCache", cache)
			
		row = layout.row()
		row.template_list(cache, "point_cache_list", cache, "active_point_cache_index")
		col = row.column(align=True)
		col.itemO("ptcache.add_new", icon='ICON_ZOOMIN', text="")
		col.itemO("ptcache.remove", icon='ICON_ZOOMOUT', text="")
			
		row = layout.row()
		row.itemR(cache, "name")
			
		row = layout.row()
		row.itemR(cache, "start_frame")
		row.itemR(cache, "end_frame")
			
		row = layout.row()
			
		if cache.baked == True:
			row.itemO("ptcache.free_bake", text="Free Bake")
		else:
			row.item_booleanO("ptcache.bake", "bake", True, text="Bake")
			
		subrow = row.row()
		subrow.enabled = cache.frames_skipped or cache.outdated
		subrow.itemO("ptcache.bake", "bake", False, text="Calculate to Current Frame")
				
		row = layout.row()
		#row.enabled = smoke_panel_enabled(psys)
		row.itemO("ptcache.bake_from_cache", text="Current Cache to Bake")
		
		row = layout.row()
		#row.enabled = smoke_panel_enabled(psys)
			
		layout.itemL(text=cache.info)
			
		layout.itemS()
			
		row = layout.row()
		row.itemO("ptcache.bake_all", "bake", True, text="Bake All Dynamics")
		row.itemO("ptcache.free_bake_all", text="Free All Bakes")
		layout.itemO("ptcache.bake_all", "bake", False, text="Update All Dynamics to current frame")

bpy.types.register(PHYSICS_PT_smoke)
bpy.types.register(PHYSICS_PT_smoke_cache)
bpy.types.register(PHYSICS_PT_smoke_groups)
#bpy.types.register(PHYSICS_PT_smoke_highres)
#bpy.types.register(PHYSICS_PT_smoke_cache_highres)
