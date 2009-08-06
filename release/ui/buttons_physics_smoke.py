
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
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
			layout.itemR(md, "smoke_type")
		
			if md.smoke_type == 'TYPE_DOMAIN':
				
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Behavior:")
				col.itemR(md.domain_settings, "alpha")
				col.itemR(md.domain_settings, "beta")
				
				col.itemL(text="Resolution:")
				col.itemR(md.domain_settings, "maxres", text="Low")
				sub = col.column()
				sub.active = md.domain_settings.highres
				sub.itemR(md.domain_settings, "amplify", text="High")
				col.itemR(md.domain_settings, "highres", text="Use High Resolution")
				
				sub = split.column()
				sub.itemL(text="Display:")
				sub.itemR(md.domain_settings, "visibility")
				sub.itemR(md.domain_settings, "color", slider=True)
				mysub = sub.column()
				mysub.active = md.domain_settings.highres
				mysub.itemR(md.domain_settings, "viewhighres")
				
				layout.itemL(text="Noise Type:")
				layout.itemR(md.domain_settings, "noise_type", expand=True)
				
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Flow Group:")
				col.itemR(md.domain_settings, "fluid_group", text="")
				
				#col.itemL(text="Effector Group:")
				#col.itemR(md.domain_settings, "eff_group", text="")
				
				col = split.column()
				col.itemL(text="Collision Group:")
				col.itemR(md.domain_settings, "coll_group", text="")
				
			elif md.smoke_type == 'TYPE_FLOW':
				
				split = layout.split()
				
				col = split.column()
				col.itemR(md.flow_settings, "outflow")
				col.itemL(text="Particle System:")
				col.item_pointerR(md.flow_settings, "psys", ob, "particle_systems", text="")
				
				if md.flow_settings.outflow:				
					col = split.column()
				else:
					sub = split.column()
					sub.itemL(text="Behavior:")
					sub.itemR(md.flow_settings, "temperature")
					sub.itemR(md.flow_settings, "density")
					
			elif md.smoke_type == 'TYPE_COLL':
				layout.itemS()

bpy.types.register(PHYSICS_PT_smoke)