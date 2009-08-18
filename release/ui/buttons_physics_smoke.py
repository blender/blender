
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "PROPERTIES"
	__region_type__ = "WINDOW"
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

				col.itemL(text="Display:")
				col.itemR(domain, "visibility", text="Resolution")
				col.itemR(domain, "color", slider=True)
				sub = col.column()
				sub.active = domain.highres
				sub.itemR(domain, "viewhighres")
				
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
			
class PHYSICS_PT_smoke_highres(PhysicButtonsPanel):
	__label__ = "Smoke High Resolution"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.smoke
		if md:
				return (md.smoke_type == 'TYPE_DOMAIN')
		
		return False

	def draw_header(self, context):
		layout = self.layout
		
		high = context.smoke.domain_settings
	
		layout.itemR(high, "highres", text="")
		
	def draw(self, context):
		layout = self.layout
		
		high = context.smoke.domain_settings
		
		layout.active = high.highres
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Resolution:")
		col.itemR(high, "amplify", text="Divisions")
		
		sub = split.column()
		sub.itemL(text="Noise Method:")
		sub.row().itemR(high, "noise_type", text="")
		sub.itemR(high, "strength")
			
class PHYSICS_PT_smoke_groups(PhysicButtonsPanel):
	__label__ = "Smoke Groups"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.smoke
		if md:
				return (md.smoke_type == 'TYPE_DOMAIN')
		
		return False

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

bpy.types.register(PHYSICS_PT_smoke)
bpy.types.register(PHYSICS_PT_smoke_highres)
bpy.types.register(PHYSICS_PT_smoke_groups)
