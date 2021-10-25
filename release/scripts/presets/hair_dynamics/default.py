import bpy
psys = bpy.context.particle_system
cloth = bpy.context.particle_system.cloth
settings = bpy.context.particle_system.cloth.settings
collision = bpy.context.particle_system.cloth.collision_settings

settings.quality = 5
settings.mass = 0.30000001192092896
settings.bending_stiffness = 0.5
psys.settings.bending_random = 0.0
settings.bending_damping = 0.5
settings.air_damping = 1.0
settings.internal_friction = 0.0
settings.density_target = 0.0
settings.density_strength = 0.0
settings.voxel_cell_size = 0.10000000149011612
settings.pin_stiffness = 0.0
