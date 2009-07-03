
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
			split.itemO("OBJECT_OT_modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
		else:
			# add modifier
			split.item_enumO("OBJECT_OT_modifier_add", "type", "FLUID_SIMULATION", text="Add")
			split.itemL()

		if md:
			fluid = md.settings

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
				layout.itemO("FLUID_OT_bake", text="BAKE")
				layout.itemL(text="Required Memory: " + fluid.memory_estimate)
				
				layout.itemL(text="Resolution:")
				
				split = layout.split()
				
				col = split.column()
				colsub = col.column(align=True)
				colsub.itemR(fluid, "resolution", text="Final")
				colsub.itemR(fluid, "render_display_mode", text="")
				colsub = col.column(align=True)
				colsub.itemL(text="Time:")
				colsub.itemR(fluid, "start_time", text="Start")
				colsub.itemR(fluid, "end_time", text="End")
				
				col = split.column()
				colsub = col.column(align=True)
				colsub.itemR(fluid, "preview_resolution", text="Preview", slider=True)
				colsub.itemR(fluid, "viewport_display_mode", text="")
				colsub = col.column()
				colsub.itemR(fluid, "reverse_frames")
				colsub.itemR(fluid, "generate_speed_vectors")
				colsub.itemR(fluid, "path", text="")
				
			if fluid.type in ('FLUID', 'OBSTACLE', 'INFLOW', 'OUTFLOW'):
				layout.itemR(fluid, "volume_initialization")
				
			if fluid.type == 'FLUID':
				row = layout.row()
				row.column().itemR(fluid, "initial_velocity")
				row.itemR(fluid, "export_animated_mesh")
				
			if fluid.type == 'OBSTACLE':
				row = layout.row()
				row.itemL()
				row.itemR(fluid, "export_animated_mesh")
				layout.itemR(fluid, "slip_type", expand=True)
				if fluid.slip_type == 'PARTIALSLIP':
					layout.itemR(fluid, "partial_slip_amount", text="Amount")
					
				layout.itemR(fluid, "impact_factor")
				
			if fluid.type == 'INFLOW':
				row = layout.row()
				row.column().itemR(fluid, "inflow_velocity")
				row.itemR(fluid, "export_animated_mesh")
				layout.itemR(fluid, "local_coordinates")
				
			if fluid.type == 'OUTFLOW':
				row = layout.row()
				row.itemL()
				row.itemR(fluid, "export_animated_mesh")
				
			if fluid.type == 'PARTICLE':
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Type:")
				col.itemR(fluid, "drops")
				col.itemR(fluid, "floats")
				col.itemR(fluid, "tracer")
				
				col = split.column()
				col.itemL(text="Influence:")
				col.itemR(fluid, "particle_influence", text="Particle")
				col.itemR(fluid, "alpha_influence", text="Alpha")
				
				layout.itemR(fluid, "path")
				
			if fluid.type == 'CONTROL':
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Time:")
				col.itemR(fluid, "start_time", text="Start")
				col.itemR(fluid, "end_time", text="End")
				
				col = split.column()
				col.itemR(fluid, "quality", slider=True)
				col.itemR(fluid, "reverse_frames")
				
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Attraction:")
				col.itemR(fluid, "attraction_strength", text="Strength")
				col.itemR(fluid, "attraction_radius", text="Radius")
				
				col = split.column()
				col.itemL(text="Velocity:")
				col.itemR(fluid, "velocity_strength", text="Strength")
				col.itemR(fluid, "velocity_radius", text="Radius")

class PHYSICS_PT_domain_gravity(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_gravity"
	__label__ = "Domain World/Gravity"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			return (md.settings.type == 'DOMAIN')

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		split = layout.split()
		
		col = split.column()
		col.itemR(fluid, "gravity")
		
		col = split.column(align=True)
		col.itemL(text="Viscosity:")
		col.itemR(fluid, "viscosity_preset", text="")
		if fluid.viscosity_preset == 'MANUAL':
			col.itemR(fluid, "viscosity_base", text="Base")
			col.itemR(fluid, "viscosity_exponent", text="Exponent")
			
		col = layout.column_flow()
		col.itemR(fluid, "real_world_size")
		col.itemR(fluid, "grid_levels")
		col.itemR(fluid, "compressibility")
	
class PHYSICS_PT_domain_boundary(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_boundary"
	__label__ = "Domain Boundary"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			return (md.settings.type == 'DOMAIN')

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		layout.itemL(text="Slip:")
		
		layout.itemR(fluid, "slip_type", expand=True)
		if fluid.slip_type == 'PARTIALSLIP':
			layout.itemR(fluid, "partial_slip_amount", text="Amount")
		
		layout.itemL(text="Surface:")
		row = layout.row()
		row.itemR(fluid, "surface_smoothing", text="Smoothing")
		row.itemR(fluid, "surface_subdivisions", text="Subdivisions")
		
class PHYSICS_PT_domain_particles(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_domain_particles"
	__label__ = "Domain Particles"
	__default_closed__ = True
	
	def poll(self, context):
		md = context.fluid
		if md:
			return (md.settings.type == 'DOMAIN')

	def draw(self, context):
		layout = self.layout
		fluid = context.fluid.settings
		
		layout.itemR(fluid, "tracer_particles")
		layout.itemR(fluid, "generate_particles")

bpy.types.register(PHYSICS_PT_fluid)
bpy.types.register(PHYSICS_PT_domain_gravity)
bpy.types.register(PHYSICS_PT_domain_boundary)
bpy.types.register(PHYSICS_PT_domain_particles)
