
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		return (context.object != None)
		
class PHYSICS_PT_field(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_field"
	__label__ = "Field"

	def draw(self, context):
		layout = self.layout
		ob = context.object
		field = ob.field

		layout.itemR(field, "type")

		if field.type != "NONE":
			layout.itemR(field, "strength")

		if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "LENNARDj"):
			if ob.type in ("MESH", "SURFACE", "FONT", "CURVE"):
				layout.itemR(field, "surface")

class PHYSICS_PT_collision(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_collision"
	__label__ = "Collision"

	def draw_header(self, context):
		settings = context.object.collision
		self.layout.itemR(settings, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		md = context.collision
		settings = context.object.collision

		if settings.enabled:
			pass

bpy.types.register(PHYSICS_PT_field)
bpy.types.register(PHYSICS_PT_collision)

