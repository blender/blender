#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Alembic Export Tests

This test suite runs outside of Blender. Tests run Blender to call the exporter,
and then use the Alembic CLI tools to inspect the exported Alembic files.
"""


import argparse
import pathlib
import subprocess
import sys
import unittest

from modules.test_utils import (
    with_tempdir,
    AbstractBlenderRunnerTest,
)


class AbcPropError(Exception):
    """Raised when AbstractAlembicTest.abcprop() finds an error."""


class AbstractAlembicTest(AbstractBlenderRunnerTest):
    @classmethod
    def setUpClass(cls):
        import re

        cls.blender = args.blender
        cls.testdir = pathlib.Path(args.testdir)
        cls.alembic_root = pathlib.Path(args.alembic_root)

        # 'abcls' outputs ANSI colour codes, even when stdout is not a terminal.
        # See https://github.com/alembic/alembic/issues/120
        cls.ansi_remove_re = re.compile(rb'\x1b[^m]*m')

        # 'abcls' array notation, like "name[16]"
        cls.abcls_array = re.compile(r'^(?P<name>[^\[]+)(\[(?P<arraysize>\d+)\])?$')

    def abcls(self, *arguments) -> tuple[int, str]:
        """Uses abcls and return its output.

        :return: tuple (process exit status code, stdout)
        """

        command = (self.alembic_root / 'bin' / 'abcls', *arguments)
        # Convert Path to str; Path works fine on Linux, but not on Windows.
        command_str = [str(arg) for arg in command]
        proc = subprocess.run(command_str, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=30)

        coloured_output = proc.stdout
        output = self.ansi_remove_re.sub(b'', coloured_output).decode('utf8')

        # Because of the ANSI colour codes, we need to remove those first before
        # decoding to text. This means that we cannot use the universal_newlines
        # parameter to subprocess.run(), and have to do the conversion ourselves
        output = output.replace('\r\n', '\n').replace('\r', '\n')

        if proc.returncode:
            str_command = " ".join(str(c) for c in command)
            print(f'command {str_command} failed with status {proc.returncode}')

        return (proc.returncode, output)

    def abcprop(self, filepath: pathlib.Path, proppath: str) -> dict:
        """Uses abcls to obtain compound property values from an Alembic object.

        A dict of subproperties is returned, where the values are Python values.

        The Python bindings for Alembic are old, and only compatible with Python 2.x,
        so that's why we can't use them here, and have to rely on other tooling.
        """
        import collections

        command = ('-vl', '%s%s' % (filepath, proppath))
        returncode, output = self.abcls(*command)
        if returncode:
            raise AbcPropError('Error %d running abcls:\n%s' % (returncode, output))

        # Mapping from value type to callable that can convert a string to Python values.
        converters = {
            'bool_t': int,
            'uint8_t': int,
            'int16_t': int,
            'int32_t': int,
            'uint32_t': int,
            'uint64_t': int,
            'float64_t': float,
            'float32_t': float,
            'string': str,
        }

        result = {}

        # Ideally we'd get abcls to output JSON, see https://github.com/alembic/alembic/issues/121
        lines = collections.deque(output.split('\n'))
        while lines:
            info = lines.popleft()
            if not info:
                continue
            parts = info.split()
            proptype = parts[0]

            if proptype == 'CompoundProperty':
                # To read those, call self.abcprop() on it.
                continue

            try:
                valtype_and_arrsize, name_and_extent = parts[1:]
            except ValueError as ex:
                raise ValueError(f'Error parsing result from abcprop "{info.strip()}": {ex}') from ex

            # Parse name and extent
            m = self.abcls_array.match(name_and_extent)
            if not m:
                self.fail('Unparsable name/extent from abcls: %s' % name_and_extent)
            name, extent = m.group('name'), m.group('arraysize')

            if extent != '1':
                self.fail('Unsupported extent %s for property %s/%s' % (extent, proppath, name))

            # Parse type
            m = self.abcls_array.match(valtype_and_arrsize)
            if not m:
                self.fail('Unparsable value type from abcls: %s' % valtype_and_arrsize)
            valtype, scalarsize = m.group('name'), m.group('arraysize')

            # Convert values
            try:
                conv = converters[valtype]
            except KeyError:
                self.fail('Unsupported type %s for property %s/%s' % (valtype, proppath, name))

            def convert_single_line(linevalue):
                try:
                    if scalarsize is None:
                        return conv(linevalue)
                    else:
                        return [conv(v.strip()) for v in linevalue.split(',')]
                except ValueError as ex:
                    return str(ex)

            if proptype == 'ScalarProperty':
                value = lines.popleft()
                result[name] = convert_single_line(value)
            elif proptype == 'ArrayProperty':
                arrayvalue = []
                # Arrays consist of a variable number of items, and end in a blank line.
                while True:
                    linevalue = lines.popleft()
                    if not linevalue:
                        break
                    arrayvalue.append(convert_single_line(linevalue))
                result[name] = arrayvalue
            else:
                self.fail('Unsupported type %s for property %s/%s' % (proptype, proppath, name))

        return result

    def assertAlmostEqualFloatArray(self, actual, expect, places=6, delta=None):
        """Asserts that the arrays of floats are almost equal."""

        self.assertEqual(len(actual), len(expect),
                         'Actual array has %d items, expected %d' % (len(actual), len(expect)))

        for idx, (act, exp) in enumerate(zip(actual, expect)):
            self.assertAlmostEqual(act, exp, places=places, delta=delta,
                                   msg='%f != %f at index %d' % (act, exp, idx))


class HierarchicalAndFlatExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_hierarchical_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=False)" % abc.as_posix()
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube/Cube_002/Cube_012/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             3.07484, -2.92265, 0.0586434, 1.0]
        )

    @with_tempdir
    def test_flat_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_flat.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=True)" % abc.as_posix()
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube_012/.xform')
        self.assertEqual(1, xform['.inherits'], "Blender transforms always inherit")

        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [0.343134, 0.485243, 0.804238, 0,
             0.0, 0.856222, -0.516608, 0,
             -0.939287, 0.177266, 0.293799, 0,
             1, 3, 4, 1],
        )


class DupliGroupExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_hierarchical_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'dupligroup_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=False)" % abc.as_posix()
        self.run_blender('dupligroup-scene.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Real_Cube/Linked_Suzanne/Cylinder-0/Suzanne-1/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             0.0, 2.0, 0.0, 1.0]
        )

    @with_tempdir
    def test_flat_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'dupligroup_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=True)" % abc.as_posix()
        self.run_blender('dupligroup-scene.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Suzanne-1/.xform')
        self.assertEqual(1, xform['.inherits'])

        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.5, 0.0, 0.0, 0.0,
             0.0, 1.5, 0.0, 0.0,
             0.0, 0.0, 1.5, 0.0,
             2.0, 3.0, 0.0, 1.0]
        )

    @with_tempdir
    def test_multiple_duplicated_hierarchies(self, tempdir: pathlib.Path):
        abc = tempdir / "multiple-duplicated-hierarchies.abc"
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1)" % abc.as_posix()
        self.run_blender('multiple-duplicated-hierarchies.blend', script)

        # This is the expected hierarchy:
        # ABC
        #  `--Triangle
        #      |--Triangle
        #      |--Empty-1
        #      |    `--Pole-1-0
        #      |        |--Pole
        #      |        `--Block-1-1
        #      |            `--Block
        #      |--Empty
        #      |    `--Pole-0
        #      |        |--Pole
        #      |        `--Block-1
        #      |            `--Block
        #      |--Empty-2
        #      |    `--Pole-2-0
        #      |        |--Pole
        #      |        `--Block-2-1
        #      |            `--Block
        #      `--Empty-0
        #          `--Pole-0-0
        #              |--Pole
        #              `--Block-0-1
        #                  `--Block

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Triangle/Empty-1/Pole-1-0/Block-1-1/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             0.0, 2.0, 0.0, 1.0]
        )

        # If the property can be gotten, the hierarchy is okay. No need to actually check each xform.
        self.abcprop(abc, '/Triangle/.xform')
        self.abcprop(abc, '/Triangle/Empty-1/.xform')
        self.abcprop(abc, '/Triangle/Empty-1/Pole-1-0/.xform')
        self.abcprop(abc, '/Triangle/Empty-1/Pole-1-0/Block-1-1/.xform')
        self.abcprop(abc, '/Triangle/Empty/.xform')
        self.abcprop(abc, '/Triangle/Empty/Pole-0/.xform')
        self.abcprop(abc, '/Triangle/Empty/Pole-0/Block-1/.xform')
        self.abcprop(abc, '/Triangle/Empty-2/.xform')
        self.abcprop(abc, '/Triangle/Empty-2/Pole-2-0/.xform')
        self.abcprop(abc, '/Triangle/Empty-2/Pole-2-0/Block-2-1/.xform')
        self.abcprop(abc, '/Triangle/Empty-0/.xform')
        self.abcprop(abc, '/Triangle/Empty-0/Pole-0-0/.xform')
        self.abcprop(abc, '/Triangle/Empty-0/Pole-0-0/Block-0-1/.xform')


class CurveExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_export_single_curve(self, tempdir: pathlib.Path):
        abc = tempdir / 'single-curve.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=False)" % abc.as_posix()
        self.run_blender('single-curve.blend', script)

        # Now check the resulting Alembic file.
        abcprop = self.abcprop(abc, '/NurbsCurve/CurveData/.geom')
        self.assertEqual(abcprop['.orders'], [4])

        abcprop = self.abcprop(abc, '/NurbsCurve/CurveData/.geom/.userProperties')
        self.assertEqual(abcprop['blender:resolution'], 10)


class HairParticlesExportTest(AbstractAlembicTest):
    """Tests exporting with/without hair/particles.

    Just a basic test to ensure that the enabling/disabling works, and that export
    works at all. NOT testing the quality/contents of the exported file.
    """

    def _do_test(self, tempdir: pathlib.Path, export_hair: bool, export_particles: bool) -> pathlib.Path:
        abc = tempdir / 'hair-particles.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=False, " \
                 "export_hair=%r, export_particles=%r, as_background_job=False)" \
                 % (abc.as_posix(), export_hair, export_particles)
        self.run_blender('hair-particles.blend', script)
        return abc

    @with_tempdir
    def test_with_both(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, True, True)

        abcprop = self.abcprop(abc, '/Suzanne/Hair_system/.geom')
        self.assertIn('nVertices', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/Non-hair_particle_system/.geom')
        self.assertIn('.velocities', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/MonkeyMesh/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_hair_only(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, True, False)

        abcprop = self.abcprop(abc, '/Suzanne/Hair_system/.geom')
        self.assertIn('nVertices', abcprop)

        self.assertRaises(AbcPropError, self.abcprop, abc,
                          '/Suzanne/Non-hair_particle_system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/MonkeyMesh/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_particles_only(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, False, True)

        self.assertRaises(AbcPropError, self.abcprop, abc, '/Suzanne/Hair_system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/Non-hair_particle_system/.geom')
        self.assertIn('.velocities', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/MonkeyMesh/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_neither(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, False, False)

        self.assertRaises(AbcPropError, self.abcprop, abc, '/Suzanne/Hair_system/.geom')
        self.assertRaises(AbcPropError, self.abcprop, abc,
                          '/Suzanne/Non-hair_particle_system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/MonkeyMesh/.geom')
        self.assertIn('.faceIndices', abcprop)


class UVMapExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_uvmap_export(self, tempdir: pathlib.Path):
        """Minimal test for exporting multiple UV maps on an animated mesh.

        This covers the issue reported in #77021.
        """
        basename = 'T77021-multiple-uvmaps-animated-mesh'
        abc = tempdir / f'{basename}.abc'
        script = f"import bpy; bpy.ops.wm.alembic_export(filepath='{abc.as_posix()}', start=1, end=1, " \
            f"flatten=False)"
        self.run_blender(f'{basename}.blend', script)

        self.maxDiff = 1000

        # The main UV map should be written to .geom
        abcprop = self.abcprop(abc, '/Cube/Cube/.geom/uv')
        self.assertEqual(abcprop['.vals'], [
            [0.625, 0.75],
            [0.875, 0.75],
            [0.875, 0.5],
            [0.625, 0.5],
            [0.375, 1.0],
            [0.625, 1.0],
            [0.375, 0.75],
            [0.375, 0.25],
            [0.625, 0.25],
            [0.625, 0.0],
            [0.375, 0.0],
            [0.125, 0.75],
            [0.375, 0.5],
            [0.125, 0.5],
        ])

        # The second UV map should be written to .arbGeomParams
        abcprop = self.abcprop(abc, '/Cube/Cube/.geom/.arbGeomParams/Secondary')
        self.assertEqual(abcprop['.vals'], [
            [0.75, 0.375],
            [0.75, 0.125],
            [0.5, 0.125],
            [0.5, 0.375],
            [1.0, 0.625],
            [1.0, 0.375],
            [0.75, 0.625],
            [0.25, 0.625],
            [0.25, 0.375],
            [0.0, 0.375],
            [0.0, 0.625],
            [0.75, 0.875],
            [0.5, 0.625],
            [0.5, 0.875],
        ])


class LongNamesExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_export_long_names(self, tempdir: pathlib.Path):
        abc = tempdir / 'long-names.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "flatten=False)" % abc.as_posix()
        self.run_blender('long-names.blend', script)

        name_parts = [
            'foG9aeLahgoh5goacee1dah6Hethaghohjaich5pasizairuWigee1ahPeekiGh',
            'yoNgoisheedah2ua0eigh2AeCaiTee5bo0uphoo7Aixephah9racahvaingeeH4',
            'zuthohnoi1thooS3eezoo8seuph2Boo5aefacaethuvee1aequoonoox1sookie',
            'wugh4ciTh3dipiepeequait5uug7thiseek5ca7Eijei5ietaizokohhaecieto',
            'up9aeheenein9oteiX6fohP3thiez6Ahvah0oohah1ep2Eesho4Beboechaipoh',
            'coh4aehiacheTh0ue0eegho9oku1lohl4loht9ohPoongoow7dasiego6yimuis',
            'lohtho8eigahfeipohviepajaix4it2peeQu6Iefee1nevihaes4cee2soh4noy',
            'kaht9ahv0ieXaiyih7ohxe8bah7eeyicahjoa2ohbu7Choxua7oongah6sei4bu',
            'deif0iPaechohkee5nahx6oi2uJeeN7ze3seunohJibe4shai0mah5Iesh3Quai',
            'ChohDahshooNee0NeNohthah0eiDeese3Vu6ohShil1Iey9ja0uebi2quiShae6',
            'Dee1kai7eiph2ahh2nufah3zai3eexeengohQue1caj0eeW0xeghi3eshuadoot',
            'aeshiup3aengajoog0AhCoo5tiu3ieghaeGhie4Tu1ohh1thee8aepheingah1E',
            'ooRa6ahciolohshaifoopeo9ZeiGhae2aech4raisheiWah9AaNga0uas9ahquo',
            'thaepheip2aip6shief4EaXopei8ohPo0ighuiXah2ashowai9nohp4uach6Mei',
            'ohph4yaev3quieji3phophiem3OoNuisheepahng4waithae3Naichai7aw3noo',
            'aibeawaneBahmieyuph8ieng8iopheereeD2uu9Uyee5bei2phahXeir8eeJ8oo',
            'ooshahphei2hoh3uth5chaen7ohsai6uutiesucheichai8ungah9Gie1Aiphie',
            'eiwohchoo7ere2iebohn4Aapheichaelooriiyaoxaik7ooqua7aezahx0aeJei',
            'Vah0ohgohphiefohTheshieghichaichahch5moshoo0zai5eeva7eisi4yae8T',
            'EibeeN0fee0Gohnguz8iec6yeigh7shuNg4eingu3siph9joucahpeidoom4ree',
            'iejiu3shohheeZahHusheimeefaihoh5eecachu5eeZie9ceisugu9taidohT3U',
            'eex6dilakaix5Eetai7xiCh5Jaa8aiD4Ag3tuij1aijohv5fo0heevah8hohs3m',
            'ohqueeNgahraew6uraemohtoo5qua3oojiex6ohqu6Aideibaithaiphuriquie',
            'cei0eiN4Shiey7Aeluy3unohboo5choiphahc2mahbei5paephaiKeso1thoog1',
            'ieghif4ohKequ7ong0jah5ooBah0eiGh1caechahnahThae9Shoo0phopashoo4',
            'roh9er3thohwi5am8iequeequuSh3aic0voocai3ihi5nie2abahphupiegh7vu',
            'uv3Quei7wujoo5beingei2aish5op4VaiX0aebai7iwoaPee5pei8ko9IepaPig',
            'co7aegh5beitheesi9lu7jeeQu3johgeiphee9cheichi8aithuDehu2gaeNein',
            'thai3Tiewoo4nuir1ohy4aithiuZ7shae1luuwei5phibohriepe2paeci1Ach8',
            'phoi3ribah7ufuvoh8eigh1oB6deeBaiPohphaghiPieshahfah5EiCi3toogoo',
            'aiM8geil7ooreinee4Cheiwea4yeec8eeshi7Sei4Shoo3wu6ohkaNgooQu1mai',
            'agoo3faciewah9ZeesiXeereek7am0eigaeShie3Tisu8haReeNgoo0ci2Hae5u',
            'Aesatheewiedohshaephaenohbooshee8eu7EiJ8isal1laech2eiHo0noaV3ta',
            'liunguep3ooChoo4eir8ahSie8eenee0oo1TooXu8Cais8Aimo4eir6Phoo3xei',
            'toe9heepeobein3teequachemei0Cejoomef9ujie3ohwae9AiNgiephi3ep0de',
            'ua6xooY9uzaeB3of6sheiyaedohoiS5Eev0Aequ9ahm1zoa5Aegh3ooz9ChahDa',
            'eevasah6Bu9wi7EiwiequumahkaeCheegh6lui8xoh4eeY4ieneavah8phaibun',
            'AhNgei2sioZeeng6phaecheemeehiShie5eFeiTh6ooV8iiphabud0die4siep4',
            'kushe6Xieg6ahQuoo9aex3aipheefiec1esa7OhBuG0ueziep9phai5eegh1vie',
            'Jie5yu8aafuQuoh9shaep3moboh3Pooy7och8oC6obeik6jaew2aiLooweib3ch',
            'ohohjajaivaiRail3odaimei6aekohVaicheip2wu7phieg5Gohsaing2ahxaiy',
            'hahzaht6yaiYu9re9jah9loisiit4ahtoh2quoh9xohishioz4oo4phofu3ogha',
            'pu4oorea0uh2tahB8aiZoonge1aophaes6ogaiK9ailaigeej4zoVou8ielotee',
            'cae2thei3Luphuqu0zeeG8leeZuchahxaicai4ui4Eedohte9uW6gae8Geeh0ea',
            'air7tuy7ohw5sho2Tahpai8aep4so5ria7eaShus5weaqu0Naquei2xaeyoo2ae',
            'vohge4aeCh7ahwoo7Jaex6sohl0Koong4Iejisei8Coir0iemeiz9uru9Iebaep',
            'aepeidie8aiw6waish9gie4Woolae2thuj5phae4phexux7gishaeph4Deu7ooS',
            'vahc5ia0xohHooViT0uyuxookiaquu2ogueth0ahquoudeefohshai8aeThahba',
            'mun3oagah2eequaenohfoo8DaigeghoozaV2eiveeQuee7kah0quaa6tiesheet',
            'ooSet4IdieC4ugow3za0die4ohGoh1oopoh6luaPhaeng4Eechea1hae0eimie5',
            'iedeimadaefu2NeiPaey2jooloov5iehiegeakoo4ueso7aeK9ahqu2Thahkaes',
            'nahquah9Quuu2uuf0aJah7eishi2siegh8ue5eiJa2EeVu8ebohkepoh4dahNgo',
            'io1bie7chioPiej5ae2oohe2fee6ooP2thaeJohjohb9Se8tang3eipaifeimai',
            'oungoqu6dieneejiechez1xeD2Zi9iox2Ahchaiy9ithah3ohVoolu2euQuuawo',
            'thaew0veigei4neishohd8mecaixuqu7eeshiex1chaigohmoThoghoitoTa0Eo',
            'ahroob2phohvaiz0Ohteik2ohtakie6Iu1vitho8IyiyeeleeShae9defaiw9ki',
            'DohHoothohzeaxolai3Toh5eJie7ahlah9reF0ohn1chaipoogain2aibahw4no',
            'aif8lo5she4aich5cho2rie8ieJaujeem2Joongeedae4vie3tah1Leequaix1O',
            'Aang0Shaih6chahthie1ahZ7aewei9thiethee7iuThah3yoongi8ahngiobaa5',
            'iephoBuayoothah0Ru6aichai4aiw8deg1umongauvaixai3ohy6oowohlee8ei',
            'ohn5shigoameer0aejohgoh8oChohlaecho9jie6shu0ahg9Bohngau6paevei9',
            'edahghaishak0paigh1eecuich3aad7yeB0ieD6akeeliem2beifufaekee6eat',
            'hiechahgheloh2zo7Ieghaiph0phahhu8aeyuiKie1xeipheech9zai4aeme0ee',
            'Cube'
        ]
        name = '/' + '/'.join(name_parts)

        # Now check the resulting Alembic file.
        abcprop = self.abcprop(abc, '%s/.xform' % name)
        self.assertEqual(abcprop['.vals'], [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 3.0, 0.0, 1.0,
        ])

        abcprop = self.abcprop(abc, '%s/Cube/.geom' % name)
        self.assertIn('.faceCounts', abcprop)


class CustomPropertiesExportTest(AbstractAlembicTest):
    """Test export of custom properties."""

    def _run_export(self, tempdir: pathlib.Path) -> pathlib.Path:
        abc = tempdir / 'custom-properties.abc'
        script = (
            "import bpy; bpy.context.scene.frame_set(1); "
            "bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1)" % abc.as_posix()
        )
        self.run_blender('custom-properties.blend', script)
        return abc

    @with_tempdir
    def test_xform_props(self, tempdir: pathlib.Path) -> None:
        abc = self._run_export(tempdir)
        abcprop = self.abcprop(abc, '/Cube/.xform/.userProperties')

        # Simple, single values.
        self.assertEqual(abcprop['static_int'], [327])
        self.assertEqual(abcprop['static_float'], [47.01])
        self.assertEqual(abcprop['static_string'], ['Agents'])
        self.assertEqual(abcprop['keyed_float'], [-1])
        self.assertEqual(abcprop['keyed_int'], [-47])

        # Arrays.
        self.assertEqual(abcprop['keyed_array_float'], [-1.000, 0.000, 1.000])
        self.assertEqual(abcprop['keyed_array_int'], [42, 47, 327])

        # Multi-dimensional arrays.
        self.assertEqual(abcprop['array_of_strings'], ['ผัดไทย', 'Pad Thai'])
        self.assertEqual(
            abcprop['matrix_tuple'],
            [1.0, 0.0, 0.0, 3.33333, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0])
        self.assertEqual(
            abcprop['static_matrix'],
            [1.0, 0.0, 0.0, 3.33333, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0])
        self.assertEqual(
            abcprop['nonuniform_array'],
            [10, 20, 30, 1, 2, 47])

    @with_tempdir
    def test_mesh_props(self, tempdir: pathlib.Path) -> None:
        abc = self._run_export(tempdir)
        abcprop = self.abcprop(abc, '/Cube/Cube/.geom/.userProperties')
        self.assertEqual(abcprop['mesh_tags'], ['cube', 'box', 'low-poly-sphere'])

    @with_tempdir
    def test_camera_props(self, tempdir: pathlib.Path) -> None:
        abc = self._run_export(tempdir)
        abcprop = self.abcprop(abc, '/Camera/Hasselblad/.geom/.userProperties')
        self.assertEqual(abcprop['type'], ['500c/m'])

    @with_tempdir
    def test_disabled_export_option(self, tempdir: pathlib.Path) -> None:
        abc = tempdir / 'custom-properties.abc'
        script = (
            "import bpy; bpy.context.scene.frame_set(1); "
            "bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, export_custom_properties=False)" % abc.as_posix()
        )
        self.run_blender('custom-properties.blend', script)

        abcprop = self.abcprop(abc, '/Camera/Hasselblad/.geom/.userProperties')
        self.assertIn('eyeSeparation', abcprop, 'Regular non-standard properties should still be written')
        self.assertNotIn('type', abcprop, 'Custom properties should not be written')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--blender', required=True)
    parser.add_argument('--testdir', required=True)
    parser.add_argument('--alembic-root', required=True)
    args, remaining = parser.parse_known_args()

    unittest.main(argv=sys.argv[0:1] + remaining)
