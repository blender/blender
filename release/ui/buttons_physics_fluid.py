
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		ob = context.object
		return (ob and ob.type == 'MESH')
		
class PHYSICS_PT_fluid(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_fluid"
	__label__ = "Fluid"

	def draw(self, context):
		layout = self.layout
		md = context.fluid
		ob = context.object

		split = layout.split()
		split.operator_context = "EXEC_DEFAULT"

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("object.modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
			
			fluid = md.settings
			
		else:
			# add modifier
			split.item_enumO("object.modifier_add", "type", "FLUID_SIMULATION", text="Add")
			split.itemL()
			
			fluid = None
		
		
		if fluid:
			

			col = layout.column(align=True)
			row = col.row()
			row.item_enumR(fluid, "type", "DOMAIN")
			row.item_enumR(fluid, "type", "FLUID")
			row.item_enumR(fluid, "type", "OBSTACLE")
			row = col.row()
			row.item_enumR(fluid, "type", "INFLOW")
			row.item_enumR(fluid, "type", "OUTFLOW")
			row.item_enumR(fluid, "type", "PARTICLE")
			row.item_enumR(fluid, "type", "CONTROL")

			if fluid.type == 'DOMAIN':
				layout.itemO("fluid.bake", text="BAKE")
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Resolution:")
				colsub = col.column()
				colsub.itemR(fluid, "resolution", text="Final")
				colsub.itemL(text="Render Display:")
				colsub.itemR(fluid, "render_display_mode", text="")
				col.itemL(text="Time:")
				colsub = col.column(align=True)
				colsub.itemR(fluid, "start_time", text="Start")
				colsub.itemR(fluid, "end_time", text="End")
				
				col = split.column()
				colsub = col.column()
				colsub.itemL(text="Required Memory: " + fluid.memory_estimate)
				colsub.itemR(fluid, "preview_resolution", text="Preview")
				colsub.itemL(text="Viewport Display:")
				colsub.itemR(fluid, "viewport_display_mode", text="")
				colsub = col.column()
				colsub.itemL(text="")
				colsub.itemR(fluid, "reverse_frames")
				colsub.itemR(fluid, "generate_speed_vectors")
				
				layout.itemL(text="Path:")
				layout.itemR(fluid, "path", text="")
				
			if fluid.type == 'FLUID':
				split = layout.split()
				col = split.column()
				col.itemL(text="Volume Initialization:")
				col.itemR(fluid, "volume_initialization", text="")
				col.itemR(fluid, "export_animated_mesh")
				col = split.column()
				col.itemL(text="Initial Velocity:")
				col.itemR(fluid, "initial_velocity", text="")
				
			if fluid.type == 'OBSTACLE':
				split = layout.split()
				col = split.column()
				col.itemL(text="Volume Initialization:")
				col.itemR(fluid, "volume_initialization", text="")
				col.itemR(fluid, "export_animated_mesh")
				col = split.column()
				col.itemL(text="Slip Type:")
				colsub=col.column(align=True)
				colsub.itemR(fluid, "slip_type", text="")
				if fluid.slip_type == 'PARTIALSLIP':
					colsub.itemR(fluid, "partial_slip_amount", text="Amount")
					
				col.itemR(fluid, "impact_factor")
				
			if fluid.type == 'INFLOW':
				split = layout.split()
				col = split.column()
				col.itemL(text="Volume Initialization:")
				col.itemR(fluid, "volume_initialization", text="")
				col.itemR(fluid, "export_animated_mesh")
				col.itemR(fluid, "local_coordinates")
				
				col = split.column()
				col.itemL(text="Inflow Velocity:")
				col.itemR(fluid, "inflow_velocity", text="")
				
			if fluid.type == 'OUTFLOW':
				split = layout.split()
				col = split.column()
				col.itemL(text="Volume Initialization:")
				col.itemR(fluid, "volume_initialization", text="")
				col.itemR(fluid, "export_animated_mesh")
				col = split.column()
				
			if fluid.type == 'PARTICLE':
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Influence:")
				colsub = col.column(align=True)
				colsub.itemR(fluid, "particle_influence", text="Size")
				colsub.itemR(fluid, "alpha_influence", text="Alpha")
				col.itemL(text="Path:")
				
				layout.itemR(fluid, "path", text="")
				
				col = split.column()
				col.itemL(text="Type:")
				col.itemR(fluid, "drops")
				col.itemR(fluid, "floats")
				col.itemR(fluid, "tracer")
				
			if fluid.type == 'CONTROL':
				split = layout.split()
				
				col = split.column()
				col.itemL(text="")
				col.itemR(fluid, "quality", slider=True)
				col.itemR(fluid, "reverse_frames")
				
				col = split.column()
				col.itemL(text="Time:")
				col=col.column(align=True)
				col.itemR(fluid, "start_time", text="Start")
				col.itemR(fluid, "end_time", text="End")
				
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Attraction Force:")
				col=col.column(align=True)
				col.itemR(fluid, "attraction_strength", text="Strength")
				col.itemR(fluid, "attraction_radius", text="Radius")
				
				col = split.column()
				col.itemL(text="Velocity Force:")
				col=col.column(align=True)
				col.itemR(fluid, "velocity_strength", text="Strength")
				col.itemR(fluid, "velocity_radius", text="Radius")

class PHYSICS_PT_domain_gravity(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_gravity"
	__label__ = "Domain World"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			settings = md.settings
			if settings:
				return (settings.type == 'DOMAIN')
		
		return False

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Gravity:")
		col.itemR(fluid, "gravity", text="")
		
		col.itemL(text="Size:")
		col.itemR(fluid, "real_world_size", text="Real World Size")
		
		col = split.column()
		col.itemL(text="Viscosity Presets:")
		colsub=col.column(align=True)
		colsub.itemR(fluid, "viscosity_preset", text="")
		
		if fluid.viscosity_preset == 'MANUAL':
			colsub.itemR(fluid, "viscosity_base", text="Base")
			colsub.itemR(fluid, "viscosity_exponent", text="Exponent", slider=True)
		else:
			colsub.itemL(text="")
			colsub.itemL(text="")
			
		col.itemL(text="Optimization:")
		col=col.column(align=True)
		col.itemR(fluid, "grid_levels", slider=True)
		col.itemR(fluid, "compressibility")
	
class PHYSICS_PT_domain_boundary(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_boundary"
	__label__ = "Domain Boundary"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			settings = md.settings
			if settings:
				return (settings.type == 'DOMAIN')
		
		return False

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		split = layout.split()
		col = split.column()
		col.itemL(text="Slip Type:")
		col=col.column(align=True)
		col.itemR(fluid, "slip_type", text="")
		if fluid.slip_type == 'PARTIALSLIP':
			col.itemR(fluid, "partial_slip_amount", text="Amount")
		col = split.column()
		col.itemL(text="Surface:")
		col=col.column(align=True)
		col.itemR(fluid, "surface_smoothing", text="Smoothing")
		col.itemR(fluid, "surface_subdivisions", text="Subdivisions")	
		
class PHYSICS_PT_domain_particles(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_particles"
	__label__ = "Domain Particles"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			settings = md.settings
			if settings:
				return (settings.type == 'DOMAIN')
		
		return False

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		col=layout.column(align=True)
		col.itemR(fluid, "tracer_particles")
		col.itemR(fluid, "generate_particles")

bpy.types.register(PHYSICS_PT_fluid)
bpy.types.register(PHYSICS_PT_domain_gravity)
bpy.types.register(PHYSICS_PT_domain_boundary)
bpy.types.register(PHYSICS_PT_domain_particles)