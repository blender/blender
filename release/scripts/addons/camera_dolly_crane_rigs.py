# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####


bl_info = {
    "name": "Add Camera Rigs",
    "author": "Wayne Dixon, Kris Wittig",
    "version": (1, 1, 1),
    "blender": (2, 77, 0),
    "location": "View3D > Add > Camera > Dolly or Crane Rig",
    "description": "Adds a Camera Rig with UI",
    "warning": "Enable Auto Run Python Scripts in User Preferences > File",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Rigging/Add_Camera_Rigs",
    "category": "Camera",
}

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from rna_prop_ui import rna_idprop_ui_prop_get
from math import radians


# =========================================================================
# Define the functions to build the Widgets
# =========================================================================
def create_widget(self, name):
    """ Creates an empty widget object for a bone, and returns the object."""
    obj_name = "WDGT_" + name
    scene = bpy.context.scene

    # Check if it already exists
    if obj_name in scene.objects:
        return None
    else:
        mesh = bpy.data.meshes.new(obj_name)
        obj = bpy.data.objects.new(obj_name, mesh)
        scene.objects.link(obj)

        # this will put the Widget objects out of the way on layer 19
        WDGT_layers = (False, False, False, False, False, False, False, False, False, True,
                       False, False, False, False, False, False, False, False, False, False)
        obj.layers = WDGT_layers

        return obj


def create_root_widget(self, name):
    # Creates a compass-shaped widget.

    obj = create_widget(self, name)
    if obj is not None:
        verts = [(0.2102552056312561, -0.0012103617191314697, 0.21025514602661133),
                 (0.11378927528858185, -0.001210339367389679, 0.274711549282074),
                 (-3.070153553608179e-08, -0.0012103626504540443, 0.29734566807746887),
                 (-0.11378933489322662, -0.0012103542685508728, 0.27471157908439636),
                 (-0.2102552056312561, -0.0012103617191314697, 0.21025516092777252),
                 (-0.27471160888671875, -0.0012103617191314697, 0.11378928273916245),
                 (-0.29734569787979126, -0.0012103617191314697, -1.6809221392577456e-07),
                 (0.29734572768211365, -0.001210331916809082, -1.0901101177296368e-07),
                 (0.2747114598751068, -0.0012103617191314697, 0.11378948390483856),
                 (0.07152898609638214, -0.0012103691697120667, 0.5070746541023254),
                 (-0.07152895629405975, -0.0012103617191314697, 0.5070746541023254),
                 (-0.07152898609638214, -0.0012103915214538574, 0.38030144572257996),
                 (0.07152898609638214, -0.0012103691697120667, 0.38030144572257996),
                 (-0.1325872540473938, -0.0012103617191314697, 0.5070746541023254),
                 (0.13258719444274902, -0.0012103617191314697, 0.5070746541023254),
                 (-3.070154264150915e-08, -0.0012104818597435951, 0.6688110828399658),
                 (-0.274711549282074, -0.0012103617191314697, -0.11378948390483856),
                 (0.274711549282074, -0.001210331916809082, -0.1137893795967102),
                 (0.21025514602661133, -0.001210331916809082, -0.21025525033473969),
                 (0.11378927528858185, -0.001210339367389679, -0.27471160888671875),
                 (-9.030617320604506e-08, -0.0012103328481316566, -0.29734572768211365),
                 (-0.11378933489322662, -0.0012103542685508728, -0.27471157908439636),
                 (-0.2102552056312561, -0.001210331916809082, -0.21025516092777252),
                 (-0.6688110828399658, -0.0012103915214538574, 5.982118267411352e-08),
                 (-0.5070747137069702, -0.0012103915214538574, 0.13258729875087738),
                 (-0.5070747137069702, -0.001210331916809082, -0.1325872540473938),
                 (-0.38030147552490234, -0.0012103617191314697, 0.07152903825044632),
                 (-0.38030147552490234, -0.0012103617191314697, -0.07152897119522095),
                 (-0.5070747137069702, -0.001210331916809082, -0.07152896374464035),
                 (-0.5070747137069702, -0.0012103915214538574, 0.07152900844812393),
                 (0.5070745944976807, -0.001210331916809082, -0.07152891904115677),
                 (0.5070745944976807, -0.001210331916809082, 0.07152905315160751),
                 (0.38030144572257996, -0.0012103617191314697, 0.07152903825044632),
                 (0.38030141592025757, -0.001210331916809082, -0.07152897119522095),
                 (0.5070745944976807, -0.001210331916809082, 0.13258734345436096),
                 (0.5070745944976807, -0.001210331916809082, -0.13258720934391022),
                 (0.6688110828399658, -0.001210331916809082, 5.279173720396102e-08),
                 (1.4811239168466273e-07, -0.001210303045809269, -0.6688110828399658),
                 (-0.13258716464042664, -0.0012103021144866943, -0.5070746541023254),
                 (0.13258737325668335, -0.0012103021144866943, -0.5070746541023254),
                 (-0.07152889668941498, -0.0012103617191314697, -0.38030150532722473),
                 (0.07152910530567169, -0.0012103095650672913, -0.38030150532722473),
                 (0.07152910530567169, -0.0012103095650672913, -0.5070746541023254),
                 (-0.07152886688709259, -0.0012103021144866943, -0.5070746541023254)]

        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (7, 8), (0, 8),
                 (10, 11), (9, 12), (11, 12), (10, 13), (9, 14), (13, 15), (14, 15),
                 (16, 22), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (7, 17),
                 (6, 16), (23, 24), (23, 25), (24, 29), (25, 28), (26, 27), (26, 29),
                 (27, 28), (31, 32), (30, 33), (32, 33), (31, 34), (30, 35), (34, 36),
                 (35, 36), (37, 38), (37, 39), (38, 43), (39, 42), (40, 41), (40, 43), (41, 42)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_camera_widget(self, name):
    # Creates a camera ctrl widget

    obj = create_widget(self, name)
    if obj is not None:
        verts = [(0.13756819069385529, 1.0706068032106941e-08, -0.13756819069385529),
                 (0.1797415018081665, 5.353034016053471e-09, -0.07445136457681656),
                 (0.19455081224441528, -6.381313819948996e-16, 8.504085435845354e-09),
                 (0.1797415018081665, -5.353034016053471e-09, 0.07445138692855835),
                 (0.13756819069385529, -1.0706068032106941e-08, 0.13756819069385529),
                 (0.07445137947797775, -2.1412136064213882e-08, 0.1797415018081665),
                 (-9.740904971522468e-08, -2.1412136064213882e-08, 0.19455081224441528),
                 (-5.87527146933553e-08, 2.1412136064213882e-08, -0.19455081224441528),
                 (0.0744515135884285, 2.1412136064213882e-08, -0.17974145710468292),
                 (0.3317747414112091, 5.353034016053471e-09, -0.04680081456899643),
                 (0.3317747414112091, -5.353034016053471e-09, 0.04680081456899643),
                 (0.24882805347442627, -5.353034016053471e-09, 0.04680081456899643),
                 (0.24882805347442627, 5.353034016053471e-09, -0.04680084437131882),
                 (0.3317747414112091, -5.353034016053471e-09, 0.08675074577331543),
                 (0.3317747414112091, 5.353034016053471e-09, -0.08675074577331543),
                 (0.43759751319885254, 0.0, 0.0), (-0.07445148378610611, -2.1412136064213882e-08, 0.17974145710468292),
                 (-0.07445141673088074, 2.1412136064213882e-08, -0.1797415018081665),
                 (-0.13756820559501648, 1.0706068032106941e-08, -0.1375681608915329),
                 (-0.1797415018081665, 5.353034016053471e-09, -0.07445136457681656),
                 (-0.19455081224441528, -1.2762627639897992e-15, 2.0872269246297037e-08),
                 (-0.1797415018081665, -5.353034016053471e-09, 0.07445140182971954),
                 (-0.1375681608915329, -1.0706068032106941e-08, 0.13756820559501648),
                 (5.1712785165136665e-08, -4.2824272128427765e-08, 0.43759751319885254),
                 (0.08675077557563782, -2.1412136064213882e-08, 0.3317747414112091),
                 (-0.08675073087215424, -2.1412136064213882e-08, 0.3317747414112091),
                 (0.046800870448350906, -2.1412136064213882e-08, 0.24882805347442627),
                 (-0.04680079594254494, -2.1412136064213882e-08, 0.24882805347442627),
                 (-0.04680079594254494, -2.1412136064213882e-08, 0.3317747414112091),
                 (0.04680084437131882, -2.1412136064213882e-08, 0.3317747414112091),
                 (-0.04680076241493225, 2.1412136064213882e-08, -0.3317747414112091),
                 (0.046800874173641205, 2.1412136064213882e-08, -0.3317747414112091),
                 (0.04680086299777031, 2.1412136064213882e-08, -0.24882805347442627),
                 (-0.046800799667835236, 2.1412136064213882e-08, -0.24882805347442627),
                 (0.0867508053779602, 2.1412136064213882e-08, -0.3317747414112091),
                 (-0.08675070106983185, 2.1412136064213882e-08, -0.3317747414112091),
                 (4.711345980012993e-08, 4.2824272128427765e-08, -0.43759751319885254),
                 (-0.43759751319885254, 1.0210102111918393e-14, -9.882624141255292e-08),
                 (-0.3317747414112091, -5.353034016053471e-09, 0.08675065636634827),
                 (-0.3317747414112091, 5.353034016053471e-09, -0.08675083518028259),
                 (-0.24882805347442627, -5.353034016053471e-09, 0.04680076986551285),
                 (-0.24882805347442627, 5.353034016053471e-09, -0.0468008853495121),
                 (-0.3317747414112091, 5.353034016053471e-09, -0.046800896525382996),
                 (-0.3317747414112091, -5.353034016053471e-09, 0.04680073633790016),
                 (-0.08263588696718216, -7.0564780685344886e-09, 0.08263592422008514),
                 (-0.10796899348497391, -3.5282390342672443e-09, 0.04472224414348602),
                 (-0.11686481535434723, -8.411977372806655e-16, 1.2537773486087644e-08),
                 (-0.10796899348497391, 3.5282390342672443e-09, -0.04472222551703453),
                 (-0.08263592422008514, 7.0564780685344886e-09, -0.08263588696718216),
                 (-0.04472225159406662, 7.0564780685344886e-09, -0.10796899348497391),
                 (-0.0447222925722599, -7.0564780685344886e-09, 0.10796897858381271),
                 (0.0447223074734211, 7.0564780685344886e-09, -0.10796897858381271),
                 (-3.529219583242593e-08, 7.0564780685344886e-09, -0.11686481535434723),
                 (-5.8512675593647145e-08, -7.0564780685344886e-09, 0.11686481535434723),
                 (0.04472222924232483, -7.0564780685344886e-09, 0.10796899348497391),
                 (0.08263590186834335, -7.0564780685344886e-09, 0.08263590186834335),
                 (0.10796899348497391, -3.5282390342672443e-09, 0.04472223296761513),
                 (0.11686481535434723, -4.2059886864033273e-16, 5.108323541946902e-09),
                 (0.10796899348497391, 3.5282390342672443e-09, -0.04472222924232483),
                 (0.08263590186834335, 7.0564780685344886e-09, -0.08263590186834335),
                 (3.725290298461914e-08, -2.1412136064213882e-08, 0.24882805347442627)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (7, 8), (0, 8),
                 (10, 11), (9, 12), (11, 12), (10, 13), (9, 14), (13, 15), (14, 15), (16, 22),
                 (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (7, 17), (6, 16), (23, 24),
                 (23, 25), (24, 29), (25, 28), (26, 29), (27, 28), (31, 32), (30, 33), (32, 33),
                 (31, 34), (30, 35), (34, 36), (35, 36), (37, 38), (37, 39), (38, 43), (39, 42),
                 (40, 41), (40, 43), (41, 42), (50, 53), (49, 52), (44, 45), (45, 46), (46, 47),
                 (47, 48), (48, 49), (44, 50), (51, 59), (51, 52), (53, 54), (54, 55), (55, 56),
                 (56, 57), (57, 58), (58, 59), (26, 60), (27, 60), (23, 60)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_aim_widget(self, name):
    """ Creates a camera aim widget."""

    obj = create_widget(self, name)
    if obj is not None:
        verts = [(0.15504144132137299, 1.4901161193847656e-08, 0.15504144132137299),
                 (0.20257140696048737, 7.450580596923828e-09, 0.0839078277349472),
                 (0.21926172077655792, -8.881784197001252e-16, -9.584233851001045e-09),
                 (0.20257140696048737, -7.450580596923828e-09, -0.0839078426361084),
                 (0.15504144132137299, -1.4901161193847656e-08, -0.15504144132137299),
                 (0.0839078351855278, -1.4901161193847656e-08, -0.20257140696048737),
                 (-1.0978147457763043e-07, -1.4901161193847656e-08, -0.21926172077655792),
                 (-6.621520043381679e-08, 1.4901161193847656e-08, 0.21926172077655792),
                 (0.08390798419713974, 1.4901161193847656e-08, 0.2025713473558426),
                 (0.39969685673713684, 3.725290298461914e-09, 0.05274524539709091),
                 (0.39969685673713684, -3.725290298461914e-09, -0.05274524167180061),
                 (0.4931790232658386, -3.725290298461914e-09, -0.05274524167180061),
                 (0.4931790232658386, 3.725290298461914e-09, 0.052745271474123),
                 (0.39969685673713684, -7.450580596923828e-09, -0.09776943176984787),
                 (0.39969685673713684, 7.450580596923828e-09, 0.09776943176984787),
                 (0.28043296933174133, 6.226862126577502e-17, -6.226862788321993e-17),
                 (-0.08390796184539795, -1.4901161193847656e-08, -0.2025713473558426),
                 (-0.08390787988901138, 1.4901161193847656e-08, 0.20257140696048737),
                 (-0.15504147112369537, 1.4901161193847656e-08, 0.1550414115190506),
                 (-0.20257140696048737, 7.450580596923828e-09, 0.08390782028436661),
                 (-0.21926172077655792, -1.7763568394002505e-15, -2.352336458955051e-08),
                 (-0.20257140696048737, -7.450580596923828e-09, -0.08390786498785019),
                 (-0.1550414115190506, -1.4901161193847656e-08, -0.15504147112369537),
                 (2.9140544199890428e-08, 2.9802322387695312e-08, 0.2804329991340637),
                 (-0.09776944667100906, 2.9802322387695312e-08, 0.3996969163417816),
                 (0.09776947647333145, 2.9802322387695312e-08, 0.3996969163417816),
                 (-0.052745264023542404, 2.9802322387695312e-08, 0.4931790828704834),
                 (0.05274529010057449, 2.9802322387695312e-08, 0.4931790828704834),
                 (0.052745264023542404, 2.9802322387695312e-08, 0.3996969163417816),
                 (-0.052745234221220016, 2.9802322387695312e-08, 0.3996969163417816),
                 (0.05274517461657524, -2.9802322387695312e-08, -0.3996969759464264),
                 (-0.052745334804058075, -2.9802322387695312e-08, -0.3996969759464264),
                 (-0.05274537205696106, -2.9802322387695312e-08, -0.49317920207977295),
                 (0.05274519696831703, -2.9802322387695312e-08, -0.49317920207977295),
                 (-0.09776955097913742, -2.9802322387695312e-08, -0.3996969163417816),
                 (0.09776940196752548, -2.9802322387695312e-08, -0.39969703555107117),
                 (-7.148475589247028e-08, -2.9802322387695312e-08, -0.2804329991340637),
                 (-0.2804330289363861, 3.552713678800501e-15, 4.234420103443881e-08),
                 (-0.3996969759464264, -7.450580596923828e-09, -0.09776938706636429),
                 (-0.39969685673713684, 7.450580596923828e-09, 0.09776950627565384),
                 (-0.4931790232658386, -3.725290298461914e-09, -0.05274520441889763),
                 (-0.4931790232658386, 3.725290298461914e-09, 0.05274531990289688),
                 (-0.3996969163417816, 3.725290298461914e-09, 0.052745312452316284),
                 (-0.3996969163417816, -3.725290298461914e-09, -0.05274519324302673),
                 (-0.06401804089546204, -7.450580596923828e-09, -0.06401806324720383),
                 (-0.0836436077952385, -3.725290298461914e-09, -0.03464633598923683),
                 (-0.09053517132997513, -8.881784197001252e-16, -9.713016169143884e-09),
                 (-0.0836436077952385, 3.725290298461914e-09, 0.03464631363749504),
                 (-0.06401806324720383, 7.450580596923828e-09, 0.06401804089546204),
                 (-0.03464633598923683, 7.450580596923828e-09, 0.0836436077952385),
                 (-0.034646373242139816, -7.450580596923828e-09, -0.0836435854434967),
                 (0.03464638441801071, 7.450580596923828e-09, 0.0836435854434967),
                 (-2.734086912425937e-08, 7.450580596923828e-09, 0.09053517132997513),
                 (-4.532979147597871e-08, -7.450580596923828e-09, -0.09053517132997513),
                 (0.034646324813365936, -7.450580596923828e-09, -0.0836436077952385),
                 (0.06401804834604263, -7.450580596923828e-09, -0.06401804834604263),
                 (0.0836436077952385, -3.725290298461914e-09, -0.034646324813365936),
                 (0.09053517132997513, -4.440892098500626e-16, -3.957419281164221e-09),
                 (0.0836436077952385, 3.725290298461914e-09, 0.03464632108807564),
                 (0.06401804834604263, 7.450580596923828e-09, 0.06401804834604263),
                 (1.1175870895385742e-08, 2.9802322387695312e-08, 0.4931790828704834),
                 (-3.3337176574832483e-08, 2.4835267176115394e-09, 0.030178390443325043),
                 (-3.9333485801762436e-08, -2.4835271617007493e-09, -0.030178390443325043),
                 (-0.030178390443325043, -7.40148665436918e-16, -7.794483281031717e-09),
                 (0.030178390443325043, -5.921189111737107e-16, -5.875951281097969e-09)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (7, 8), (0, 8),
                 (10, 11), (9, 12), (11, 12), (10, 13), (9, 14), (13, 15), (14, 15), (16, 22),
                 (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (7, 17), (6, 16), (23, 24),
                 (23, 25), (24, 29), (25, 28), (26, 29), (27, 28), (31, 32), (30, 33), (32, 33),
                 (31, 34), (30, 35), (34, 36), (35, 36), (37, 38), (37, 39), (38, 43), (39, 42),
                 (40, 41), (40, 43), (41, 42), (50, 53), (49, 52), (44, 45), (45, 46), (46, 47),
                 (47, 48), (48, 49), (44, 50), (51, 59), (51, 52), (53, 54), (54, 55), (55, 56),
                 (56, 57), (57, 58), (58, 59), (26, 60), (27, 60), (23, 60), (61, 62), (63, 64)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


# =========================================================================
# Define the fuction to make the camera active
# =========================================================================
def sceneCamera():
    ob = bpy.context.active_object
    # find the children on the rig (the camera name)
    active_cam = ob.children[0].name
    # cam = bpy.data.cameras[bpy.data.objects[active_cam]]
    scene_cam = bpy.context.scene.camera

    if active_cam != scene_cam.name:
        bpy.context.scene.camera = bpy.data.objects[active_cam]
    else:
        return None


class MakeCameraActive(Operator):
    bl_idname = "scene.make_camera_active"
    bl_label = "Make Camera Active"
    bl_description = "Makes the camera parented to this rig the active scene camera"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        sceneCamera()

        return {'FINISHED'}


# =========================================================================
# Define function to add marker to timeline and bind camera
# =========================================================================
def markerBind():
    ob = bpy.context.active_object  # rig object
    active_cam = ob.children[0]     # camera object

    # switch area to timeline to add marker
    bpy.context.area.type = 'TIMELINE'
    # add marker
    bpy.ops.marker.add()
    bpy.ops.marker.rename(name="cam_" + str(bpy.context.scene.frame_current))
    # select rig camera
    bpy.context.scene.objects.active = active_cam
    # bind marker to selected camera
    bpy.ops.marker.camera_bind()
    # switch selected object back to the rig
    bpy.context.scene.objects.active = ob
    # switch back to 3d view
    bpy.context.area.type = 'VIEW_3D'


class AddMarkerBind(Operator):
    bl_idname = "add.marker_bind"
    bl_label = "Add marker and Bind Camera"
    bl_description = ("Add marker to current frame then bind "
                      "rig camera to it (for camera switching)")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        markerBind()

        return {'FINISHED'}


# =========================================================================
# Define the function to add an Empty as DOF object
# =========================================================================
def add_DOF_Empty():
    smode = bpy.context.mode
    rig = bpy.context.active_object
    bone = rig.data.bones['AIM_child']
    active_cam = rig.children[0].name
    cam = bpy.data.cameras[bpy.data.objects[active_cam].data.name]

    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
    # Add Empty
    bpy.ops.object.empty_add()
    obj = bpy.context.active_object

    obj.name = "Empty_DOF"
    # parent to AIM_Child
    obj.parent = rig
    obj.parent_type = "BONE"
    obj.parent_bone = "AIM_child"
    # clear loc and rot
    bpy.ops.object.location_clear()
    bpy.ops.object.rotation_clear()
    # move to bone head
    obj.location = bone.head

    # make this new empty the dof_object
    cam.dof_object = obj
    # reselect the rig
    bpy.context.scene.objects.active = rig
    obj.select = False
    rig.select = True

    bpy.ops.object.mode_set(mode=smode, toggle=False)


class AddDofEmpty(Operator):
    bl_idname = "add.dof_empty"
    bl_label = "Add DOF Empty"
    bl_description = "Create empty and add as DOF Object"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        add_DOF_Empty()

        return {'FINISHED'}


# =========================================================================
# Define the function to build the Dolly Rig
# =========================================================================
def build_dolly_rig(context):
    # Define some useful variables:
    boneLayer = (False, True, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False)

    # Add the new armature object:
    bpy.ops.object.armature_add()
    rig = context.active_object

    # it will try to name the rig "Dolly_Rig" but if that name exists it will
    # add 000 to the name
    if "Dolly_Rig" not in context.scene.objects:
        rig.name = "Dolly_Rig"
    else:
        rig.name = "Dolly_Rig.000"
    rig["rig_id"] = "Dolly_Rig"

    bpy.ops.object.mode_set(mode='EDIT')

    # Remove default bone:
    bones = rig.data.edit_bones
    bones.remove(bones[0])

    # Add new bones:
    root = bones.new("Root")
    root.tail = (0.0, 0.0, -5.0)
    root.roll = radians(90)

    bpy.ops.object.mode_set(mode='EDIT')
    ctrlAimChild = bones.new("AIM_child")
    ctrlAimChild.head = (0.0, 5.0, 3.0)
    ctrlAimChild.tail = (0.0, 7.0, 3.0)
    ctrlAimChild.layers = boneLayer

    ctrlAim = bones.new("AIM")
    ctrlAim.head = (0.0, 5.0, 3.0)
    ctrlAim.tail = (0.0, 7.0, 3.0)

    ctrl = bones.new("CTRL")
    ctrl.head = (0.0, 0.0, 3.0)
    ctrl.tail = (0.0, 2.0, 3.0)

    # Setup hierarchy:
    ctrl.parent = root
    ctrlAim.parent = root
    ctrlAimChild.parent = ctrlAim

    # jump into pose mode and change bones to euler
    bpy.ops.object.mode_set(mode='POSE')
    for x in bpy.context.object.pose.bones:
        x.rotation_mode = 'XYZ'

    # jump into pose mode and add the custom bone shapes
    bpy.ops.object.mode_set(mode='POSE')
    bpy.context.object.pose.bones["Root"].custom_shape = \
            bpy.data.objects["WDGT_Camera_Root"]  # add the widget as custom shape
    # set the wireframe checkbox to true
    bpy.context.object.data.bones["Root"].show_wire = True
    bpy.context.object.pose.bones["AIM"].custom_shape = \
            bpy.data.objects["WDGT_AIM"]
    bpy.context.object.data.bones["AIM"].show_wire = True
    bpy.context.object.pose.bones["AIM"].custom_shape_transform = \
            bpy.data.objects[rig.name].pose.bones["AIM_child"]  # sets the "At" field to the child
    bpy.context.object.pose.bones["CTRL"].custom_shape = \
            bpy.data.objects["WDGT_CTRL"]
    bpy.context.object.data.bones["CTRL"].show_wire = True

    # jump into object mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Add constraints to bones:
    con = rig.pose.bones['AIM_child'].constraints.new('COPY_ROTATION')
    con.target = rig
    con.subtarget = "CTRL"

    con = rig.pose.bones['CTRL'].constraints.new('TRACK_TO')
    con.target = rig
    con.subtarget = "AIM"
    con.use_target_z = True

    # Add custom Bone property to CTRL bone
    ob = bpy.context.object.pose.bones['CTRL']
    prop = rna_idprop_ui_prop_get(ob, "Lock", create=True)
    ob["Lock"] = 1.0
    prop["soft_min"] = prop["min"] = 0.0
    prop["soft_max"] = prop["max"] = 1.0

    # Add Driver to Lock/Unlock Camera from Aim Target
    rig = bpy.context.scene.objects.active
    pose_bone = bpy.data.objects[rig.name].pose.bones['CTRL']

    constraint = pose_bone.constraints["Track To"]
    inf_driver = constraint.driver_add('influence')
    inf_driver.driver.type = 'SCRIPTED'
    var = inf_driver.driver.variables.new()
    var.name = 'var'
    var.type = 'SINGLE_PROP'

    # Target the Custom bone property
    var.targets[0].id = bpy.data.objects[rig.name]
    var.targets[0].data_path = 'pose.bones["CTRL"]["Lock"]'
    inf_driver.driver.expression = 'var'

    # Add the camera object:
    bpy.ops.object.mode_set(mode='OBJECT')

    bpy.ops.object.camera_add(
        view_align=False, enter_editmode=False, location=(0, 0, 0), rotation=(0, 0, 0))
    cam = bpy.context.active_object

    # this will name the Camera Object
    if 'Dolly_Camera' not in context.scene.objects:
        cam.name = "Dolly_Camera"
    else:
        cam.name = "Dolly_Camera.000"

    # this will name the camera Data Object
    if "Dolly_Camera" not in bpy.context.scene.objects.data.camera:
        cam.data.name = "Dolly_Camera"
    else:
        cam.data.name = "Dolly_Camera.000"

    cam_data_name = bpy.context.object.data.name
    bpy.data.cameras[cam_data_name].draw_size = 1.0
    cam.rotation_euler[0] = 1.5708   # rotate the camera 90 degrees in x
    cam.location = (0.0, -2.0, 0.0)  # move the camera to the correct postion
    cam.parent = rig
    cam.parent_type = "BONE"
    cam.parent_bone = "CTRL"

    # Add blank drivers to lock the camera loc, rot scale
    cam.driver_add('location', 0)
    cam.driver_add('location', 1)
    cam.driver_add('location', 2)
    cam.driver_add('rotation_euler', 0)
    cam.driver_add('rotation_euler', 1)
    cam.driver_add('rotation_euler', 2)
    cam.driver_add('scale', 0)
    cam.driver_add('scale', 1)
    cam.driver_add('scale', 2)

    # Set new camera as active camera
    bpy.context.scene.camera = cam

    # make sure the camera is selectable by default (this can be locked in the UI)
    bpy.context.object.hide_select = False

    # make the rig the active object before finishing
    bpy.context.scene.objects.active = rig
    cam.select = False
    rig.select = True

    return rig


# =========================================================================
# Define the function to build the Crane Rig
# =========================================================================
def build_crane_rig(context):
    # Define some useful variables:
    boneLayer = (False, True, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False,
                 False, False, False, False, False, False, False, False)

    # Add the new armature object:
    bpy.ops.object.armature_add()
    rig = context.active_object

    # it will try to name the rig "Dolly_Rig" but if that name exists it will
    # add .000 to the name
    if "Crane_Rig" not in context.scene.objects:
        rig.name = "Crane_Rig"
    else:
        rig.name = "Crane_Rig.000"
    rig["rig_id"] = "Crane_Rig"

    bpy.ops.object.mode_set(mode='EDIT')

    # Remove default bone:
    bones = rig.data.edit_bones
    bones.remove(bones[0])

    # Add new bones:
    root = bones.new("Root")
    root.tail = (0.0, 0.0, -5.0)

    ctrlAimChild = bones.new("AIM_child")
    ctrlAimChild.head = (0.0, 10.0, 1.0)
    ctrlAimChild.tail = (0.0, 12.0, 1.0)
    ctrlAimChild.layers = boneLayer

    ctrlAim = bones.new("AIM")
    ctrlAim.head = (0.0, 10.0, 1.0)
    ctrlAim.tail = (0.0, 12.0, 1.0)

    ctrl = bones.new("CTRL")
    ctrl.head = (0.0, 1.0, 1.0)
    ctrl.tail = (0.0, 3.0, 1.0)

    arm = bones.new("Crane_Arm")
    arm.head = (0.0, 0.0, 1.0)
    arm.tail = (0.0, 1.0, 1.0)

    height = bones.new("Height")
    height.head = (0.0, 0.0, 0.0)
    height.tail = (0.0, 0.0, 1.0)

    # Setup hierarchy:
    ctrl.parent = arm
    ctrl.use_inherit_rotation = False
    ctrl.use_inherit_scale = False

    arm.parent = height
    arm.use_inherit_scale = False

    height.parent = root
    ctrlAim.parent = root
    ctrlAimChild.parent = ctrlAim

    # change display to BBone: it just looks nicer
    bpy.context.object.data.draw_type = 'BBONE'
    # change display to wire for object
    bpy.context.object.draw_type = 'WIRE'

    # jump into pose mode and change bones to euler
    bpy.ops.object.mode_set(mode='POSE')
    for x in bpy.context.object.pose.bones:
        x.rotation_mode = 'XYZ'

    # lock the relevant loc, rot and scale
    bpy.context.object.pose.bones[
        "Crane_Arm"].lock_rotation = [False, True, False]
    bpy.context.object.pose.bones["Crane_Arm"].lock_scale = [True, False, True]
    bpy.context.object.pose.bones["Height"].lock_location = [True, True, True]
    bpy.context.object.pose.bones["Height"].lock_rotation = [True, True, True]
    bpy.context.object.pose.bones["Height"].lock_scale = [True, False, True]

    # add the custom bone shapes
    bpy.context.object.pose.bones["Root"].custom_shape = bpy.data.objects[
        "WDGT_Camera_Root"]  # add the widget as custom shape
    # set the wireframe checkbox to true
    bpy.context.object.data.bones["Root"].show_wire = True
    bpy.context.object.pose.bones[
        "AIM"].custom_shape = bpy.data.objects["WDGT_AIM"]
    bpy.context.object.data.bones["AIM"].show_wire = True
    bpy.context.object.pose.bones["AIM"].custom_shape_transform = bpy.data.objects[
        rig.name].pose.bones["AIM_child"]  # sets the "At" field to the child
    bpy.context.object.pose.bones[
        "CTRL"].custom_shape = bpy.data.objects["WDGT_CTRL"]
    bpy.context.object.data.bones["CTRL"].show_wire = True

    # jump into object mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Add constraints to bones:
    con = rig.pose.bones['AIM_child'].constraints.new('COPY_ROTATION')
    con.target = rig
    con.subtarget = "CTRL"

    con = rig.pose.bones['CTRL'].constraints.new('TRACK_TO')
    con.target = rig
    con.subtarget = "AIM"
    con.use_target_z = True

    # Add custom Bone property to CTRL bone
    ob = bpy.context.object.pose.bones['CTRL']
    prop = rna_idprop_ui_prop_get(ob, "Lock", create=True)
    ob["Lock"] = 1.0
    prop["soft_min"] = prop["min"] = 0.0
    prop["soft_max"] = prop["max"] = 1.0

    # Add Driver to Lock/Unlock Camera from Aim Target
    rig = bpy.context.scene.objects.active
    pose_bone = bpy.data.objects[rig.name].pose.bones['CTRL']

    constraint = pose_bone.constraints["Track To"]
    inf_driver = constraint.driver_add('influence')
    inf_driver.driver.type = 'SCRIPTED'
    var = inf_driver.driver.variables.new()
    var.name = 'var'
    var.type = 'SINGLE_PROP'

    # Target the Custom bone property
    var.targets[0].id = bpy.data.objects[rig.name]
    var.targets[0].data_path = 'pose.bones["CTRL"]["Lock"]'
    inf_driver.driver.expression = 'var'

    # Add the camera object:
    bpy.ops.object.mode_set(mode='OBJECT')

    bpy.ops.object.camera_add(
        view_align=False, enter_editmode=False, location=(0, 0, 0), rotation=(0, 0, 0))
    cam = bpy.context.active_object

    # this will name the Camera Object
    if 'Crane_Camera' not in context.scene.objects:
        cam.name = "Crane_Camera"
    else:
        cam.name = "Crane_Camera.000"

    # this will name the camera Data Object
    if "Crane_Camera" not in bpy.context.scene.objects.data.camera:
        cam.data.name = "Crane_Camera"
    else:
        cam.data.name = "Crane_Camera.000"

    cam_data_name = bpy.context.object.data.name
    bpy.data.cameras[cam_data_name].draw_size = 1.0
    cam.rotation_euler[0] = 1.5708   # rotate the camera 90 degrees in x
    cam.location = (0.0, -2.0, 0.0)  # move the camera to the correct postion
    cam.parent = rig
    cam.parent_type = "BONE"
    cam.parent_bone = "CTRL"

    # Add blank drivers to lock the camera loc, rot scale
    cam.driver_add('location', 0)
    cam.driver_add('location', 1)
    cam.driver_add('location', 2)
    cam.driver_add('rotation_euler', 0)
    cam.driver_add('rotation_euler', 1)
    cam.driver_add('rotation_euler', 2)
    cam.driver_add('scale', 0)
    cam.driver_add('scale', 1)
    cam.driver_add('scale', 2)

    # Set new camera as active camera
    bpy.context.scene.camera = cam

    # make sure the camera is selectable by default (this can be locked in the UI)
    bpy.context.object.hide_select = False

    # make the rig the active object before finishing
    bpy.context.scene.objects.active = rig
    cam.select = False
    rig.select = True

    return rig


# =========================================================================
# This is the UI for the Dolly Camera Rig
# =========================================================================
class DollyCameraUI(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Dolly Camera UI"

    @classmethod
    def poll(self, context):
        try:
            ob = bpy.context.active_object
            return (ob["rig_id"] == "Dolly_Rig")
        except (AttributeError, KeyError, TypeError):
            return False

    def draw(self, context):
        layout = self.layout
        ob = bpy.context.active_object
        pose_bones = context.active_object.pose.bones
        # find the children on the rig (the camera name)
        active_cam = ob.children[0].name

        cam = bpy.data.cameras[bpy.data.objects[active_cam].data.name]
        box = layout.box()
        col = box.column()
        col.separator()

        # Display Camera Properties
        col.label(text="Clipping:")
        col.prop(cam, "clip_start", text="Start")
        col.prop(cam, "clip_end", text="End")
        col.prop(cam, "type")
        col.prop(cam, "dof_object")

        if cam.dof_object is None:
            col.operator("add.dof_empty", text="Add DOF Empty")
            col.prop(cam, "dof_distance")
        # added the comp guides here
        col.prop_menu_enum(cam, "show_guide", text="Compostion Guides")
        col.prop(bpy.data.objects[active_cam],
                 "hide_select", text="Make Camera Unselectable")

        col.operator("add.marker_bind", text="Add Marker and Bind")

        if bpy.context.scene.camera.name != active_cam:
            col.operator("scene.make_camera_active",
                         text="Make Active Camera", icon='CAMERA_DATA')

        col.prop(context.active_object,
                'show_x_ray', toggle=False, text='X Ray')
        col.prop(cam, "show_limits")
        col.prop(cam, "show_safe_areas")
        col.prop(cam, "show_passepartout")
        col.prop(cam, "passepartout_alpha")

        # Camera Lens
        col.label(text="Focal Length:")
        col.prop(cam, "lens", text="Angle")

        # Track to Constraint
        col.label(text="Tracking:")
        col.prop(pose_bones["CTRL"], '["Lock"]', text="Aim Lock", slider=True)


# =========================================================================
# This is the UI for the Crane Rig Camera
# =========================================================================
class CraneCameraUI(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Crane Camera UI"

    @classmethod
    def poll(self, context):
        try:
            ob = bpy.context.active_object
            return (ob["rig_id"] == "Crane_Rig")
        except (AttributeError, KeyError, TypeError):
            return False

    def draw(self, context):
        layout = self.layout
        ob = bpy.context.active_object
        pose_bones = context.active_object.pose.bones
        # find the children on the rig (camera)
        active_cam = ob.children[0].name
        cam = bpy.data.cameras[bpy.data.objects[active_cam].data.name]

        box = layout.box()
        col = box.column()
        col.separator()

        # Display Camera Properties
        col.label(text="Clipping:")
        col.prop(cam, "clip_start", text="Start")
        col.prop(cam, "clip_end", text="End")
        col.prop(cam, "type")
        col.prop(cam, "dof_object")

        if cam.dof_object is None:
            col.operator("add.dof_empty", text="Add DOF object")
            col.prop(cam, "dof_distance")
        # added the comp guides here
        col.prop_menu_enum(cam, "show_guide", text="Compostion Guides")
        col.prop(bpy.data.objects[active_cam],
                 "hide_select", text="Make Camera Unselectable")
        col.operator("add.marker_bind", text="Add Marker and Bind")

        if bpy.context.scene.camera.name != active_cam:
            col.operator(
                "scene.make_camera_active", text="Make Active Camera", icon='CAMERA_DATA')
        col.prop(
            context.active_object, 'show_x_ray', toggle=False, text='X Ray')
        col.prop(cam, "show_limits")
        col.prop(cam, "show_safe_areas")
        col.prop(cam, "show_passepartout")
        col.prop(cam, "passepartout_alpha")

        # Camera Lens
        col.label(text="Focal Length:")
        col.prop(cam, "lens", text="Angle")

        # Track to Constraint
        col.label(text="Tracking:")
        col.prop(pose_bones["CTRL"], '["Lock"]', text="Aim Lock", slider=True)

        # make this camera active if more than one camera exists
        """
        if cam != bpy.context.scene.camera:
            col.op(, text="Make Active Camera", toggle=True)
        """

        box = layout.box()
        col = box.column()
        col.separator()

        # Crane arm stuff
        col.label(text="Crane Arm:")
        col.prop(pose_bones["Height"], 'scale', index=1, text="Arm Height")
        col.prop(pose_bones["Crane_Arm"], 'scale', index=1, text="Arm Length")


# =========================================================================
# This is the operator that will call all the functions and build the dolly rig
# =========================================================================
class BuildDollyRig(Operator):
    bl_idname = "object.build_dolly_rig"
    bl_label = "Build Dolly Camera Rig"
    bl_description = "Build a Camera Dolly Rig"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):

        # build the Widgets
        create_root_widget(self, "Camera_Root")
        create_camera_widget(self, "CTRL")
        create_aim_widget(self, "AIM")

        # call the function to build the rig
        build_dolly_rig(context)

        return {'FINISHED'}


# =========================================================================
# This is the operator that will call all the functions and build the crane rig
# =========================================================================
class BuildCraneRig(Operator):
    bl_idname = "object.build_crane_rig"
    bl_label = "Build Crane Camera Rig"
    bl_description = "Build a Camera Crane Rig"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):

        # build the Widgets
        create_root_widget(self, "Camera_Root")
        create_camera_widget(self, "CTRL")
        create_aim_widget(self, "AIM")

        # call the function to build the rig
        build_crane_rig(context)

        return {'FINISHED'}


# =========================================================================
# Registration:
# =========================================================================

# dolly and crane entries in the Add Object > Camera Menu
def add_dolly_crane_buttons(self, context):
    if context.mode == 'OBJECT':
        self.layout.operator(
                    BuildDollyRig.bl_idname,
                    text="Dolly Camera Rig",
                    icon='CAMERA_DATA'
                    )
        self.layout.operator(
                    BuildCraneRig.bl_idname,
                    text="Crane Camera Rig",
                    icon='CAMERA_DATA'
                    )


def register():
    bpy.utils.register_class(BuildDollyRig)
    bpy.utils.register_class(BuildCraneRig)
    bpy.utils.register_class(DollyCameraUI)
    bpy.utils.register_class(CraneCameraUI)
    bpy.utils.register_class(MakeCameraActive)
    bpy.utils.register_class(AddMarkerBind)
    bpy.utils.register_class(AddDofEmpty)
    bpy.types.INFO_MT_camera_add.append(add_dolly_crane_buttons)


def unregister():
    bpy.utils.unregister_class(BuildDollyRig)
    bpy.utils.unregister_class(BuildCraneRig)
    bpy.utils.unregister_class(DollyCameraUI)
    bpy.utils.unregister_class(CraneCameraUI)
    bpy.utils.unregister_class(MakeCameraActive)
    bpy.utils.unregister_class(AddMarkerBind)
    bpy.utils.unregister_class(AddDofEmpty)
    bpy.types.INFO_MT_camera_add.remove(add_dolly_crane_buttons)


if __name__ == "__main__":
    register()
