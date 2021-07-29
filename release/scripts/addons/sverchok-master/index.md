> ### This file is parsed by menu.py
>
> The following strict rules apply to editing this file:
>
> - do not use tabs, anywhere
> - indent the Node's line using 4 spaces
> - use `>` to add a comment, place it at the start of the line.
> - if you aren't sure, follow the existing convention
>
> Failing to follow these points will break the node category parser.

## Generator
    SvLineNodeMK2
    SvPlaneNodeMK2
    SvNGonNode
    SvBoxNode
    SvCircleNode
    CylinderNode
    SphereNode
    SvIcosphereNode
    SvTorusNode
    SvSuzanneNode
    ---
    BasicSplineNode
    svBasicArcNode
    RandomVectorNodeMK2
    SvScriptNodeLite
    ImageNode

## Generators Extended
    SvBoxRoundedNode
    SvBricksNode
    HilbertNode
    Hilbert3dNode
    HilbertImageNode
    SvProfileNodeMK2
    SvMeshEvalNode
    SvGenerativeArtNode
    SvImageComponentsNode
    SvScriptNode
    SvTorusKnotNode
    SvHexaGridNodeMK1
    SvRingNode

## Analyzers
    SvBBoxNode
    SvVolumeNode
    AreaNode
    DistancePPNode
    CentersPolsNodeMK2
    CentersPolsNodeMK3
    GetNormalsNode
    VectorNormalNode
    SvKDTreeNodeMK2
    SvKDTreeEdgesNodeMK2
    SvBvhOverlapNodeNew
    SvMeshFilterNode
    SvEdgeAnglesNode
    SvMeshSelectNode
    SvSelectSimilarNode
    SvPointInside
    SvProportionalEditNode
    SvRaycasterLiteNode

## Transforms
    SvRotationNode
    SvScaleNodeMK2
    SvMoveNodeMK2
    SvMirrorNode
    MatrixApplyNode
    SvSimpleDeformNode

## Modifier Change
    SvDeleteLooseNode
    SvRemoveDoublesNode
    SvSeparateMeshNode
    SvLimitedDissolve
    ---
    PolygonBoomNode
    Pols2EdgsNode
    SvMeshJoinNode
    ---
    SvBevelNode
    SvSubdivideNode
    SvSmoothNode
    SvIntersectEdgesNodeMK2
    SvOffsetNode
    SvFillsHoleNode
    SvTriangulateNode
    ---
    SvFlipNormalsNode
    SvRecalcNormalsNode
    SvRandomizeVerticesNode
    ---
    SvIterateNode
    SvExtrudeEdgesNode
    SvExtrudeSeparateNode
    SvExtrudeRegionNode
    SvBendAlongPathNode
    SvBendAlongSurfaceNode
    SvVertMaskNode
    SvTransformSelectNode
    SvSplitEdgesNode

## Modifier Make
    LineConnectNodeMK2
    SvLatheNode
    SvConvexHullNode
    SvConvexHullNodeMK2
    DelaunayTriangulation2DNode
    Voronoi2DNode
    SvWafelNode
    CrossSectionNode
    SvBisectNode
    ---
    SvAdaptiveEdgeNode
    AdaptivePolsNode
    SvDuplicateAlongEdgeNode
    SvSolidifyNode
    SvWireframeNode
    SvPipeNode
    SvMatrixTubeNode

## List Masks
    MaskListNode
    SvMaskJoinNode
    SvMaskConvertNode

## List Mutators
    SvListModifierNode

## List Main
    ListJoinNode
    ZipNode
    ListLevelsNode
    ListLengthNode
    ListSumNodeMK2
    ListMatchNode
    ListFuncNode
    SvListDecomposeNode

## List Struct
    ShiftNodeMK2
    ListRepeaterNode
    ListSliceNode
    SvListSplitNode
    ListFLNode
    ListItem2Node
    ListReverseNode
    ListShuffleNode
    ListSortNodeMK2
    ListFlipNode

## Number
    SvNumberNode
    FloatNode
    IntegerNode
    Float2IntNode
    ScalarMathNode
    SvScalarMathNodeMK2
    Formula2Node
    SvExecNodeMod
    ---
    GenListRangeIntNode
    SvGenFloatRange
    SvMapRangeNode
    SvListInputNode
    SvGenFibonacci
    SvGenExponential
    ---
    SvRndNumGen
    RandomNode
    SvEasingNode
    SvMixNumbersNode

## Vector
    GenVectorsNode
    VectorsOutNode
    SvAxisInputNodeMK2
    SvVectorMathNodeMK2
    VertsDelDoublesNode
    SvVectorRewire
    ---
    SvVertSortNode
    VectorDropNode
    VectorPolarInNode
    VectorPolarOutNode
    SvAttractorNode
    ---
    EvaluateLineNode
    SvVectorLerp
    SvInterpolationStripesNode
    SvInterpolationNode
    SvInterpolationNodeMK2
    SvInterpolationNodeMK3
    ---
    SvHomogenousVectorField
    SvNoiseNodeMK2
    SvVectorFractal
    SvLacunarityNode
    SvTurbulenceNode

## Matrix
    SvMatrixGenNodeMK2
    MatrixOutNode
    MatrixDeformNode
    SvMatrixValueIn
    SvMatrixEulerNode
    MatrixShearNode
    MatrixInterpolationNode
    SvMatrixApplyJoinNode

## Logic
    SvLogicNode
    SvSwitchNode
    SvNeuroElman1LNode

## Viz
    ViewerNode2
    SvBmeshViewerNodeMK2
    IndexViewerNode
    SvMetaballOutNode
    SvTextureViewerNode
    Sv3DviewPropsNode

## Text
    ViewerNodeTextMK3
    SvTextInNodeMK2
    SvTextOutNodeMK2
    NoteNode
    SvDataShapeNode
    GTextNode
    SvDebugPrintNode
    SvStethoscopeNodeMK2

## BPY Data
    SvGetPropNode
    SvSetPropNode
    SvObjRemoteNodeMK2
    SvNodeRemoteNode
    SvGetAssetProperties
    SvSetDataObjectNodeMK2
    SvSortObjsNode
    SvFilterObjsNode
    SvObjectToMeshNodeMK2
    SvPointOnMeshNodeMK2
    SvOBJRayCastNodeMK2
    SvSCNRayCastNodeMK2

## Scene
    SvObjectsNodeMK3
    SvObjInLite
    SvObjEdit
    SvFrameInfoNodeMK2
    SvEmptyOutNode
    SvInstancerNode
    SvDupliInstancesMK4

## Objects
    SvVertexGroupNodeMK2
    SvVertexColorNodeMK3

## Layout
    WifiInNode
    WifiOutNode
    NodeReroute
    ConverterNode

## Network
    UdpClientNode

## Beta Nodes
    SvFormulaShapeNode
    SvHeavyTriangulateNode
    SvFormulaDeformNode
    ---
    SvColorsInNodeMK1
    SvColorsOutNodeMK1
    ---
    SvMatrixNormalNode
    SvMatrixTrackToNode
    SvMatrixMathNode
    ---
    SvSculptMaskNode
    SvGreasePencilStrokes
    SvTextureViewerNodeLite
    ---
    SvSpiralNode

## Alpha Nodes
    SvCurveViewerNode
    SvCurveViewerNodeAlt
    SvPolylineViewerNodeMK1
    SvTypeViewerNode
    SvSkinViewerNodeMK1b
    SvMatrixViewer
    ---
    SvBMVertsNode
    SvBMinputNode
    SvBMoutputNode
    SvBMtoElementNode
    SvBMOpsNodeMK2
    ---
    SvInsetSpecial
    SvCSGBooleanNodeMK2
    SvNumpyArrayNode
    SvParticlesNode
    SvParticlesMK2Node
    SvJoinTrianglesNode
    SvListSliceLiteNode
    SvCacheNode
    SvUVtextureNode
    SvSeparateMeshNodeMK2
    SvIndexToMaskNode
    SvMultiExtrudeAlt
    SvOffsetLineNode
    SvContourNode
