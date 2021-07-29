import bpy
from bpy.props import *
from ... utils.layout import writeText
from ... events import executionCodeChanged
from ... base_types import VectorizedNode

class TextObjectOutputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TextObjectOutputNode"
    bl_label = "Text Object Output"
    bl_width_default = 170
    autoVectorizeExecution = True

    errorMessage = StringProperty()

    for attr in ["Object", "Text", "Size", "Extrude", "Shear", "BevelDepth",
                 "BevelResolution", "LetterSpacing", "WordSpacing", "LineSpacing",
                 "XOffset", "YOffset", "HorizontalAlign", "VerticalAlign", "Font",
                 "BoldFont", "ItalicFont", "BoldItalicFont"]:
        exec("use{}List = VectorizedNode.newVectorizeProperty()".format(attr), globals(), locals())

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        toProp = lambda prop: (prop, ["useObjectList"])

        self.newVectorizedInput("Text", toProp("useTextList"),
            ("Text", "text"), ("Texts", "texts"))
        self.newVectorizedInput("Float", toProp("useSizeList"),
            ("Size", "size", dict(value = 1)),
            ("Sizes", "sizes"))
        self.newVectorizedInput("Float", toProp("useExtrudeList"),
            ("Extrude", "extrude"), ("Extrudes", "extrudes"))
        self.newVectorizedInput("Float", toProp("useShearList"),
            ("Shear", "shear"), ("Shears", "shears"))
        self.newVectorizedInput("Float", toProp("useBevelDepthList"),
            ("Bevel Depth", "bevelDepth"),
            ("Bevel Depths", "bevelDepths"))
        self.newVectorizedInput("Integer", toProp("useBevelResolutionList"),
            ("Bevel Resolution", "bevelResolution"),
            ("Bevel Resolutions", "bevelResolutions"))

        self.newVectorizedInput("Float", toProp("useLetterSpacingList"),
            ("Letter Spacing", "letterSpacing", dict(value = 1)),
            ("Letter Spacings", "letterSpacings"))
        self.newVectorizedInput("Float", toProp("useWordSpacingList"),
            ("Word Spacing", "wordSpacing", dict(value = 1)),
            ("Word Spacings", "wordSpacings"))
        self.newVectorizedInput("Float", toProp("useLineSpacingList"),
            ("Line Spacing", "lineSpacing", dict(value = 1)),
            ("Line Spacings", "lineSpacings"))

        self.newVectorizedInput("Float", toProp("useXOffsetList"),
            ("X Offset", "xOffset"), ("X Offsets", "xOffsets"))
        self.newVectorizedInput("Float", toProp("useYOffsetList"),
            ("Y Offset", "yOffset"), ("Y Offsets", "yOffsets"))
        self.newVectorizedInput("Text", toProp("useHorizontalAlignList"),
            ("Horizontal Align", "horizontalAlign", dict(value = "CENTER")),
            ("Horizontal Aligns", "horizontalAligns"))
        self.newVectorizedInput("Text", toProp("useVerticalAlignList"),
            ("Vertical Align", "verticalAlign", dict(value = "CENTER")),
            ("Vertical Aligns", "verticalAligns"))

        self.newVectorizedInput("Font", toProp("useFontList"),
            ("Font", "font"), ("Fonts", "fonts"))
        self.newVectorizedInput("Font", toProp("useBoldFontList"),
            ("Bold Font", "boldFont"), ("Bold Fonts", "boldFonts"))
        self.newVectorizedInput("Font", toProp("useItalicFontList"),
            ("Italic Font", "italicFont"), ("Italic Fonts", "italicFonts"))
        self.newVectorizedInput("Font", toProp("useBoldItalicFontList"),
            ("Bold Italic Font", "boldItalicFont"), ("Bold Italic Fonts", "boldItalicFonts"))

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
        for socket in self.inputs[4:]:
            socket.hide = True

        self.newVectorizedOutput("Object", "useObjectList",
            ("Object", "object"), ("Objects", "objects"))

    def draw(self, layout):
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, width = 25, icon = "ERROR")

    def drawAdvanced(self, layout):
        writeText(layout, "'Horizontal Align' in [LEFT, CENTER, RIGHT, JUSTIFY, FLUSH]", autoWidth = True)
        writeText(layout, "'Vertical Align' in ['TOP_BASELINE', 'TOP', 'CENTER', 'BOTTOM']", autoWidth = True)

    def getExecutionCode(self):
        yield "self.errorMessage = ''"
        yield "if getattr(object, 'type', '') == 'FONT':"
        yield "    textObject = object.data"

        s = self.inputs
        if s[1].isUsed: yield "    if str(textObject.body) != text: textObject.body = text"
        if s[2].isUsed: yield "    textObject.size = size"
        if s[3].isUsed: yield "    textObject.extrude = extrude"
        if s[4].isUsed: yield "    textObject.shear = shear"
        if s[5].isUsed: yield "    textObject.bevel_depth = bevelDepth"
        if s[6].isUsed: yield "    textObject.bevel_resolution = bevelResolution"

        if s[7].isUsed: yield "    textObject.space_character = letterSpacing"
        if s[8].isUsed: yield "    textObject.space_word = wordSpacing"
        if s[9].isUsed: yield "    textObject.space_line = lineSpacing"

        if s[10].isUsed: yield "    textObject.offset_x = xOffset"
        if s[11].isUsed: yield "    textObject.offset_y = yOffset"
        if s[12].isUsed: yield "    self.setAlignmentX(textObject, horizontalAlign)"
        if s[13].isUsed: yield "    self.setAlignmentY(textObject, verticalAlign)"

        if s[14].isUsed: yield "    textObject.font = font"
        if s[15].isUsed: yield "    textObject.font_bold = boldFont"
        if s[16].isUsed: yield "    textObject.font_italic = italicFont"
        if s[17].isUsed: yield "    textObject.font_bold_italic = boldItalicFont"

    def setAlignmentX(self, textObject, align):
        if align in ("LEFT", "CENTER", "RIGHT", "JUSTIFY", "FLUSH"):
            textObject.align_x = align
        else:
            self.errorMessage = "Invalid align type. More info in the advanced panel"

    def setAlignmentY(self, textObject, align):
        if align in ("TOP_BASELINE", "TOP", "CENTER", "BOTTOM"):
            textObject.align_y = align
        else:
            self.errorMessage = "Invalid align type. More info in the advanced panel"

    def getBakeCode(self):
        yield "if getattr(object, 'type', '') == 'FONT':"
        yield "    textObject = object.data"

        s = self.inputs
        if s["Size"].isUsed:                yield "    textObject.keyframe_insert('size')"
        if s["Extrude"].isUsed:             yield "    textObject.keyframe_insert('extrude')"
        if s["Shear"].isUsed:               yield "    textObject.keyframe_insert('shear')"
        if s["Bevel Depth"].isUsed:         yield "    textObject.keyframe_insert('bevel_depth')"
        if s["Bevel Resolution"].isUsed:    yield "    textObject.keyframe_insert('bevel_resolution')"

        if s["Letter Spacing"].isUsed:      yield "    textObject.keyframe_insert('space_character')"
        if s["Word Spacing"].isUsed:        yield "    textObject.keyframe_insert('space_word')"
        if s["Line Spacing"].isUsed:        yield "    textObject.keyframe_insert('space_line')"

        if s["X Offset"].isUsed:            yield "    textObject.keyframe_insert('offset_x')"
        if s["Y Offset"].isUsed:            yield "    textObject.keyframe_insert('offset_y')"
        if s["Align"].isUsed:               yield "    textObject.keyframe_insert('align')"
