/*
    viewer.cpp: Contains the graphical user interface of Instant Meshes

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <half.hpp>
#include "viewer.h"
#include "dedge.h"
#include "meshio.h"
#include "normal.h"
#include "extract.h"
#include "subdivide.h"
#include "reorder.h"
#include "smoothcurve.h"
#include "gui_serializer.h"
#include <resources.h>
#include <pcg32.h>
#include <fstream>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <sys/wait.h>
#endif

Viewer::Viewer(bool fullscreen, bool deterministic)
    : Screen(Vector2i(1280, 960), "Instant Meshes", true, fullscreen),
      mOptimizer(mRes, true), mBVH(nullptr) {
    resizeEvent(mSize);
    mCreaseAngle = -1;
    mDeterministic = deterministic;

    mBaseColor = Vector3f(0.4f, 0.5f, 0.7f);
    mEdgeFactor0 = mEdgeFactor1 = mEdgeFactor2 = Vector3f::Constant(1.0f);
    mEdgeFactor0[0] = mEdgeFactor1[2] = 0.5f;
    mInteriorFactor = Vector3f::Constant(0.5f);
    mSpecularColor = Vector3f::Constant(1.0f);

    /* Some drivers don't support packed half precision storage for 3D vectors
       (e.g. AMD..) -- be extra careful */
    mUseHalfFloats = false;
    if (strstr((const char *) glGetString(GL_VENDOR), "NVIDIA") != nullptr)
        mUseHalfFloats = true;

    Timer<> timer;
    cout << "Compiling shaders .. ";
    cout.flush();

    /* Initialize shaders for rendering geometry and fields */
    mMeshShader63.define("ROSY", "6");
    mMeshShader63.define("POSY", "3");
    mMeshShader63.init("mesh_shader_63",
        (const char *)shader_mesh_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_mesh_geo);

    mMeshShader24.define("ROSY", "2");
    mMeshShader24.define("POSY", "4");
    mMeshShader24.init("mesh_shader_24",
        (const char *)shader_mesh_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_mesh_geo);

    mMeshShader44.define("ROSY", "4");
    mMeshShader44.define("POSY", "4");
    mMeshShader44.init("mesh_shader_44",
        (const char *)shader_mesh_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_mesh_geo);

    mPointShader63.define("ROSY", "6");
    mPointShader63.define("POSY", "3");
    mPointShader63.define("POINT_MODE", "1");
    mPointShader63.init("point_shader_63",
        (const char *)shader_point_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_point_geo);

    mPointShader24.define("ROSY", "2");
    mPointShader24.define("POSY", "4");
    mPointShader24.define("POINT_MODE", "1");
    mPointShader24.init("point_shader_24",
        (const char *)shader_point_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_point_geo);

    mPointShader44.define("ROSY", "4");
    mPointShader44.define("POSY", "4");
    mPointShader44.define("POINT_MODE", "1");
    mPointShader44.init("point_shader_44",
        (const char *)shader_point_vert,
        (const char *)shader_mesh_frag,
        (const char *)shader_point_geo);

    mOrientationFieldShader.init("orientation_field_shader",
        (const char *)shader_orientation_field_vert,
        (const char *)shader_orientation_field_frag,
        (const char *)shader_orientation_field_geo);

    mPositionFieldShader.init("position_field_shader",
        (const char *)shader_position_field_vert,
        (const char *)shader_position_field_frag);

    mPositionSingularityShader.init("position_singularity_shader",
        (const char *)shader_singularity_vert,
        (const char *)shader_singularity_frag,
        (const char *)shader_singularity_geo);

    mOrientationSingularityShader.init("orientation_singularity_shader",
        (const char *)shader_singularity_vert,
        (const char *)shader_singularity_frag,
        (const char *)shader_singularity_geo);

    mFlowLineShader.init("flowline_shader",
        (const char *)shader_flowline_vert,
        (const char *)shader_flowline_frag);

    mStrokeShader.init("stroke_shader",
        (const char *)shader_flowline_vert,
        (const char *)shader_flowline_frag);

    mOutputMeshShader.init("output_mesh_shader",
        (const char *)shader_quadmesh_vert,
        (const char *)shader_quadmesh_frag);

    mOutputMeshWireframeShader.init("output_mesh_wireframe_shader",
        (const char *)shader_lines_vert,
        (const char *)shader_lines_frag);

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;

    auto ctx = nvgContext();
    /* Scan over example files in the 'datasets' directory */
    try {
        mExampleImages = nanogui::loadImageDirectory(ctx, "datasets");
    } catch (const std::runtime_error &e) {
        cout << "Unable to load image data: " << e.what() << endl;
    }
    mExampleImages.insert(mExampleImages.begin(),
                          std::make_pair(nvgImageIcon(ctx, loadmesh), ""));

    /* Initialize user interface */
    Window *window = new Window(this, "Instant Meshes");
    window->setPosition(Vector2i(15, 15));
    window->setLayout(new GroupLayout());
    window->setId("viewer");

    mProgressWindow = new Window(this, "Please wait");
    mProgressLabel = new Label(mProgressWindow, " ");
    mProgressWindow->setLayout(new BoxLayout(Orientation::Vertical, Alignment::Minimum, 15, 15));
    mProgressBar = new ProgressBar(mProgressWindow);
    mProgressBar->setFixedWidth(250);
    mProgressWindow->setVisible(false);

    PopupButton *openBtn = new PopupButton(window, "Open mesh");
    openBtn->setBackgroundColor(Color(0, 255, 0, 25));
    openBtn->setIcon(ENTYPO_ICON_FOLDER);
    Popup *popup = openBtn->popup();
    VScrollPanel *vscroll = new VScrollPanel(popup);
    ImagePanel *panel = new ImagePanel(vscroll);
    panel->setImages(mExampleImages);
    panel->setCallback([&, openBtn](int i) {
        openBtn->setPushed(false);
        loadInput(mExampleImages[i].second);
    });

    PopupButton *advancedBtn = new PopupButton(window, "Advanced");
    advancedBtn->setIcon(ENTYPO_ICON_ROCKET);
    advancedBtn->setBackgroundColor(Color(100, 0, 0, 25));
    Popup *advancedPopup = advancedBtn->popup();
    advancedPopup->setAnchorHeight(61);

    advancedPopup->setLayout(new GroupLayout());
    new Label(advancedPopup, "Current state", "sans-bold");
    Widget *statePanel = new Widget(advancedPopup);
    statePanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));

    Button *resetBtn = new Button(statePanel, "Reset", ENTYPO_ICON_SQUARED_CROSS);
    resetBtn->setCallback([&] { resetState(); });

    Button *loadStateBtn = new Button(statePanel, "Load", ENTYPO_ICON_UPLOAD);
    loadStateBtn->setCallback([&] { loadState(""); });

    Button *saveStateBtn = new Button(statePanel, "Save", ENTYPO_ICON_DOWNLOAD);
    saveStateBtn->setCallback([&] { saveState(""); });

    new Label(advancedPopup, "Visualize", "sans-bold");
    mVisualizeBox = new ComboBox(advancedPopup,
        { "Disabled", "Parameterization", "Hierarchy", "Creases",  "Boundaries", "Non-manifold vertices" });

    mVisualizeBox->setIcon(ENTYPO_ICON_AREA_GRAPH);
    mVisualizeBox->setCallback([&](int index){ refreshColors(); });
    mVisualizeBox->setId("visualizeBox");

    new Label(advancedPopup, "Hierarchy level", "sans-bold");
    Widget *hierarchyPanel = new Widget(advancedPopup);
    hierarchyPanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Minimum, 0, 10));

    mHierarchyMinusButton = new Button(hierarchyPanel, "", ENTYPO_ICON_MINUS);
    mHierarchyMinusButton->setCallback([&]() { setLevel(std::max(-1, mSelectedLevel - 1)); });
    mHierarchyMinusButton->setId("hierarchyMinusButton");

    mHierarchyLevelBox = new TextBox(hierarchyPanel);
    mHierarchyLevelBox->setFixedSize(Vector2i(145, 29));
    mHierarchyLevelBox->setId("hierarchyLevelBox");
    mHierarchyPlusButton = new Button(hierarchyPanel, "", ENTYPO_ICON_PLUS);
    mHierarchyPlusButton->setCallback([&]() { setLevel(std::min(mRes.levels() - 1, mSelectedLevel + 1)); });
    mHierarchyPlusButton->setId("hierarchyPlusButton");

    new Label(advancedPopup, "Crease angle", "sans-bold");

    Widget *creasePanel = new Widget(advancedPopup);
    creasePanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 20));
    mCreaseAngleSlider = new Slider(creasePanel);
    mCreaseAngleSlider->setFixedWidth(160);
    mCreaseAngleSlider->setId("creaseAngleSlider");
    mCreaseAngleBox = new TextBox(creasePanel);
    mCreaseAngleBox->setFixedSize(Vector2i(50, 25));
    mCreaseAngleBox->setId("creaseAngleBox");
    mCreaseAngleSlider->setCallback([&](Float value) {
        mCreaseAngleBox->setValue(std::to_string((int) (value * 90)));
    });
    mCreaseAngleSlider->setFinalCallback([&](Float value) {
        setCreaseAnglePrompt(true, value*90);
    });

    auto layerCB = [&] (bool) {
        repaint();
        mOrientationFieldSizeSlider->setEnabled(mLayers[OrientationField]->checked());
        mOrientationFieldSingSizeSlider->setEnabled(mLayers[OrientationFieldSingularities]->checked());
        mPositionFieldSingSizeSlider->setEnabled(mLayers[PositionFieldSingularities]->checked());
        mFlowLineSlider->setEnabled(mLayers[FlowLines]->checked());
    };
    new Label(advancedPopup, "Render layers", "sans-bold");

    mLayers[InputMesh] = new CheckBox(advancedPopup, "Input mesh", layerCB);
    mLayers[InputMeshWireframe] = new CheckBox(advancedPopup, "Input mesh wireframe", layerCB);
    mLayers[FaceLabels] = new CheckBox(advancedPopup, "Face IDs", layerCB);
    mLayers[VertexLabels] = new CheckBox(advancedPopup, "Vertex IDs", layerCB);

    Widget *flowLinePanel = new Widget(advancedPopup);
    flowLinePanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 22));

    mLayers[FlowLines] = new CheckBox(flowLinePanel, "Orientation field (flow lines)", layerCB);

    mFlowLineSlider = new Slider(flowLinePanel);
    mFlowLineSlider->setFixedWidth(30);
    mFlowLineSlider->setId("flowLineSlider");
    mFlowLineSlider->setTooltip("Controls the number of flow lines");
    mFlowLineSlider->setFinalCallback([&](Float value) { traceFlowLines(); });

    Widget *orientFieldPanel = new Widget(advancedPopup);
    orientFieldPanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 34));

    mLayers[OrientationField] = new CheckBox(orientFieldPanel, "Orientation field (n-RoSy)", layerCB);

    mOrientationFieldSizeSlider = new Slider(orientFieldPanel);
    mOrientationFieldSizeSlider->setFixedWidth(30);
    mOrientationFieldSizeSlider->setId("orientFieldSize");
    mOrientationFieldSizeSlider->setTooltip("Controls the scale of the orientation field visualization");
    mOrientationFieldSizeSlider->setCallback([&](Float value) { repaint(); });

    Widget *orientFieldSingPanel = new Widget(advancedPopup);
    orientFieldSingPanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 15));
    mLayers[OrientationFieldSingularities] = new CheckBox(orientFieldSingPanel, "Orientation field singularities", layerCB);

    mOrientationFieldSingSizeSlider = new Slider(orientFieldSingPanel);
    mOrientationFieldSingSizeSlider->setFixedWidth(30);
    mOrientationFieldSingSizeSlider->setId("orientFieldSingSize");
    mOrientationFieldSingSizeSlider->setTooltip("Controls the scale of the orientation field singularity visualization");
    mOrientationFieldSingSizeSlider->setCallback([&](Float value) { repaint(); });

    mLayers[PositionField] = new CheckBox(advancedPopup, "Position field", layerCB);

    Widget *posFieldSingPanel = new Widget(advancedPopup);
    posFieldSingPanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 30));
    mLayers[PositionFieldSingularities] = new CheckBox(posFieldSingPanel, "Position field singularities", layerCB);
    mPositionFieldSingSizeSlider = new Slider(posFieldSingPanel);
    mPositionFieldSingSizeSlider->setFixedWidth(30);
    mPositionFieldSingSizeSlider->setId("posFieldSingSize");
    mPositionFieldSingSizeSlider->setTooltip("Controls the scale of the position field singularity visualization");
    mPositionFieldSingSizeSlider->setCallback([&](Float value) { repaint(); });

    mLayers[BrushStrokes] = new CheckBox(advancedPopup, "Brush strokes", layerCB);

    mLayers[OutputMesh] = new CheckBox(advancedPopup, "Output mesh", layerCB);
    mLayers[OutputMeshWireframe] = new CheckBox(advancedPopup, "Output mesh wireframe", layerCB);
    for (int i=0; i<LayerCount; ++i)
        mLayers[i]->setId("layer_" + std::to_string(i));

    new Label(window, "Remesh as", "sans-bold");
    mSymmetryBox = new ComboBox(window,
        { "Triangles (6-RoSy, 6-PoSy)",
        "Quads (2-RoSy, 4-PoSy)",
        "Quads (4-RoSy, 4-PoSy)"},
        { "Triangles", "Quads (2/4)", "Quads (4/4)"}
    );
    mSymmetryBox->setFixedHeight(25);
    mSymmetryBox->setId("symmetryBox");

    mSymmetryBox->setCallback([&](int index) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        mOptimizer.stop();
        if (index == 0) {
            mOptimizer.setRoSy(6);
            mOptimizer.setPoSy(3); // TODO this should be referred to as 6 consistently (just cosmetic change)
        } else if (index == 1) {
            mOptimizer.setRoSy(2);
            mOptimizer.setPoSy(4);
        } else if (index == 2) {
            mOptimizer.setRoSy(4);
            mOptimizer.setPoSy(4);
        }
        mPureQuadBox->setEnabled(mOptimizer.posy() == 4);
        mPureQuadBox->setChecked(false);
        mSolvePositionBtn->setEnabled(false);
        mExportBtn->setEnabled(false);
        mOrientationScareBrush->setEnabled(false);
        mOrientationAttractor->setEnabled(false);
        mVisualizeBox->setSelectedIndex(0);
        mRes.resetSolution();
        repaint();
    });

    new Label(window, "Configuration details", "sans-bold");

    mExtrinsicBox = new CheckBox(window, "Extrinsic");
    mExtrinsicBox->setId("extrinsic");
    mExtrinsicBox->setCallback([&](bool value) {
        mOptimizer.setExtrinsic(value);
        mOrientationSingularityShader.resetAttribVersion("position");
        mPositionSingularityShader.resetAttribVersion("position");
    });
    mExtrinsicBox->setTooltip("Use an extrinsic smoothness energy with "
                              "automatic parameter-free alignment to geometric "
                              "features");

    mAlignToBoundariesBox = new CheckBox(window, "Align to boundaries");
    mAlignToBoundariesBox->setId("alignToBoundaries");
    mAlignToBoundariesBox->setTooltip(
        "When the mesh is not closed, ensure "
        "that boundaries of the output mesh follow those of "
        "the input mesh");
    mAlignToBoundariesBox->setCallback([&](bool) { refreshStrokes(); });

    mCreaseBox = new CheckBox(window, "Sharp creases");
    mCreaseBox->setTooltip("Don't smooth discontinuous surface "
                           "normals in CAD models and similar input data.");
    mCreaseBox->setCallback([&](bool value) {
        setCreaseAnglePrompt(value, -1);
    });

    mCreaseBox->setId("creaseBox");
    new Label(window, "Target vertex count", "sans-bold");
    Widget *densityPanel = new Widget(window);
    densityPanel->setLayout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));

    mScaleSlider = new Slider(densityPanel);
    mScaleSlider->setValue(0.5f);
    mScaleSlider->setId("scaleSlider");
    mScaleSlider->setFixedWidth(60);

    mScaleBox = new TextBox(densityPanel);
    mScaleBox->setFixedSize(Vector2i(80, 25));
    mScaleBox->setValue("0");
    mScaleBox->setId("scaleBox");
    mScaleSlider->setCallback([&](Float value) {
        Float min = std::log(std::min(100, (int) mRes.V().cols() / 10));
        Float max = std::log(2*mRes.V().cols());
        uint32_t v = (uint32_t) std::exp((1-value) * min + value * max);
        char tmp[10];
        if (v > 1e6f) {
            mScaleBox->setUnits("M");
            snprintf(tmp, sizeof(tmp), "%.2f", v*1e-6f);
        } else if (v > 1e3f) {
            mScaleBox->setUnits("K");
            snprintf(tmp, sizeof(tmp), "%.2f", v*1e-3f);
        } else {
            mScaleBox->setUnits(" ");
            snprintf(tmp, sizeof(tmp), "%i", v);
        }
        mScaleBox->setValue(tmp);
    });

    mScaleSlider->setFinalCallback([&](Float value) {
        Float min = std::log(std::min(100, (int) mRes.V().cols() / 10));
        Float max = std::log(2*mRes.V().cols());
        uint32_t v = (uint32_t) std::exp((1-value) * min + value * max);
        setTargetVertexCountPrompt(v);
    });

    new Label(window, "Orientation field", "sans-bold");
    Widget *orientTools = new Widget(window);
    new Label(orientTools, "Tool:        ", "sans-bold");
    mOrientationComb = new ToolButton(orientTools, nvgImageIcon(ctx, comb));
    mOrientationComb->setTooltip("Orientation comb: make local adjustments to the orientation field");
    mOrientationComb->setId("orientationComb");

    //mOrientationComb->setCallback([&]() {
        //new MessageDialog(
            //this, MessageDialog::Type::Warning, "Discard singularity modifications?",
            //" New comb and contour strokes cannot be added after using the singularity attractor and scare brush."
            //" If you continue, singularity adjustments will be discarded in favor of new stroke annotations.",
            //"Continue", "Cancel", true);
    //});

    mOrientationAttractor = new ToolButton(orientTools, ENTYPO_ICON_MAGNET);
    mOrientationAttractor->setTooltip(
            "Singularity Attractor: move/create/cancel orientation singularities");
    mOrientationAttractor->setId("orientationAttractor");
    mOrientationAttractor->setCallback([&] { repaint(); } );
    mOrientationAttractor->setChangeCallback([&](bool value) { if (!value) setLevel(-1); } );

    mOrientationScareBrush = new ToolButton(orientTools, nvgImageIcon(ctx, scare));
    mOrientationScareBrush->setTooltip(
            "Singularity Scaring Brush: expel orientation singularities from a region");
    mOrientationScareBrush->setId("orientationScareBrush");

    orientTools->setLayout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

    Widget *orientSingPanel = new Widget(window);
    orientSingPanel->setLayout(new BoxLayout(Orientation::Horizontal));
    mOrientationSingularityBox = new TextBox(orientSingPanel);
    mOrientationSingularityBox->setFixedSize(Vector2i(73, 25));
    mOrientationSingularityBox->setUnitsImage(nvgImageIcon(ctx, sing_dir));
    mOrientationSingularityBox->setValue("0");
    mOrientationSingularityBox->setId("orientationSingularityBox");

    new Label(orientSingPanel, "  singularities");

    mSolveOrientationBtn = new ProgressButton(window, "Solve", ENTYPO_ICON_FLASH);
    mSolveOrientationBtn->setBackgroundColor(Color(0, 0, 255, 25));
    mSolveOrientationBtn->setFixedHeight(25);
    mSolveOrientationBtn->setFlags(Button::ToggleButton);
    mSolveOrientationBtn->setId("solveOrientationBtn");
    mSolveOrientationBtn->setChangeCallback([&](bool value) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        if (value) {
            bool pointcloud = mRes.F().size() == 0;
            mOptimizer.optimizeOrientations(mSelectedLevel);
            if (mVisualizeBox->selectedIndex() == 1)
                mVisualizeBox->setSelectedIndex(0);
            mSolvePositionBtn->setPushed(false);
            mSolvePositionBtn->setEnabled(true);
            mExportBtn->setEnabled(false);
            //mOrientationScareBrush->setEnabled(!pointcloud);
            mOrientationAttractor->setEnabled(!pointcloud && mOptimizer.rosy() == 4);
            mLayers[InputMesh]->setChecked(true);
            mLayers[FlowLines]->setChecked(!pointcloud);
            mLayers[OutputMesh]->setChecked(false);
            mLayers[OutputMeshWireframe]->setChecked(false);
            mFlowLineSlider->setEnabled(!pointcloud);
        } else {
            mOptimizer.stop();
        }
        mOptimizer.notify();
    });

    new Label(window, "Position field", "sans-bold");

    Widget *uvTools = new Widget(window);
    new Label(uvTools, "Tool:        ", "sans-bold");

    mEdgeBrush = new ToolButton(uvTools, ENTYPO_ICON_BRUSH);
    mEdgeBrush->setId("edgeBrush");
    mEdgeBrush->setTooltip("Edge Brush: specify edges paths of the output mesh");

    mPositionAttractor = new ToolButton(uvTools, ENTYPO_ICON_MAGNET);
    mPositionAttractor->setId("positionAttractor");
    mPositionAttractor->setTooltip("Singularity Attractor: move/create/cancel position singularities");
    mPositionAttractor->setCallback([&] { repaint(); } );
    mPositionAttractor->setChangeCallback([&](bool value) { if (!value) setLevel(-1); } );

    mPositionScareBrush = new ToolButton(uvTools, nvgImageIcon(ctx, scare));
    mPositionScareBrush->setId("positionScareBrush");
    mPositionScareBrush->setTooltip(
            "Singularity Scaring Brush: expel position singularities from a region");

    std::vector<Button *> bgroup;
    bgroup.push_back(mOrientationComb);
    bgroup.push_back(mOrientationAttractor);
    bgroup.push_back(mOrientationScareBrush);
    bgroup.push_back(mEdgeBrush);
    bgroup.push_back(mPositionAttractor);
    bgroup.push_back(mPositionScareBrush);

    for (auto b : bgroup)
        b->setButtonGroup(bgroup);

    uvTools->setLayout(
        new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

    Widget *posSingPanel = new Widget(window);
    posSingPanel->setLayout(new BoxLayout(Orientation::Horizontal));
    mPositionSingularityBox = new TextBox(posSingPanel);
    mPositionSingularityBox->setFixedSize(Vector2i(73, 25));
    mPositionSingularityBox->setUnitsImage(nvgImageIcon(ctx, sing_pos));
    mPositionSingularityBox->setValue("0");
    mPositionSingularityBox->setId("positionSingularityBox");
    new Label(posSingPanel, "  singularities");

    mSolvePositionBtn = new ProgressButton(window, "Solve", ENTYPO_ICON_FLASH);
    mSolvePositionBtn->setBackgroundColor(Color(0, 0, 255, 25));
    mSolvePositionBtn->setFixedHeight(25);
    mSolvePositionBtn->setEnabled(false);
    mSolvePositionBtn->setFlags(Button::ToggleButton);
    mSolvePositionBtn->setId("solvePositionBtn");
    mSolvePositionBtn->setChangeCallback([&](bool value) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        if (value) {
            bool pointcloud = mRes.F().size() == 0;
            mOptimizer.optimizePositions(mSelectedLevel);
            mSolveOrientationBtn->setPushed(false);
            mSolveOrientationBtn->setEnabled(true);
            mPositionAttractor->setEnabled(!pointcloud && mOptimizer.rosy() == 4 && mOptimizer.posy() == 4);
            mExportBtn->setEnabled(true);
            mSaveBtn->setEnabled(false);
            mSwitchBtn->setEnabled(false);
            mVisualizeBox->setSelectedIndex(1);
            mLayers[InputMesh]->setChecked(true);
            mLayers[FlowLines]->setChecked(false);
            mLayers[OutputMesh]->setChecked(false);
            mLayers[OutputMeshWireframe]->setChecked(false);
            mFlowLineSlider->setEnabled(false);
            repaint();
        } else {
            mOptimizer.stop();
        }
        mOptimizer.notify();
    });

    new Label(window, "", "sans-bold");

    mExportBtn = new PopupButton(window, "Export mesh", ENTYPO_ICON_EXPORT);
    mExportBtn->setBackgroundColor(Color(0, 255, 0, 25));
    mExportBtn->setId("exportBtn");
    Popup *exportPopup = mExportBtn->popup();
    exportPopup->setAnchorHeight(307);
    exportPopup->setLayout(new GroupLayout());

    new Label(exportPopup, "Mesh settings", "sans-bold");
    mPureQuadBox = new CheckBox(exportPopup, "Pure quad mesh");
    mPureQuadBox->setTooltip("Apply one step of subdivision to extract a pure quad mesh");
    mPureQuadBox->setId("pureQuadBox");

    new Label(exportPopup, "Smoothing iterations", "sans-bold");
    Widget *smoothPanel = new Widget(exportPopup);
    smoothPanel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 20));
    mSmoothSlider = new Slider(smoothPanel);
    mSmoothSlider->setValue(0.5f);
    mSmoothSlider->setId("mSmoothSlider");
    mSmoothSlider->setFixedWidth(80);

    mSmoothBox = new TextBox(smoothPanel);
    mSmoothBox->setFixedSize(Vector2i(50, 25));
    mSmoothBox->setId("smoothBox");
    std::string smoothTooltip = "To increase the mesh uniformity, Laplacian "
                                "smoothing and reprojection steps can be performed "
                                "as a post process";
    mSmoothBox->setTooltip(smoothTooltip);
    mSmoothSlider->setTooltip(smoothTooltip);

    mSmoothSlider->setCallback([&](Float value) {
        mSmoothBox->setValue(std::to_string((int) (value * 10)));
    });

    new Label(exportPopup, "Actions", "sans-bold");
    Button *generateBtn = new Button(exportPopup, "Extract mesh", ENTYPO_ICON_FLASH);
    generateBtn->setBackgroundColor(Color(0, 0, 255, 25));
    generateBtn->setId("generateMeshBtn");
    generateBtn->setCallback([&]() {
        extractMesh();
        mSaveBtn->setEnabled(true);
        mSwitchBtn->setEnabled(true);
    });

    mSwitchBtn = new Button(exportPopup, "Show output", ENTYPO_ICON_SHUFFLE);
    mSwitchBtn->setTooltip("Switches between input and output mesh views");
    mSwitchBtn->setId("mSwitchBtn");
    mSwitchBtn->setCallback([&]() {
        keyboardEvent(GLFW_KEY_BACKSLASH, 0, GLFW_PRESS, 0);
    });

    mSaveBtn = new Button(exportPopup, "Save ...", ENTYPO_ICON_SAVE);
    mSaveBtn->setBackgroundColor(Color(0, 255, 0, 25));
    mSaveBtn->setId("saveMeshBtn");
    mSaveBtn->setCallback([&]() {
        try {
            std::string filename = nanogui::file_dialog({
                {"obj", "Wavefront OBJ"},
                {"ply", "Stanford PLY"}
            }, true);

            if (filename == "")
                return;
            write_mesh(filename, mF_extracted, mV_extracted, MatrixXf(), mNf_extracted);
        } catch (const std::exception &e) {
            new MessageDialog(this, MessageDialog::Type::Warning, "Error", e.what());
        }
    });

    new Label(exportPopup, "Advanced", "sans-bold");
    Button *consensusGraphBtn = new Button(exportPopup, "Consensus graph", ENTYPO_ICON_FLOW_TREE);
    consensusGraphBtn->setBackgroundColor(Color(100, 0, 0, 25));
    consensusGraphBtn->setId("consensusGraphBtn");
    consensusGraphBtn->setTooltip("Visualize the graph of position integer values");
    consensusGraphBtn->setCallback([&]() {
        extractConsensusGraph();
    });

#ifdef VISUALIZE_ERROR
    mGraph = new Graph(window, "Energy");
#endif
    Button *about = new Button(window->buttonPanel(), "", ENTYPO_ICON_INFO);
    about->setCallback([&,ctx]() {
        auto dlg = new MessageDialog(
            this, MessageDialog::Type::Information, "About Instant Meshes",
            "Instant Meshes is freely available under a BSD-style license. "
            "If you use the meshes obtained with this software, we kindly "
            "request that you acknowledge this and link to the project page at\n\n"
            "\thttp://igl.ethz.ch/projects/instant-meshes/\n\n"
            "In the context of scientific articles or books, please cite paper\n\n"
            "Instant Field-Aligned Meshes\n"
            "Wenzel Jakob, Marco Tarini, Daniele Panozzo, Olga Sorkine-Hornung\n"
            "In ACM Transactions on Graphics (Proceedings of SIGGRAPH Asia 2015)\n");
        dlg->messageLabel()->setFixedWidth(550);
        dlg->messageLabel()->setFontSize(20);
        performLayout(ctx);
        dlg->center();
    });

    performLayout(ctx);

    mProgress = std::bind(&Viewer::showProgress, this, _1, _2);
    mOperationStart = mLastProgressMessage = glfwGetTime();
    resetState();
}

Viewer::~Viewer() {
    if (mBVH)
        delete mBVH;
    mOptimizer.shutdown();
    mRes.free();
    mPointShader24.free();
    mPointShader63.free();
    mPointShader44.free();
    mMeshShader24.free();
    mMeshShader63.free();
    mMeshShader44.free();
    mOrientationFieldShader.free();
    mPositionFieldShader.free();
    mOrientationSingularityShader.free();
    mPositionSingularityShader.free();
    mFlowLineShader.free();
    mStrokeShader.free();
    mOutputMeshWireframeShader.free();
    mOutputMeshShader.free();
    mFBO.free();
}

void Viewer::draw(NVGcontext *ctx) {
    if (mRes.levels() == 0) {
        int appIcon = nvgImageIcon(ctx, instantmeshes);
        int size = mSize.norm()/2;

        NVGpaint imgPaint =
            nvgImagePattern(ctx, (mSize[0] - size) / 2, (mSize[1] - size) / 2,
                            size, size, 0, appIcon, 1.0f);
        nvgBeginPath(ctx);
        nvgRect(ctx, (mSize[0] - size) / 2, (mSize[1] - size) / 2, size, size);
        nvgFillPaint(ctx, imgPaint);
        nvgFill(ctx);
    }

    Screen::draw(ctx);
}

bool Viewer::resizeEvent(const Vector2i &size) {
    if (mFBO.ready())
        mFBO.free();
    int nSamples = 4;

    if (strstr((const char *)glGetString(GL_VENDOR), "Intel") != nullptr) {
        cout << "Detected Intel HD Graphics card, disabling MSAA as a precaution .." << endl;
        nSamples = 1;
    }

    mFBO.init(mFBSize, nSamples);
    mCamera.arcball.setSize(mSize);
    repaint();
    return true;
}

void Viewer::setSymmetry(int rosy, int posy) {
    if (rosy == 6 && posy == 3)
        mSymmetryBox->setSelectedIndex(0);
    else if (rosy == 2 && posy == 4)
        mSymmetryBox->setSelectedIndex(1);
    else if (rosy == 4 && posy == 4)
        mSymmetryBox->setSelectedIndex(2);
    else
        throw std::runtime_error("Selected RoSy/PoSy combination is not supported by the user interface");
    mPureQuadBox->setEnabled(posy == 4);
    mPureQuadBox->setChecked(false);
    mPositionAttractor->setEnabled(false);
    mOrientationAttractor->setEnabled(false);
    mOptimizer.setRoSy(rosy);
    mOptimizer.setPoSy(posy);
}

void Viewer::setExtrinsic(bool extrinsic) {
    mExtrinsicBox->setChecked(extrinsic);
    mOptimizer.setExtrinsic(extrinsic);
}

void Viewer::setLevel(int level) {
    mSelectedLevel = level;
    if (level < 0)
        mHierarchyLevelBox->setValue("Automatic");
    else
        mHierarchyLevelBox->setValue(std::to_string(level));
    if (mVisualizeBox->selectedIndex() == 2 /* Hierarchy */)
        refreshColors();
    std::lock_guard<ordered_lock> lock(mRes.mutex());
    mOptimizer.setLevel(level);
    if (level == -1)
        mOptimizer.stop();
}

void Viewer::setTargetScale(Float scale) {
    if (!mRes.levels())
        return;
    int posy = mOptimizer.posy();
    Float face_area = posy == 4 ? (scale*scale) : (std::sqrt(3.f)/4.f*scale*scale);
    uint32_t face_count = mMeshStats.mSurfaceArea / face_area;
    uint32_t vertex_count = posy == 4 ? face_count : (face_count / 2);
    setTargetVertexCount(vertex_count);
    std::lock_guard<ordered_lock> lock(mRes.mutex());
    mRes.setScale(scale);
}

void Viewer::setTargetVertexCount(uint32_t v) {
    if (!mRes.levels())
        return;
    char tmp[10];

    if (v > 1e6f) {
        mScaleBox->setUnits("M");
        snprintf(tmp, sizeof(tmp), "%.2f", v*1e-6f);
    } else if (v > 1e3f) {
        mScaleBox->setUnits("K");
        snprintf(tmp, sizeof(tmp), "%.2f", v*1e-3f);
    } else {
        mScaleBox->setUnits(" ");
        snprintf(tmp, sizeof(tmp), "%i", v);
    }
    Float value = std::log((Float) v);
    Float min = std::log(std::min(100, (int) mRes.V().cols() / 10));
    Float max = std::log(2*mRes.V().cols());

    mScaleSlider->setValue((value - min) / (max-min));
    mScaleBox->setValue(tmp);

    int posy = mOptimizer.posy();
    int face_count = posy == 4 ? v : (v * 2);
    Float face_area = mMeshStats.mSurfaceArea / face_count;
    Float scale = posy == 4 ? std::sqrt(face_area) : (2*std::sqrt(face_area * std::sqrt(1.f/3.f)));

    std::lock_guard<ordered_lock> lock(mRes.mutex());
    mRes.setScale(scale);
}

void Viewer::setTargetVertexCountPrompt(uint32_t v) {
    Float min = std::log(std::min(100, (int) mRes.V().cols() / 10));
    Float max = std::log(2*mRes.V().cols());
    Float safe = std::exp(min + mScaleSlider->highlightedRange().first * (max-min));
    if (v <= safe || mRes.F().size() == 0) {
        setTargetVertexCount(v);
        return;
    }

    MessageDialog *msg = new MessageDialog(
        this, MessageDialog::Type::Question, "Change target vertex count?",
        "The input has insufficient resolution to generate a mesh with the selected vertex count. "
        "It can be subdivided to a finer resolution, but any current edits will be lost. Proceed?",
        "Yes", "No", true);

    msg->setCallback([&, v, safe](int button) {
        if (button == 0) {
            Arcball savedArcball = mCamera.arcball;
            Float savedZoom = mCamera.zoom;
            Vector3f savedTranslation = mCamera.modelTranslation;
            loadInput(mFilename, mCreaseAngle, -1, -1, v, mOptimizer.rosy(), mOptimizer.posy());
            mCamera.arcball = savedArcball;
            mCamera.zoom = savedZoom;
            mCamera.modelTranslation = savedTranslation;
        } else {
            setTargetVertexCount(safe);
        }
    });
}

void Viewer::setCreaseAnglePrompt(bool enabled, Float value) {
    MessageDialog *msg = new MessageDialog(
        this, MessageDialog::Type::Question, "Change crease settings?",
        "Changes to the crease settings require the mesh to be reloaded. Any "
        "current edits will be lost. Proceed?",
        "Yes", "No", true);

    msg->setCallback([&, value, enabled](int button) {
        if (button == 0) {
            Arcball savedArcball = mCamera.arcball;
            Float savedZoom = mCamera.zoom;
            Vector3f savedTranslation = mCamera.modelTranslation;
            loadInput(
                mFilename, enabled ? (value < 0 ? 20 : value) : (Float) -1.0f,
                mRes.scale(), -1, -1, mOptimizer.rosy(), mOptimizer.posy());
            mCamera.arcball = savedArcball;
            mCamera.zoom = savedZoom;
            mCamera.modelTranslation = savedTranslation;
        } else {
            if (value == -1) {
                mCreaseBox->setChecked(!enabled);
            } else {
                mCreaseAngleSlider->setValue(mCreaseAngle / 90.0f);
                mCreaseAngleBox->setValue(std::to_string((int) mCreaseAngle));
            }
        }
    });
}

Vector3f compat_cross(const Vector3f &q, const Vector3f &ref, const Vector3f &n) {
    Vector3f t = n.cross(q);
    Float dp0 = q.dot(ref), dp1 = t.dot(ref);
    if (std::abs(dp0) > std::abs(dp1))
        return q * signum(dp0);
    else
        return t * signum(dp1);
}

Vector3f compat_uv(const Vector3f &o, const Vector3f &ref, const Vector3f &q, const Vector3f &t, Float scale, Float inv_scale) {
    Vector3f d = ref - o;
    return o +
       q * std::round(q.dot(d) * inv_scale) * scale +
       t * std::round(q.dot(d) * inv_scale) * scale;
}

bool Viewer::keyboardEvent(int key, int scancode, int event, int modifiers) {
    if (event == GLFW_PRESS) {
#if DEV_MODE
        if (key == GLFW_KEY_SPACE && mRes.levels() > 0) {
            if (mSolvePositionBtn->enabled()) {
                mSolvePositionBtn->setPushed(true);
                mSolvePositionBtn->changeCallback()(true);
            } else {
                mSolveOrientationBtn->setPushed(true);
                mSolveOrientationBtn->changeCallback()(true);
            }
        } else if (key == GLFW_KEY_UP) {
            mCamera.viewAngle -= 1;
            cout << "fov = " << mCamera.viewAngle << endl;
            repaint();
            return true;
        } else if (key == GLFW_KEY_DOWN) {
            mCamera.viewAngle += 1;
            cout << "fov = " << mCamera.viewAngle << endl;
            repaint();
            return true;
        } else if (key == '/') {
            const int supersampling = 8;
            mBackground.setConstant(1.f);
            mBaseColor = Vector3f(.312f, .39f, .546f);
            mSpecularColor = Vector3f::Constant(0.5f);
            Vector3f edgeFactor = Vector3f::Constant(0.95f).array() / mBaseColor.array();
            mEdgeFactor0 = mEdgeFactor1 = mEdgeFactor2 = edgeFactor;
            mInteriorFactor.setConstant(1.0f);
            mSize *= supersampling;
            mFBSize *= supersampling;
            mFBO.free();
            mFBO.init(mFBSize, 1);
            glViewport(0, 0, mFBSize[0], mFBSize[1]);
            repaint();
            mSize /= supersampling;
            mFBSize /= supersampling;
            Timer<> timer;
            cout << "Rendering .. ";
            cout.flush();
            while (mNeedsRepaint)
                drawContents();
            cout << "done. (took " << timeString(timer.value()) << ")" << endl;
            mFBO.downloadTGA("screenshot.tga");
            glViewport(0, 0, mFBSize[0], mFBSize[1]);
            resizeEvent(mSize);
        } else if (key == 'M') {
            renderMitsuba();
        } else if (key == 'F') {
            setFloorPosition();
        }
#endif
        if ((key >= '0' && key <= '9') || key == '-' || key == '=') {
            int idx = key - '1';
            if (key == '0')
                idx = 9;
            if (key == '-')
                idx = 10;
            if (key == '=')
                idx = 11;
            if (modifiers == GLFW_MOD_CONTROL) {
                mCameraSnapshots[idx] = mCamera;
                return true;
            } else if (modifiers == GLFW_MOD_SHIFT) {
                mCamera = mCameraSnapshots[idx];
                repaint();
                return true;
            }
            if (!mLayers[idx]->enabled())
                return false;
            mLayers[idx]->setChecked(!mLayers[idx]->checked());
            mOrientationFieldSizeSlider->setEnabled(mLayers[OrientationField]->checked());
            mOrientationFieldSingSizeSlider->setEnabled(mLayers[OrientationFieldSingularities]->checked());
            mPositionFieldSingSizeSlider->setEnabled(mLayers[PositionFieldSingularities]->checked());
            mFlowLineSlider->setEnabled(mLayers[FlowLines]->checked());
            repaint();
        } else if (key == GLFW_KEY_BACKSLASH) {
            bool inputMesh = mLayers[InputMesh]->checked();
            mLayers[InputMesh]->setChecked(!inputMesh);
            mLayers[InputMeshWireframe]->setChecked(false);
            mLayers[OutputMesh]->setChecked(inputMesh);
            mLayers[OutputMeshWireframe]->setChecked(inputMesh);
            if (mLayers[InputMesh]->checked())
                mSwitchBtn->setCaption("Show output");
            else
                mSwitchBtn->setCaption("Show input");
            repaint();
        } else if (key == 'L') {
            try {
                loadState("state.ply");
            } catch (const std::exception &ex) {
                cout << "Could not load current state: "<< ex.what() << endl;
            }
        } else if (key == 'S') {
            try {
                saveState("state.ply");
            } catch (const std::exception &ex) {
                cout << "Could not save current state: "<< ex.what() << endl;
            }
        }
    }
    return false;
}

void Viewer::setFloorPosition() {
    Eigen::Matrix4f model, view, proj;
    computeCameraMatrices(model, view, proj);

    auto transform_point = [&](const Vector3f &v) {
        Eigen::Matrix<float, 4, 1> v2;
        v2 << v.cast<float>(), 1;
        return Vector3f((model * v2).head<3>());
    };

    cout << "Computing floor .. " << endl;
    AABB aabb;
    for (uint32_t i=0; i<mRes.size(); ++i)
        aabb.expandBy(transform_point(mRes.V().col(i)));
    Vector3f floorAxis1 = Vector3f(aabb.max.x() - aabb.min.x(), 0, 0);
    Vector3f floorAxis2 = Vector3f(0, 0, aabb.max.z() - aabb.min.z());
    Vector3f floorCenter =
        Vector3f(.5f * (aabb.max.x() + aabb.min.x()), aabb.min.y(),
                 .5f * (aabb.max.z() + aabb.min.z()));

    mFloor.col(0) << 20 * floorAxis1, 0;
    mFloor.col(1) << 20 * floorAxis2, 0;
    mFloor.col(2) << floorAxis2.cross(floorAxis1).normalized(), 0;
    mFloor.col(3) << floorCenter, 1;

    mFloor = (model.cast<Float>().inverse() * mFloor).eval();
}

void Viewer::renderMitsuba() {
    if (mRes.levels() == 0)
        return;

    int rosy = mOptimizer.rosy(), posy = mOptimizer.posy();

    if (mLayers[InputMesh]->checked()) {
        MatrixXu F;
        MatrixXf V, N, Nd, C, UV, Q, O;

        mMeshShader44.bind();
        mMeshShader44.downloadAttrib("indices", F);
        mMeshShader44.downloadAttrib("position", V);
        mMeshShader44.downloadAttrib("tangent", Q);
        mMeshShader44.downloadAttrib("uv", O);

        {
            if (mUseHalfFloats) {
                Eigen::Matrix<half_float::half, Eigen::Dynamic, Eigen::Dynamic> Np, Npd;
                mMeshShader44.downloadAttrib("normal", Np);
                N = Np.cast<Float>();
                if (mMeshShader44.hasAttrib("normal_data")) {
                    mMeshShader44.downloadAttrib("normal_data", Npd);
                    Nd = Npd.cast<Float>();
                } else {
                    Nd = N;
                }
            } else {
                mMeshShader44.downloadAttrib("normal", N);
                if (mMeshShader44.hasAttrib("normal_data"))
                    mMeshShader44.downloadAttrib("normal_data", Nd);
                else
                    Nd = N;
            }
            MatrixXu8 Cp;
            mMeshShader44.downloadAttrib("color", Cp);
            C = Cp.cast<Float>() * (1.f / 255.f);
        }

        if (mVisualizeBox->selectedIndex() == 1) {
            {
                std::vector<MatrixXf> V_props = { std::move(V), std::move(N), std::move(Nd), std::move(C), std::move(Q), std::move(O) };
                replicate_vertices(F, V_props);
                V = std::move(V_props[0]);
                N = std::move(V_props[1]);
                Nd = std::move(V_props[2]);
                C = std::move(V_props[3]);
                Q = std::move(V_props[4]);
                O = std::move(V_props[5]);
            }

            auto compat_orientation = compat_orientation_intrinsic_2;
            if (rosy == 4)
                compat_orientation = compat_orientation_intrinsic_4;
            else if (rosy == 6)
                compat_orientation = compat_orientation_intrinsic_6;

            auto compat_position = compat_position_intrinsic_3;
            if (posy == 4)
                compat_position = compat_position_intrinsic_4;

            Float scale = mRes.scale(), inv_scale = 1/mRes.scale();

            UV.resize(2, F.cols() * 3);

            tbb::parallel_for(
                tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
                [&](const tbb::blocked_range<uint32_t> &range) {
                for (uint32_t f = range.begin(); f != range.end(); ++f) {
                        uint32_t i0 = F(0, f), i1 = F(1, f), i2 = F(2, f);

                        Vector3f q[3] = { Q.col(i0), Q.col(i1), Q.col(i2) };
                        Vector3f n[3] = { Nd.col(i0), Nd.col(i1), Nd.col(i2) };
                        Vector3f o[3] = { O.col(i0), O.col(i1), O.col(i2) };
                        Vector3f v[3] = { V.col(i0), V.col(i1), V.col(i2) };
                        Vector3f fn = (v[1] - v[0]).cross(v[2] - v[0]).normalized();

                        for (int i=0; i<3; ++i) {
                            q[i] = rotate_vector_into_plane(q[i], n[i], fn);
                            o[i] = rotate_vector_into_plane(o[i]-v[i], n[i], fn) + v[i];
                        }

                        for (int i=1; i<3; ++i) {
                            q[i] = compat_orientation(q[0], fn, q[i], fn).second;
                            o[i] = compat_position(v[0], fn, q[0], o[0],
                                                   v[i], fn, q[i], o[i],
                                                   scale, inv_scale).second;
                        }

                        for (int i=0; i<3; ++i) {
                            Vector3f rel = v[i] - o[i];
                            UV.col(f*3+i) = Vector2f(
                                rel.dot(q[i]) * inv_scale,
                                rel.dot(fn.cross(q[i])) * inv_scale
                            );
                        }
                    }
                }
            );
        }
        write_ply("scene_input.ply", F, V, N, MatrixXf(), UV, C);
    }

    if (mLayers[OutputMesh]->checked() && mF_extracted.size() > 0) {
        std::vector<MatrixXf> V_props = { mV_extracted };
        MatrixXu F = mF_extracted;
        replicate_vertices(F, V_props);
        MatrixXf V = std::move(V_props[0]);
        MatrixXf N(3, F.size());
        for (uint32_t i=0; i<F.cols(); ++i) {
            for (uint32_t j=0; j<F.rows(); ++j)
                N.col(F.rows()*i+j) = mNf_extracted.col(i);
        }
        write_ply("scene_output.ply", F, V, N);
    }

    if (mLayers[FlowLines]->checked()) {
        MatrixXu F;
        MatrixXf V;
        MatrixXu8 Cp;
        mFlowLineShader.bind();
        mFlowLineShader.downloadAttrib("indices", F);
        mFlowLineShader.downloadAttrib("position", V);
        mFlowLineShader.downloadAttrib("color", Cp);
        MatrixXf C = Cp.cast<Float>() * (1.f / 255.f);
        write_ply("scene_flowlines.ply", F, V, MatrixXf(), MatrixXf(), MatrixXf(), C);
    }

    if (mLayers[BrushStrokes]->checked()) {
        MatrixXu F;
        MatrixXf V;
        MatrixXu8 Cp;
        mStrokeShader.bind();
        mStrokeShader.downloadAttrib("indices", F);
        mStrokeShader.downloadAttrib("position", V);
        mStrokeShader.downloadAttrib("color", Cp);
        MatrixXf C = Cp.cast<Float>() * (1.f / 255.f);
        write_ply("scene_brushstrokes.ply", F, V, MatrixXf(), MatrixXf(), MatrixXf(), C);
    }

    Eigen::Matrix4f model, view, proj;
    computeCameraMatrices(model, view, proj);
    auto mat_str = [](const Eigen::Matrix4f &m) {
        std::ostringstream oss;
        for (uint32_t i=0; i<4; ++i)
            for (uint32_t j=0; j<4; ++j)
                oss << m(i, j) << " ";
        return oss.str();
    };

    auto transform_point = [&](const Vector3f &v) {
        Eigen::Matrix<float, 4, 1> v2;
        v2 << v.cast<float>(), 1;
        return Vector3f((model * v2).head<3>());
    };

    auto transform_vector = [&](const Vector3f &v) {
        Eigen::Matrix<float, 4, 1> v2;
        v2 << v.cast<float>(), 0;
        return Vector3f((model * v2).head<3>());
    };

    if ((mFloor.array() == 0).all())
        setFloorPosition();

    Vector3f cameraPos = Vector3f((view.inverse() * Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f)).head<3>());
    AABB aabb;
    for (uint32_t i=0; i<mRes.size(); ++i)
        aabb.expandBy(transform_point(mRes.V().col(i)));
    Float focusDistance = (cameraPos - aabb.center()).norm();

    const Vector3f base_color = Vector3f(.312f, .39f, .546f);
    const Vector3f finalmesh_color = Vector3f::Constant(0.95f);

    Vector3f edge_color0, edge_color1, edge_color2;
    edge_color0 = edge_color1 = edge_color2 = Vector3f::Constant(0.95f);

    if (rosy == 2) {
        edge_color0[0] *= 0.5f;
        edge_color1[2] *= 0.5f;
    }

    Float view_scale = transform_vector(Vector3f::UnitX()).norm();
    std::string envmap = "envmap_desat.exr";

    std::ofstream of("scene.xml");
    of << "<?xml version=\"1.0\"?>" << endl << endl
       << "<scene version=\"0.5.0\">" << endl
       << "\t<integrator type=\"path\">" << endl
       << "\t\t<boolean name=\"hideEmitters\" value=\"true\"/>" << endl
       << "\t</integrator>" << endl << endl;

    if (mLayers[InputMesh]->checked()) {
        of << "\t<shape type=\"ply\">" << endl
           << "\t\t<string name=\"filename\" value=\"scene_input.ply\"/>" << endl
           << "\t\t<transform name=\"toWorld\">" << endl
           << "\t\t\t<matrix value=\"" << mat_str(model) << "\"/>" << endl
           << "\t\t</transform>" << endl
           << "\t\t<bsdf type=\"roughplastic\">" << endl;
        if (mVisualizeBox->selectedIndex() == 1) {
            of << "\t\t\t<texture name=\"diffuseReflectance\" type=\"posyfield\">" << endl
               << "\t\t\t\t<integer name=\"posy\" value=\"" << posy << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"interiorColor\" value=\"" << base_color.transpose() << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"edgeColor0\" value=\"" << edge_color0.transpose() << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"edgeColor1\" value=\"" << edge_color1.transpose() << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"edgeColor2\" value=\"" << edge_color2.transpose() << "\"/>" << endl
               << "\t\t\t</texture>" << endl;
        } else if (mVisualizeBox->selectedIndex() == 2) {
            of << "\t\t\t<texture name=\"diffuseReflectance\" type=\"vertexcolors\"/>" << endl;
        } else if (mLayers[InputMeshWireframe]->checked()) {
            of << "\t\t\t<texture name=\"diffuseReflectance\" type=\"wireframe\">" << endl
               << "\t\t\t\t<float name=\"lineWidth\" value=\"" << mMeshStats.mAverageEdgeLength * view_scale / 20.f << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"interiorColor\" value=\"" << base_color.transpose() << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"edgeColor\" value=\"" << edge_color0.transpose() << "\"/>" << endl
               << "\t\t\t</texture>" << endl;
        } else {
            of << "\t\t\t<srgb name=\"diffuseReflectance\" value=\"" << base_color.transpose() << "\"/>" << endl;
        }
        of << "\t\t</bsdf>" << endl
           << "\t</shape>" << endl << endl;
    }

    if (mLayers[BrushStrokes]->checked()) {
        of << "\t<shape type=\"ply\">" << endl
           << "\t\t<string name=\"filename\" value=\"scene_brushstrokes.ply\"/>" << endl
           << "\t\t<transform name=\"toWorld\">" << endl
           << "\t\t\t<matrix value=\"" << mat_str(model) << "\"/>" << endl
           << "\t\t</transform>" << endl
           << "\t\t<bsdf type=\"roughplastic\">" << endl
           << "\t\t\t<texture name=\"diffuseReflectance\" type=\"vertexcolors\"/>" << endl
           << "\t\t</bsdf>" << endl
           << "\t</shape>" << endl << endl;
    }

    if (mLayers[FlowLines]->checked()) {
        bool dim = mLayers[FlowLines]->checked() && mLayers[BrushStrokes]->checked();
        of << "\t<shape type=\"ply\">" << endl
           << "\t\t<string name=\"filename\" value=\"scene_flowlines.ply\"/>" << endl
           << "\t\t<transform name=\"toWorld\">" << endl
           << "\t\t\t<matrix value=\"" << mat_str(model) << "\"/>" << endl
           << "\t\t</transform>" << endl;

        if (dim) {
           of << "\t\t<bsdf type=\"mask\">" << endl
              << "\t\t\t<spectrum name=\"opacity\" value=\"0.3\"/>" << endl
              << "\t\t\t<bsdf type=\"roughplastic\">" << endl
              << "\t\t\t\t<texture name=\"diffuseReflectance\" type=\"vertexcolors\"/>" << endl
              << "\t\t\t</bsdf>" << endl
              << "\t\t</bsdf>" << endl;
        } else {
           of << "\t\t<bsdf type=\"roughplastic\">" << endl
              << "\t\t\t<texture name=\"diffuseReflectance\" type=\"vertexcolors\"/>" << endl
              << "\t\t</bsdf>" << endl;
        }
        of << "\t</shape>" << endl << endl;
    }

    if (mLayers[OutputMesh]->checked() && mF_extracted.size() > 0) {
        envmap = "envmap.exr";
        of << "\t<shape type=\"ply\">" << endl
           << "\t\t<string name=\"filename\" value=\"scene_output.ply\"/>" << endl
           << "\t\t<transform name=\"toWorld\">" << endl
           << "\t\t\t<matrix value=\"" << mat_str(model) << "\"/>" << endl
           << "\t\t</transform>" << endl
           << "\t\t<bsdf type=\"force_twosided\">" << endl
           << "\t\t\t<bsdf type=\"roughplastic\">" << endl;
        if (mLayers[OutputMeshWireframe]->checked()) {
            Float lineWidth = mRes.scale() * view_scale / 20.f;
            if (posy == 3)
                lineWidth *= 2;
            of << "\t\t\t\t<texture name=\"diffuseReflectance\" type=\"wireframe\">" << endl
               << "\t\t\t\t\t<float name=\"lineWidth\" value=\"" << lineWidth << "\"/>" << endl
               << "\t\t\t\t\t<srgb name=\"interiorColor\" value=\"" << finalmesh_color.transpose() << "\"/>" << endl
               << "\t\t\t\t\t<srgb name=\"edgeColor\" value=\"0 0 0\"/>" << endl
               << "\t\t\t\t\t<boolean name=\"quads\" value=\"" << (posy == 4 ? "true" : "false") << "\"/>" << endl
               << "\t\t\t\t</texture>" << endl;
        } else {
            of << "\t\t\t\t<srgb name=\"diffuseReflectance\" value=\"" << finalmesh_color.transpose() << "\"/>" << endl;
        }
        of << "\t\t\t</bsdf>" << endl
           << "\t\t</bsdf>" << endl
           << "\t</shape>" << endl;
    }

    if (mLayers[OrientationFieldSingularities]->checked()) {
        Float radius = mRes.scale() * 0.4f * view_scale * 0.5f
         * std::pow((Float) 2, mOrientationFieldSingSizeSlider->value() * 4 - 2);

        of << "\t<bsdf type=\"roughplastic\" id=\"red\">" << endl
           << "\t\t<rgb name=\"diffuseReflectance\" value=\".8 0 0\"/>" << endl
           << "\t</bsdf>" << endl << endl
           << "\t<bsdf type=\"roughplastic\" id=\"green\">" << endl
           << "\t\t<rgb name=\"diffuseReflectance\" value=\"0 .8 0\"/>" << endl
           << "\t</bsdf>" << endl << endl
           << "\t<bsdf type=\"roughplastic\" id=\"blue\">" << endl
           << "\t\t<rgb name=\"diffuseReflectance\" value=\"0 0 .8\"/>" << endl
           << "\t</bsdf>" << endl << endl;

        for (auto &sing : mOrientationSingularities) {
            Vector3f p = transform_point(mRes.faceCenter(sing.first));

            std::string refname = "green";
            if (sing.second == 1)
                refname = "blue";
            else if (sing.second == (uint32_t) rosy-1)
                refname = "red";

            of << "\t<shape type=\"sphere\">" << endl
               << "\t\t<point name=\"center\" x=\"" << p.x() << "\" y=\"" << p.y()
               << "\" z=\"" << p.z() << "\"/>" << endl
               << "\t\t<float name=\"radius\" value=\"" << radius << "\"/>" << endl
               << "\t\t<ref id=\"" << refname << "\"/>" << endl
               << "\t</shape>" << endl << endl;
        }
    }

    if (mLayers[PositionFieldSingularities]->checked()) {
        Float radius = mRes.scale() * 0.4f * view_scale * 0.5f
         * std::pow((Float) 2, mPositionFieldSingSizeSlider->value() * 4 - 2);

        const Vector3f yellow(1.0f, 0.933f, 0.0f);
        const Vector3f orange(1.0f, 0.5f, 0.0f);

        int ctr = 0;
        for (auto &sing : mPositionSingularities) {
            Vector3f n = transform_vector(mRes.faceNormal(sing.first)).normalized();
            Vector3f p = transform_point(mRes.faceCenter(sing.first));

            int v = mRes.F()(0, sing.first);
            Vector3f q = transform_vector(mRes.Q().col(v));
            q = (q-n*n.dot(q)).normalized();

            auto shift = sing.second;
            auto rot = mOptimizer.posy() == 3 ? rotate60 : rotate90;
            Vector3f dir0, dir1;

            if (mOptimizer.posy() == 3) {
                Vector3f base = shift.x() * q + shift.y() * rot(q, n);
                dir0 = rot(base, -n);
                dir1 = rot(rot(base, -n), -n);
            } else {
                dir0 = rot(shift.x() * q, -n);
                dir1 = shift.y() * q;
            }
            of << "\t<shape type=\"sphere\" id=\"pos_sing_" << ctr++ << "\">" << endl
               << "\t\t<point name=\"center\" x=\"" << p.x() << "\" y=\"" << p.y()
               << "\" z=\"" << p.z() << "\"/>" << endl
               << "\t\t<float name=\"radius\" value=\"" << radius << "\"/>" << endl
               << "\t\t<bsdf type=\"roughplastic\">" << endl
               << "\t\t\t<texture type=\"posysing\" name=\"diffuseReflectance\">" << endl
               << "\t\t\t\t<point name=\"position\" x=\"" << p.x() << "\" y=\"" << p.y() << "\" z=\"" << p.z() << "\"/>" << endl
               << "\t\t\t\t<vector name=\"dir0\" x=\"" << dir0.x() << "\" y=\"" << dir0.y() << "\" z=\"" << dir0.z() << "\"/>" << endl
               << "\t\t\t\t<vector name=\"dir1\" x=\"" << dir1.x() << "\" y=\"" << dir1.y() << "\" z=\"" << dir1.z() << "\"/>" << endl
               << "\t\t\t\t<vector name=\"normal\" x=\"" << n.x() << "\" y=\"" << n.y() << "\" z=\"" << n.z() << "\"/>" << endl
               << "\t\t\t\t<srgb name=\"color\" value=\"" << ((shift.array().abs() <= 1).all() ? yellow : orange).transpose() << "\"/>" << endl
               << "\t\t\t</texture>" << endl
               << "\t\t</bsdf>" << endl
               << "\t</shape>" << endl << endl;
        }
    }

    of << "\t<emitter type=\"envmap\">" << endl
       << "\t\t<string name=\"filename\" value=\"" << envmap << "\"/>" << endl
       << "\t\t<float name=\"scale\" value=\"1.5\"/>" << endl
       << "\t\t<transform name=\"toWorld\">" << endl
       << "\t\t\t<rotate y=\"1\" angle=\"70\"/>" << endl
       << "\t\t\t<rotate x=\"1\" angle=\"-50\"/>" << endl
       << "\t\t</transform>" << endl
       << "\t</emitter>" << endl << endl
       << "\t<shape type=\"rectangle\" id=\"background\">" << endl
       << "\t\t<transform name=\"toWorld\">" << endl
       << "\t\t\t<matrix value=\"" << mat_str(model * mFloor.cast<float>()) << "\"/>" << endl
       << "\t\t</transform>" << endl
       << "\t\t<bsdf type=\"diffuse\">" << endl
       << "\t\t\t<spectrum name=\"reflectance\" value=\"0.7\"/>" << endl
       << "\t\t</bsdf>" << endl
       << "\t</shape>" << endl << endl
       << "\t<sensor type=\"perspective\">" << endl
       << "\t\t<float name=\"fov\" value=\"" << mCamera.viewAngle << "\"/>" << endl
       << "\t\t<float name=\"focusDistance\" value=\"" << focusDistance << "\"/>" << endl
       << "\t\t<string name=\"fovAxis\" value=\"y\"/>" << endl << endl
       << "\t\t<sampler type=\"halton\">" << endl
       << "\t\t\t<integer name=\"sampleCount\" value=\"256\"/>" << endl
       << "\t\t</sampler>" << endl
       << "\t\t<transform name=\"toWorld\">" << endl
       << "\t\t\t<scale x=\"-1\" z=\"-1\"/>" << endl
       << "\t\t\t<matrix value=\"" << mat_str(view.inverse()) << "\"/>" << endl
       << "\t\t</transform>" << endl
       << "\t\t<film type=\"ldrfilm\">" << endl
       << "\t\t\t<string name=\"pixelFormat\" value=\"rgb\"/>" << endl
       << "\t\t\t<string name=\"fileFormat\" value=\"jpeg\"/>" << endl
       << "\t\t\t<boolean name=\"banner\" value=\"false\"/>" << endl
       << "\t\t\t<integer name=\"width\" value=\"" << mSize.x() << "\"/>" << endl
       << "\t\t\t<integer name=\"height\" value=\"" << mSize.y() << "\"/>" << endl
       << "\t\t\t<rfilter type=\"mitchell\"/>" << endl
       << "\t\t</film>" << endl
       << "\t</sensor>" << endl
       << "</scene>" << endl;

#if !defined(_WIN32)
    pid_t pid = fork(); /* Create a child process */
    if (pid == -1) {
        throw std::runtime_error("Internal error!");
    } else if (pid == 0) {
        execlp("Mitsuba.app/Contents/MacOS/mtsgui", "Mitsuba.app/Contents/MacOS/mtsgui", "scene.xml", nullptr);
        execlp("bash", "bash", "-c", "source ~/projects/mitsuba-git/setpath.sh; mtsgui scene.xml", nullptr);
        exit(-1);
    }
#endif
}

void Viewer::extractConsensusGraph() {
    if (mRes.levels() == 0 || mRes.iterationsO() < 0)
        return;
    Float eps = mMeshStats.mAverageEdgeLength * (1.0f / 5.0f);

    int rosy = mOptimizer.rosy(), posy = mOptimizer.posy();
    Float scale = mRes.scale(), inv_scale = 1 / scale;
    auto compat_orientation = rosy == 2 ? compat_orientation_extrinsic_2 :
        (rosy == 4 ? compat_orientation_extrinsic_4 : compat_orientation_extrinsic_6);
    auto compat_position = posy == 4 ? compat_position_extrinsic_index_4 : compat_position_extrinsic_index_3;

    const MatrixXf &Q = mRes.Q(), &O = mRes.O(), &N = mRes.N(), &V = mRes.V();
    const AdjacencyMatrix &adj = mRes.adj();
    MatrixXf outputMeshWireframe;
    outputMeshWireframe.resize(6, 2*(adj[mRes.size()] - adj[0]));
    outputMeshWireframe.setZero();

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i<range.end(); ++i) {
                for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                    uint32_t j = link->id;
                    if (j <= i)
                        continue;

                    std::pair<Vector3f, Vector3f> Q_rot = compat_orientation(
                            Q.col(i), N.col(i), Q.col(j), N.col(j));

                    std::pair<Vector2i, Vector2i> shift = compat_position(
                            V.col(i), N.col(i), Q_rot.first, O.col(i),
                            V.col(j), N.col(j), Q_rot.second, O.col(j),
                            scale, inv_scale, nullptr);

                    Vector2i diff = shift.first-shift.second;
                    int diffSum = diff.cwiseAbs().sum();
                    Vector3f color;

                    if (diff.cwiseAbs() == Vector2i(1, 1)) {
                        if (posy == 4)
                            continue; /* Ignore diagonal lines for quads */
                        else
                            diffSum = 1;
                    }

                    if (diffSum == 0)
                        color = Vector3f::UnitZ();
                    else if (diffSum == 1)
                        color = Vector3f::UnitX();
                    else
                        color = Vector3f::Constant(1.0f);

                    outputMeshWireframe.col(2*(link-adj[0]))   << O.col(i) + N.col(i) * eps, color;
                    outputMeshWireframe.col(2*(link-adj[0])+1) << O.col(j) + N.col(j) * eps, color;
                }
            }
        }
    );

    mOutputMeshLines = outputMeshWireframe.cols();
    mOutputMeshWireframeShader.bind();
    mOutputMeshWireframeShader.uploadAttrib("position", MatrixXf(outputMeshWireframe.block(0, 0, 3, mOutputMeshLines)));
    mOutputMeshWireframeShader.uploadAttrib("color", MatrixXf(outputMeshWireframe.block(3, 0, 3, mOutputMeshLines)));
    mLayers[OutputMeshWireframe]->setChecked(true);
    repaint();
}

void Viewer::extractMesh() {
    if (mRes.levels() == 0 || mRes.iterationsO() < 0)
        return;
    int rosy = mOptimizer.rosy(), posy = mOptimizer.posy();
    repaint();

    Timer<> timer;
    std::vector<std::vector<TaggedLink>> adj_extracted;
    std::set<uint32_t> creaseOut;
    extract_graph(mRes, mExtrinsicBox->checked(), rosy, posy, adj_extracted,
                  mV_extracted, mN_extracted, mCreaseSet, creaseOut,
                  mDeterministic);

    Vector3f red = Vector3f::UnitX();

    int smooth_iterations = (int) (mSmoothSlider->value() * 10);
    extract_faces(adj_extracted, mV_extracted, mN_extracted, mNf_extracted,
                  mF_extracted, posy, mRes.scale(), creaseOut, true,
                  mPureQuadBox->checked(), mBVH, smooth_iterations);

    cout << "Extraction is done. (total time: " << timeString(timer.value()) << ")" << endl;

    int fmult = posy == 3 ? 1 : 2;

    MatrixXu F_gpu(3, mF_extracted.cols()*fmult);
    MatrixXf N_gpu(3, mF_extracted.cols()*posy);
    MatrixXf O_gpu(3, mF_extracted.cols()*posy);
    MatrixXf outputMeshWireframe(6, mF_extracted.cols() * posy * 2);

    for (uint32_t i=0; i<(uint32_t) mF_extracted.cols(); ++i) {
        int base = posy*i;
        F_gpu.col(fmult*i) = Vector3u(base+0, base+1, base+2);
        if (posy == 4)
            F_gpu.col(fmult*i+1) = Vector3u(base+2, base+3, base+0);
        bool irregular = posy == 4 && mF_extracted(2, i) == mF_extracted(3, i);
        for (int j=0; j<posy; ++j) {
            uint32_t k = mF_extracted(j, i), kn = mF_extracted((j+1)%posy, i);

            Vector3f col = red;
            if (irregular && j >= 1)
                col = Vector3f::Zero();
            outputMeshWireframe.col(i*2*posy + j*2 + 0) << mV_extracted.col(k), col;
            outputMeshWireframe.col(i*2*posy + j*2 + 1) << mV_extracted.col(kn), col;
            O_gpu.col(i*posy + j) = mV_extracted.col(k);
            N_gpu.col(i*posy + j) = mNf_extracted.col(i);
        }
    }

    mOutputMeshShader.bind();
    mOutputMeshShader.uploadAttrib("position", O_gpu);
    mOutputMeshShader.uploadAttrib("normal", N_gpu);
    mOutputMeshShader.uploadIndices(F_gpu);
    mOutputMeshFaces = F_gpu.cols();
    mOutputMeshLines = outputMeshWireframe.cols();
    mOutputMeshWireframeShader.bind();
    mOutputMeshWireframeShader.uploadAttrib("position", MatrixXf(outputMeshWireframe.block(0, 0, 3, mOutputMeshLines)));
    mOutputMeshWireframeShader.uploadAttrib("color", MatrixXf(outputMeshWireframe.block(3, 0, 3, mOutputMeshLines)));

    while (!(mLayers[OutputMesh]->checked() && mLayers[OutputMeshWireframe]->checked()))
        keyboardEvent(GLFW_KEY_BACKSLASH, 0, true, 0);
}

void Viewer::traceFlowLines() {
    if (toolActive() && !mScreenCurve.empty())
        return;
    bool pointcloud = mRes.F().size() == 0;
    int version = mRes.iterationsQ();
    if (mRes.levels() == 0 || !mBVH || version < 0 || pointcloud)
        return;
    int rosy = mOptimizer.rosy(), posy = mOptimizer.posy();

    const MatrixXf &V = mRes.V(), &N = mRes.N(), &Q = mRes.Q();
    const MatrixXu &F = mRes.F();

    auto rotate = rosy == 2 ? rotate180 : (rosy == 4 ? rotate90 : rotate60);

    Float scale = std::pow((Float) 2, mFlowLineSlider->value() * 4 - 2);
    Float edgeLength = mMeshStats.mAverageEdgeLength*2;

    const Float targetLength = edgeLength * 20;
    const Float stepSize  = edgeLength / 2.0f;
    const Float thickness = edgeLength / 3.0f;

    const Float eps = pointcloud ? edgeLength : (edgeLength / 10);
    const uint32_t nSteps = (uint32_t) (targetLength / stepSize);
    uint32_t nLines = mMeshStats.mSurfaceArea / (edgeLength * thickness) * 0.02f * scale;
    if (posy == 3)
        nLines *= 2;
    const uint32_t meshSize = pointcloud ? mRes.V().size() : mRes.F().size();
    const size_t nTriangles = nLines*nSteps*2;
    const size_t nVertices = nLines*(nSteps+1)*2;

    MatrixXu indices(3, nTriangles);
    MatrixXf position(3, nVertices);
    MatrixXu8 color(4, nVertices);
    std::vector<tbb::spin_mutex> locks(meshSize * (rosy/2));
    indices.setZero();
    cout << "Tracing " << nLines << " flow lines ..";
    cout.flush();
    Timer<> timer;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) nLines, GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            std::set<uint32_t> locked;
            pcg32 rng;
            for (uint32_t k = range.begin(); k != range.end(); ++k) {
                rng.seed(1, k);
                Float hue = std::fmod(k*0.61803398f, 1.f);
                Vector4u8 cvalue;
                cvalue << (hsv_to_rgb(hue, 0.5f, 0.5f * rng.nextFloat() + 0.5f) * 255).cast<uint8_t>(), (uint8_t) 0x0;
                int nTries = 0;
                Float height = rng.nextFloat() * eps + eps;

                do {
                    size_t vertexIdx = k * 2 * (nSteps+1);
                    size_t triangleIdx = k*nSteps*2;
                    uint32_t startIdx = rng.nextUInt(V.cols());

                    Vector3f p = V.col(startIdx), n = N.col(startIdx), q = Q.col(startIdx);
                    for (int i=0, nrot=rng.nextUInt(rosy); i<nrot; ++i)
                        q = rotate(q, n);

                    Vector3f t = n.cross(q);

                    position.col(vertexIdx) = p + n*height;
                    color.col(vertexIdx++) = cvalue;
                    position.col(vertexIdx) = p + n*height;
                    color.col(vertexIdx++) = cvalue;

                    bool fail = false;
                    locked.clear();
                    for (uint32_t step=0; step<nSteps; ++step) {
                        p += q * stepSize;
                        Vector3f o = p;
                        if (pointcloud)
                            o += n * eps;
                        Ray ray1(o,  n, 0, edgeLength * 2);
                        Ray ray2(o, -n, 0, edgeLength * 2);
                        uint32_t idx1 = 0, idx2 = 0;
                        Float t1 = 0, t2 = 0;
                        Vector2f uv1, uv2;
                        bool hit1 = mBVH->rayIntersect(ray1, idx1, t1, &uv1);
                        bool hit2 = mBVH->rayIntersect(ray2, idx2, t2, &uv2);
                        if (!hit1 && !hit2) {
                            fail = true;
                            break;
                        }
                        if (t2 < t1) {
                            p = ray2(t2);
                            uv1 = uv2;
                            t1 = t2;
                            idx1 = idx2;
                        } else {
                            p = ray1(t1);
                        }

                        Vector3f qp;
                        if (pointcloud) {
                            n = N.col(idx1);
                            qp  = Q.col(idx1);
                        } else {
                            n = ((1 - uv1.sum()) * N.col(F(0, idx1)) + uv1.x() * N.col(F(1, idx1)) + uv1.y() * N.col(F(2, idx1))).normalized();
                            qp  = Q.col(F(0, idx1));
                        }
                        q = (q-n.dot(q)*n).normalized();
                        qp = (qp-n.dot(qp)*n).normalized();

                        Vector3f best = Vector3f::Zero();
                        Float best_dp = -std::numeric_limits<Float>::infinity();
                        int best_index = -1;
                        for (int j=0; j<rosy; ++j) {
                            Float dp = qp.dot(q);
                            if (dp > best_dp) {
                                best_dp = dp;
                                best = qp;
                                best_index = j;
                            }
                            qp = rotate(qp, n);
                        }

                        uint32_t lock_idx = idx1*(rosy/2) + best_index%(rosy/2);
                        if (locked.find(lock_idx) == locked.end()) {
                            if (!locks[lock_idx].try_lock()) {
                                fail = true;
                                break;
                            }
                            locked.insert(lock_idx);
                        }

                        q = (best-n.dot(best)*n).normalized();
                        t = n.cross(q);
                        indices.col(triangleIdx++) << vertexIdx, vertexIdx-2, vertexIdx-1;
                        indices.col(triangleIdx++) << vertexIdx, vertexIdx-1, vertexIdx+1;

                        Float thicknessP = (1-std::pow((2*((step+1) / (Float) nSteps)-1), 2.0f)) * thickness;
                        position.col(vertexIdx) = p + t * thicknessP + n*height;
                        color.col(vertexIdx++) = cvalue;
                        position.col(vertexIdx) = p - t * thicknessP + n*height;
                        color.col(vertexIdx++) = cvalue;
                    }
                    if (fail) {
                        for (auto l : locked)
                            locks[l].unlock();
                        if (++nTries > 10) {
                            indices.block(0, k*nSteps*2, 3, nSteps*2).setConstant(0);
                            break;
                        }
                        continue;
                    } else {
                        break;
                    }
                } while (true);
            }
        }
    );

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
    mFlowLineShader.bind();
    mFlowLineShader.uploadAttrib("position", position, version);
    mFlowLineShader.uploadAttrib("color", color, version);
    mFlowLineShader.uploadIndices(indices);
    mFlowLineFaces = nTriangles;
    repaint();
}

void Viewer::resetState() {
    {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        mOptimizer.stop();
    }
    bool hasData = mRes.levels() > 0 && mRes.size() > 0;
    bool pointcloud = hasData && mRes.F().size() == 0;
    mOutputMeshFaces = 0;
    mOutputMeshLines = 0;
    mFlowLineFaces = 0;
    mStrokeFaces = 0;
    mContinueWithPositions = false;
    if (mStrokes.size() > 0)
        mRes.clearConstraints();
    mStrokes.clear();
    mCamera.arcball = Arcball();
    mCamera.arcball.setSize(mSize);
    mCamera.zoom = 1.0f;
    mCamera.modelTranslation = -mMeshStats.mWeightedCenter.cast<float>();
    for (int i=0; i<12; ++i)
        mCameraSnapshots[i] = mCamera;
    mDrawIndex = 0;
    mNeedsRepaint = true;
    mTranslate = mDrag = false;
    mFloor.setZero();
    mLayers[InputMesh]->setCaption(pointcloud ? "Input point cloud" : "Input mesh");
    mLayers[InputMesh]->setChecked(hasData);
    mLayers[InputMesh]->setEnabled(hasData);
    mLayers[InputMeshWireframe]->setEnabled(hasData);
    mLayers[InputMeshWireframe]->setChecked(false);
    mLayers[InputMeshWireframe]->setEnabled(mRes.F().size() > 0);
    mLayers[VertexLabels]->setChecked(false);
    mLayers[VertexLabels]->setEnabled(hasData);
    mLayers[FaceLabels]->setChecked(false);
    mLayers[FaceLabels]->setEnabled(hasData);
    mLayers[FlowLines]->setChecked(false);
    mLayers[FlowLines]->setEnabled(hasData);
    mLayers[OrientationField]->setChecked(false);
    mLayers[OrientationField]->setEnabled(hasData);
    mLayers[PositionField]->setChecked(false);
    mLayers[PositionField]->setEnabled(hasData);
    mLayers[OrientationFieldSingularities]->setChecked(false);
    mLayers[OrientationFieldSingularities]->setEnabled(hasData && !pointcloud);
    mLayers[PositionFieldSingularities]->setChecked(false);
    mLayers[PositionFieldSingularities]->setEnabled(hasData && !pointcloud);
    mLayers[OutputMeshWireframe]->setChecked(false);
    mLayers[OutputMeshWireframe]->setEnabled(hasData);
    mLayers[OutputMesh]->setChecked(false);
    mLayers[OutputMesh]->setEnabled(hasData);
    mLayers[BrushStrokes]->setChecked(false);
    mLayers[BrushStrokes]->setEnabled(hasData);
    mVisualizeBox->setSelectedIndex(0);
    mScaleSlider->setEnabled(hasData);
    mScaleBox->setEnabled(hasData);
    mOrientationFieldSizeSlider->setEnabled(mLayers[OrientationField]->checked());
    mOrientationFieldSizeSlider->setValue(0.5f);
    mFlowLineSlider->setEnabled(mLayers[FlowLines]->checked());
    mFlowLineSlider->setValue(0.5f);
    mOrientationFieldSingSizeSlider->setEnabled(mLayers[OrientationFieldSingularities]->checked());
    mOrientationFieldSingSizeSlider->setValue(0.5f);
    mPositionFieldSingSizeSlider->setEnabled(mLayers[PositionFieldSingularities]->checked());
    mPositionFieldSingSizeSlider->setValue(0.5f);
    mHierarchyLevelBox->setEnabled(hasData);
    mHierarchyPlusButton->setEnabled(hasData);
    mHierarchyMinusButton->setEnabled(hasData);
    mVisualizeBox->setEnabled(hasData);

    /* Orientation field */
    mSolveOrientationBtn->setPushed(false);
    mSolveOrientationBtn->setEnabled(hasData);
    mOrientationComb->setPushed(false);
    mOrientationComb->setEnabled(hasData && !pointcloud);
    mOrientationAttractor->setEnabled(false);
    mOrientationAttractor->setPushed(false);
    mOrientationScareBrush->setEnabled(false);
    mOrientationScareBrush->setPushed(false);

    /* Position field */
    mSolvePositionBtn->setEnabled(false);
    mSolvePositionBtn->setPushed(false);
    mPositionAttractor->setEnabled(false);
    mPositionAttractor->setPushed(false);
    mPositionScareBrush->setEnabled(false);
    mPositionScareBrush->setPushed(false);
    mEdgeBrush->setEnabled(hasData && !pointcloud);
    mEdgeBrush->setPushed(false);

    mExportBtn->setEnabled(false);

    mExtrinsicBox->setChecked(true);
    mOptimizer.setExtrinsic(true);
    mSymmetryBox->setEnabled(hasData);
    setSymmetry(4, 4);
    mOrientationSingularities.clear();
    mPositionSingularities.clear();
    mSelectedLevel = -1;
    if (hasData)
        setTargetVertexCount(mRes.V().cols() / 16);
    setLevel(-1);
    refreshColors();
    mOrientationSingularityBox->setValue("0");
    mPositionSingularityBox->setValue("0");
    mOrientationSingularityBox->setEnabled(!pointcloud && hasData);
    mPositionSingularityBox->setEnabled(!pointcloud && hasData);
    mSaveBtn->setEnabled(false);
    mSwitchBtn->setEnabled(false);
    mSmoothSlider->setValue(0.0f/10.f);
    mSmoothSlider->callback()(mSmoothSlider->value());

    mCreaseBox->setChecked(mCreaseAngle >= 0);
    mCreaseBox->setEnabled(hasData && !pointcloud);
    mExtrinsicBox->setEnabled(hasData);
    mAlignToBoundariesBox->setEnabled(hasData && !pointcloud);
    mAlignToBoundariesBox->setChecked(false);
    mCreaseAngleSlider->setEnabled(hasData && mCreaseAngle >= 0);
    mCreaseAngleBox->setEnabled(hasData && mCreaseAngle >= 0);

    if (mCreaseAngle >= 0) {
        mCreaseAngleSlider->setValue(mCreaseAngle / 90.0f);
        mCreaseAngleBox->setValue(std::to_string((int) mCreaseAngle));
        mCreaseAngleBox->setUnits(utf8(0x00B0).data());
    } else {
        mCreaseAngleSlider->setValue(0.0f);
        mCreaseAngleBox->setValue(utf8(0x00D8).data());
        mCreaseAngleBox->setUnits("");
    }
}

void Viewer::refreshColors() {
    if (mRes.levels() == 0)
        return;

    uint32_t n = mRes.V().cols(), extra = (uint32_t) mCreaseMap.size();
    MatrixXu8 C(4, n + extra);
    Vector4u8 red(255, 76, 76, 255);
    C.setZero();
    int level = mSelectedLevel;

    switch (mVisualizeBox->selectedIndex()) {
        case 0:
        case 1:
            break;
        case 2: /* Hierarchy */
            if (level >= 0) {
                const std::vector<std::vector<uint32_t>> &phases = mRes.phases(level);
                MatrixXu8 Cp(4, mRes.size(level));

                Float hue = 0.0f;
                for (auto phase : phases) {
                    hue = std::fmod(hue + 0.61803398f, 1.f);
                    Vector4u8 color;
                    pcg32 rng;
                    for (uint32_t i : phase) {
                        color << (hsv_to_rgb(hue, 0.5f, 0.5f * rng.nextFloat() + 0.5f) * 255).cast<uint8_t>(), (uint8_t) 255;
                        Cp.col(i) = color;
                    }
                }

                for (int i=level-1; i>=0; --i) {
                    MatrixXu8 Cp2(4, mRes.size(i));
                    const MatrixXu &toUpper = mRes.toUpper(i);
                    for (size_t j=0; j < (size_t) Cp.cols(); ++j) {
                        for (int k=0; k<2; ++k) {
                            uint32_t dest = toUpper(k, j);
                            if (dest != INVALID)
                                Cp2.col(dest) = Cp.col(j);
                        }
                    }
                    Cp = std::move(Cp2);
                }
                C.topLeftCorner(4, n) = Cp;
            }

            break;
        case 3: /* Creases */
            for (auto c : mCreaseSet)
                C.col(c) = red;
            break;
        case 4: /* Boundaries */
            C.topLeftCorner(4, n) = red * mBoundaryVertices.cast<uint8_t>().transpose();
            break;
        case 5: /* Non-manifold vertices */
            C.topLeftCorner(4, n) = red * mNonmanifoldVertices.cast<uint8_t>().transpose();
            break;
        case 6: {/* Curl */
                //VectorXf curl;
                //compute_curl(mRes, curl);
                //jet(curl, C);
            }

            break;
    }
    for (auto &c : mCreaseMap)
        C.col(c.first) = C.col(c.second);
    mMeshShader44.bind();
    mMeshShader44.uploadAttrib("color", C);
    repaint();
}

void Viewer::saveState(std::string filename) {
    if (filename.empty()) {
        filename = nanogui::file_dialog({{"ply", "Stanford PLY"}}, true);

        if (filename == "")
            return;
    }
    std::lock_guard<ordered_lock> lock(mRes.mutex());

    try {
        GUISerializer state;
        state.set("gui", this);
        state.set("gui.size", mSize);

        /* Camera configuration */
        state.pushPrefix("camera");
        state.set("arcball", mCamera.arcball.state());
        state.set("zoom", mCamera.zoom);
        state.set("modelZoom", mCamera.modelZoom);
        state.set("modelTranslation", mCamera.modelTranslation);
        state.set("viewAngle", mCamera.viewAngle);
        state.popPrefix();

        for (int i=0; i<12; ++i) {
            state.pushPrefix("camera." + std::to_string(i));
            state.set("arcball", mCameraSnapshots[i].arcball.state());
            state.set("zoom", mCameraSnapshots[i].zoom);
            state.set("modelZoom", mCameraSnapshots[i].modelZoom);
            state.set("modelTranslation", mCameraSnapshots[i].modelTranslation);
            state.set("viewAngle", mCameraSnapshots[i].viewAngle);
            state.popPrefix();
        }

        state.pushPrefix("meshStats");
        state.set("weightedCenter", mMeshStats.mWeightedCenter);
        state.set("averageEdgeLength", mMeshStats.mAverageEdgeLength);
        state.set("surfaceArea", mMeshStats.mSurfaceArea);
        state.set("aabb.min", mMeshStats.mAABB.min);
        state.set("aabb.max", mMeshStats.mAABB.max);
        state.popPrefix();

        state.pushPrefix("data");
        state.set("nonmanifoldVertices", mNonmanifoldVertices);
        state.set("boundaryVertices", mBoundaryVertices);
        state.set("creaseAngle", mCreaseAngle);
        state.set("outputMeshFaces", mOutputMeshFaces);
        state.set("outputMeshLines", mOutputMeshLines);
        state.set("flowLineFaces", mFlowLineFaces);
        state.set("strokeFaces", mStrokeFaces);
        state.set("selectedLevel", mSelectedLevel);
        state.set("creaseMap", mCreaseMap);
        state.set("creaseSet", mCreaseSet);
        state.set("orientationSingularities", mOrientationSingularities);
        state.set("positionSingularities", mPositionSingularities);
        state.set("filename", mFilename);
        state.set("deterministic", mDeterministic);
        state.set("floor", MatrixXf(mFloor));

        state.set("strokeCount", (uint32_t) mStrokes.size());
        for (uint32_t i = 0; i<mStrokes.size(); ++i) {
            state.pushPrefix("stroke." + std::to_string(i));
            state.set("type", mStrokes[i].first);
            auto const &stroke = mStrokes[i].second;
            MatrixXf p(3, stroke.size()), n(3, stroke.size());
            VectorXu f(stroke.size());
            for (uint32_t j=0; j<stroke.size(); ++j) {
                p.col(j) = stroke[j].p;
                n.col(j) = stroke[j].n;
                f[j] = stroke[j].f;
            }
            state.set("p", p);
            state.set("n", n);
            state.set("f", f);
            state.popPrefix();
        }

        state.popPrefix();

        state.pushPrefix("extracted");
        state.set("F", mF_extracted);
        state.set("V", mV_extracted);
        state.set("N", mN_extracted);
        state.set("Nf", mNf_extracted);
        state.popPrefix();

        state.pushPrefix("optimizer");
        mOptimizer.save(state);
        state.popPrefix();

        state.pushPrefix("mres");
        mRes.save(state);
        state.popPrefix();

        state.pushPrefix("gpu");
        mMeshShader44.save(state);
        mOutputMeshShader.save(state);
        mOutputMeshWireframeShader.save(state);
        mFlowLineShader.save(state);
        mStrokeShader.save(state);
        mPositionSingularityShader.save(state);
        mOrientationSingularityShader.save(state);
        state.popPrefix();

        //cout << "Writing " << state << endl;
        mOperationStart = mLastProgressMessage = glfwGetTime();
        mProcessEvents = false;
        glfwMakeContextCurrent(nullptr);
        state.write(filename, mProgress);
        mTranslate = mDrag = false;
        mTranslateStart = Vector2i::Constant(-1);
        mProgressWindow->setVisible(false);
        mProcessEvents = true;
        glfwMakeContextCurrent(mGLFWWindow);
    } catch (...) {
        mProgressWindow->setVisible(false);
        mProcessEvents = true;
        glfwMakeContextCurrent(mGLFWWindow);
        throw;
    }
}

void Viewer::loadState(std::string filename, bool compat) {
    if (filename.empty()) {
        filename = nanogui::file_dialog({{"ply", "Stanford PLY"}}, false);
        if (filename == "")
            return;
    }
    std::lock_guard<ordered_lock> lock(mRes.mutex());
    try {
        mOperationStart = mLastProgressMessage = glfwGetTime();
        mProcessEvents = false;
        glfwMakeContextCurrent(nullptr);
        GUISerializer state(filename, compat, mProgress);
        mProgressWindow->setVisible(false);
        mProcessEvents = true;
        glfwMakeContextCurrent(mGLFWWindow);
        //cout << state << endl;

        /* Gui settings */
        state.get("gui", this);
        state.get("gui.size", mSize);
        performLayout(mNVGContext);

        /* Camera configuration */
        state.pushPrefix("camera");
        state.get("arcball", mCamera.arcball.state());
        state.get("zoom", mCamera.zoom);
        state.get("modelZoom", mCamera.modelZoom);
        state.get("modelTranslation", mCamera.modelTranslation);
        state.get("viewAngle", mCamera.viewAngle);
        state.popPrefix();

        for (int i=0; i<12; ++i) {
            state.pushPrefix("camera." + std::to_string(i));
            state.get("arcball", mCameraSnapshots[i].arcball.state());
            state.get("zoom", mCameraSnapshots[i].zoom);
            state.get("modelZoom", mCameraSnapshots[i].modelZoom);
            state.get("modelTranslation", mCameraSnapshots[i].modelTranslation);
            state.get("viewAngle", mCameraSnapshots[i].viewAngle);
            state.popPrefix();
        }

        state.pushPrefix("meshStats");
        state.get("weightedCenter", mMeshStats.mWeightedCenter);
        state.get("averageEdgeLength", mMeshStats.mAverageEdgeLength);
        state.get("surfaceArea", mMeshStats.mSurfaceArea);
        state.get("aabb.min", mMeshStats.mAABB.min);
        state.get("aabb.max", mMeshStats.mAABB.max);
        state.popPrefix();

        state.pushPrefix("data");
        state.get("nonmanifoldVertices", mNonmanifoldVertices);
        state.get("boundaryVertices", mBoundaryVertices);
        state.get("creaseAngle", mCreaseAngle);
        state.get("outputMeshFaces", mOutputMeshFaces);
        state.get("outputMeshLines", mOutputMeshLines);
        state.get("flowLineFaces", mFlowLineFaces);
        state.get("strokeFaces", mStrokeFaces);
        state.get("selectedLevel", mSelectedLevel);
        state.get("creaseMap", mCreaseMap);
        state.get("creaseSet", mCreaseSet);
        state.get("orientationSingularities", mOrientationSingularities);
        state.get("positionSingularities", mPositionSingularities);
        state.get("filename", mFilename);
        state.get("deterministic", mDeterministic);
        MatrixXf floor;
        state.get("floor", floor);
        mFloor = floor;

        uint32_t strokeCount;
        state.get("strokeCount", strokeCount);
        mStrokes.resize(strokeCount);
        for (uint32_t i = 0; i<strokeCount; ++i) {
            auto &stroke = mStrokes[i].second;
            state.pushPrefix("stroke." + std::to_string(i));

            state.get("type", mStrokes[i].first);

            MatrixXf p, n;
            VectorXu f;
            state.get("p", p);
            state.get("n", n);
            state.get("f", f);

            stroke.resize(p.cols());
            for (uint32_t j=0; j<stroke.size(); ++j) {
                stroke[j].p = p.col(j);
                stroke[j].n = n.col(j);
                stroke[j].f = f[j];
            }
            state.popPrefix();
        }

        state.popPrefix();

        state.pushPrefix("extracted");
        state.get("F", mF_extracted);
        state.get("V", mV_extracted);
        state.get("N", mN_extracted);
        state.get("Nf", mNf_extracted);
        state.popPrefix();

        state.pushPrefix("optimizer");
        mOptimizer.load(state);
        state.popPrefix();

        state.pushPrefix("gpu");
        mMeshShader44.load(state);
        mOutputMeshShader.load(state);
        mOutputMeshWireframeShader.load(state);
        mFlowLineShader.load(state);
        mStrokeShader.load(state);
        mPositionSingularityShader.load(state);
        mOrientationSingularityShader.load(state);
        state.popPrefix();

        if (mBVH) {
            delete mBVH;
            mBVH = NULL;
        }
        mRes.free();
        state.pushPrefix("mres");
        mRes.load(state);
        state.popPrefix();
        if (mRes.levels() > 0 && mRes.size() > 0) {
            mBVH = new BVH(&mRes.F(), &mRes.V(), &mRes.N(), mMeshStats.mAABB);
            mBVH->build();
            mRes.printStatistics();
            mBVH->printStatistics();
        }

        shareGLBuffers();
        repaint();
        glfwSetWindowSize(mGLFWWindow, mSize.x(), mSize.y());
    } catch (...) {
        mProgressWindow->setVisible(false);
        mProcessEvents = true;
        glfwMakeContextCurrent(mGLFWWindow);
        throw;
    }
}

void Viewer::computeCameraMatrices(Eigen::Matrix4f &model,
                                   Eigen::Matrix4f &view,
                                   Eigen::Matrix4f &proj) {
    view = lookAt(mCamera.eye, mCamera.center, mCamera.up);

    float fH = std::tan(mCamera.viewAngle / 360.0f * M_PI) * mCamera.dnear;
    float fW = fH * (float) mSize.x() / (float) mSize.y();

    proj = frustum(-fW, fW, -fH, fH, mCamera.dnear, mCamera.dfar);
    model = mCamera.arcball.matrix();

    model = scale(model, Eigen::Vector3f::Constant(mCamera.zoom * mCamera.modelZoom));
    model = translate(model, mCamera.modelTranslation);
}

std::pair<Vector3f, Vector3f> Viewer::singularityPositionAndNormal(uint32_t f) const {
    const MatrixXu &F = mRes.F();
    const MatrixXf &N = mRes.N(), &V = mRes.V();

    uint32_t i0 = F(0, f), i1 = F(1, f), i2 = F(2, f);
    Vector3f v0 = V.col(i0), v1 = V.col(i1), v2 = V.col(i2);
    Vector3f n0 = N.col(i0), n1 = N.col(i1), n2 = N.col(i2);
    uint32_t k = 0;
    Vector3f n = Vector3f::Zero(), p = Vector3f::Zero();

    if (mCreaseSet.find(i0) != mCreaseSet.end()) { p += v0; n += n0; k++; }
    if (mCreaseSet.find(i1) != mCreaseSet.end()) { p += v1; n += n1; k++; }
    if (mCreaseSet.find(i2) != mCreaseSet.end()) { p += v2; n += n2; k++; }

    if (k == 0) {
        p += v0 + v1 + v2;
        n += n0 + n1 + n2;
        k = 3;
    }

    return std::make_pair(p/k, n.normalized());
}

bool Viewer::refreshOrientationSingularities() {
    if (mRes.iterationsQ() == mOrientationSingularityShader.attribVersion("position") || mRes.iterationsQ() < 0)
        return false;

    compute_orientation_singularities(mRes, mOrientationSingularities,
            mExtrinsicBox->checked(), mOptimizer.rosy());

    uint32_t n = mOrientationSingularities.size();

    MatrixXf position(3, n), color(3, n), normal(3, n), dir(3, n);
    dir.setZero();

    const Vector3f red   = Vector3f::UnitX(),
                   green = Vector3f::UnitY(),
                   blue  = Vector3f::UnitZ();

    uint32_t ctr = 0;
    const Float eps = mMeshStats.mAverageEdgeLength * 1.0f / 5.0f;
    for (auto &sing : mOrientationSingularities) {
        auto data = singularityPositionAndNormal(sing.first);
        position.col(ctr) = data.first + data.second * eps;
        normal.col(ctr) = data.second;

        Vector3f c;
        if (sing.second == 1)
            c = blue;
        else if (sing.second == (uint32_t) mOptimizer.rosy() - 1u)
            c = red;
        else
            c = green;

        color.col(ctr) = c;
        ctr++;
    }

    int version = mRes.iterationsQ();
    mOrientationSingularityShader.bind();
    mOrientationSingularityShader.uploadAttrib("position", position, version);
    mOrientationSingularityShader.uploadAttrib("color", color, version);
    mOrientationSingularityShader.uploadAttrib("normal", normal, version);
    mOrientationSingularityShader.uploadAttrib("dir1", dir, version);
    mOrientationSingularityShader.shareAttrib(mOrientationSingularityShader, "dir1", "dir2");

    mOrientationSingularityBox->setValue(std::to_string(n));
    return true;
}

bool Viewer::refreshPositionSingularities() {
    if (mRes.iterationsO() == mPositionSingularityShader.attribVersion("position") || mRes.iterationsO() < 0)
        return false;

    compute_position_singularities(mRes, mOrientationSingularities, mPositionSingularities,
        mExtrinsicBox->checked(), mOptimizer.rosy(), mOptimizer.posy());

    uint32_t size = mPositionSingularities.size();
    MatrixXf position(3, size), color(3, size), normal(3, size), dir1(3, size), dir2(3, size);
    dir1.setZero();
    dir2.setZero();

    const Vector3f yellow(1.0f, 0.933f, 0.0f);
    const Vector3f orange(1.0f, 0.5f, 0.0f);

    uint32_t ctr = 0;
    const Float eps = mMeshStats.mAverageEdgeLength * 1.0f / 5.0f;
    for (auto &sing : mPositionSingularities) {
        auto data = singularityPositionAndNormal(sing.first);
        position.col(ctr) = data.first + data.second * eps;
        normal.col(ctr) = data.second;

        int v = mRes.F()(0, sing.first);
        Vector3f q = mRes.Q().col(v);
        Vector3f n = data.second;
        q = (q-n*n.dot(q)).normalized();

        auto shift = sing.second;
        auto rot = mOptimizer.posy() == 3 ? rotate60 : rotate90;

        if (mOptimizer.posy() == 3) {
            Vector3f base = shift.x() * q + shift.y() * rot(q, n);
            dir1.col(ctr) = rot(base, -n);
            dir2.col(ctr) = rot(rot(base, -n), -n);
        } else {
            dir1.col(ctr) = rot(shift.x() * q, -n);
            dir2.col(ctr) = shift.y() * q;
        }
        color.col(ctr) = (shift.array().abs() <= 1).all() ? yellow : orange;
        ctr++;
    }

    int version = mRes.iterationsO();
    mPositionSingularityShader.bind();
    mPositionSingularityShader.uploadAttrib("position", position, version);
    mPositionSingularityShader.uploadAttrib("color", color, version);
    mPositionSingularityShader.uploadAttrib("normal", normal, version);
    mPositionSingularityShader.uploadAttrib("dir1", dir1, version);
    mPositionSingularityShader.uploadAttrib("dir2", dir2, version);
    mPositionSingularityBox->setValue(std::to_string(size));
    return true;
}

void Viewer::drawContents() {
    if (!mProcessEvents) {
        mFBO.blit();
        return;
    }

#ifdef VISUALIZE_ERROR
    static int lastUpdate = -1;
    if (lastUpdate != mRes.iterationsQ() + mRes.iterationsO()) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        VectorXf error = mOptimizer.error();
        char header[20], footer[20];
        memset(header, 0, 20);
        if (error.size() > 0)
            snprintf(header, 20, "%.2e", error[error.size()-1]);
        snprintf(footer, 20, "Iteration %i", mRes.iterationsQ());
        for (int i=0; i<error.size(); ++i)
            error[i] = std::log(error[i]);
        if (error.size() > 0) {
            error.array() -= error.minCoeff();
            error.array() /= error.maxCoeff();
        }
        mGraph->setValues(error);
        mGraph->setHeader(header);
        mGraph->setFooter(footer);
        lastUpdate = mRes.iterationsQ() + mRes.iterationsO();
    }
#endif

    bool canRefresh = mRes.levels() > 0 && mDrawIndex == 0, overpaint = false;

    if (mOptimizer.active() && mSolveOrientationBtn->pushed())
        mSolveOrientationBtn->setProgress(mOptimizer.progress());
    else
        mSolveOrientationBtn->setProgress(1.f);

    if (mOptimizer.active() && mSolvePositionBtn->pushed())
        mSolvePositionBtn->setProgress(mOptimizer.progress());
    else
        mSolvePositionBtn->setProgress(1.f);

    if (canRefresh && (mLayers[OrientationFieldSingularities]->checked() ||
                       mOrientationAttractor->pushed()) &&
        refreshOrientationSingularities())
        repaint();

    if (canRefresh && (mLayers[PositionFieldSingularities]->checked() ||
                       mPositionAttractor->pushed()) &&
        refreshPositionSingularities())
        repaint();

    if (canRefresh && mLayers[FlowLines]->checked() && !mSolveOrientationBtn->pushed() &&
        mRes.iterationsQ() != mFlowLineShader.attribVersion("position"))
        traceFlowLines();

    if (!mOptimizer.active()) {
        if (mSolveOrientationBtn->pushed()) {
            mSolveOrientationBtn->setPushed(false);
            refreshOrientationSingularities();
            if (mContinueWithPositions) {
                mSolvePositionBtn->setPushed(true);
                mSolvePositionBtn->changeCallback()(true);
                mContinueWithPositions = false;
            }
        }
        if (mSolvePositionBtn->pushed()) {
            mSolvePositionBtn->setPushed(false);
            refreshPositionSingularities();
        }
    }

    if (canRefresh && (mLayers[OrientationField]->checked() ||
                       (mLayers[InputMesh]->checked() &&
                        mVisualizeBox->selectedIndex() == 1)) &&
        mRes.iterationsQ() != mMeshShader44.attribVersion("tangent")) {
        MatrixXf Q_gpu(3, mRes.size() + mCreaseMap.size());
        Q_gpu.topLeftCorner(3, mRes.size()) = mRes.Q(0);
        for (auto &c : mCreaseMap)
            Q_gpu.col(c.first) = Q_gpu.col(c.second);
        int version = mRes.iterationsQ();
        mMeshShader44.bind();
        mMeshShader44.uploadAttrib("tangent", Q_gpu, version);
        repaint();
    }

    if (canRefresh && (mLayers[PositionField]->checked() ||
                       (mLayers[InputMesh]->checked() ||
                        mVisualizeBox->selectedIndex() == 1)) &&
        mRes.iterationsO() != mMeshShader44.attribVersion("uv")) {
        MatrixXf O_gpu(3, mRes.size() + mCreaseMap.size());
        O_gpu.topLeftCorner(3, mRes.size()) = mRes.O(0);
        for (auto &c : mCreaseMap)
            O_gpu.col(c.first) = O_gpu.col(c.second);
        int version = mRes.iterationsO();
        mMeshShader44.bind();
        mMeshShader44.uploadAttrib("uv", O_gpu, version);
        if (mLayers[PositionField]->checked())
            repaint();
        else if (mLayers[InputMesh]->checked() && !mNeedsRepaint)
            overpaint = true;
    }

    if (!mNeedsRepaint && !overpaint) {
        mFBO.blit();
        drawOverlay();
        return;
    }

    mFBO.bind();
    if (mDrawIndex == 0 && !overpaint) {
        glClearColor(mBackground[0], mBackground[1], mBackground[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    Eigen::Matrix4f model, view, proj;
    computeCameraMatrices(model, view, proj);
    Eigen::Vector4f civ =
        (view * model).inverse() * Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f);

    if (mRes.levels() == 0) {
        mFBO.release();
        mFBO.blit();
        return;
    }

    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);

    bool pointcloud = mRes.F().size() == 0 && mRes.V().size() > 0;

    std::function<void(uint32_t, uint32_t)> drawFunctor[LayerCount];
    drawFunctor[InputMesh] = [&](uint32_t offset, uint32_t count) {
        glDepthFunc(overpaint ? GL_EQUAL : GL_LEQUAL);
        SerializableGLShader *shader = nullptr;
        if (mOptimizer.posy() == 4) {
            if (!pointcloud)
                shader = mOptimizer.rosy() == 2 ? &mMeshShader24 : &mMeshShader44;
            else
                shader = mOptimizer.rosy() == 2 ? &mPointShader24 : &mPointShader44;
        } else {
            if (!pointcloud)
                shader = &mMeshShader63;
            else
                shader = &mPointShader63;
        }
        bool show_uvs = mMeshShader44.attribVersion("uv") > 0 &&
                        mVisualizeBox->selectedIndex() == 1;
        shader->bind();
        shader->setUniform("show_uvs", show_uvs ? 1.0f : 0.0f);
        shader->setUniform("light_position", Vector3f(0.0f, 0.3f, 5.0f));
        shader->setUniform("model", model);
        shader->setUniform("view", view);
        shader->setUniform("proj", proj);
        if (!pointcloud) {
            shader->setUniform("camera_local", Vector3f(civ.head(3)));
            shader->setUniform("scale", mRes.scale());
        } else {
            shader->setUniform("point_size", (Float) mMeshStats.mAverageEdgeLength);
        }
        shader->setUniform("inv_scale", 1.0f / mRes.scale());
        shader->setUniform("fixed_color", Vector4f(Vector4f::Zero()));
        shader->setUniform("base_color", mBaseColor, false);
        shader->setUniform("specular_color", mSpecularColor, false);
        shader->setUniform("interior_factor", mInteriorFactor, false);
        shader->setUniform("edge_factor_0", mEdgeFactor0, false);
        shader->setUniform("edge_factor_1", mEdgeFactor1, false);
        shader->setUniform("edge_factor_2", mEdgeFactor2, false);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0, 1.0);
        if (!pointcloud)
            shader->drawIndexed(GL_TRIANGLES, offset, count);
        else
            shader->drawArray(GL_POINTS, offset, count);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthFunc(GL_LEQUAL);
    };

    drawFunctor[InputMeshWireframe] = [&](uint32_t offset, uint32_t count) {
        if (mFBO.samples() == 1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        mMeshShader44.bind();
        mMeshShader44.setUniform("show_uvs", 0.0f);
        mMeshShader44.setUniform("model", model);
        mMeshShader44.setUniform("view", view);
        mMeshShader44.setUniform("proj", proj);
        mMeshShader44.setUniform("fixed_color", Vector4f(0.1f, 0.1f, 0.2f, 1.0f));
        mMeshShader44.setUniform("camera_local", Vector3f(civ.head(3)));
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        mMeshShader44.drawIndexed(GL_TRIANGLES, offset, count);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        if (mFBO.samples() == 1)
            glDisable(GL_BLEND);
    };

    drawFunctor[OrientationField] = [&](uint32_t offset, uint32_t count) {
        mOrientationFieldShader.bind();
        mOrientationFieldShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mOrientationFieldShader.setUniform("offset", (Float) mMeshStats.mAverageEdgeLength * (1.0f/5.0f));
        mOrientationFieldShader.setUniform("scale", (Float) mMeshStats.mAverageEdgeLength * 0.4f
                * std::pow((Float) 2, mOrientationFieldSizeSlider->value() * 4 - 2));
        mOrientationFieldShader.setUniform("rosy", mOptimizer.rosy());
        mOrientationFieldShader.drawArray(GL_POINTS, offset, count);
    };

    drawFunctor[FlowLines] = [&](uint32_t offset, uint32_t count) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        mFlowLineShader.bind();
        mFlowLineShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mFlowLineShader.setUniform("alpha", 0.5f);
        mFlowLineShader.drawIndexed(GL_TRIANGLES, offset, count);
        glDisable(GL_BLEND);
    };

    drawFunctor[OrientationFieldSingularities] = [&](uint32_t offset, uint32_t count) {
        mOrientationSingularityShader.bind();
        mOrientationSingularityShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mOrientationSingularityShader.setUniform("point_size", mRes.scale() * 0.4f
         * std::pow((Float) 2, mOrientationFieldSingSizeSlider->value() * 4 - 2));
        mOrientationSingularityShader.drawArray(GL_POINTS, offset, count);
    };

    drawFunctor[PositionField] = [&](uint32_t offset, uint32_t count) {
        mPositionFieldShader.bind();
        mPositionFieldShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mPositionFieldShader.setUniform("scale", (Float) mMeshStats.mAverageEdgeLength);
        mPositionFieldShader.setUniform("fixed_color", Eigen::Vector3f(0.5f, 1.0f, 0.5f));
        glPointSize(3.0f * mPixelRatio);
        mPositionFieldShader.drawArray(GL_POINTS, offset, count);
    };

    drawFunctor[PositionFieldSingularities] = [&](uint32_t offset, uint32_t count) {
        mPositionSingularityShader.bind();
        mPositionSingularityShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mPositionSingularityShader.setUniform("point_size", mRes.scale() * 0.4f
         * std::pow((Float) 2, mPositionFieldSingSizeSlider->value() * 4 - 2));
        mPositionSingularityShader.drawArray(GL_POINTS, offset, count);
    };

    drawFunctor[OutputMesh] = [&](uint32_t offset, uint32_t count) {
        mOutputMeshShader.bind();
        mOutputMeshShader.setUniform("model", model);
        mOutputMeshShader.setUniform("view", view);
        mOutputMeshShader.setUniform("proj", proj);
        mOutputMeshShader.setUniform("light_position", Vector3f(0.0f, 0.3f, 5.0f));
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0, 1.0);
        mOutputMeshShader.drawIndexed(GL_TRIANGLES, offset, count);
        glDisable(GL_POLYGON_OFFSET_FILL);
    };

    drawFunctor[OutputMeshWireframe] = [&](uint32_t offset, uint32_t count) {
        if (mFBO.samples() == 1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        mOutputMeshWireframeShader.bind();
        mOutputMeshWireframeShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mOutputMeshWireframeShader.drawArray(GL_LINES, offset, count);
        if (mFBO.samples() == 1)
            glDisable(GL_BLEND);
    };

    drawFunctor[FaceLabels] = [&](uint32_t offset, uint32_t count) {
        nvgBeginFrame(mNVGContext, mSize[0], mSize[1], mPixelRatio);
        nvgFontSize(mNVGContext, 14.0f);
        nvgFontFace(mNVGContext, "sans-bold");
        nvgTextAlign(mNVGContext, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        const MatrixXf &V = mRes.V(), &N = mRes.N();
        const MatrixXu &F = mRes.F();
        nvgFillColor(mNVGContext, Color(200, 200, 255, 200));

        for (uint32_t i=offset; i<offset+count; ++i) {
            Vector4f pos;
            pos << (1.0f / 3.0f) * (V.col(F(0, i)) + V.col(F(1, i)) +
                                    V.col(F(2, i))).cast<float>(), 1.0f;
            Vector3f n = (N.col(F(0, i)) + N.col(F(1, i)) + N.col(F(2, i))).normalized();

            Vector3f ray_origin = pos.head<3>() + n * pos.cwiseAbs().maxCoeff() * 1e-4f;
            Eigen::Vector3f coord = project(Vector3f((model * pos).head<3>()), view, proj, mSize);
            if (coord.x() < -50 || coord.x() > mSize[0] + 50 || coord.y() < -50 || coord.y() > mSize[1] + 50)
                continue;
            if (!mBVH->rayIntersect(Ray(ray_origin, civ.head<3>() - ray_origin, 0.0f, 1.1f)))
                nvgText(mNVGContext, coord.x(), mSize[1] - coord.y(), std::to_string(i).c_str(), nullptr);
        }
        nvgEndFrame(mNVGContext);
    };

    drawFunctor[VertexLabels] = [&](uint32_t offset, uint32_t count) {
        nvgBeginFrame(mNVGContext, mSize[0], mSize[1], mPixelRatio);
        nvgFontSize(mNVGContext, 14.0f);
        nvgFontFace(mNVGContext, "sans-bold");
        nvgTextAlign(mNVGContext, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        const MatrixXf &V = mRes.V(), &N = mRes.N();
        nvgFillColor(mNVGContext, Color(200, 255, 200, 200));
        for (uint32_t i=offset; i<offset+count; ++i) {
            Vector4f pos;
            pos << V.col(i).cast<float>(), 1.0f;
            Vector3f n = N.col(i);

            Vector3f ray_origin = pos.head<3>() + n * pos.cwiseAbs().maxCoeff() * 1e-4f;
            Eigen::Vector3f coord = project(Vector3f((model * pos).head<3>()), view, proj, mSize);
            if (coord.x() < -50 || coord.x() > mSize[0] + 50 || coord.y() < -50 || coord.y() > mSize[1] + 50)
                continue;
            if (!mBVH->rayIntersect(Ray(ray_origin, civ.head<3>() - ray_origin, 0.0f, 1.1f)))
                nvgText(mNVGContext, coord.x(), mSize[1] - coord.y(), std::to_string(i).c_str(), nullptr);
        }
        nvgEndFrame(mNVGContext);
    };

    uint32_t drawAmount[LayerCount], blockSize[LayerCount];
    bool checked[LayerCount];
    drawAmount[InputMesh] = !pointcloud ? mRes.F().cols() : mRes.V().cols();
    drawAmount[InputMeshWireframe] = mRes.F().cols();
    drawAmount[OrientationField] = mRes.size();
    drawAmount[FlowLines] = mFlowLineFaces;
    drawAmount[PositionField] = mRes.size();
    drawAmount[OrientationFieldSingularities] = mOrientationSingularities.size();
    drawAmount[PositionFieldSingularities] = mPositionSingularities.size();
    drawAmount[OutputMesh] = mOutputMeshFaces;
    drawAmount[OutputMeshWireframe] = mOutputMeshLines;
    drawAmount[FaceLabels] = mRes.F().cols();
    drawAmount[VertexLabels] = mRes.size();

    for (int i=0; i<LayerCount; ++i)
        checked[i] = mLayers[i]->checked();

    checked[OrientationFieldSingularities] |= mOrientationAttractor->pushed();
    checked[PositionFieldSingularities] |= mPositionAttractor->pushed();

    for (int i=0; i<LayerCount; ++i) {
        blockSize[i] = 200000;
        if (checked[i] == false)
            drawAmount[i] = 0;
    }

    blockSize[InputMesh] = blockSize[OutputMesh] = blockSize[FlowLines] = 1000000;
    blockSize[FaceLabels] = 20000;
    blockSize[VertexLabels] = 20000;

    const int drawOrder[] = {
        InputMesh,
        InputMeshWireframe,
        OrientationField,
        OrientationFieldSingularities,
        PositionField,
        PositionFieldSingularities,
        OutputMesh,
        OutputMeshWireframe,
        FlowLines,
        FaceLabels,
        VertexLabels
    };

    if (mFBO.samples() == 1) {
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    }

    bool finished = true;
    for (uint64_t j=0, base = 0; j<sizeof(drawOrder)/sizeof(int); ++j) {
        uint32_t i = drawOrder[j];

        if (mDrawIndex - base < drawAmount[i]) {
            uint32_t remaining = drawAmount[i] - (mDrawIndex - base);
            uint32_t drawNow = std::min(blockSize[i], remaining);
            drawFunctor[i](mDrawIndex - base, drawNow);
            mDrawIndex += drawNow;
            if (drawNow < remaining) {
                finished = false;
                break;
            }
        }
        base += drawAmount[i];
    }

    if (mFBO.samples() == 1)
        glDisable(GL_LINE_SMOOTH);

    if (finished) {
        mNeedsRepaint = false;
        mDrawIndex = 0;
    } else {
        mNeedsRepaint = true;
        glfwPostEmptyEvent();
    }

    mFBO.release();
    mFBO.blit();
    drawOverlay();
}

void Viewer::refreshStrokes() {
    if (mRes.F().size() == 0)
        return;
    const Float thickness = mMeshStats.mAverageEdgeLength / 1.5f;
    const Float eps = mMeshStats.mAverageEdgeLength / 3.0f;

    uint32_t nVertices = 0, nTriangles = 0;
    for (auto const &curve : mStrokes) {
        nVertices += curve.second.size() * 2;
        nTriangles += (curve.second.size() - 1) * 2;
    }
    MatrixXu indices(3, nTriangles);
    MatrixXf position(3, nVertices);
    MatrixXu8 color(4, nVertices);

    uint32_t triangleIdx = 0, vertexIdx = 0;

    for (auto const &stroke : mStrokes) {
        auto const &curve = stroke.second;
        Vector4u8 cvalue;
        {
            pcg32 rng;
            rng.seed(1, union_cast<uint32_t>(curve[0].p.sum()));
            Float hue = std::fmod(rng.nextUInt(100)*0.61803398f, 1.f);
            cvalue << (hsv_to_rgb(hue, 1.0f, 1.f) * 255).cast<uint8_t>(), (uint8_t) 220;
        }
        bool edgeStroke = stroke.first == 1;

        position.col(vertexIdx) = curve[0].p;
        color.col(vertexIdx++) << cvalue[0], cvalue[1], cvalue[2], (uint8_t) 0x00;
        position.col(vertexIdx) = curve[0].p;
        color.col(vertexIdx++) << cvalue[0], cvalue[1], cvalue[2], (uint8_t) (edgeStroke ? 0xFF : 0x00);

        for (uint32_t i=1; i<curve.size(); ++i) {
            indices.col(triangleIdx++) << vertexIdx, vertexIdx-2, vertexIdx-1;
            indices.col(triangleIdx++) << vertexIdx, vertexIdx-1, vertexIdx+1;

            Vector3f t = curve[i].p - curve[i-1].p, n = curve[i].n;
            t = n.cross((t - n.dot(t) * n).normalized());
            Float thicknessP = (1-std::pow((2*((i+1) / (Float) curve.size())-1), 2.0f)) * thickness;
            position.col(vertexIdx) = curve[i].p + t * thicknessP + n*eps;
            color.col(vertexIdx++) << cvalue[0], cvalue[1], cvalue[2], (uint8_t) 0x00;
            position.col(vertexIdx) = curve[i].p - t * thicknessP + n*eps;
            color.col(vertexIdx++) << cvalue[0], cvalue[1], cvalue[2], (uint8_t) (edgeStroke ? 0xFF : 0x00);
        }
    }

    mStrokeShader.bind();
    mStrokeShader.uploadAttrib("position", position);
    mStrokeShader.uploadAttrib("color", color);
    mStrokeShader.uploadIndices(indices);
    mStrokeFaces = nTriangles;

    const MatrixXu &F = mRes.F();
    const MatrixXf &N = mRes.N(), &V = mRes.V();
    const VectorXu &E2E = mRes.E2E();

    mRes.clearConstraints();
    if (mAlignToBoundariesBox->checked()) {
        for (uint32_t i=0; i<3*F.cols(); ++i) {
            if (E2E[i] == INVALID) {
                uint32_t i0 = F(i%3, i/3);
                uint32_t i1 = F((i+1)%3, i/3);
                Vector3f p0 = V.col(i0), p1 = V.col(i1);
                Vector3f edge = p1-p0;
                if (edge.squaredNorm() > 0) {
                    edge.normalize();
                    mRes.CO().col(i0) = p0;
                    mRes.CO().col(i1) = p1;
                    mRes.CQ().col(i0) = mRes.CQ().col(i1) = edge;
                    mRes.CQw()[i0] = mRes.CQw()[i1] = mRes.COw()[i0] =
                        mRes.COw()[i1] = 1.0f;
                }
            }
        }
    }

    for (auto const &stroke : mStrokes) {
        auto const &curve = stroke.second;
        for (uint32_t i=0; i<curve.size(); ++i) {
            Vector3f tangent;
            if (i == 0)
                tangent = curve[1].p - curve[0].p;
            else if (i == curve.size() - 1)
                tangent = curve[curve.size()-1].p - curve[curve.size()-2].p;
            else
                tangent = curve[i+1].p - curve[i-1].p;
            tangent.normalize();

            for (int j=0; j<3; ++j) {
                uint32_t v = F(j, curve[i].f);
                Vector3f tlocal = tangent;
                tlocal -= tlocal.dot(N.col(v)) * N.col(v);
                tlocal.normalize();

                mRes.CQ().col(v) = tlocal;
                mRes.CQw()[v] = 1.0f;

                if (stroke.first == 1) {
                    mRes.CO().col(v) = curve[i].p;
                    mRes.COw()[v] = 1.0f;
                }
            }
        }
    }
    mRes.propagateConstraints(mOptimizer.rosy(), mOptimizer.posy());
}

void Viewer::drawOverlay() {
    if (mRes.F().size() == 0)
        return;
    std::string message = "";

    Eigen::Matrix4f model, view, proj;
    computeCameraMatrices(model, view, proj);

    if (mLayers[BrushStrokes]->checked() || toolActive()) {
        mStrokeShader.bind();
        mStrokeShader.setUniform("mvp", Eigen::Matrix4f(proj * view * model));
        mStrokeShader.setUniform("alpha", 0.85f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        mStrokeShader.drawIndexed(GL_TRIANGLES, 0, mStrokeFaces);
        glDisable(GL_BLEND);
    }

    if (mOrientationComb->pushed())
        message = "Selected tool: Orientation Comb";
    else if (mOrientationAttractor->pushed())
        message = "Selected tool: Orientation Singularity Attractor";
    else if (mOrientationScareBrush->pushed())
        message = "Selected tool: Orientation Singularity Scaring Brush";
    else if (mEdgeBrush->pushed())
        message = "Selected tool: Edge Brush";
    else if (mPositionAttractor->pushed())
        message = "Selected tool: Position Singularity Attractor";
    else if (mPositionScareBrush->pushed())
        message = "Selected tool: Position Singularity Scaring Brush";
    else
        return;

    auto ctx = mNVGContext;
    int delIcon = nvgImageIcon(ctx, delete_stroke);

    nvgBeginFrame(ctx, mSize[0], mSize[1], mPixelRatio);

    if (!mScreenCurve.empty()) {
        nvgBeginPath(ctx);
        nvgStrokeColor(ctx, Color(255, 100));
        nvgStrokeWidth(ctx, 4);
        nvgMoveTo(ctx, mScreenCurve[0].x(), mScreenCurve[0].y());

        for (uint32_t i=1; i<mScreenCurve.size(); ++i)
            nvgLineTo(ctx, mScreenCurve[i].x(), mScreenCurve[i].y());
        nvgStroke(ctx);
    }

    /* Tool indicator */
    int height = 20, width;
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, height);
    width = nvgTextBounds(ctx, 0, 0, message.c_str(), nullptr, nullptr);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, mSize[0] - width - 15, 10, width + 10, height + 10, 3);
    nvgFillColor(ctx, Color(0, 100));
    nvgFill(ctx);
    nvgFillColor(ctx, Color(255, 255));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(ctx, mSize[0] - width - 10, 15, message.c_str(), nullptr);

    Eigen::Vector4f civ =
        (view * model).inverse() * Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f);

    for (auto const &stroke : mStrokes) {
        auto const &curve = stroke.second;
        Vector4f pos;
        pos << curve[0].p + curve[0].n * mMeshStats.mAverageEdgeLength/10, 1.0f;
        Eigen::Vector3f coord = project(Vector3f((model * pos).head<3>()), view, proj, mSize);
        coord.y() = mSize[1] - coord.y() - 16; coord.x() -= 16;

        if (!mBVH->rayIntersect(Ray(pos.head<3>(), (civ-pos).head<3>(), 0.0f, 1.0f))) {
            NVGpaint imgPaint = nvgImagePattern(
                ctx, coord.x(), coord.y(), 32, 32, 0, delIcon, 0.6f);
            nvgBeginPath(ctx);
            nvgRect(ctx, coord.x(), coord.y(), 32, 32);
            nvgFillPaint(ctx, imgPaint);
            nvgFill(ctx);
        }
    }

    nvgEndFrame(ctx);
}

void Viewer::repaint() {
    mDrawIndex = 0;
    mNeedsRepaint = true;
    glfwPostEmptyEvent();
}

bool Viewer::scrollEvent(const Vector2i &p, const Vector2f &rel) {
    if (!Screen::scrollEvent(p, rel)) {
        mCamera.zoom = std::max(0.1, mCamera.zoom * (rel.y() > 0 ? 1.1 : 0.9));
        repaint();
    }
    return true;
}

bool Viewer::toolActive() const {
    return
        mOrientationComb->pushed() || mOrientationAttractor->pushed() || mOrientationScareBrush->pushed() ||
        mEdgeBrush->pushed() || mPositionAttractor->pushed() || mPositionScareBrush->pushed();
}

bool Viewer::mouseMotionEvent(const Vector2i &p, const Vector2i &rel,
                              int button, int modifiers) {
    if (mDrag && toolActive()) {
        mScreenCurve.push_back(p);
        return true;
    }

    if (!Screen::mouseMotionEvent(p, rel, button, modifiers)) {
        if (mCamera.arcball.motion(p)) {
            repaint();
        } else if (mTranslate) {
            Eigen::Matrix4f model, view, proj;
            computeCameraMatrices(model, view, proj);
            float zval = project(mMeshStats.mWeightedCenter.cast<float>(), view * model, proj, mSize).z();
            Eigen::Vector3f pos1 = unproject(Eigen::Vector3f(p.x(), mSize.y() - p.y(), zval), view * model, proj, mSize);
            Eigen::Vector3f pos0 = unproject(Eigen::Vector3f(mTranslateStart.x(), mSize.y() - mTranslateStart.y(), zval), view * model, proj, mSize);
            mCamera.modelTranslation = mCamera.modelTranslation_start + (pos1-pos0);
            repaint();
        }
    }
    return true;
}

bool Viewer::mouseButtonEvent(const Vector2i &p, int button, bool down, int modifiers) {
    if (!Screen::mouseButtonEvent(p, button, down, modifiers)) {
        if (toolActive()) {
            bool drag = down && button == GLFW_MOUSE_BUTTON_1;
            if (drag == mDrag)
                return false;
            Eigen::Matrix4f model, view, proj;
            computeCameraMatrices(model, view, proj);
            Eigen::Vector4f civ =
                (view * model).inverse() * Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
            mDrag = drag;
            bool attractor = mOrientationAttractor->pushed() || mPositionAttractor->pushed();

            if (drag) {
                /* Check whether a stroke should be deleted */
                mScreenCurve.clear();
                if (!attractor) {
                    for (auto it = mStrokes.begin(); it != mStrokes.end(); ++it) {
                        auto &curve = it->second;
                        Vector4f pos;
                        pos << curve[0].p + curve[0].n * mMeshStats.mAverageEdgeLength/10, 1.0f;
                        Eigen::Vector3f coord = project(Vector3f((model * pos).head<3>()), view, proj, mSize);
                        coord.y() = mSize[1] - coord.y();
                        if ((coord.head<2>() - p.cast<float>()).norm() > 16)
                            continue;
                        if (!mBVH->rayIntersect(Ray(pos.head<3>(), (civ-pos).head<3>(), 0.0f, 1.0f))) {
                            mStrokes.erase(it);
                            mDrag = false;
                            refreshStrokes();
                            mContinueWithPositions = mEdgeBrush->pushed();
                            mSolveOrientationBtn->setPushed(true);
                            mSolveOrientationBtn->changeCallback()(true);
                            return true;
                        }
                    }
                }
                mScreenCurve.push_back(p);
            } else {
                std::vector<CurvePoint> curve;
                const MatrixXf &N = mRes.N();
                const MatrixXu &F = mRes.F();

                for (uint32_t i=0; i<mScreenCurve.size(); ++i) {
                    Eigen::Vector3f pos1 = unproject(Eigen::Vector3f(mScreenCurve[i].x(), mSize.y() - mScreenCurve[i].y(), 0.0f), view * model, proj, mSize);
                    Eigen::Vector3f pos2 = unproject(Eigen::Vector3f(mScreenCurve[i].x(), mSize.y() - mScreenCurve[i].y(), 1.0f), view * model, proj, mSize);

                    Ray ray(pos1, (pos2-pos1).normalized());
                    Vector2f uv;
                    uint32_t f;
                    Float t;

                    if (!mBVH->rayIntersect(ray, f, t, &uv)) {
                        mScreenCurve.clear();
                        return false;
                    }

                    CurvePoint pt;
                    pt.p = ray(t);
                    pt.n = ((1 - uv.sum()) * N.col(F(0, f)) + uv.x() * N.col(F(1, f)) + uv.y() * N.col(F(2, f))).normalized();
                    pt.f = f;
                    curve.push_back(pt);
                }
                mScreenCurve.clear();
                int strokeType = 0;
                if (mEdgeBrush->pushed())
                    strokeType = 1;

                if (smooth_curve(mBVH, mRes.E2E(), curve, attractor)) {
                    if (attractor) {
                        std::vector<uint32_t> curve_faces;
                        for (auto it = curve.rbegin(); it != curve.rend(); ++it)
                            curve_faces.push_back(it->f);
                        setLevel(0);
                        if (mOrientationAttractor->pushed()) {
                            mSolveOrientationBtn->setPushed(true);
                            mSolveOrientationBtn->changeCallback()(true);
                        } else {
                            mSolvePositionBtn->setPushed(true);
                            mSolvePositionBtn->changeCallback()(true);
                        }
                        mOptimizer.moveSingularity(curve_faces, mOrientationAttractor->pushed());
                    } else {
                        mStrokes.push_back(std::make_pair(strokeType, curve));
                        refreshStrokes();
                        mContinueWithPositions = mEdgeBrush->pushed();
                        mSolveOrientationBtn->setPushed(true);
                        mSolveOrientationBtn->changeCallback()(true);
                    }
                }
            }
        } else if (button == GLFW_MOUSE_BUTTON_1 && modifiers == 0) {
            mCamera.arcball.button(p, down);
        } else if (button == GLFW_MOUSE_BUTTON_2 ||
                   (button == GLFW_MOUSE_BUTTON_1 && modifiers == GLFW_MOD_SHIFT)) {
            mCamera.modelTranslation_start = mCamera.modelTranslation;
            mTranslate = true;
            mTranslateStart = p;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_1 && !down)
        mCamera.arcball.button(p, false);
    if (!down) {
        mDrag = false;
        mTranslate = false;
    }
    return true;
}

void Viewer::loadInput(std::string filename, Float creaseAngle, Float scale,
                      int face_count, int vertex_count, int rosy, int posy, int knn_points) {
    std::string extension;
    if (filename.size() > 4)
        extension = str_tolower(filename.substr(filename.size()-4));

    if (filename.empty()) {
        filename = nanogui::file_dialog({
            {"obj", "Wavefront OBJ"},
            {"ply", "Stanford PLY"},
            {"aln", "Aligned point cloud"}
        }, false);
        if (filename == "")
            return;
    } else if (extension != ".ply" && extension != ".obj" && extension != ".aln")
        filename = filename + ".ply";

    if (!std::isfinite(creaseAngle)) {
        if (filename.find("fandisk") != std::string::npos || filename.find("cube_twist") != std::string::npos)
            creaseAngle = 20;
        else
            creaseAngle = -1;
    }

    /* Load triangle mesh data */
    MatrixXu F, F_gpu;
    MatrixXf V, N, V_gpu, N_gpu;
    VectorXf A;
    AdjacencyMatrix adj = nullptr;

    mOperationStart = mLastProgressMessage = glfwGetTime();
    mProcessEvents = false;
    glfwMakeContextCurrent(nullptr);

    try {
        load_mesh_or_pointcloud(filename, F, V, N, mProgress);
    } catch (const std::exception &e) {
        new MessageDialog(this, MessageDialog::Type::Warning, "Error", e.what());
        glfwMakeContextCurrent(mGLFWWindow);
        mProcessEvents = true;
        return;
    }
    mFilename = filename;
    bool pointcloud = F.size() == 0;

    {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        mOptimizer.stop();
    }

    if (mBVH) {
        delete mBVH;
        mBVH = nullptr;
    }

    mMeshStats = compute_mesh_stats(F, V, mDeterministic, mProgress);

    if (pointcloud) {
        mBVH = new BVH(&F, &V, &N, mMeshStats.mAABB);
        mBVH->build(mProgress);
        adj = generate_adjacency_matrix_pointcloud(V, N, mBVH, mMeshStats, knn_points, mDeterministic, mProgress);
        A.resize(V.cols());
        A.setConstant(1.0f);
    }

    if (scale < 0 && vertex_count < 0 && face_count < 0) {
        cout << "No target vertex count/face count/scale argument provided. "
                "Setting to the default of 1/16 * input vertex count." << endl;
        vertex_count = V.cols() / 16;
    }

    if (scale > 0) {
        Float face_area = posy == 4 ? (scale*scale) : (std::sqrt(3.f)/4.f*scale*scale);
        face_count = mMeshStats.mSurfaceArea / face_area;
        vertex_count = posy == 4 ? face_count : (face_count / 2);
    } else if (face_count > 0) {
        Float face_area = mMeshStats.mSurfaceArea / face_count;
        vertex_count = posy == 4 ? face_count : (face_count / 2);
        scale = posy == 4 ? std::sqrt(face_area) : (2*std::sqrt(face_area * std::sqrt(1.f/3.f)));
    } else if (vertex_count > 0) {
        face_count = posy == 4 ? vertex_count : (vertex_count * 2);
        Float face_area = mMeshStats.mSurfaceArea / face_count;
        scale = posy == 4 ? std::sqrt(face_area) : (2*std::sqrt(face_area * std::sqrt(1.f/3.f)));
    }

    cout << "Output mesh goals (approximate)" << endl;
    cout << "   Vertex count         = " << vertex_count << endl;
    cout << "   Face count           = " << face_count << endl;
    cout << "   Edge length          = " << scale << endl;

    if (!pointcloud) {
        /* Subdivide the mesh if necessary */
        if (mMeshStats.mMaximumEdgeLength*2 > scale || mMeshStats.mMaximumEdgeLength > mMeshStats.mAverageEdgeLength * 2) {
            VectorXu V2E, E2E;
            cout << "Input mesh is too coarse for the desired output edge length "
                    "(max input mesh edge length=" << mMeshStats.mMaximumEdgeLength
                 << "), subdividing .." << endl;
            build_dedge(F, V, V2E, E2E, mBoundaryVertices, mNonmanifoldVertices,
                        mProgress);
            subdivide(F, V, V2E, E2E, mBoundaryVertices,
                      mNonmanifoldVertices, std::min(scale/2, (Float) mMeshStats.mAverageEdgeLength*2), mDeterministic, mProgress);
            mMeshStats = compute_mesh_stats(F, V, mDeterministic, mProgress);
        }
    } else {
        mBoundaryVertices.resize(V.cols());
        mNonmanifoldVertices.resize(V.cols());
        mBoundaryVertices.setConstant(false);
        mNonmanifoldVertices.setConstant(false);
    }

    mRes.free();
    mRes.setF(std::move(F));
    mRes.setV(std::move(V));

    if (!pointcloud) {
        VectorXu V2E, E2E;
        /* Build a directed edge data structure */
        build_dedge(mRes.F(), mRes.V(), V2E, E2E, mBoundaryVertices,
                mNonmanifoldVertices, mProgress);

        /* Compute an adjacency matrix */
        //AdjacencyMatrix adj = generate_adjacency_matrix_cotan(
            //mRes.F(), mRes.V(), V2E, E2E, mNonmanifoldVertices, mProgress);
        adj = generate_adjacency_matrix_uniform(
            mRes.F(), V2E, E2E, mNonmanifoldVertices, mProgress);
        mRes.setAdj(std::move(adj));

        /* Generate crease normals. This changes F and V */
        mCreaseMap.clear();
        mCreaseSet.clear();
        if (creaseAngle >= 0) {
            V_gpu = mRes.V();
            F_gpu = mRes.F();
            generate_crease_normals(F_gpu, V_gpu, V2E, E2E, mBoundaryVertices,
                                    mNonmanifoldVertices, creaseAngle, N_gpu, mCreaseMap, mProgress);
            N = N_gpu.topLeftCorner(3, mRes.V().cols());
        } else {
            generate_smooth_normals(mRes.F(), mRes.V(), V2E, E2E, mNonmanifoldVertices, N, mProgress);
        }
        for (auto const &kv : mCreaseMap)
            mCreaseSet.insert(kv.second);
        mCreaseAngle = creaseAngle;

        compute_dual_vertex_areas(mRes.F(), mRes.V(), V2E, E2E, mNonmanifoldVertices, A);

        mRes.setE2E(std::move(E2E));
    }
    mRes.setAdj(std::move(adj));
    mRes.setN(std::move(N));
    mRes.setA(std::move(A));

    setTargetScale(scale);
    mRes.build(mDeterministic, mProgress);
    mRes.resetSolution();

    mStrokes.clear();
    if (!mBVH) {
        mBVH = new BVH(&mRes.F(), &mRes.V(), &mRes.N(), mMeshStats.mAABB);
        mBVH->build(mProgress);
    } else {
        mBVH->setData(&mRes.F(), &mRes.V(), &mRes.N());
    }

    mRes.printStatistics();
    mBVH->printStatistics();

    showProgress("Uploading to GPU", 0.0f);

    glfwMakeContextCurrent(mGLFWWindow);
    mMeshShader63.invalidateAttribs();
    mMeshShader44.invalidateAttribs();
    mMeshShader24.invalidateAttribs();
    mPointShader63.invalidateAttribs();
    mPointShader44.invalidateAttribs();
    mPointShader24.invalidateAttribs();
    mFlowLineShader.invalidateAttribs();
    mStrokeShader.invalidateAttribs();
    mOrientationFieldShader.invalidateAttribs();
    mPositionFieldShader.invalidateAttribs();
    mPositionSingularityShader.invalidateAttribs();
    mOrientationSingularityShader.invalidateAttribs();
    mOutputMeshWireframeShader.invalidateAttribs();
    mOutputMeshShader.invalidateAttribs();

    mMeshShader44.bind();

    if (V_gpu.size() > 0) {
        mMeshShader44.uploadAttrib("position", V_gpu);
        if (mUseHalfFloats)
            mMeshShader44.uploadAttrib_half("normal", N_gpu);
        else
            mMeshShader44.uploadAttrib("normal", N_gpu);
        MatrixXf N_data(3, mRes.size() + mCreaseMap.size());
        N_data.topLeftCorner(3, mRes.size()) = mRes.N();
        for (auto &c : mCreaseMap)
            N_data.col(c.first) = N_data.col(c.second);
        if (mUseHalfFloats)
            mMeshShader44.uploadAttrib_half("normal_data", N_data);
        else
            mMeshShader44.uploadAttrib("normal_data", N_data);
        N_data.resize(0, 0);
    } else {
        mMeshShader44.uploadAttrib("position", mRes.V());
        if (mUseHalfFloats)
            mMeshShader44.uploadAttrib_half("normal", mRes.N());
        else
            mMeshShader44.uploadAttrib("normal", mRes.N());
        if (mMeshShader44.hasAttrib("normal_data"))
            mMeshShader44.freeAttrib("normal_data");
    }

    MatrixXu8 C = MatrixXu8::Zero(4, V.cols());
    mMeshShader44.uploadAttrib("color", C);
    C.resize(0, 0);

    if (!pointcloud) {
        if (F_gpu.size() > 0)
            mMeshShader44.uploadIndices(F_gpu);
        else
            mMeshShader44.uploadIndices(mRes.F());
    }

    MatrixXf Q_gpu(3, mRes.size() + mCreaseMap.size());
    Q_gpu.topLeftCorner(3, mRes.size()) = mRes.Q(0);
    for (auto &c : mCreaseMap)
        Q_gpu.col(c.first) = Q_gpu.col(c.second);
    mMeshShader44.uploadAttrib("tangent", Q_gpu);
    Q_gpu.resize(0, 0);

    MatrixXf O_gpu(3, mRes.size() + mCreaseMap.size());
    O_gpu.topLeftCorner(3, mRes.size()) = mRes.O(0);
    for (auto &c : mCreaseMap)
        O_gpu.col(c.first) = O_gpu.col(c.second);
    mMeshShader44.uploadAttrib("uv", O_gpu);
    O_gpu.resize(0, 0);

    cout << endl << "GPU statistics:" << endl;
    cout << "    Vertex buffers      : " << memString(mMeshShader44.bufferSize()) << endl;

    shareGLBuffers();

    resetState();
    setSymmetry(rosy, posy);
    setTargetScale(scale);
    mScaleSlider->setHighlightedRange(std::make_pair(0.f, 0.f));

    if (!pointcloud) {
        /* Mark the range of target resolutions which will require re-tesselation */
        Float el = mMeshStats.mMaximumEdgeLength * 2;
        Float fc = mMeshStats.mSurfaceArea / (rosy == 4 ? (el*el) : (std::sqrt(3.f)/4.f*el*el));
        Float unsafe = std::log(posy == 4 ? fc : (fc / 2));
        Float min = std::log(std::min(100, (int) mRes.V().cols() / 10));
        Float max = std::log(2*mRes.V().cols());
        if (unsafe < max)
            mScaleSlider->setHighlightedRange(
                    std::make_pair((unsafe-min) / (max-min), 1.f));
    }

    mCamera.modelTranslation = -mMeshStats.mWeightedCenter.cast<float>();
    mCamera.modelZoom = 3.0f / (mMeshStats.mAABB.max - mMeshStats.mAABB.min).cwiseAbs().maxCoeff();
    mProgressWindow->setVisible(false);
    mProcessEvents = true;
}

void Viewer::shareGLBuffers() {
    if (!mMeshShader44.hasAttrib("position"))
        return;

    if (!mMeshShader44.hasAttrib("normal_data")) {
        mMeshShader44.bind();
        mMeshShader44.shareAttrib(mMeshShader44, "normal", "normal_data");
    }

    for (auto sh : { &mMeshShader24, &mMeshShader63, &mPointShader24, &mPointShader44, &mPointShader63 }) {
        sh->bind();
        sh->shareAttrib(mMeshShader44, "position");
        sh->shareAttrib(mMeshShader44, "normal");
        sh->shareAttrib(mMeshShader44, "tangent");
        sh->shareAttrib(mMeshShader44, "uv");
        sh->shareAttrib(mMeshShader44, "color");

        if (sh->name().find("mesh") != std::string::npos && mRes.F().size() > 0) {
            sh->shareAttrib(mMeshShader44,
                mMeshShader44.hasAttrib("normal_data") ? "normal_data" : "normal", "normal_data");
            sh->shareAttrib(mMeshShader44, "indices");
        }
    }

    mOrientationFieldShader.bind();
    mOrientationFieldShader.shareAttrib(mMeshShader44, "position");
    mOrientationFieldShader.shareAttrib(mMeshShader44, "normal");
    mOrientationFieldShader.shareAttrib(mMeshShader44, "tangent");

    mPositionFieldShader.bind();
    mPositionFieldShader.shareAttrib(mMeshShader44, "uv");
    mPositionFieldShader.shareAttrib(mMeshShader44, "normal");
}

void Viewer::showProgress(const std::string &_caption, Float value) {
    std::string caption = _caption + " ..";
    tbb::spin_mutex::scoped_lock lock(mProgressMutex);
    float newValue = mProgressBar->value();
    if (mProgressLabel->caption() != caption) {
        newValue = 0;
        mProgressLabel->setCaption(caption);
    }

    if (value >= 0)
        newValue = value; /* Positive: absolute progress values */
    else
        newValue -= value; /* Negative: relative progress values (OpenMP) */

    mProgressBar->setValue(newValue);

    double time = glfwGetTime();
    if (time - mLastProgressMessage < 0.05 &&
        (value != 0 || time - mOperationStart < 1))
        return;
    glfwMakeContextCurrent(mGLFWWindow);
    mProgressWindow->setVisible(true);
    Vector2i prefSize = mProgressWindow->preferredSize(mNVGContext);
    if (prefSize.x() > mProgressWindow->size().x()) {
        mProgressWindow->setSize(prefSize);
        mProgressWindow->performLayout(mNVGContext);
    }
    mProgressWindow->center();
    mProgressWindow->requestFocus();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(mNVGContext, mSize[0], mSize[1], mPixelRatio);
    draw(mNVGContext);
    nvgEndFrame(mNVGContext);
#if !defined(__APPLE__)
    glfwPollEvents();
#endif
    glfwSwapBuffers(mGLFWWindow);
    mLastProgressMessage = glfwGetTime();
    glfwMakeContextCurrent(nullptr);
}
