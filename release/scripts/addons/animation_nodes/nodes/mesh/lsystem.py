import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import Mesh, DoubleList
from ... algorithms.lsystem import calculateLSystem

class LSystemNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LSystemNode"
    bl_label = "LSystem"
    errorHandlingType = "EXCEPTION"
    bl_width_default = 180

    def getPresetItems(self, context):
        return [(name, name, "") for name in presets.keys()]

    useSymbolLimit = BoolProperty(name = "Use Symbol Limit", default = True)

    symbolLimit = IntProperty(name = "Symbol Limit", default = 100000,
        description = "To prevent freezing Blender when trying to calculate too many generations.",
        min = 0)

    preset = EnumProperty(name = "Preset", items = getPresetItems)

    def create(self):
        self.newInput("Text", "Axiom", "axiom")
        self.newInput("Text List", "Rules", "rules")
        self.newInput("Float", "Generations", "generations", minValue = 0)
        self.newInput("Float", "Step Size", "stepSize", value = 1)
        self.newInput("Float", "Angle", "angle", value = 90)
        self.newInput("Integer", "Seed", "seed")

        self.newInput("Float", "Scale Width", "scaleWidth", value = 0.8, hide = True)
        self.newInput("Float", "Scale Step Size", "scaleStepSize", value = 0.9, hide = True)
        self.newInput("Float", "Gravity", "gravity", value = 0, hide = True)
        self.newInput("Float", "Random Angle", "randomAngle", value = 180, hide = True)
        self.newInput("Boolean", "Only Partial Moves", "onlyPartialMoves", value = True, hide = True)

        self.newOutput("Mesh", "Mesh", "mesh")
        self.newOutput("Float List", "Edge Widths", "edgeWidths")
        self.newOutput("Text", "Symbols", "symbols")
        self.newOutput("Matrix List", "J", "statesJ", hide = True)
        self.newOutput("Matrix List", "K", "statesK", hide = True)
        self.newOutput("Matrix List", "M", "statesM", hide = True)

    def drawAdvanced(self, layout):
        row = layout.row(align = True)
        subrow = row.row(align = True)
        subrow.active = self.useSymbolLimit
        subrow.prop(self, "symbolLimit")
        icon = "LAYER_ACTIVE" if self.useSymbolLimit else "LAYER_USED"
        row.prop(self, "useSymbolLimit", text = "", icon = icon)

        box = layout.box()
        col = box.column(align = True)
        col.prop(self, "preset")
        preset = presets[self.preset]
        col.label("Axiom: " + preset.axiom)
        for i, rule in enumerate(preset.rules):
            col.label("Rule {}: {}".format(i, rule))
        col.label("Angle: " + str(preset.angle))

    def execute(self, axiom, rules, generations, stepSize, angle, seed, scaleWidth, scaleStepSize, gravity, randomAngle, onlyPartialMoves):
        defaults = {
            "Step Size" : stepSize,
            "Angle" : angle,
            "Random Angle" : randomAngle,
            "Scale Step Size" : scaleStepSize,
            "Gravity" : gravity,
            "Scale Width" : scaleWidth
        }

        rules = [rule.strip() for rule in rules if len(rule.strip()) > 0]
        limit = self.symbolLimit if self.useSymbolLimit else None

        try:
            vertices, edges, widths, finalSymbols, statesJ, statesK, statesM = calculateLSystem(
                axiom, rules, generations, seed, defaults,
                onlyPartialMoves, limit
            )
        except Exception as e:
            self.raiseErrorMessage(str(e))

        mesh = Mesh(vertices, edges, skipValidation = True)
        widths = DoubleList.fromValues(widths)
        symbols = finalSymbols.toString() if self.outputs["Symbols"].isLinked else ""

        return mesh, widths, symbols, statesJ, statesK, statesM


class LSystemPreset:
    def __init__(self, axiom, rules, angle, generations):
        self.axiom = axiom
        self.rules = rules
        self.angle = angle
        self.generations = generations

presets = {
    "Koch Snowflake": LSystemPreset(
        axiom = "F--F--F",
        rules = ["F = F+F--F+F"],
        angle = 60,
        generations = 2
    ),
    "Hilbert Curve" : LSystemPreset(
        axiom = "A",
        rules = ["A = -BF+AFA+FB-", "B = +AF-BFB-FA+"],
        angle = 90,
        generations = 4
    ),
    "Tree" : LSystemPreset(
        axiom = "FFFA",
        rules = ["A = \"! [&FFFA] //// [&FFFA] //// [&FFFA]"],
        angle = 3,
        generations = 4
    ),
    "Cracy Cubes" : LSystemPreset(
        axiom = "A",
        rules = ["A = F[+FA][-^FA]"],
        angle = 90,
        generations = 4
    ),
    "Twin Dragon" : LSystemPreset(
        axiom = "FX+FX+",
        rules = ["X = X+YF", "Y = FX-Y"],
        angle = 90,
        generations = 9
    ),
    "Dragon Curve" : LSystemPreset(
        axiom = "F1",
        rules = ["F1 = F1+F2+", "F2 = -F1-F2"],
        angle = 90,
        generations = 9
    )

}