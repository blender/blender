import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import executionCodeChanged
from ... algorithms.lists import mask_CList
from ... data_structures import BooleanList, Vector3DList, DoubleList, FloatList

particleAttributes = [
    ("Locations", "locations", "location", "Vector List", Vector3DList),
    ("Velocities", "velocities", "velocity", "Vector List", Vector3DList),
    ("Sizes", "sizes", "size", "Float List", FloatList),
    ("Birth Times", "birthTimes", "birth_time", "Float List", FloatList),
    ("Die Times", "dieTimes", "die_time", "Float List", FloatList)
]

outputsData = [(type, name, identifier) for name, identifier, _, type, *_ in particleAttributes]
executionData = [(identifier, attribute, CListType) for _, identifier, attribute, _, CListType in particleAttributes]

class ParticleSystemParticlesDataNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ParticleSystemParticlesDataNode"
    bl_label = "Particles Data"

    includeUnborn = BoolProperty(name = "Include Unborn", default = False, update = executionCodeChanged)
    includeAlive = BoolProperty(name = "Include Alive", default = True, update = executionCodeChanged)
    includeDying = BoolProperty(name = "Include Dying", default = False, update = executionCodeChanged)
    includeDead = BoolProperty(name = "Include Dead", default = False, update = executionCodeChanged)

    useParticleSystemList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Particle System", "useParticleSystemList",
            ("Particle System", "particleSystem"),
            ("Particle Systems", "particleSystems"))

        for dataType, name, identifier in outputsData:
            self.newOutput(dataType, name, identifier)

        for socket in self.outputs[1:]:
            socket.hide = True

    def draw(self, layout):
        col = layout.column(align = True)
        row = col.row(align = True)
        row.prop(self, "includeUnborn", text = "Unborn", toggle = True)
        row.prop(self, "includeAlive", text = "Alive", toggle = True)
        row = col.row(align = True)
        row.prop(self, "includeDying", text = "Dying", toggle = True)
        row.prop(self, "includeDead", text = "Dead", toggle = True)

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not self.useParticleSystemList:
            yield "particleSystems = [particleSystem]"

        for identifier, attribute, CListType in executionData:
            if isLinked[identifier]:
                yield "{} = {}()".format(identifier, CListType.__name__)

        yield "for system in particleSystems:"
        yield "    if system is None: continue"
        yield "    if system.settings.type != 'EMITTER': continue"

        yield "    _mask = self.getParticlesMask(system.particles)"
        for identifier, attribute, CListType in executionData:
            if not isLinked[identifier]: continue
            yield "    values = self.getParticleProperties(system, '{}', {}, _mask)".format(attribute, CListType.__name__)
            yield "    {}.extend(values)".format(identifier)

        # convert FloatList to DoubleList
        for identifier, attribute, CListType in executionData:
            if isLinked[identifier]:
                if CListType is FloatList:
                    yield "{0} = DoubleList.fromValues({0})".format(identifier)

    def getParticleProperties(self, particleSystem, attribute, CListType, mask):
        particles = particleSystem.particles
        values = CListType(length = len(particles))
        particles.foreach_get(attribute, values.asMemoryView())
        return mask_CList(values, mask)

    def getParticlesMask(self, particles):
        allowedStates = self.getIncludedParticleStates()
        maskList = [p.alive_state in allowedStates for p in particles]
        return BooleanList.fromValues(maskList)

    def getIncludedParticleStates(self):
        return set(self.iterIncludedParticleStates())

    def iterIncludedParticleStates(self):
        if self.includeUnborn: yield "UNBORN"
        if self.includeAlive: yield "ALIVE"
        if self.includeDying: yield "DYING"
        if self.includeDead: yield "DEAD"
