
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def cloth_modifier(self, context):
		ob = context.active_object
		for md in ob.modifiers:
			if md.type == 'CLOTH':
				return md

		return None
	
	def poll(self, context):
		md = self.cloth_modifier(context)
		return (md != None)
		
class Physic_PT_cloth(PhysicButtonsPanel):
	__idname__ = "Physic_PT_cloth"
	__label__ = "Cloth"

	def draw(self, context):
		layout = self.layout
		md = self.cloth_modifier(context)
		cloth = md.settings
		
		split = layout.split()
		
		col = split.column()
		col.itemR(cloth, "quality", slider=True)
		col.itemR(cloth, "gravity")
		col.itemR(cloth, "mass")
		col.itemR(cloth, "mass_vertex_group", text="Vertex Group")

		col = split.column()
		col.itemL(text="Stiffness:")
		col.itemR(cloth, "structural_stiffness", text="Structural")
		col.itemR(cloth, "bending_stiffness", text="Bending")
		col.itemL(text="Damping:")
		col.itemR(cloth, "spring_damping", text="Spring")
		col.itemR(cloth, "air_damping", text="Air")
		
		# Disabled for now#
		"""
		if cloth.mass_vertex_group:
			layout.itemL(text="Goal:")
		
			col = layout.column_flow()
			col.itemR(cloth, "goal_default", text="Default")
			col.itemR(cloth, "goal_spring", text="Stiffness")
			col.itemR(cloth, "goal_friction", text="Friction")
		"""
	
class Physic_PT_cloth_collision(PhysicButtonsPanel):
	__idname__ = "Physic_PT_clothcollision"
	__label__ = "Cloth Collision"
	
	def draw_header(self, context):
		layout = self.layout
		md = self.cloth_modifier(context)			
		cloth = md.collision_settings
	
		layout.itemR(cloth, "enable_collision", text="")

	def draw(self, context):
		layout = self.layout
		md = self.cloth_modifier(context)		
		cloth = md.collision_settings
		
		col = layout.column_flow()
		col.itemR(cloth, "collision_quality", slider=True)
		col.itemR(cloth, "min_distance", text="MinDistance")
		col.itemR(cloth, "friction")
		
		layout.itemR(cloth, "enable_self_collision", text="Self Collision")
		
		row = layout.row()
		row.itemR(cloth, "self_collision_quality", slider=True)
		row.itemR(cloth, "self_min_distance", text="MinDistance")

class Physic_PT_cloth_stiffness(PhysicButtonsPanel):
	__idname__ = "Physic_PT_stiffness"
	__label__ = "Cloth Stiffness Scaling"
	
	def draw_header(self, context):
		layout = self.layout
		md = self.cloth_modifier(context)
		cloth = md.settings
	
		layout.itemR(cloth, "stiffness_scaling", text="")

	def draw(self, context):
		layout = self.layout
		md = self.cloth_modifier(context)
		cloth = md.settings
		
		split = layout.split()
		
		sub = split.column()
		sub.itemL(text="Structural Stiffness:")
		sub.column().itemR(cloth, "structural_stiffness_vertex_group", text="VGroup")
		sub.itemR(cloth, "structural_stiffness_max", text="Max")
		
		sub = split.column()
		sub.itemL(text="Bending Stiffness:")
		sub.column().itemR(cloth, "bending_vertex_group", text="VGroup")
		sub.itemR(cloth, "bending_stiffness_max", text="Max")
		
bpy.types.register(Physic_PT_cloth)
bpy.types.register(Physic_PT_cloth_collision)
bpy.types.register(Physic_PT_cloth_stiffness)