# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# A framework to run regression tests on mesh operators
#
# General idea:
# A test is:
#    Object mode
#    Select <test object>
#    Duplicate the object
#    Edit mode
#    Select None
#    Select according to <select spec>
#    Apply <mesh operator> with <test params>
#    Object mode
#    test_mesh = <test object>.data
#    run test_mesh.validate()
#    run test_mesh.unit_test_compare(<expected object>.data)
#    delete the duplicate object
#
# The things in angle brackets are parameters of the test, and are specified in
# a declarative TestSpec.
#
# If tests fail and it is because of a known and OK change due to things that have
# changed in Blender, we can use the 'update_expected' parameter of RunTest
# to update the <expected object>.

import bpy


class TestSpec:
    """Test specification.

    Holds names of test and expected result objects,
    a selection specification,
    a mesh op to run and its arguments
    """

    def __init__(self, op, test_obj, expected_obj, select, params):
        """Construct a test spec.

        Args:
            op: string - name of a function in bpy.ops.mesh
            test_obj: string - name of the object to apply the test to
            expected_obj: string - name of the object that has the expected result
            select: string - should be V, E, or F followed by space separated indices of desired selection
            params: string - space-separated name=val pairs giving operator arguments
        """

        self.test_obj = test_obj
        self.expected_obj = expected_obj
        self.select = select
        self.op = op
        self.params = params

    def __str__(self):
        return self.op + "(" + self.params + ") on " + self.test_obj + " selecting " + self.select

    def ParseParams(self, verbose=False):
        """Parse a space-separated list of name=value pairs.

        Args:
            self: TestSpec
        Returns:
            dict - the parsed self.params
        """

        ans = {}
        nvs = self.params.split()
        for nv in nvs:
            parts = nv.split('=')
            if len(parts) != 2:
                if verbose:
                    print('Parameter syntax error at', nv)
                break
            name = parts[0]
            try:
                val = eval(parts[1])
            except SyntaxError:
                if verbose:
                    print('Parameter value syntax error at', nv)
                break
            ans[name] = val
        return ans


# This only works if there is a 3D View area visible somewhere in current window
def GetView3DContext(verbose=False):
    """Get a context dictionary for a View3D window.

    This can be used as a context override to ensure that an operator will
    be executed with a View3D window and other variables implied by that context.

    Args:
        verbose: bool - should we be wordy about errors
    Returns:
        dict - with keys for window, screen, area, region, and scene
    """

    win = bpy.context.window
    scr = win.screen
    a3d = None
    for a in scr.areas:
        if a.type == 'VIEW_3D':
            a3d = a
            break
    if a3d is None:
        if verbose:
            print('No 3d view area')
        return None
    rwin = None
    for r in a3d.regions:
        if r.type == 'WINDOW':
            rwin = r
            break
    if rwin is None:
        if verbose:
            print('No window in 3d view area')
        return None
    return {'window': win, 'screen': scr, 'area': a3d, 'region': rwin, 'scene': bpy.context.scene}


def DoMeshSelect(select, verbose=False):
    """Given a selection spec string, switch to the desired selection mode and do the select.

    Assume we are in Object mode on a mesh object.
    The selection spec string should start with V, E, or F (for vertex, edge, face) to choose
    the kind of element selected, and the is followed with a list of element indices
    (use --debug to Blender to visualize these).

    Args:
        select: string - see comment above for syntax
        verbose: bool - should we be wordy
    Returns:
        string - which select_mode to switch to
    """

    m = bpy.context.active_object.data
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode='OBJECT')
    if m.is_editmode:
        if verbose:
            print('Select failed, not in Object mode')
        return 'VERT'
    if not select:
        if verbose:
            print('No select spec')
        return 'VERT'
    if select[0] == 'V':
        seltype = 'VERT'
    elif select[0] == 'E':
        seltype = 'EDGE'
    elif select[0] == 'F':
        seltype = 'FACE'
    else:
        if verbose:
            print('Bad select type', select[0])
        return 'VERT'
    bpy.context.tool_settings.mesh_select_mode = (seltype == 'VERT', seltype == 'EDGE', seltype == 'FACE')
    parts = select[1:].split()
    try:
        elems = set([int(p) for p in parts])
    except ValueError:
        if verbose:
            print('Bad syntax in select spec', select)
        return
    for i in elems:
        if seltype == 'VERT':
            m.vertices[i].select = True
        elif seltype == 'EDGE':
            m.edges[i].select = True
        else:
            m.polygons[i].select = True
    return seltype


def RunTest(t, cleanup=True, verbose=False, update_expected=False):
    """Run the test specified by given TestSpec.

    Args:
        t: TestSpec
        cleanup: bool - should we clean up duplicate after the test
        verbose: bool - should be we wordy
        update_expected: bool - should we replace the golden expected object
                                 with the result of current run?
                                 Only has effect if cleanup is false.
    Returns:
        bool - True if test passes, False otherwise
    """

    if verbose:
        print("Run test:", t)
    objs = bpy.data.objects
    if t.test_obj in objs:
        otest = objs[t.test_obj]
    else:
        if verbose:
            print('No test object', t.test_obj)
        return False
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = otest
    otest.select_set(True)
    bpy.ops.object.duplicate()
    otestdup = bpy.context.active_object
    smode = DoMeshSelect(t.select, verbose=verbose)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_mode(type=smode)
    f = getattr(bpy.ops.mesh, t.op)
    if not f:
        if verbose:
            print('No mesh op', t.op)
        if cleanup:
            bpy.ops.object.delete()
        return False
    kw = t.ParseParams(verbose)
    retval = f(**kw)
    if retval != {'FINISHED'}:
        if verbose:
            print('unexpected operator return value', retval)
        if cleanup:
            bpy.ops.object.delete()
        return False
    bpy.ops.object.mode_set(mode='OBJECT')
    if t.expected_obj in objs:
        oexpected = objs[t.expected_obj]
    else:
        # If no expected object, test is run just for effect
        if verbose:
            print('No expected object', t.expected_obj)
        return True
    mtest = otestdup.data
    mexpected = oexpected.data
    cmpret = mtest.unit_test_compare(mesh=mexpected)
    success = (cmpret == 'Same')
    if success:
        if verbose:
            print('Success')
    else:
        if verbose:
            print('Fail', cmpret)
    if cleanup:
        bpy.ops.object.delete()
        otest.select_set(state=True, view_layer=None)
        bpy.context.view_layer.objects.active = otest
    elif update_expected:
        if verbose:
            print('Updating expected object', t.expected_obj)
        oexpected.name = oexpected.name + '_pendingdelete'
        otestdup.location = oexpected.location
        expected_collections = oexpected.users_collection
        testdup_collections = otestdup.users_collection
        # should be exactly 1 collection each for otestdup and oexpected
        tcoll = testdup_collections[0]
        ecoll = expected_collections[0]
        ecoll.objects.link(otestdup)
        tcoll.objects.unlink(otestdup)
        bpy.context.view_layer.objects.active = oexpected
        bpy.ops.object.select_all(action='DESELECT')
        oexpected.select_set(state=True, view_layer=None)
        bpy.ops.object.delete()
        otestdup.name = t.expected_obj
        bpy.context.view_layer.objects.active = otest
    return success


def RunAllTests(tests, cleanup=True, verbose=False, update_expected=False):
    """Run all tests.

    Args:
        tests: list of TestSpec - tests to run
        cleanup: bool - if True, don't leave result objects lying around
        verbose: bool - if True, chatter about running tests and failures
        update_expected: bool - if True, replace all expected objects with
                                current results
    Returns:
        bool - True if all tests pass
    """

    tot = 0
    failed = 0
    for t in tests:
        tot += 1
        if not RunTest(t, cleanup=cleanup, verbose=verbose, update_expected=update_expected):
            failed += 1
    if verbose:
        print('Ran', tot, 'tests,' if tot > 1 else 'test,', failed, 'failed')
    if failed > 0:
        print('Tests Failed')
    return failed == 0
