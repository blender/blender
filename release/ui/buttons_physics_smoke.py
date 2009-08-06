
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
				layout.itemS()
				layout.itemR(md.domain_settings, "maxres")
				layout.itemR(md.domain_settings, "color")
				layout.itemR(md.domain_settings, "amplify")
				layout.itemR(md.domain_settings, "highres")
				layout.itemR(md.domain_settings, "noise_type")
				layout.itemR(md.domain_settings, "visibility")
				layout.itemR(md.domain_settings, "alpha")
				layout.itemR(md.domain_settings, "beta")
				layout.itemR(md.domain_settings, "fluid_group")
				layout.itemR(md.domain_settings, "eff_group")
				layout.itemR(md.domain_settings, "coll_group")
			elif md.smoke_type == 'TYPE_FLOW':
				layout.itemS()
				layout.itemR(md.flow_settings, "outflow")
				layout.itemR(md.flow_settings, "density")
				layout.itemR(md.flow_settings, "temperature")
				layout.item_pointerR(md.flow_settings, "psys", ob, "particle_systems")
			elif md.smoke_type == 'TYPE_COLL':
				layout.itemS()

bpy.types.register(PHYSICS_PT_smoke)
