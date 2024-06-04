#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring

import unittest
from dataclasses import dataclass

# XXX Not really nice, but that hack is needed to allow execution of that test
#     from both automated CTest and by directly running the file manually.
if __name__ == "__main__":
    from paths import match_files_to_socket_names
else:
    from .paths import match_files_to_socket_names


# From NWPrincipledPreferences 2023-01-06
TAGS_DISPLACEMENT = "displacement displace disp dsp height heightmap".split(" ")
TAGS_BASE_COLOR = "diffuse diff albedo base col color basecolor".split(" ")
TAGS_METALLIC = "metallic metalness metal mtl".split(" ")
TAGS_SPECULAR = "specularity specular spec spc".split(" ")
TAGS_ROUGHNESS = "roughness rough rgh".split(" ")
TAGS_GLOSS = "gloss glossy glossiness".split(" ")
TAGS_NORMAL = "normal nor nrm nrml norm".split(" ")
TAGS_BUMP = "bump bmp".split(" ")
TAGS_TRANSMISSION = "transmission transparency".split(" ")
TAGS_EMISSION = "emission emissive emit".split(" ")
TAGS_ALPHA = "alpha opacity".split(" ")
TAGS_AMBIENT_OCCLUSION = "ao ambient occlusion".split(" ")


@dataclass
class MockFile:
    name: str


def sockets_fixture():
    return [
        ["Displacement", TAGS_DISPLACEMENT, None],
        ["Base Color", TAGS_BASE_COLOR, None],
        ["Metallic", TAGS_METALLIC, None],
        ["Specular", TAGS_SPECULAR, None],
        ["Roughness", TAGS_ROUGHNESS + TAGS_GLOSS, None],
        ["Normal", TAGS_NORMAL + TAGS_BUMP, None],
        ["Transmission Weight", TAGS_TRANSMISSION, None],
        ["Emission Color", TAGS_EMISSION, None],
        ["Alpha", TAGS_ALPHA, None],
        ["Ambient Occlusion", TAGS_AMBIENT_OCCLUSION, None],
    ]


def assert_sockets(asserter, sockets, expected):
    checked_sockets = set()
    errors = []
    for socket_name, expected_path in expected.items():
        if isinstance(expected_path, str):
            expected_path = [expected_path]

        socket_found = False
        for socket in sockets:
            if socket[0] != socket_name:
                continue
            socket_found = True

            actual_path = socket[2]
            if actual_path not in expected_path:
                errors.append(
                    f"{socket_name:12}: Got {actual_path} but expected {expected_path}"
                )
            checked_sockets.add(socket_name)
            break
        asserter.assertTrue(socket_found)
    asserter.assertCountEqual([], errors)

    for socket in sockets:
        if socket[0] in checked_sockets:
            continue
        asserter.assertEqual(socket[2], None)


class TestPutFileNamesInSockets(unittest.TestCase):
    def test_no_files_selected(self):
        sockets = sockets_fixture()
        match_files_to_socket_names([], sockets)

        assert_sockets(self, sockets, {})

    def test_weird_filename(self):
        sockets = sockets_fixture()
        match_files_to_socket_names(
            [MockFile(""), MockFile(".jpg"), MockFile(" .png"), MockFile("...")],
            sockets,
        )

        assert_sockets(self, sockets, {})

    def test_poliigon(self):
        """Texture from: https://www.poliigon.com/texture/metal-spotty-discoloration-001/3225"""

        # NOTE: These files all have directory prefixes. That's on purpose. Files
        # without directory prefixes are tested in test_ambientcg_metal().
        files = [
            MockFile("d/MetalSpottyDiscoloration001_COL_2K_METALNESS.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_Cube.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_DISP16_2K_METALNESS.tif"),
            MockFile("d/MetalSpottyDiscoloration001_DISP_2K_METALNESS.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_Flat.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_METALNESS_2K_METALNESS.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_NRM16_2K_METALNESS.tif"),
            MockFile("d/MetalSpottyDiscoloration001_NRM_2K_METALNESS.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_ROUGHNESS_2K_METALNESS.jpg"),
            MockFile("d/MetalSpottyDiscoloration001_Sphere.jpg"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Base Color": "d/MetalSpottyDiscoloration001_COL_2K_METALNESS.jpg",
                "Displacement": [
                    "d/MetalSpottyDiscoloration001_DISP16_2K_METALNESS.tif",
                    "d/MetalSpottyDiscoloration001_DISP_2K_METALNESS.jpg",
                ],
                "Metallic": "d/MetalSpottyDiscoloration001_METALNESS_2K_METALNESS.jpg",
                "Normal": [
                    "d/MetalSpottyDiscoloration001_NRM16_2K_METALNESS.tif",
                    "d/MetalSpottyDiscoloration001_NRM_2K_METALNESS.jpg",
                ],
                "Roughness": "d/MetalSpottyDiscoloration001_ROUGHNESS_2K_METALNESS.jpg",
            },
        )

    def test_ambientcg(self):
        """Texture from: https://ambientcg.com/view?id=MetalPlates003"""

        # NOTE: These files have no directory prefix. That's on purpose. Files
        # with directory prefixes are tested in test_poliigon_metal().
        files = [
            MockFile("MetalPlates001_1K-JPG.usda"),
            MockFile("MetalPlates001_1K-JPG.usdc"),
            MockFile("MetalPlates001_1K_Color.jpg"),
            MockFile("MetalPlates001_1K_Displacement.jpg"),
            MockFile("MetalPlates001_1K_Metalness.jpg"),
            MockFile("MetalPlates001_1K_NormalDX.jpg"),
            MockFile("MetalPlates001_1K_NormalGL.jpg"),
            MockFile("MetalPlates001_1K_Roughness.jpg"),
            MockFile("MetalPlates001_PREVIEW.jpg"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Base Color": "MetalPlates001_1K_Color.jpg",
                "Displacement": "MetalPlates001_1K_Displacement.jpg",
                "Metallic": "MetalPlates001_1K_Metalness.jpg",
                # Blender wants GL normals:
                # https://www.reddit.com/r/blender/comments/rbuaua/texture_contains_normaldx_and_normalgl_files/
                "Normal": "MetalPlates001_1K_NormalGL.jpg",
                "Roughness": "MetalPlates001_1K_Roughness.jpg",
            },
        )

    def test_3dtextures_me(self):
        """Texture from: https://3dtextures.me/2022/05/13/metal-006/"""

        files = [
            MockFile("Material_2079.jpg"),
            MockFile("Metal_006_ambientOcclusion.jpg"),
            MockFile("Metal_006_basecolor.jpg"),
            MockFile("Metal_006_height.png"),
            MockFile("Metal_006_metallic.jpg"),
            MockFile("Metal_006_normal.jpg"),
            MockFile("Metal_006_roughness.jpg"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Ambient Occlusion": "Metal_006_ambientOcclusion.jpg",
                "Base Color": "Metal_006_basecolor.jpg",
                "Displacement": "Metal_006_height.png",
                "Metallic": "Metal_006_metallic.jpg",
                "Normal": "Metal_006_normal.jpg",
                "Roughness": "Metal_006_roughness.jpg",
            },
        )

    def test_polyhaven(self):
        """Texture from: https://polyhaven.com/a/rusty_metal_02"""

        files = [
            MockFile("rusty_metal_02_ao_1k.jpg"),
            MockFile("rusty_metal_02_arm_1k.jpg"),
            MockFile("rusty_metal_02_diff_1k.jpg"),
            MockFile("rusty_metal_02_disp_1k.png"),
            MockFile("rusty_metal_02_nor_dx_1k.exr"),
            MockFile("rusty_metal_02_nor_gl_1k.exr"),
            MockFile("rusty_metal_02_rough_1k.exr"),
            MockFile("rusty_metal_02_spec_1k.png"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Ambient Occlusion": "rusty_metal_02_ao_1k.jpg",
                "Base Color": "rusty_metal_02_diff_1k.jpg",
                "Displacement": "rusty_metal_02_disp_1k.png",
                "Normal": "rusty_metal_02_nor_gl_1k.exr",
                "Roughness": "rusty_metal_02_rough_1k.exr",
                "Specular": "rusty_metal_02_spec_1k.png",
            },
        )

    def test_texturecan(self):
        """Texture from: https://www.texturecan.com/details/67/"""

        files = [
            MockFile("metal_0010_ao_1k.jpg"),
            MockFile("metal_0010_color_1k.jpg"),
            MockFile("metal_0010_height_1k.png"),
            MockFile("metal_0010_metallic_1k.jpg"),
            MockFile("metal_0010_normal_directx_1k.png"),
            MockFile("metal_0010_normal_opengl_1k.png"),
            MockFile("metal_0010_roughness_1k.jpg"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Ambient Occlusion": "metal_0010_ao_1k.jpg",
                "Base Color": "metal_0010_color_1k.jpg",
                "Displacement": "metal_0010_height_1k.png",
                "Metallic": "metal_0010_metallic_1k.jpg",
                "Normal": "metal_0010_normal_opengl_1k.png",
                "Roughness": "metal_0010_roughness_1k.jpg",
            },
        )

    def test_single_file_good(self):
        """Regression test for https://projects.blender.org/blender/blender-addons/issues/104573"""

        files = [
            MockFile("banana-color.webp"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {
                "Base Color": "banana-color.webp",
            },
        )

    def test_single_file_bad(self):
        """Regression test for https://projects.blender.org/blender/blender-addons/issues/104573"""

        files = [
            MockFile("README-banana.txt"),
        ]
        sockets = sockets_fixture()
        match_files_to_socket_names(files, sockets)

        assert_sockets(
            self,
            sockets,
            {},
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
