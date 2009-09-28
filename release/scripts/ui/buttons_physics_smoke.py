
import bpy

from buttons_particle import point_cache_ui

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
				sub.itemR(domain, "dissolve_speed", text="Time")
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
		cache = md.point_cache_low
			
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
		col.itemR(md, "viewhighres")
		
class PHYSICS_PT_smoke_cache_highres(PhysicButtonsPanel):
	__label__ = "Smoke High Resolution Cache"
	__default_closed__ = True

	def poll(self, context):
		md = context.smoke
		return md and (md.smoke_type == 'TYPE_DOMAIN') and md.domain_settings.highres

	def draw(self, context):
		layout = self.layout

		md = context.smoke.domain_settings
		cache = md.point_cache_high
			
		point_cache_ui(self, cache, cache.baked==False, 0, 1)
					
bpy.types.register(PHYSICS_PT_smoke)
bpy.types.register(PHYSICS_PT_smoke_cache)
bpy.types.register(PHYSICS_PT_smoke_highres)
bpy.types.register(PHYSICS_PT_smoke_groups)
bpy.types.register(PHYSICS_PT_smoke_cache_highres)
