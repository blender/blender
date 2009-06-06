
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		return (context.cloth != None)
		
class Physic_PT_cloth(PhysicButtonsPanel):
	__idname__ = "Physic_PT_cloth"
	__label__ = "Cloth"

	def draw(self, context):
		layout = self.layout
		md = context.cloth
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
		
		# Disabled for now
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
		md = context.cloth
		cloth = md.collision_settings
	
		layout.itemR(cloth, "enable_collision", text="")

	def draw(self, context):
		layout = self.layout
		
		md = context.cloth
		cloth = md.collision_settings
		layout.active = cloth.enable_collision	
		
		col = layout.column_flow()
		col.itemR(cloth, "collision_quality", slider=True)
		col.itemR(cloth, "friction")
		col.itemR(cloth, "min_distance", text="MinDistance")
		
		
		layout.itemR(cloth, "enable_self_collision", text="Self Collision")
		
		col = layout.column_flow()
		col.active = cloth.enable_self_collision
		col.itemR(cloth, "self_collision_quality", slider=True)
		col.itemR(cloth, "self_min_distance", text="MinDistance")

class Physic_PT_cloth_stiffness(PhysicButtonsPanel):
	__idname__ = "Physic_PT_stiffness"
	__label__ = "Cloth Stiffness Scaling"
	
	def draw_header(self, context):
		layout = self.layout
		md = context.cloth
		cloth = md.settings
	
		layout.itemR(cloth, "stiffness_scaling", text="")

	def draw(self, context):
		layout = self.layout
		
		md = context.cloth
		cloth = md.settings
		layout.active = cloth.stiffness_scaling	
		
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