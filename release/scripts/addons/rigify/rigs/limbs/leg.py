
import bpy, re, math
from ..widgets import create_foot_widget, create_ballsocket_widget, create_gear_widget
from .ui import create_script
from .limb_utils import *
from mathutils import Vector
from ...utils import copy_bone, flip_bone, put_bone, create_cube_widget
from ...utils import strip_org, strip_mch, make_deformer_name, create_widget
from ...utils import create_circle_widget, create_sphere_widget, create_line_widget
from ...utils import MetarigError, make_mechanism_name, org
from ...utils import create_limb_widget, connected_children_names
from ...utils import align_bone_y_axis, align_bone_x_axis, align_bone_z_axis
from rna_prop_ui import rna_idprop_ui_prop_get
from ..widgets import create_ikarrow_widget
from math import trunc, pi

extra_script = """
controls = [%s]
ctrl    = '%s'

if is_selected( controls ):
    layout.prop( pose_bones[ ctrl ], '["%s"]')
    if '%s' in pose_bones[ctrl].keys():
        layout.prop( pose_bones[ ctrl ], '["%s"]', slider = True )
    if '%s' in pose_bones[ctrl].keys():
        layout.prop( pose_bones[ ctrl ], '["%s"]', slider = True )
"""

IMPLEMENTATION = True   # Include and set True if Rig is just an implementation for a wrapper class
                        # add_parameters and parameters_ui are unused for implementation classes


class Rig:

    def __init__(self, obj, bone_name, params):
        """ Initialize leg rig and key rig properties """
        self.obj = obj
        self.params = params

        self.org_bones = list(
            [bone_name] + connected_children_names(obj, bone_name)
            )[:3]  # The basic limb is the first 3 bones

        self.segments = params.segments
        self.bbones = params.bbones
        self.limb_type = params.limb_type
        self.rot_axis = params.rotation_axis
        self.auto_align_extremity = params.auto_align_extremity

        # Assign values to tweak/FK layers props if opted by user
        if params.tweak_extra_layers:
            self.tweak_layers = list(params.tweak_layers)
        else:
            self.tweak_layers = None

        if params.fk_extra_layers:
            self.fk_layers = list(params.fk_layers)
        else:
            self.fk_layers = None

    def orient_org_bones(self):

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        thigh = self.org_bones[0]
        org_bones = list(
            [thigh] + connected_children_names(self.obj, thigh)
        )  # All the provided orgs

        # Get heel bone
        heel = ''
        for b in self.obj.data.bones[org_bones[2]].children:
            if not b.use_connect and not b.children:
                heel = b.name
        if heel:
            org_bones.append(heel)

        org_thigh = eb[org_bones[0]]
        org_shin = eb[org_bones[1]]
        org_foot = eb[org_bones[2]]
        org_toe = eb[org_bones[3]]
        org_heel = eb[org_bones[4]]

        foot_projection_on_xy = Vector((org_foot.y_axis[0], org_foot.y_axis[1], 0))
        foot_x = foot_projection_on_xy.cross(Vector((0, 0, -1))).normalized()

        if self.rot_axis != 'automatic':

            # Orient foot and toe
            if self.auto_align_extremity:
                if self.rot_axis == 'x':
                    align_bone_x_axis(self.obj, org_foot.name, foot_x)
                    align_bone_x_axis(self.obj, org_toe.name, -foot_x)
                elif self.rot_axis == 'z':
                    align_bone_z_axis(self.obj, org_foot.name, foot_x)
                    align_bone_z_axis(self.obj, org_toe.name, -foot_x)
                else:
                    raise MetarigError(message='IK on %s has forbidden rotation axis (Y)' % self.org_bones[0])

            return

        # Orient thigh and shin bones
        chain_y_axis = org_thigh.y_axis + org_shin.y_axis
        chain_rot_axis = org_thigh.y_axis.cross(chain_y_axis).normalized()  # ik-plane normal axis (rotation)

        align_bone_x_axis(self.obj, org_thigh.name, chain_rot_axis)
        align_bone_x_axis(self.obj, org_shin.name, chain_rot_axis)

        # Orient foot and toe
        align_bone_x_axis(self.obj, org_foot.name, foot_x)
        align_bone_x_axis(self.obj, org_toe.name, -foot_x)

        # Orient heel
        align_bone_z_axis(self.obj, org_heel.name, Vector((0, 0, 1)))

    def create_parent(self):

        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        name = get_bone_name( strip_org( org_bones[0] ), 'mch', 'parent' )

        mch = copy_bone( self.obj, org_bones[0], name )
        orient_bone( self, eb[mch], 'y' )
        eb[ mch ].length = eb[ org_bones[0] ].length / 4

        eb[ mch ].parent = eb[ org_bones[0] ].parent

        eb[ mch ].roll = 0.0

        # Add non-MCH main limb control
        name = get_bone_name(strip_org(org_bones[0]), 'ctrl', 'parent')
        main_parent = copy_bone(self.obj, org_bones[0], name)
        eb[main_parent].length = eb[org_bones[0]].length / 4
        eb[main_parent].parent = eb[org_bones[0]]
        eb[main_parent].roll = 0.0

        # Constraints
        make_constraint( self, mch, {
            'constraint'  : 'COPY_ROTATION',
            'subtarget'   : 'root'
        })

        make_constraint( self, mch, {
            'constraint'  : 'COPY_SCALE',
            'subtarget'   : 'root'
        })

        # Limb Follow Driver
        pb = self.obj.pose.bones

        name = 'FK_limb_follow'

        # pb[ mch ][ name ] = 0.0
        # prop = rna_idprop_ui_prop_get( pb[ mch ], name, create = True )
        pb[main_parent][name] = 0.0
        prop = rna_idprop_ui_prop_get(pb[main_parent], name, create=True)

        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0
        prop["description"] = name

        drv = pb[mch].constraints[0].driver_add("influence").driver

        drv.type = 'AVERAGE'
        var = drv.variables.new()
        var.name = name
        var.type = "SINGLE_PROP"
        var.targets[0].id = self.obj
        var.targets[0].data_path = pb[main_parent].path_from_id() + \
                                   '[' + '"' + name + '"' + ']'

        size = pb[main_parent].bone.y_axis.length * 10
        create_gear_widget(self.obj, main_parent, size=size, bone_transform_name=None)

        return [mch, main_parent]

    def create_tweak(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        tweaks         = {}
        tweaks['ctrl'] = []
        tweaks['mch' ] = []

        # Create and parent mch and ctrl tweaks
        for i,org in enumerate(org_bones):
            if i < len(org_bones) - 1:
                # Create segments if specified
                for j in range( self.segments ):
                    # MCH
                    name = get_bone_name( strip_org(org), 'mch', 'tweak' )
                    mch = copy_bone( self.obj, org, name )

                    # CTRL
                    name = get_bone_name( strip_org(org), 'ctrl', 'tweak' )
                    ctrl = copy_bone( self.obj, org, name )

                    eb[ mch  ].length /= self.segments
                    eb[ ctrl ].length /= self.segments

                    # If we have more than one segments, place the head of the
                    # 2nd and onwards at the correct position
                    if j > 0:
                        put_bone(self.obj, mch,  eb[ tweaks['mch' ][-1] ].tail)
                        put_bone(self.obj, ctrl, eb[ tweaks['ctrl'][-1] ].tail)

                    tweaks['ctrl'] += [ ctrl ]
                    tweaks['mch' ] += [ mch  ]

                    # Parenting the tweak ctrls to mchs
                    eb[ mch  ].parent = eb[ org ]
                    eb[ ctrl ].parent = eb[ mch ]

            else: # Last limb bone - is not subdivided
                name = get_bone_name( strip_org(org), 'mch', 'tweak' )
                mch = copy_bone( self.obj, org_bones[i-1], name )
                eb[ mch ].length = eb[org].length / 4
                put_bone(
                    self.obj,
                    mch,
                    eb[org_bones[i-1]].tail
                )

                ctrl = get_bone_name( strip_org(org), 'ctrl', 'tweak' )
                ctrl = copy_bone( self.obj, org, ctrl )
                eb[ ctrl ].length = eb[org].length / 2

                tweaks['mch']  += [ mch  ]
                tweaks['ctrl'] += [ ctrl ]

                # Parenting the tweak ctrls to mchs
                eb[ mch  ].parent = eb[ org ]
                eb[ ctrl ].parent = eb[ mch ]

        # Scale to reduce widget size and maintain conventions!
        for mch, ctrl in zip( tweaks['mch'], tweaks['ctrl'] ):
            eb[ mch  ].length /= 4
            eb[ ctrl ].length /= 2

        # Contraints

        for i,b in enumerate( tweaks['mch'] ):
            first  = 0
            middle = trunc( len( tweaks['mch'] ) / 2 )
            last   = len( tweaks['mch'] ) - 1

            if i == first or i == middle:
                make_constraint( self, b, {
                    'constraint'  : 'COPY_SCALE',
                    'subtarget'   : 'root'
                })
            elif i != last:
                targets       = []
                dt_target_idx = middle
                factor        = 0
                if i < middle:
                    targets = [first,middle]
                else:
                    targets       = [middle,last]
                    factor        = self.segments
                    dt_target_idx = last

                # Use copy transforms constraints to position each bone
                # exactly in the location respective to its index (between
                # the two edges)
                make_constraint( self, b, {
                    'constraint'  : 'COPY_TRANSFORMS',
                    'subtarget'   : tweaks['ctrl'][targets[0]]
                })
                make_constraint( self, b, {
                    'constraint'  : 'COPY_TRANSFORMS',
                    'subtarget'   : tweaks['ctrl'][targets[1]],
                    'influence'   : (i - factor) / self.segments
                })
                make_constraint( self, b, {
                    'constraint'  : 'DAMPED_TRACK',
                    'subtarget'   : tweaks['ctrl'][ dt_target_idx ],
                })

        # Ctrl bones Locks and Widgets
        pb = self.obj.pose.bones
        for t in tweaks['ctrl']:
            pb[t].lock_rotation = True, False, True
            pb[t].lock_scale    = False, True, False

            create_sphere_widget(self.obj, t, bone_transform_name=None)

            if self.tweak_layers:
                pb[t].bone.layers = self.tweak_layers

        return tweaks

    def create_def(self, tweaks):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        def_bones = []
        for i, org in enumerate(org_bones):
            if i < len(org_bones) - 1:
                # Create segments if specified
                for j in range(self.segments):
                    name = get_bone_name(strip_org(org), 'def')
                    def_name = copy_bone(self.obj, org, name)

                    eb[def_name].length /= self.segments

                    # If we have more than one segments, place the 2nd and
                    # onwards on the tail of the previous bone
                    if j > 0:
                        put_bone(self.obj, def_name, eb[ def_bones[-1] ].tail)

                    def_bones += [def_name]
            else:
                name = get_bone_name(strip_org(org), 'def')
                def_name = copy_bone(self.obj, org, name)
                def_bones.append(def_name)

        # Parent deform bones
        for i,b in enumerate( def_bones ):
            if i > 0: # For all bones but the first (which has no parent)
                eb[b].parent      = eb[ def_bones[i-1] ] # to previous
                eb[b].use_connect = True

        # Constraint def to tweaks
        for d,t in zip(def_bones, tweaks):
            tidx = tweaks.index(t)

            make_constraint( self, d, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : t
            })

            if tidx != len(tweaks) - 1:
                make_constraint( self, d, {
                    'constraint'  : 'DAMPED_TRACK',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

                make_constraint( self, d, {
                    'constraint'  : 'STRETCH_TO',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

        # Create bbone segments
        for bone in def_bones[:-1]:
            self.obj.data.bones[bone].bbone_segments = self.bbones

        self.obj.data.bones[ def_bones[0]  ].bbone_in = 0.0
        self.obj.data.bones[ def_bones[-2] ].bbone_out = 0.0
        self.obj.data.bones[ def_bones[-1] ].bbone_in = 0.0
        self.obj.data.bones[ def_bones[-1] ].bbone_out = 0.0


        # Rubber hose drivers
        pb = self.obj.pose.bones
        for i, t in enumerate(tweaks[1:-1]):
            # Create custom property on tweak bone to control rubber hose
            name = 'rubber_tweak'

            if i == trunc( len( tweaks[1:-1] ) / 2 ):
                pb[t][name] = 0.0
            else:
                pb[t][name] = 1.0

            prop = rna_idprop_ui_prop_get( pb[t], name, create=True )

            prop["min"]         = 0.0
            prop["max"]         = 2.0
            prop["soft_min"]    = 0.0
            prop["soft_max"]    = 1.0
            prop["description"] = name

        for j,d in enumerate(def_bones[:-1]):
            drvs = {}
            if j != 0:
                tidx = j
                drvs[tidx] = self.obj.data.bones[d].driver_add("bbone_in").driver

            if j != len( def_bones[:-1] ) - 1:
                tidx = j + 1
                drvs[tidx] = self.obj.data.bones[d].driver_add("bbone_out").driver

            for d in drvs:
                drv = drvs[d]
                name = 'rubber_tweak'
                drv.type = 'AVERAGE'
                var = drv.variables.new()
                var.name = name
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = pb[tweaks[d]].path_from_id() + \
                                           '[' + '"' + name + '"' + ']'

        return def_bones

    def create_ik(self, parent):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        ctrl = get_bone_name(org_bones[0], 'ctrl', 'ik')
        mch_ik = get_bone_name(org_bones[0], 'mch', 'ik')
        mch_target = get_bone_name(org_bones[0], 'mch', 'ik_target')

        for o, ik in zip(org_bones, [ctrl, mch_ik, mch_target]):
            bone = copy_bone(self.obj, o, ik)

            if org_bones.index(o) == len(org_bones) - 1:
                eb[bone].length /= 4

        # Create MCH Stretch
        mch_str = copy_bone(
            self.obj,
            org_bones[0],
            get_bone_name( org_bones[0], 'mch', 'ik_stretch' )
        )

        eb[ mch_str ].tail = eb[ org_bones[-1] ].head

        # Parenting
        eb[ctrl].parent = eb[parent]
        eb[mch_str].parent = eb[parent]
        eb[mch_ik].parent = eb[ctrl]

        # Make standard pole target bone
        pole_name = get_bone_name(org_bones[0], 'ctrl', 'ik_target')
        pole_target = copy_bone(self.obj, org_bones[0], pole_name)

        lo_vector = eb[org_bones[1]].tail - eb[org_bones[1]].head
        tot_vector = eb[org_bones[0]].head - eb[org_bones[1]].tail
        tot_vector.normalize()
        elbow_vector = lo_vector.dot(tot_vector)*tot_vector - lo_vector    # elbow_vec as rejection of lo on tot
        elbow_vector.normalize()
        elbow_vector *= (eb[org_bones[1]].tail - eb[org_bones[0]].head).length

        if self.rot_axis == 'x' or self.rot_axis == 'automatic':
            z_vector = eb[org_bones[0]].z_axis + eb[org_bones[1]].z_axis
            alfa = elbow_vector.angle(z_vector)
        elif self.rot_axis == 'z':
            x_vector = eb[org_bones[0]].x_axis + eb[org_bones[1]].x_axis
            alfa = elbow_vector.angle(x_vector)

        if alfa > pi/2:
            pole_angle = -pi/2
        else:
            pole_angle = pi/2

        if self.rot_axis == 'z':
            pole_angle = 0

        eb[pole_target].head = eb[org_bones[0]].tail + elbow_vector
        eb[pole_target].tail = eb[pole_target].head - elbow_vector/8
        eb[pole_target].roll = 0.0

        # Make visual pole
        vispole_name = 'VIS_' + get_bone_name(org_bones[0], 'ctrl', 'ik_pole')
        vispole = copy_bone(self.obj, org_bones[1], vispole_name)
        eb[vispole].tail = eb[vispole].head + Vector((0.0, 0.0, eb[org_bones[1]].length/10))
        eb[vispole].use_connect = False
        eb[vispole].hide_select = True
        eb[vispole].parent = None

        make_constraint(self, mch_ik, {
            'constraint': 'IK',
            'subtarget': mch_target,
            'chain_count': 2,
        })

        make_constraint(self, mch_ik, {     # 2_nd IK for pole targeted chain
            'constraint': 'IK',
            'subtarget': mch_target,
            'chain_count': 2,
        })

        # VIS pole constraints
        make_constraint(self, vispole, {
            'constraint': 'COPY_LOCATION',
            'name': 'copy_loc',
            'subtarget': org_bones[1],
        })

        pb = self.obj.pose.bones

        make_constraint(self, vispole, {
            'constraint': 'STRETCH_TO',
            'name': 'stretch_to',
            'subtarget': pole_target,
            'volume': 'NO_VOLUME',
            'rest_length': pb[vispole].length
        })

        pb[mch_ik].constraints[-1].pole_target = self.obj
        pb[mch_ik].constraints[-1].pole_subtarget = pole_target
        pb[mch_ik].constraints[-1].pole_angle = pole_angle

        pb[ mch_ik ].ik_stretch = 0.1
        pb[ ctrl   ].ik_stretch = 0.1

        # IK constraint Rotation locks
        if self.rot_axis == 'z':
            pb[mch_ik].lock_ik_x = True
            pb[mch_ik].lock_ik_y = True
        else:
            pb[mch_ik].lock_ik_y = True
            pb[mch_ik].lock_ik_z = True

        # Locks and Widget
        pb[ctrl].lock_rotation = True, False, True
        if self.rot_axis == 'x' or self.rot_axis == 'automatic':
            roll = 0
        else:
            roll = pi/2
        create_ikarrow_widget(self.obj, ctrl, bone_transform_name=None, roll=roll)
        create_sphere_widget(self.obj, pole_target, bone_transform_name=None)
        create_line_widget(self.obj, vispole)

        return {'ctrl': {'limb': ctrl, 'ik_target': pole_target},
                'mch_ik': mch_ik,
                'mch_target': mch_target,
                'mch_str': mch_str,
                'visuals': {'vispole': vispole}
        }

    def create_fk(self, parent):
        org_bones = self.org_bones.copy()

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        ctrls = []

        for o in org_bones:
            bone = copy_bone(self.obj, o, get_bone_name( o, 'ctrl', 'fk'))
            ctrls.append(bone)

        # MCH
        mch = copy_bone(
            self.obj, org_bones[-1], get_bone_name(o, 'mch', 'fk')
        )

        eb[mch].length /= 4

        # Parenting
        eb[ctrls[0]].parent = eb[parent]
        eb[ctrls[1]].parent = eb[ctrls[0]]
        eb[ctrls[1]].use_connect = True
        eb[ctrls[2]].parent = eb[mch]
        eb[mch].parent = eb[ctrls[1]]
        eb[mch].use_connect = True

        # Constrain MCH's scale to root
        make_constraint(self, mch, {
            'constraint': 'COPY_SCALE',
            'subtarget': 'root'
        })

        # Locks and widgets
        pb = self.obj.pose.bones
        pb[ctrls[2]].lock_location = True, True, True

        create_limb_widget(self.obj, ctrls[0])
        create_limb_widget(self.obj, ctrls[1])

        create_circle_widget(self.obj, ctrls[2], radius=0.4, head_tail=0.0)

        for c in ctrls:
            if self.fk_layers:
                pb[c].bone.layers = self.fk_layers

        return {'ctrl': ctrls, 'mch': mch}

    def org_parenting_and_switch(self, org_bones, ik, fk, parent):
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones
        # re-parent ORGs in a connected chain
        for i, o in enumerate(org_bones):
            if i > 0:
                eb[o].parent = eb[org_bones[i-1]]
                if i <= len(org_bones)-1:
                    eb[o].use_connect = True

        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones
        pb_parent = pb[parent]

        # Create ik/fk switch property
        pb_parent['IK_FK'] = 0.0
        prop = rna_idprop_ui_prop_get(pb_parent, 'IK_FK', create=True)
        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0
        prop["description"] = 'IK/FK Switch'

        # Constrain org to IK and FK bones
        iks = [ik['ctrl']['limb']]
        iks += [ik[k] for k in ['mch_ik', 'mch_target']]

        for o, i, f in zip(org_bones, iks, fk):
            make_constraint( self, o, {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': i
            })
            make_constraint(self, o, {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': f
            })

            # Add driver to relevant constraint
            drv = pb[o].constraints[-1].driver_add("influence").driver
            drv.type = 'AVERAGE'

            var = drv.variables.new()
            var.name = prop.name
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = \
                pb_parent.path_from_id() + '[' + '"' + prop.name + '"' + ']'

    def create_leg(self, bones):
        org_bones = list(
            [self.org_bones[0]] + connected_children_names(self.obj, self.org_bones[0])
        )

        bones['ik']['ctrl']['terminal'] = []

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Create toes def bone
        toes_def = get_bone_name(org_bones[-1], 'def')
        toes_def = copy_bone( self.obj, org_bones[-1], toes_def )

        eb[ toes_def ].use_connect = False
        eb[ toes_def ].parent      = eb[ bones['def'][-1] ]
        eb[ toes_def ].use_connect = True

        bones['def'] += [ toes_def ]

        pole_target = get_bone_name(org_bones[0], 'ctrl', 'ik_target')

        # Create IK leg control
        ctrl = get_bone_name(org_bones[2], 'ctrl', 'ik')
        ctrl = copy_bone(self.obj, org_bones[2], ctrl)

        # clear parent (so that rigify will parent to root)
        eb[ctrl].parent = None
        eb[ctrl].use_connect = False

        # MCH for ik control
        ctrl_socket = copy_bone(self.obj, org_bones[2], get_bone_name( org_bones[2], 'mch', 'ik_socket'))
        eb[ctrl_socket].tail = eb[ctrl_socket].head + 0.8*(eb[ctrl_socket].tail-eb[ctrl_socket].head)
        eb[ctrl_socket].parent = None
        eb[ctrl].parent = eb[ctrl_socket]

        # MCH for pole ik control
        ctrl_pole_socket = copy_bone(self.obj, org_bones[2], get_bone_name(org_bones[2], 'mch', 'pole_ik_socket'))
        eb[ctrl_pole_socket].tail = eb[ctrl_pole_socket].head + 0.8 * (eb[ctrl_pole_socket].tail - eb[ctrl_pole_socket].head)
        eb[ctrl_pole_socket].parent = None
        eb[pole_target].parent = eb[ctrl_pole_socket]

        ctrl_root = copy_bone(self.obj, org_bones[2], get_bone_name( org_bones[2], 'mch', 'ik_root'))
        eb[ctrl_root].tail = eb[ctrl_root].head + 0.7*(eb[ctrl_root].tail-eb[ctrl_root].head)
        eb[ctrl_root].use_connect = False
        eb[ctrl_root].parent = eb['root']

        if eb[org_bones[0]].parent:
            leg_parent = eb[org_bones[0]].parent
            ctrl_parent = copy_bone(self.obj, org_bones[2], get_bone_name( org_bones[2], 'mch', 'ik_parent'))
            eb[ctrl_parent].tail = eb[ctrl_parent].head + 0.6*(eb[ctrl_parent].tail-eb[ctrl_parent].head)
            eb[ctrl_parent].use_connect = False
            if eb[org_bones[0]].parent_recursive:
                eb[ctrl_parent].parent = eb[org_bones[0]].parent_recursive[-1]
            else:
                eb[ctrl_parent].parent = eb[org_bones[0]].parent
        else:
            leg_parent = None

        mch_name = get_bone_name(strip_org(org_bones[0]), 'mch', 'parent_socket')
        mch_main_parent = copy_bone(self.obj, org_bones[0], mch_name)
        eb[mch_main_parent].length = eb[org_bones[0]].length / 12
        eb[mch_main_parent].parent = eb[bones['parent']]
        eb[mch_main_parent].roll = 0.0
        eb[bones['main_parent']].parent = eb[mch_main_parent]

        # Create heel ctrl bone
        heel = get_bone_name(org_bones[2], 'ctrl', 'heel_ik')
        heel = copy_bone(self.obj, org_bones[2], heel)

        ax = eb[org_bones[2]].head - eb[org_bones[2]].tail
        ax[2] = 0
        align_bone_y_axis(self.obj, heel, ax)
        if self.rot_axis == 'x' or self.rot_axis == 'automatic':
            align_bone_x_axis(self.obj, heel, eb[org_bones[2]].x_axis)
        elif self.rot_axis == 'z':
            align_bone_z_axis(self.obj, heel, eb[org_bones[2]].z_axis)
        eb[heel].length = eb[org_bones[2]].length / 2

        # Reset control position and orientation
        if self.rot_axis == 'automatic' or self.auto_align_extremity:
            l = eb[ctrl].length
            orient_bone(self, eb[ctrl], 'y', reverse=True)
            eb[ctrl].length = l
        else:
            flip_bone(self.obj, ctrl)
            eb[ctrl].tail[2] = eb[ctrl].head[2]
            eb[ctrl].roll = 0


        # Parent
        eb[ heel ].use_connect = False
        eb[ heel ].parent      = eb[ ctrl ]

        eb[ bones['ik']['mch_target'] ].parent      = eb[ heel ]
        eb[ bones['ik']['mch_target'] ].use_connect = False

        # Create foot mch rock and roll bones

        # Get the tmp heel (floating unconnected without children)
        tmp_heel = ""
        for b in self.obj.data.bones[org_bones[2]].children:
            if not b.use_connect and not b.children:
                tmp_heel = b.name

        # roll1 MCH bone
        roll1_mch = get_bone_name(tmp_heel, 'mch', 'roll')
        roll1_mch = copy_bone(self.obj, org_bones[2], roll1_mch)

        # clear parent
        eb[roll1_mch].use_connect = False
        eb[roll1_mch].parent = None

        flip_bone(self.obj, roll1_mch)
        if self.rot_axis == 'x' or self.rot_axis == 'automatic':
            align_bone_x_axis(self.obj, roll1_mch, eb[org_bones[2]].x_axis)
        elif self.rot_axis == 'z':
            align_bone_z_axis(self.obj, roll1_mch, eb[org_bones[2]].z_axis)

        # Create 2nd roll mch, and two rock mch bones
        roll2_mch = get_bone_name(tmp_heel, 'mch', 'roll')
        roll2_mch = copy_bone(self.obj, org_bones[3], roll2_mch)

        eb[roll2_mch].use_connect = False
        eb[roll2_mch].parent = None

        put_bone(
            self.obj,
            roll2_mch,
            (eb[tmp_heel].head + eb[tmp_heel].tail) / 2
        )

        eb[ roll2_mch ].length /= 4

        # Rock MCH bones
        rock1_mch = get_bone_name( tmp_heel, 'mch', 'rock' )
        rock1_mch = copy_bone( self.obj, tmp_heel, rock1_mch )

        eb[ rock1_mch ].use_connect = False
        eb[ rock1_mch ].parent      = None

        orient_bone( self, eb[ rock1_mch ], 'y', 1.0, reverse = True )
        align_bone_y_axis(self.obj, rock1_mch, ax)
        eb[ rock1_mch ].length = eb[ tmp_heel ].length / 2

        rock2_mch = get_bone_name( tmp_heel, 'mch', 'rock' )
        rock2_mch = copy_bone( self.obj, tmp_heel, rock2_mch )

        eb[ rock2_mch ].use_connect = False
        eb[ rock2_mch ].parent      = None

        #orient_bone( self, eb[ rock2_mch ], 'y', 1.0 )
        align_bone_y_axis(self.obj, rock2_mch, ax)
        eb[ rock2_mch ].length = eb[ tmp_heel ].length / 2

        # Parent rock and roll MCH bones
        eb[ roll1_mch ].parent = eb[ roll2_mch ]
        eb[ roll2_mch ].parent = eb[ rock1_mch ]
        eb[ rock1_mch ].parent = eb[ rock2_mch ]
        eb[ rock2_mch ].parent = eb[ ctrl ]

        # make mch toe bone
        toe = ''
        foot = eb[self.org_bones[-1]]
        for c in foot.children:
            if 'org' in c.name.lower() and c.head == foot.tail:
                toe = c.name
        if not toe:
            raise MetarigError.message("Wrong metarig: can't find ORG-<toe>")

        toe_mch = get_bone_name(toe, 'mch')
        toe_mch = copy_bone(self.obj, toe, toe_mch)
        eb[toe_mch].length /= 3
        eb[toe_mch].parent = eb[self.org_bones[2]]
        eb[toe].use_connect = False
        eb[toe].parent = eb[toe_mch]

        # Constrain rock and roll MCH bones
        make_constraint( self, roll1_mch, {
            'constraint'   : 'COPY_ROTATION',
            'subtarget'    : heel,
            'owner_space'  : 'LOCAL',
            'target_space' : 'LOCAL'
        })

        if self.rot_axis == 'x'or self.rot_axis == 'automatic':
            make_constraint(self, roll1_mch, {
                'constraint': 'LIMIT_ROTATION',
                'use_limit_x': True,
                'max_x': math.radians(360),
                'owner_space': 'LOCAL'
            })
            make_constraint(self, roll2_mch, {
                'constraint': 'COPY_ROTATION',
                'subtarget': heel,
                'use_y': False,
                'use_z': False,
                'invert_x': True,
                'owner_space': 'LOCAL',
                'target_space': 'LOCAL'
            })
            make_constraint(self, roll2_mch, {
                'constraint': 'LIMIT_ROTATION',
                'use_limit_x': True,
                'max_x': math.radians(360),
                'owner_space': 'LOCAL'
            })

        elif self.rot_axis == 'z':
            make_constraint(self, roll1_mch, {
                'constraint': 'LIMIT_ROTATION',
                'use_limit_z': True,
                'max_z': math.radians(360),
                'owner_space': 'LOCAL'
            })
            make_constraint(self, roll2_mch, {
                'constraint': 'COPY_ROTATION',
                'subtarget': heel,
                'use_y': False,
                'use_x': False,
                'invert_z': True,
                'owner_space': 'LOCAL',
                'target_space': 'LOCAL'
            })
            make_constraint(self, roll2_mch, {
                'constraint': 'LIMIT_ROTATION',
                'use_limit_z': True,
                'max_z': math.radians(360),
                'owner_space': 'LOCAL'
            })

        pb = self.obj.pose.bones
        if self.rot_axis == 'x'or self.rot_axis == 'automatic':
            ik_rot_axis = pb[org_bones[0]].x_axis
        elif self.rot_axis == 'z':
            ik_rot_axis = pb[org_bones[0]].z_axis
        heel_x_orientation = pb[tmp_heel].y_axis.dot(ik_rot_axis)
        for i, b in enumerate([rock1_mch, rock2_mch]):
            if heel_x_orientation > 0:
                if not i:
                    min_y = 0
                    max_y = math.radians(360)
                else:
                    min_y = math.radians(-360)
                    max_y = 0
            else:
                if not i:
                    min_y = math.radians(-360)
                    max_y = 0
                else:
                    min_y = 0
                    max_y = math.radians(360)


            make_constraint( self, b, {
                'constraint'   : 'COPY_ROTATION',
                'subtarget'    : heel,
                'use_x'        : False,
                'use_z'        : False,
                'owner_space'  : 'LOCAL',
                'target_space' : 'LOCAL'
            })
            make_constraint( self, b, {
                'constraint'  : 'LIMIT_ROTATION',
                'use_limit_y' : True,
                'min_y'       : min_y,
                'max_y'       : max_y,
                'owner_space' : 'LOCAL'
            })

        # Cns toe_mch to MCH roll2
        make_constraint( self, toe_mch, {
            'constraint'  : 'COPY_TRANSFORMS',
            'subtarget'   : roll2_mch
        })

        # Set up constraints

        # Constrain ik ctrl to root / parent

        make_constraint( self, ctrl_socket, {
            'constraint'  : 'COPY_TRANSFORMS',
            'subtarget'   : ctrl_root,
        })

        make_constraint(self, ctrl_pole_socket, {
            'constraint': 'COPY_TRANSFORMS',
            'subtarget': ctrl_root,
        })

        if leg_parent:
            make_constraint( self, ctrl_socket, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : ctrl_parent,
                'influence'   : 0.0,
            })

            make_constraint(self, ctrl_pole_socket, {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': bones['ik']['mch_target'],
            })

        # Constrain mch target bone to the ik control and mch stretch
        make_constraint( self, bones['ik']['mch_target'], {
            'constraint'  : 'COPY_LOCATION',
            'subtarget'   : bones['ik']['mch_str'],
            'head_tail'   : 1.0
        })

        # Constrain mch ik stretch bone to the ik control
        make_constraint( self, bones['ik']['mch_str'], {
            'constraint'  : 'DAMPED_TRACK',
            'subtarget'   : roll1_mch,
            'head_tail'   : 1.0
        })
        make_constraint( self, bones['ik']['mch_str'], {
            'constraint'  : 'STRETCH_TO',
            'subtarget'   : roll1_mch,
            'head_tail'   : 1.0
        })
        make_constraint( self, bones['ik']['mch_str'], {
            'constraint'  : 'LIMIT_SCALE',
            'use_min_y'   : True,
            'use_max_y'   : True,
            'max_y'       : 1.05,
            'owner_space' : 'LOCAL'
        })
        make_constraint(self, mch_main_parent, {
            'constraint': 'COPY_ROTATION',
            'subtarget': org_bones[0]
        })

        # Create ik/fk switch property
        pb_parent = pb[bones['main_parent']]
        pb_parent.lock_location = True, True, True
        pb_parent.lock_rotation = True, True, True
        pb_parent.lock_scale = True, True, True

        pb_parent['IK_Stretch'] = 1.0
        prop = rna_idprop_ui_prop_get(pb_parent, 'IK_Stretch', create=True)
        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0
        prop["description"] = 'IK Stretch'

        # Add driver to limit scale constraint influence
        b = bones['ik']['mch_str']
        drv = pb[b].constraints[-1].driver_add("influence").driver
        drv.type = 'AVERAGE'

        var = drv.variables.new()
        var.name = prop.name
        var.type = "SINGLE_PROP"
        var.targets[0].id = self.obj
        var.targets[0].data_path = \
            pb_parent.path_from_id() + '[' + '"' + prop.name + '"' + ']'

        drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

        drv_modifier.mode = 'POLYNOMIAL'
        drv_modifier.poly_order = 1
        drv_modifier.coefficients[0] = 1.0
        drv_modifier.coefficients[1] = -1.0

        # Create leg widget
        create_foot_widget(self.obj, ctrl, bone_transform_name=None)

        # Create heel ctrl locks
        pb[heel].lock_location = True, True, True
        if self.rot_axis == 'x'or self.rot_axis == 'automatic':
            pb[heel].lock_rotation = False, False, True
        elif self.rot_axis == 'z':
            pb[heel].lock_rotation = True, False, False
        pb[heel].lock_scale = True, True, True

        # Add ballsocket widget to heel
        create_ballsocket_widget(self.obj, heel, bone_transform_name=None)

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        if len(org_bones) >= 4:
            # Create toes control bone
            toes = get_bone_name(org_bones[3], 'ctrl')
            toes = copy_bone(self.obj, org_bones[3], toes)

            eb[toes].use_connect = False
            eb[toes].parent = eb[toe_mch]

            # Constrain 4th ORG to toes
            make_constraint(self, org_bones[3], {
                'constraint': 'COPY_TRANSFORMS',
                # 'subtarget'   : roll2_mch
                'subtarget': toes
            })

            # Constrain toes def bones
            make_constraint(self, bones['def'][-2], {
                'constraint': 'DAMPED_TRACK',
                'subtarget': toes
            })
            make_constraint(self, bones['def'][-2], {
                'constraint': 'STRETCH_TO',
                'subtarget': toes
            })
            make_constraint(self, bones['def'][-1], {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': toes
            })

            # Find IK/FK switch property
            pb = self.obj.pose.bones
            prop = rna_idprop_ui_prop_get( pb[bones['fk']['ctrl'][-1]], 'IK_FK' )

            # Modify rotation mode for ik and tweak controls
            pb[bones['ik']['ctrl']['limb']].rotation_mode = 'ZXY'

            for b in bones['tweak']['ctrl']:
                pb[b].rotation_mode = 'ZXY'

            # Add driver to limit scale constraint influence
            b = toe_mch
            drv = pb[b].constraints[-1].driver_add("influence").driver
            drv.type = 'AVERAGE'

            var = drv.variables.new()
            var.name = prop.name
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = \
                pb_parent.path_from_id() + '['+ '"' + prop.name + '"' + ']'

            drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

            drv_modifier.mode = 'POLYNOMIAL'
            drv_modifier.poly_order = 1
            drv_modifier.coefficients[0] = 1.0
            drv_modifier.coefficients[1] = -1.0

            # Create toe circle widget
            create_circle_widget(self.obj, toes, radius=0.4, head_tail=0.5)

            bones['ik']['ctrl']['terminal'] += [toes]

        bones['ik']['ctrl']['terminal'] += [ heel, ctrl ]

        if leg_parent:
            bones['ik']['mch_foot'] = [ctrl_socket, ctrl_pole_socket, ctrl_root, ctrl_parent]
        else:
            bones['ik']['mch_foot'] = [ctrl_socket, ctrl_pole_socket, ctrl_root]

        return bones

    def create_drivers(self, bones):

        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        ctrl = pb[bones['ik']['mch_foot'][0]]
        ctrl_pole = pb[bones['ik']['mch_foot'][1]]

        #owner = pb[bones['ik']['ctrl']['limb']]
        owner = pb[bones['main_parent']]

        props = ["IK_follow", "root/parent", "pole_vector", "pole_follow"]

        for prop in props:

            if prop == 'pole_vector':
                owner[prop] = False
                pole_prop = rna_idprop_ui_prop_get(owner, prop, create=True)
                pole_prop["min"] = False
                pole_prop["max"] = True
                pole_prop["description"] = prop
                mch_ik = pb[bones['ik']['mch_ik']]

                # ik target hide driver
                pole_target = pb[bones['ik']['ctrl']['ik_target']]
                drv = pole_target.bone.driver_add("hide").driver
                drv.type = 'AVERAGE'

                var = drv.variables.new()
                var.name = prop
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = \
                    owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                drv_modifier.mode = 'POLYNOMIAL'
                drv_modifier.poly_order = 1
                drv_modifier.coefficients[0] = 1.0
                drv_modifier.coefficients[1] = -1.0

                # vis-pole hide driver
                vispole = pb[bones['ik']['visuals']['vispole']]
                drv = vispole.bone.driver_add("hide").driver
                drv.type = 'AVERAGE'
                var = drv.variables.new()
                var.name = prop
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = \
                    owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                drv_modifier.mode = 'POLYNOMIAL'
                drv_modifier.poly_order = 1
                drv_modifier.coefficients[0] = 1.0
                drv_modifier.coefficients[1] = -1.0

                # arrow hide driver
                # pole_target = pb[bones['ik']['ctrl']['limb']]
                # drv = pole_target.bone.driver_add("hide").driver
                # drv.type = 'AVERAGE'
                #
                # var = drv.variables.new()
                # var.name = prop
                # var.type = "SINGLE_PROP"
                # var.targets[0].id = self.obj
                # var.targets[0].data_path = \
                #     owner.path_from_id() + '[' + '"' + prop + '"' + ']'
                #
                # drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]
                #
                # drv_modifier.mode = 'POLYNOMIAL'
                # drv_modifier.poly_order = 1
                # drv_modifier.coefficients[0] = 0.0
                # drv_modifier.coefficients[1] = 1.0

                for cns in mch_ik.constraints:
                    if 'IK' in cns.type:
                        drv = cns.driver_add("mute").driver
                        drv.type = 'AVERAGE'

                        var = drv.variables.new()
                        var.name = prop
                        var.type = "SINGLE_PROP"
                        var.targets[0].id = self.obj
                        var.targets[0].data_path = \
                            owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                        drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                        drv_modifier.mode = 'POLYNOMIAL'
                        drv_modifier.poly_order = 1
                        if not cns.pole_subtarget:
                            drv_modifier.coefficients[0] = 0.0
                            drv_modifier.coefficients[1] = 1
                        else:
                            drv_modifier.coefficients[0] = 1.0
                            drv_modifier.coefficients[1] = -1.0

            elif prop == 'IK_follow':

                owner[prop] = True
                rna_prop = rna_idprop_ui_prop_get(owner, prop, create=True)
                rna_prop["min"] = False
                rna_prop["max"] = True
                rna_prop["description"] = prop

                drv = ctrl.constraints[0].driver_add("mute").driver
                drv.type = 'AVERAGE'

                var = drv.variables.new()
                var.name = prop
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = \
                    owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                drv_modifier.mode = 'POLYNOMIAL'
                drv_modifier.poly_order = 1
                drv_modifier.coefficients[0] = 1.0
                drv_modifier.coefficients[1] = -1.0

                if len(ctrl.constraints) > 1:
                    drv = ctrl.constraints[1].driver_add("mute").driver
                    drv.type = 'AVERAGE'

                    var = drv.variables.new()
                    var.name = prop
                    var.type = "SINGLE_PROP"
                    var.targets[0].id = self.obj
                    var.targets[0].data_path = \
                        owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                    drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                    drv_modifier.mode = 'POLYNOMIAL'
                    drv_modifier.poly_order = 1
                    drv_modifier.coefficients[0] = 1.0
                    drv_modifier.coefficients[1] = -1.0

                drv = ctrl_pole.constraints[0].driver_add("mute").driver
                drv.type = 'AVERAGE'

                var = drv.variables.new()
                var.name = prop
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = \
                    owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                drv_modifier.mode = 'POLYNOMIAL'
                drv_modifier.poly_order = 1
                drv_modifier.coefficients[0] = 1.0
                drv_modifier.coefficients[1] = -1.0

                if len(ctrl_pole.constraints) > 1:
                    drv = ctrl_pole.constraints[1].driver_add("mute").driver
                    drv.type = 'AVERAGE'

                    var = drv.variables.new()
                    var.name = prop
                    var.type = "SINGLE_PROP"
                    var.targets[0].id = self.obj
                    var.targets[0].data_path = \
                        owner.path_from_id() + '[' + '"' + prop + '"' + ']'

                    drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

                    drv_modifier.mode = 'POLYNOMIAL'
                    drv_modifier.poly_order = 1
                    drv_modifier.coefficients[0] = 1.0
                    drv_modifier.coefficients[1] = -1.0

            elif prop == 'root/parent':
                if len(ctrl.constraints) > 1:
                    owner[prop] = 0.0
                    rna_prop = rna_idprop_ui_prop_get(owner, prop, create=True)
                    rna_prop["min"] = 0.0
                    rna_prop["max"] = 1.0
                    rna_prop["soft_min"] = 0.0
                    rna_prop["soft_max"] = 1.0
                    rna_prop["description"] = prop

                    drv = ctrl.constraints[1].driver_add("influence").driver
                    drv.type = 'AVERAGE'

                    var = drv.variables.new()
                    var.name = prop
                    var.type = "SINGLE_PROP"
                    var.targets[0].id = self.obj
                    var.targets[0].data_path = \
                        owner.path_from_id() + '[' + '"' + prop + '"' + ']'

            elif prop == 'pole_follow':
                if len(ctrl_pole.constraints) > 1:
                    owner[prop] = 0.0
                    rna_prop = rna_idprop_ui_prop_get(owner, prop, create=True)
                    rna_prop["min"] = 0.0
                    rna_prop["max"] = 1.0
                    rna_prop["soft_min"] = 0.0
                    rna_prop["soft_max"] = 1.0
                    rna_prop["description"] = prop

                    drv = ctrl_pole.constraints[1].driver_add("influence").driver
                    drv.type = 'AVERAGE'

                    var = drv.variables.new()
                    var.name = prop
                    var.type = "SINGLE_PROP"
                    var.targets[0].id = self.obj
                    var.targets[0].data_path = \
                        owner.path_from_id() + '[' + '"' + prop + '"' + ']'

    @staticmethod
    def get_future_names(bones):

        if len(bones) != 4:
            return

        names = dict()

        thigh = strip_mch(strip_org(bones[0].name))
        shin = strip_mch(strip_org(bones[1].name))
        foot = strip_mch(strip_org(bones[2].name))
        toe = strip_mch(strip_org(bones[3].name))

        suffix = ''
        if thigh[-2:] == '.L' or thigh[-2:] == '.R':
            suffix = thigh[-2:]
            thigh = thigh.rstrip(suffix)
            shin = shin.rstrip(suffix)
            foot = foot.rstrip(suffix)
            toe = toe.rstrip(suffix)

        # the following is declared in rig_ui
        # controls = ['thigh_ik.R', 'thigh_fk.R', 'shin_fk.R', 'foot_fk.R', 'toe.R', 'foot_heel_ik.R', 'foot_ik.R',
        #             'MCH-foot_fk.R', 'thigh_parent.R']
        # tweaks = ['thigh_tweak.R.001', 'shin_tweak.R', 'shin_tweak.R.001']
        # ik_ctrl = ['foot_ik.R', 'MCH-thigh_ik.R', 'MCH-thigh_ik_target.R']
        # fk_ctrl = 'thigh_fk.R'
        # parent = 'thigh_parent.R'
        # foot_fk = 'foot_fk.R'
        # pole = 'thigh_ik_target.R'

        names['controls'] = [thigh + '_ik', thigh + '_fk', shin + '_fk', foot + '_fk', toe, foot + '_heel_ik',
                             foot + '_ik', make_mechanism_name(foot + '_fk'), thigh + '_parent']
        names['ik_ctrl'] = [foot + '_ik', make_mechanism_name(thigh) + '_ik', make_mechanism_name(thigh) + '_ik_target']
        names['fk_ctrl'] = thigh + '_fk' + suffix
        names['parent'] = thigh + '_parent' + suffix
        names['foot_fk'] = foot + '_fk' + suffix
        names['pole'] = thigh + '_ik_target' + suffix

        names['limb_type'] = 'leg'

        if suffix:
            for i, name in enumerate(names['controls']):
                names['controls'][i] = name + suffix
            for i, name in enumerate(names['ik_ctrl']):
                names['ik_ctrl'][i] = name + suffix

        return names

    def generate(self):
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Adjust org-bones rotation
        self.orient_org_bones()

        # Clear parents for org bones
        for bone in self.org_bones[1:]:
            eb[bone].use_connect = False
            eb[bone].parent = None

        bones = {}

        # Create mch limb parent
        mch_parent, main_parent = self.create_parent()
        bones['parent'] = mch_parent
        bones['main_parent'] = main_parent
        bones['tweak'] = self.create_tweak()
        bones['def'] = self.create_def(bones['tweak']['ctrl'])
        bones['ik'] = self.create_ik(bones['parent'])
        bones['fk'] = self.create_fk(bones['parent'])

        self.org_parenting_and_switch(self.org_bones, bones['ik'], bones['fk']['ctrl'], bones['main_parent'])

        bones = self.create_leg(bones)
        self.create_drivers(bones)

        controls = [bones['ik']['ctrl']['limb'], bones['ik']['ctrl']['terminal'][-1], bones['ik']['ctrl']['terminal'][-2] ]

        controls.append(bones['main_parent'])

        # Create UI
        controls_string = ", ".join(["'" + x + "'" for x in controls])

        script = create_script(bones, 'leg')
        script += extra_script % (controls_string, bones['main_parent'], 'IK_follow',
                                  'pole_follow', 'pole_follow', 'root/parent', 'root/parent')

        return [script]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """

    items = [
        ('x', 'X manual', ''),
        ('z', 'Z manual', ''),
        ('automatic', 'Automatic', '')
    ]

    params.rotation_axis = bpy.props.EnumProperty(
        items   = items,
        name    = "Rotation Axis",
        default = 'automatic'
    )

    params.auto_align_extremity = bpy.props.BoolProperty(
        name='auto_align_extremity',
        default=False,
        description="Auto Align Extremity Bone"
    )

    params.segments = bpy.props.IntProperty(
        name        = 'limb segments',
        default     = 2,
        min         = 1,
        description = 'Number of segments'
    )

    params.bbones = bpy.props.IntProperty(
        name        = 'bbone segments',
        default     = 10,
        min         = 1,
        description = 'Number of segments'
    )

    # Setting up extra layers for the FK and tweak
    params.tweak_extra_layers = bpy.props.BoolProperty(
        name        = "tweak_extra_layers",
        default     = True,
        description = ""
        )

    params.tweak_layers = bpy.props.BoolVectorProperty(
        size        = 32,
        description = "Layers for the tweak controls to be on",
        default     = tuple( [ i == 1 for i in range(0, 32) ] )
        )

    # Setting up extra layers for the FK and tweak
    params.fk_extra_layers = bpy.props.BoolProperty(
        name        = "fk_extra_layers",
        default     = True,
        description = ""
        )

    params.fk_layers = bpy.props.BoolVectorProperty(
        size        = 32,
        description = "Layers for the FK controls to be on",
        default     = tuple( [ i == 1 for i in range(0, 32) ] )
        )


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    r = layout.row()
    r.prop(params, "rotation_axis")

    if 'auto' not in params.rotation_axis.lower():
        r = layout.row()
        text = "Auto align Foot"
        r.prop(params, "auto_align_extremity", text=text)

    r = layout.row()
    r.prop(params, "segments")

    r = layout.row()
    r.prop(params, "bbones")

    bone_layers = bpy.context.active_pose_bone.bone.layers[:]

    for layer in ['fk', 'tweak']:
        r = layout.row()
        r.prop(params, layer + "_extra_layers")
        r.active = params.tweak_extra_layers

        col = r.column(align=True)
        row = col.row(align=True)

        for i in range(8):
            icon = "NONE"
            if bone_layers[i]:
                icon = "LAYER_ACTIVE"
            row.prop(params, layer + "_layers", index=i, toggle=True, text="", icon=icon)

        row = col.row(align=True)

        for i in range(16, 24):
            icon = "NONE"
            if bone_layers[i]:
                icon = "LAYER_ACTIVE"
            row.prop(params, layer + "_layers", index=i, toggle=True, text="", icon=icon)

        col = r.column(align=True)
        row = col.row(align=True)

        for i in range(8, 16):
            icon = "NONE"
            if bone_layers[i]:
                icon = "LAYER_ACTIVE"
            row.prop(params, layer + "_layers", index=i, toggle=True, text="", icon=icon)

        row = col.row(align=True)

        for i in range(24, 32):
            icon = "NONE"
            if bone_layers[i]:
                icon = "LAYER_ACTIVE"
            row.prop(params, layer + "_layers", index=i, toggle=True, text="", icon=icon)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('thigh.L')
    bone.head[:] = 0.0980, 0.0124, 1.0720
    bone.tail[:] = 0.0980, -0.0286, 0.5372
    bone.roll = 0.0000
    bone.use_connect = False
    bones['thigh.L'] = bone.name
    bone = arm.edit_bones.new('shin.L')
    bone.head[:] = 0.0980, -0.0286, 0.5372
    bone.tail[:] = 0.0980, 0.0162, 0.0852
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['thigh.L']]
    bones['shin.L'] = bone.name
    bone = arm.edit_bones.new('foot.L')
    bone.head[:] = 0.0980, 0.0162, 0.0852
    bone.tail[:] = 0.0980, -0.0934, 0.0167
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['shin.L']]
    bones['foot.L'] = bone.name
    bone = arm.edit_bones.new('toe.L')
    bone.head[:] = 0.0980, -0.0934, 0.0167
    bone.tail[:] = 0.0980, -0.1606, 0.0167
    bone.roll = -0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['foot.L']]
    bones['toe.L'] = bone.name
    bone = arm.edit_bones.new('heel.02.L')
    bone.head[:] = 0.0600, 0.0459, 0.0000
    bone.tail[:] = 0.1400, 0.0459, 0.0000
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['foot.L']]
    bones['heel.02.L'] = bone.name


    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['thigh.L']]
    pbone.rigify_type = 'limbs.super_limb'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.separate_ik_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.ik_layers = [False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.separate_hose_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.hose_layers = [False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.limb_type = "leg"
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.fk_layers = [False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.tweak_layers = [False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['shin.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['foot.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['toe.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['heel.02.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone.select = False
        bone.select_head = False
        bone.select_tail = False
    for b in bones:
        bone = arm.edit_bones[bones[b]]
        bone.select = True
        bone.select_head = True
        bone.select_tail = True
        arm.edit_bones.active = bone

    for eb in arm.edit_bones:
        eb.layers = (False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False)

    arm.layers = (False, False, False, False, False, False, False, False, False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False)


if __name__ == "__main__":
    create_sample(bpy.context.active_object)