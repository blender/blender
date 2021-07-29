import bpy
from ... base_types import AnimationNode

class ParticleSystemsInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ParticleSystemsFromObjectNode"
    bl_label = "Particle Systems from Object"
    bl_width_default = 150

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Particle System", "Active", "active")
        self.newOutput("Particle System List", "Particle Systems", "particleSystems")

    def execute(self, object):
        if not object: return None, []
        particleSystems = object.particle_systems
        active = particleSystems.active
        return active, list(particleSystems)
