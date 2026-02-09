"""
Annotation API Tests

Tests for the reintroduced annotation stroke and point manipulation API
introduced in Blender 4.3+ to restore basic programmatic control over annotations.

Based on implementation in source/blender/makesrna/intern/rna_annotations.cc:
- AnnotationStrokes.new()
- AnnotationStrokes.remove(stroke)
- AnnotationStrokePoints.add(count, pressure, strength)
- AnnotationStrokePoints.remove(index)
"""
# Usage:
#   ~/blender-git/build_linux/bin/blender --background --factory-startup --python tests/python/bl_pyapi_annotations.py -- --verbose

import bpy
import unittest


class TestAnnotationStrokes(unittest.TestCase):
    """Tests for AnnotationStrokes API (new/remove strokes)"""

    def setUp(self):
        """Create fresh annotation data for each test"""
        # Clear existing annotations.
        for ann in bpy.data.annotations[:]:
            bpy.data.annotations.remove(ann, do_unlink=True)

        # Create new annotation datablock.
        self.ann = bpy.data.annotations.new("TestAnnotation")
        self.layer = self.ann.layers.new("TestLayer")
        self.frame = self.layer.frames.new(1)

    def tearDown(self):
        """Clean up after each test"""
        for ann in bpy.data.annotations[:]:
            bpy.data.annotations.remove(ann, do_unlink=True)

    def test_stroke_new_creates_stroke(self):
        """Test that new() creates a stroke successfully"""
        stroke = self.frame.strokes.new()

        self.assertIsNotNone(stroke)
        self.assertEqual(len(self.frame.strokes), 1)
        # Verify the stroke is accessible by index.
        self.assertEqual(self.frame.strokes[0], stroke)

    def test_stroke_new_default_values(self):
        """Test that new strokes have correct default values"""
        stroke = self.frame.strokes.new()

        # Based on rna_annotation_stroke_new implementation.
        # Note: thickness is not exposed in RNA, so we check display_mode instead.
        self.assertEqual(stroke.display_mode, '3DSPACE')

    def test_stroke_new_multiple_strokes(self):
        """Test creating multiple strokes in same frame"""
        stroke1 = self.frame.strokes.new()
        stroke2 = self.frame.strokes.new()
        stroke3 = self.frame.strokes.new()

        self.assertEqual(len(self.frame.strokes), 3)
        # Verify strokes are accessible.
        self.assertEqual(self.frame.strokes[0], stroke1)
        self.assertEqual(self.frame.strokes[1], stroke2)
        self.assertEqual(self.frame.strokes[2], stroke3)

    def test_stroke_remove_valid_stroke(self):
        """Test removing a valid stroke"""
        stroke = self.frame.strokes.new()
        self.assertEqual(len(self.frame.strokes), 1)

        self.frame.strokes.remove(stroke)

        self.assertEqual(len(self.frame.strokes), 0)

    def test_stroke_remove_invalid_stroke_reports_error(self):
        """Test that removing stroke not in frame reports error"""
        # Create stroke in one frame.
        stroke1 = self.frame.strokes.new()

        # Create different frame (use different frame number to avoid conflict).
        frame2 = self.layer.frames.new(10)

        # Try to remove stroke from wrong frame.
        with self.assertRaises(RuntimeError) as context:
            frame2.strokes.remove(stroke1)

        self.assertIn("Stroke not found", str(context.exception))

    def test_stroke_remove_clears_pointer(self):
        """Test that remove() invalidates the stroke pointer"""
        stroke = self.frame.strokes.new()

        self.frame.strokes.remove(stroke)

        # After removal, the pointer should be invalidated.
        with self.assertRaises((ReferenceError, AttributeError)):
            _ = stroke.display_mode

    def test_stroke_remove_multiple_strokes(self):
        """Test removing multiple strokes"""
        strokes = [self.frame.strokes.new() for _ in range(5)]
        self.assertEqual(len(self.frame.strokes), 5)

        # Remove middle stroke.
        self.frame.strokes.remove(strokes[2])
        self.assertEqual(len(self.frame.strokes), 4)

        # Remove first stroke.
        self.frame.strokes.remove(strokes[0])
        self.assertEqual(len(self.frame.strokes), 3)

        # Remove last stroke.
        self.frame.strokes.remove(strokes[4])
        self.assertEqual(len(self.frame.strokes), 2)


class TestAnnotationStrokePoints(unittest.TestCase):
    """Tests for AnnotationStrokePoints API (add/remove points)"""

    def setUp(self):
        """Create fresh annotation data with a stroke for each test"""
        for ann in bpy.data.annotations[:]:
            bpy.data.annotations.remove(ann, do_unlink=True)

        self.ann = bpy.data.annotations.new("TestAnnotation")
        self.layer = self.ann.layers.new("TestLayer")
        self.frame = self.layer.frames.new(1)
        self.stroke = self.frame.strokes.new()

    def tearDown(self):
        """Clean up after each test"""
        for ann in bpy.data.annotations[:]:
            bpy.data.annotations.remove(ann, do_unlink=True)

    def test_point_add_multiple_points(self):
        """Test adding multiple points at once"""
        initial_count = len(self.stroke.points)

        self.stroke.points.add(count=5)

        self.assertEqual(len(self.stroke.points), initial_count + 5)

    def test_point_add_preserves_existing_points(self):
        """Test that adding points preserves existing point data"""
        # Add initial points.
        self.stroke.points.add(count=2)
        self.stroke.points[0].co = (1.0, 2.0, 3.0)
        self.stroke.points[1].co = (4.0, 5.0, 6.0)

        # Store original values.
        orig_co_0 = tuple(self.stroke.points[0].co)
        orig_co_1 = tuple(self.stroke.points[1].co)

        # Add more points.
        self.stroke.points.add(count=3)

        # Verify original points unchanged.
        self.assertAlmostEqual(self.stroke.points[0].co[0], orig_co_0[0], places=5)
        self.assertAlmostEqual(self.stroke.points[0].co[1], orig_co_0[1], places=5)
        self.assertAlmostEqual(self.stroke.points[0].co[2], orig_co_0[2], places=5)

        self.assertAlmostEqual(self.stroke.points[1].co[0], orig_co_1[0], places=5)
        self.assertAlmostEqual(self.stroke.points[1].co[1], orig_co_1[1], places=5)
        self.assertAlmostEqual(self.stroke.points[1].co[2], orig_co_1[2], places=5)

    def test_point_remove_index_too_large_raises_error(self):
        """Test that index >= totpoints raises error"""
        self.stroke.points.add(count=3)

        with self.assertRaises(RuntimeError) as context:
            self.stroke.points.remove(index=100)

        self.assertIn("out of range", str(context.exception).lower())

    def test_point_remove_valid_index(self):
        """Test removing a point at valid index"""
        self.stroke.points.add(count=3)
        initial_count = len(self.stroke.points)

        self.stroke.points.remove(index=1)

        self.assertEqual(len(self.stroke.points), initial_count - 1)

    def test_point_remove_first_point(self):
        """Test removing the first point"""
        self.stroke.points.add(count=3)
        self.stroke.points[0].co = (1.0, 0.0, 0.0)
        self.stroke.points[1].co = (2.0, 0.0, 0.0)
        self.stroke.points[2].co = (3.0, 0.0, 0.0)

        self.stroke.points.remove(index=0)

        # First point should now be the old second point.
        self.assertAlmostEqual(self.stroke.points[0].co[0], 2.0, places=5)
        self.assertEqual(len(self.stroke.points), 2)

    def test_point_remove_last_point(self):
        """Test removing the last point"""
        self.stroke.points.add(count=3)
        initial_count = len(self.stroke.points)

        self.stroke.points.remove(index=initial_count - 1)

        self.assertEqual(len(self.stroke.points), initial_count - 1)

    def test_point_remove_from_empty_stroke_raises_error(self):
        """Test that removing from stroke with no points raises error"""
        self.assertEqual(len(self.stroke.points), 0)

        with self.assertRaises(RuntimeError) as context:
            self.stroke.points.remove(index=0)

        self.assertIn("no points", str(context.exception).lower())

    def test_point_remove_last_remaining_point_raises_error(self):
        """Test that removing the only remaining point raises error"""
        self.stroke.points.add(count=1)

        with self.assertRaises(RuntimeError) as context:
            self.stroke.points.remove(index=0)

        self.assertIn("cannot remove last point", str(context.exception).lower())


def main():
    """Main test runner compatible with Blender's test framework"""
    import sys

    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    argv.insert(0, __file__)

    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    suite.addTests(loader.loadTestsFromTestCase(TestAnnotationStrokes))
    suite.addTests(loader.loadTestsFromTestCase(TestAnnotationStrokePoints))

    verbosity = 2 if "--verbose" in argv else 1
    runner = unittest.TextTestRunner(verbosity=verbosity, stream=sys.stdout)
    result = runner.run(suite)

    sys.exit(not result.wasSuccessful())


if __name__ == "__main__":
    main()
