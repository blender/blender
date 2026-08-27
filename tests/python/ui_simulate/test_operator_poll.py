import modules.ui_test_utils as ui


def _register():
    import bpy
    from bpy.types import Operator
    from bpy.utils import register_class

    if not hasattr(bpy.types, "WM_OT_ui_test_poll_gated"):
        class UiTestPollGated(Operator):
            bl_idname = "wm.ui_test_poll_gated"

            # NOTE: search matches against the operator *label*, not bl_idname,
            # so the "nqz" search text below must keep matching this label.
            # "NQZ" is an arbitrary, unlikely-to-collide search token used to
            # reliably find this test-only operator.
            bl_label = "NQZ Gated"

            @classmethod
            def poll(cls, context):
                return context.scene.get("ui_test_allow_gated", False)

            def execute(self, context):
                context.scene["ui_simulate_marker"] = context.scene.get("ui_simulate_marker", 0) + 1
                return {'FINISHED'}
        register_class(UiTestPollGated)


def test_operator_search_poll_gating():
    """
    Verify operator search respects an operator's poll() result.

    The test searches for a test-only operator with poll() first disabled
    and then enabled, confirming that the operator is only executable
    through search when poll() returns True.
    """
    import bpy
    _register()
    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    region = next(r for r in area.regions if r.type == 'WINDOW')

    scene = bpy.context.scene
    marker_before = scene.get("ui_simulate_marker", 0)
    allow_before = scene.get("ui_test_allow_gated", None)

    try:
        # Poll disabled: operator should not appear in search.
        # The search UI may emit " ERROR Failed to find 'nqz' " in the logs.
        scene["ui_test_allow_gated"] = False
        m0 = scene.get("ui_simulate_marker", 0)

        with bpy.context.temp_override(window=window, area=area, region=region):
            bpy.ops.wm.search_operator('INVOKE_DEFAULT')

        yield e.text("nqz")
        yield e.ret()
        yield

        m1 = scene.get("ui_simulate_marker", 0)
        t.assertEqual(m1, m0, "poll=False operator must not be found")
        t.assertFalse(bpy.ops.wm.ui_test_poll_gated.poll())

        # Poll enabled: now findable.
        scene["ui_test_allow_gated"] = True
        with bpy.context.temp_override(window=window, area=area, region=region):
            bpy.ops.wm.search_operator('INVOKE_DEFAULT')

        yield e.text("nqz")
        yield e.ret()
        yield

        m2 = scene.get("ui_simulate_marker", 0)
        t.assertEqual(m2, m1 + 1, "poll=True operator must be found")
        t.assertTrue(bpy.ops.wm.ui_test_poll_gated.poll())

    finally:
        # Reset scene state so this test doesn't leak into whatever runs next.
        if allow_before is None:
            scene.pop("ui_test_allow_gated", None)
        else:
            scene["ui_test_allow_gated"] = allow_before
        scene["ui_simulate_marker"] = marker_before


def test_operator_poll_sweep():
    """
    Sweep every registered operator's poll() across every settable editor
        type and confirm each call returns a plain bool rather than raising or
        returning something poll() shouldn't (None, a string, etc).
    """
    _, t, window = ui.test_window()
    area = ui.largest_area(window.screen)

    failures = []
    total = 0
    sweep = ui.poll_sweep(window, area)
    for tick in sweep:
        if tick is None:
            # Editor-switch tick: give the UI a frame to process the change.
            yield
            continue
        _, area_failures, area_total = tick
        total += area_total
        failures.extend(area_failures)

    print(f"poll sweep: {total} calls, {len(failures)} failures", flush=True)
    for f in failures[:10]:
        print("  ", f, flush=True)
    t.assertEqual(len(failures), 0, msg="\n".join(failures[:20]))
