
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		rd = context.scene.render_data
		return (context.object != None) and (not rd.use_game_engine)
		
class PHYSICS_PT_field(PhysicButtonsPanel):
	__label__ = "Force Fields"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		ob = context.object
		field = ob.field

		#layout.active = field.enabled
		
		split = layout.split(percentage=0.3)
		
		split.itemL(text="Type:")
		split.itemR(field, "type", text=""
		)

		split = layout.split()
		
		sub = split.column()
							
		if field.type == "GUIDE":
			sub = col.column()
			sub.itemR(field, "guide_path_add")
			
		if field.type == "WIND":
			sub.itemR(field, "strength")
			sub = split.column()
			sub.itemR(field, "noise")
			sub.itemR(field, "seed")

		
		if field.type == "VORTEX":
			sub.itemR(field, "strength")
			sub = split.column()
			sub.itemL(text="")

		if field.type in ("SPHERICAL", "CHARGE", "LENNARDJ"):
			sub.itemR(field, "strength")
			sub = split.column()
			sub.itemR(field, "planar")
			sub.itemR(field, "surface")
			
		if field.type == "BOID":
			sub.itemR(field, "strength")
			sub = split.column()
			sub.itemR(field, "surface")
			
		if field.type == "MAGNET":
			sub.itemR(field, "strength")
			sub = split.column()
			sub.itemR(field, "planar")
			
		if field.type == "HARMONIC":
			sub.itemR(field, "strength")
			sub.itemR(field, "harmonic_damping", text="Damping")
			sub = split.column()
			sub.itemR(field, "surface")
			sub.itemR(field, "planar")
			
		if field.type == "TEXTURE":
			sub.itemR(field, "strength")
			sub.itemR(field, "texture", text="")
			sub.itemR(field, "texture_mode")
			sub.itemR(field, "texture_nabla")
			sub = split.column()
			sub.itemR(field, "use_coordinates")
			sub.itemR(field, "root_coordinates")
			sub.itemR(field, "force_2d")
			
		if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "WIND", "VORTEX", "TEXTURE", "MAGNET", "BOID"):
		
			
			layout.itemS()			
			layout.itemL(text="Falloff:")
			layout.itemR(field, "falloff_type", expand=True)
			
			row = layout.row()
			row.itemR(field, "falloff_power", text="Power")
			row.itemR(field, "positive_z", text="Positive Z")
			
			layout.itemS()	
			split = layout.split()
			sub = split.column()
			
			sub.itemR(field, "use_min_distance", text="Minimum")
			colsub1 = sub.column()
			colsub1.active = field.use_min_distance
			colsub1.itemR(field, "minimum_distance", text="Distance")
			
			sub = split.column()
			
			sub.itemR(field, "use_max_distance", text="Maximum")
			colsub2 = sub.column()
			colsub2.active = field.use_max_distance
			colsub2.itemR(field, "maximum_distance", text="Distance")
			
			if field.falloff_type == "CONE":
				layout.itemS()	
				layout.itemL(text="Angular:")
				
				row = layout.row()
				row.itemR(field, "radial_falloff", text="Power")
				row.itemL(text="")
				
				split = layout.split()
				sub = split.column()
				
				sub.itemR(field, "use_radial_min", text="Minimum")	
				colsub1 = sub.column()
				colsub1.active = field.use_radial_min
				colsub1.itemR(field, "radial_minimum", text="Angle")
				
				sub = split.column()
				
				sub.itemR(field, "use_radial_max", text="Maximum")
				colsub2 = sub.column()
				colsub2.active = field.use_radial_max
				colsub2.itemR(field, "radial_maximum", text="Angle")
				
			if field.falloff_type == "TUBE":
				
				layout.itemS()	
				layout.itemL(text="Radial:")	
				row = layout.row()
				row.itemR(field, "radial_falloff", text="Power")
				row.itemL(text="")
				
				split = layout.split()
				sub = split.column()
				
				sub.itemR(field, "use_radial_min", text="Minimum")	
				colsub1 = sub.column()
				colsub1.active = field.use_radial_min
				colsub1.itemR(field, "radial_minimum", text="Distance")
				
				sub = split.column()
				
				sub.itemR(field, "use_radial_max", text="Maximum")
				colsub2 = sub.column()
				colsub2.active = field.use_radial_max
				colsub2.itemR(field, "radial_maximum", text="Distance")
				
		#if ob.type in "CURVE":
			#if field.type == "GUIDE":
				#colsub = col.column(align=True)
			
		#if field.type != "NONE":
			#layout.itemR(field, "strength")

		#if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "LENNARDj"):
			#if ob.type in ("MESH", "SURFACE", "FONT", "CURVE"):
				#layout.itemR(field, "surface")

class PHYSICS_PT_collision(PhysicButtonsPanel):
	__label__ = "Collision"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		rd = context.scene.render_data
		return (ob and ob.type == 'MESH') and (not rd.use_game_engine)

	def draw_header(self, context):
		settings = context.object.collision
		self.layout.itemR(settings, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		md = context.collision
		settings = context.object.collision

		layout.active = settings.enabled
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Particle:")
		col.itemR(settings, "permeability", slider=True)
		col.itemL(text="Particle Damping:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "damping_factor", text="Factor", slider=True)
		colsub.itemR(settings, "random_damping", text="Random", slider=True)
		
		col.itemL(text="Soft Body and Cloth:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "outer_thickness", text="Outer", slider=True)
		colsub.itemR(settings, "inner_thickness", text="Inner", slider=True)
		
		col.itemL(text="Force Fields:")
		layout.itemR(md, "absorption", text="Absorption")
		
		col = split.column()
		col.itemL(text="")
		col.itemR(settings, "kill_particles")
		col.itemL(text="Particle Friction:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "friction_factor", text="Factor", slider=True)
		colsub.itemR(settings, "random_friction", text="Random", slider=True)
		col.itemL(text="Soft Body Damping:")
		col.itemR(settings, "damping", text="Factor", slider=True)
		
bpy.types.register(PHYSICS_PT_field)
bpy.types.register(PHYSICS_PT_collision)
