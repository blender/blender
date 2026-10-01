# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Tests for which OpenColorIO configuration Blender picks when opening blend files
# inside a project.

import pathlib
import shutil
import sys
import tempfile
import unittest

import bpy


class OCIOConfigProjectTest(unittest.TestCase):
    """Configuration coming from the active project's `project.toml`."""

    @classmethod
    def setUpClass(cls):
        colorspace = bpy.data.colorspace
        if colorspace.ocio_config_source != 'BLENDER':
            raise unittest.SkipTest("Expected the blender config, is OCIO or BLENDER_OCIO set?")
        cls.blender_config_path = colorspace.ocio_config_path

        cls._tempdir = tempfile.TemporaryDirectory(prefix="bl_ocio_config_")
        cls.tempdir = pathlib.Path(cls._tempdir.name)

        # Blend file to copy into projects. Saved as a copy, so no project is loaded for it.
        cls.template_blend = cls.tempdir / "template.blend"
        bpy.ops.wm.save_as_mainfile(filepath=str(cls.template_blend), copy=True)

    @classmethod
    def tearDownClass(cls):
        cls._tempdir.cleanup()

    def tearDown(self):
        bpy.ops.wm.read_factory_settings()

    def make_blend(self, filepath):
        filepath.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(self.template_blend, filepath)
        return filepath

    def make_project(self, name, ocio_config=None, blend_subdir=""):
        """Create a project directory containing a blend file.

        Returns the path of the blend file. `ocio_config` of None writes a
        `project.toml` with no `ocio_config` key at all, as written by a Blender
        version that predates that key.
        """
        root = self.tempdir / name
        (root / ".blender_project").mkdir(parents=True)

        config_lines = [f'name = "{name}"\n']
        if ocio_config is not None:
            config_lines.append(f"ocio_config = '{ocio_config}'\n")
        (root / ".blender_project" / "project.toml").write_text("".join(config_lines))

        return self.make_blend(root / blend_subdir / "shot.blend")

    def open_blend(self, filepath):
        bpy.ops.wm.open_mainfile(filepath=str(filepath))
        colorspace = bpy.data.colorspace
        return colorspace.ocio_config_source, colorspace.ocio_config_path

    def test_project_config_in_nested_blend_dir(self):
        """Project discovery walks parent directories."""
        blendfile = self.make_project(
            "proj-nested",
            ocio_config="ocio://default",
            blend_subdir="sequences/010/shots",
        )
        self.assertEqual(self.open_blend(blendfile), ('PROJECT', "ocio://default"))

    def test_project_without_ocio_config_key_still_loads(self):
        """A `project.toml` written before this field existed must still work."""
        blendfile = self.make_project("proj-old")
        self.assertEqual(self.open_blend(blendfile), ('BLENDER', self.blender_config_path))
        self.assertIsNotNone(bpy.data.project)
        self.assertEqual(bpy.data.project.name, "proj-old")

    def test_config_switches_between_projects_in_one_session(self):
        """Opening files in different projects in one process follows each project."""
        blend_a = self.make_project("proj-a", ocio_config="ocio://default")
        blend_b = self.make_project("proj-b", ocio_config=self.blender_config_path)
        blend_none = self.make_blend(self.tempdir / "no-project" / "loose.blend")

        self.assertEqual(self.open_blend(blend_a), ('PROJECT', "ocio://default"))
        self.assertEqual(self.open_blend(blend_b), ('PROJECT', self.blender_config_path))
        self.assertEqual(self.open_blend(blend_none), ('BLENDER', self.blender_config_path))


if __name__ == "__main__":
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
