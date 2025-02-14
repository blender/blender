# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
Compare textual dump of imported data against reference versions and generate
a HTML report showing the differences, for regression testing.
"""

import bpy
import bpy_extras.node_shader_utils
import difflib
import json
import os
import pathlib

from . import global_report
from io import StringIO
from typing import Callable


def fmtf(f: float) -> str:
    # ensure tiny numbers are 0.0,
    # and not "-0.0" for example
    if abs(f) < 0.0005:
        return "0.000"
    return f"{f:.3f}"


class Report:
    __slots__ = (
        'title',
        'output_dir',
        'global_dir',
        'input_dir',
        'reference_dir',
        'tested_count',
        'failed_list',
        'passed_list',
        'updated_list',
        'failed_html',
        'passed_html',
        'update_templates',
    )

    def __init__(self, title: str, output_dir: pathlib.Path, input_dir: pathlib.Path, reference_dir: pathlib.Path):
        self.title = title
        self.output_dir = output_dir
        self.global_dir = os.path.dirname(output_dir)
        self.input_dir = input_dir
        self.reference_dir = reference_dir

        self.tested_count = 0
        self.failed_list = []
        self.passed_list = []
        self.updated_list = []
        self.failed_html = ""
        self.passed_html = ""

        os.makedirs(output_dir, exist_ok=True)
        self.update_templates = os.getenv('BLENDER_TEST_UPDATE', "0").strip() == "1"
        if self.update_templates:
            os.makedirs(self.reference_dir, exist_ok=True)

        # write out dummy html in case test crashes
        if not self.update_templates:
            filename = "report.html"
            filepath = os.path.join(self.output_dir, filename)
            pathlib.Path(filepath).write_text(
                '<html><body>Report not generated yet. Crashed during tests?</body></html>')

    @staticmethod
    def _navigation_item(title, href, active):
        if active:
            return """<li class="breadcrumb-item active" aria-current="page">%s</li>""" % title
        else:
            return """<li class="breadcrumb-item"><a href="%s">%s</a></li>""" % (href, title)

    def _navigation_html(self):
        html = """<nav aria-label="breadcrumb"><ol class="breadcrumb">"""
        base_path = os.path.relpath(self.global_dir, self.output_dir)
        global_report_path = os.path.join(base_path, "report.html")
        html += self._navigation_item("Test Reports", global_report_path, False)
        html += self._navigation_item(self.title, "report.html", True)
        html += """</ol></nav>"""
        return html

    def finish(self, test_suite_name: str) -> None:
        """
        Finishes the report: short summary to the console,
        generates full report as HTML.
        """
        print(f"\n============")
        if self.update_templates:
            print(f"{self.tested_count} input files tested, {len(self.updated_list)} references updated to new results")
            for test in self.updated_list:
                print(f"UPDATED {test}")
        else:
            self._write_html(test_suite_name)
            print(f"{self.tested_count} input files tested, {len(self.passed_list)} passed")
            if len(self.failed_list):
                print(f"FAILED {len(self.failed_list)} tests:")
                for test in self.failed_list:
                    print(f"FAILED {test}")

    def _write_html(self, test_suite_name: str):

        tests_html = self.failed_html + self.passed_html
        menu = self._navigation_html()

        failed = len(self.failed_html) > 0
        if failed:
            message = """<div class="alert alert-danger" role="alert">"""
            message += """<p>Run this command to regenerate reference (ground truth) output:</p>"""
            message += """<p><tt>BLENDER_TEST_UPDATE=1 ctest -R %s</tt></p>""" % test_suite_name
            message += """<p>The reference output of new and failing tests will be updated. """ \
                       """Be sure to commit the new reference """ \
                       """files to the tests/data git submodule afterwards.</p>"""
            message += """</div>"""
            message += f"Tested files: {self.tested_count}, <b>failed: {len(self.failed_list)}</b>"
        else:
            message = f"Tested files: {self.tested_count}"

        title = self.title + " Test Report"
        columns_html = "<tr><th>Name</th><th>New</th><th>Reference</th><th>Diff</th>"

        html = f"""
<html>
<head>
    <title>{title}</title>
    <style>
        div.page_container {{ text-align: center; }}
        div.page_container div {{ text-align: left; }}
        div.page_content {{  display: inline-block; }}
        .text_cell {{
          max-width: 15em;
          max-height: 8em;
          overflow: auto;
          font-family: monospace;
          white-space: pre;
          font-size: 10pt;
          border: 1px solid gray;
        }}
        .text_cell_larger {{ max-height: 14em; }}
        .text_cell_wider {{ max-width: 40em; }}
        .added {{ background-color: #d4edda; }}
        .removed {{ background-color: #f8d7da; }}
        .place {{ color: #808080; font-style: italic; }}
        p {{ margin-bottom: 0.5rem; }}
    </style>
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css" integrity="sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T" crossorigin="anonymous">
</head>
<body>
    <div class="page_container"><div class="page_content">
        <br/>
        <h1>{title}</h1>
        {menu}
        {message}
        <table class="table table-striped">
            <thead class="thead-dark">{columns_html}</thead>
            {tests_html}
        </table>
        <br/>
    </div></div>
</body>
</html>
            """

        filename = "report.html"
        filepath = os.path.join(self.output_dir, filename)
        pathlib.Path(filepath).write_text(html)

        print(f"Report saved to: {pathlib.Path(filepath).as_uri()}")

        # Update global report
        global_failed = failed
        global_report.add(self.global_dir, "IO", self.title, filepath, global_failed)

    def _relative_url(self, filepath):
        relpath = os.path.relpath(filepath, self.output_dir)
        return pathlib.Path(relpath).as_posix()

    @staticmethod
    def _colored_diff(a: str, b: str):
        a_lines = a.splitlines()
        b_lines = b.splitlines()
        diff = difflib.unified_diff(a_lines, b_lines, lineterm='')
        html = []
        for line in diff:
            if line.startswith('+++') or line.startswith('---'):
                pass
            elif line.startswith('@@'):
                html.append(f'<span class="place">{line}</span>')
            elif line.startswith('-'):
                html.append(f'<span class="removed">{line}</span>')
            elif line.startswith('+'):
                html.append(f'<span class="added">{line}</span>')
            else:
                html.append(line)
        return '\n'.join(html)

    def _add_test_result(self, testname: str, got_desc: str, ref_desc: str):
        error = got_desc != ref_desc
        status = "FAILED" if error else ""
        table_style = """ class="table-danger" """ if error else ""
        cell_class = "text_cell text_cell_larger" if error else "text_cell"
        diff_text = "&nbsp;"
        if error:
            diff_text = Report._colored_diff(ref_desc, got_desc)

        test_html = f"""
            <tr>
                <td{table_style}><b>{testname}</b><br/>{status}</td>
                <td><div class="{cell_class}">{got_desc}</div></td>
                <td><div class="{cell_class}">{ref_desc}</div></td>
                <td><div class="{cell_class} text_cell_wider">{diff_text}</div></td>
            </tr>"""

        if error:
            self.failed_html += test_html
        else:
            self.passed_html += test_html

    @staticmethod
    def _val_to_str(val) -> str:
        if isinstance(val, bpy.types.BoolAttributeValue):
            return f"{1 if val.value else 0}"
        if isinstance(val, bpy.types.IntAttributeValue):
            return f"{val.value}"
        if isinstance(val, bpy.types.FloatAttributeValue):
            return f"{val.value:.3f}"
        if isinstance(val, bpy.types.FloatVectorAttributeValue):
            return f"({val.vector[0]:.3f}, {val.vector[1]:.3f}, {val.vector[2]:.3f})"
        if isinstance(val, bpy.types.Float2AttributeValue):
            return f"({val.vector[0]:.3f}, {val.vector[1]:.3f})"
        if isinstance(val, bpy.types.FloatColorAttributeValue) or isinstance(val, bpy.types.ByteColorAttributeValue):
            return f"({val.color[0]:.3f}, {val.color[1]:.3f}, {val.color[2]:.3f}, {val.color[3]:.3f})"
        if isinstance(val, bpy.types.Int2AttributeValue) or isinstance(val, bpy.types.Short2AttributeValue):
            return f"({val.value[0]}, {val.value[1]})"
        if isinstance(val, bpy.types.ID):
            return f"'{val.name}'"
        if isinstance(val, bpy.types.MeshLoop):
            return f"{val.vertex_index}"
        if isinstance(val, bpy.types.MeshEdge):
            return f"{min(val.vertices[0], val.vertices[1])}/{max(val.vertices[0], val.vertices[1])}"
        if isinstance(val, bpy.types.MaterialSlot):
            return f"('{val.name}', {val.link})"
        if isinstance(val, bpy.types.VertexGroup):
            return f"'{val.name}'"
        if isinstance(val, bpy.types.Keyframe):
            res = f"({val.co[0]:.1f}, {val.co[1]:.1f})"
            res += f" lh:({val.handle_left[0]:.1f}, {val.handle_left[1]:.1f} {val.handle_left_type})"
            res += f" rh:({val.handle_right[0]:.1f}, {val.handle_right[1]:.1f} {val.handle_right_type})"
            if val.interpolation != 'LINEAR':
                res += f" int:{val.interpolation}"
            if val.easing != 'AUTO':
                res += f" ease:{val.easing}"
            return res
        if isinstance(val, bpy.types.SplinePoint):
            return f"({val.co[0]:.3f}, {val.co[1]:.3f}, {val.co[2]:.3f}) w:{val.weight:.3f}"
        return str(val)

    # single-line dump of head/tail
    @staticmethod
    def _write_collection_single(col, desc: StringIO) -> None:
        desc.write(f"    - ")
        side_to_print = 5
        if len(col) <= side_to_print * 2:
            for val in col:
                desc.write(f"{Report._val_to_str(val)} ")
        else:
            for val in col[:side_to_print]:
                desc.write(f"{Report._val_to_str(val)} ")
            desc.write(f"... ")
            for val in col[-side_to_print:]:
                desc.write(f"{Report._val_to_str(val)} ")
        desc.write(f"\n")

    # multi-line dump of head/tail
    @staticmethod
    def _write_collection_multi(col, desc: StringIO) -> None:
        side_to_print = 3
        if len(col) <= side_to_print * 2:
            for val in col:
                desc.write(f"    - {Report._val_to_str(val)}\n")
        else:
            for val in col[:side_to_print]:
                desc.write(f"    - {Report._val_to_str(val)}\n")
            desc.write(f"      ...\n")
            for val in col[-side_to_print:]:
                desc.write(f"    - {Report._val_to_str(val)}\n")

    @staticmethod
    def _write_attr(attr: bpy.types.Attribute, desc: StringIO) -> None:
        if len(attr.data) == 0:
            return
        desc.write(f"  - attr '{attr.name}' {attr.data_type} {attr.domain}\n")
        if isinstance(
                attr,
                bpy.types.BoolAttribute) or isinstance(
                attr,
                bpy.types.IntAttribute) or isinstance(
                attr,
                bpy.types.FloatAttribute):
            Report._write_collection_single(attr.data, desc)
        else:
            Report._write_collection_multi(attr.data, desc)

    @staticmethod
    def _write_custom_props(bid, desc: StringIO) -> None:
        items = bid.items()
        if not items:
            return

        rna_properties = {prop.identifier for prop in bid.bl_rna.properties if prop.is_runtime}

        had_any = False
        for k, v in items:
            if k in rna_properties:
                continue

            if not had_any:
                desc.write(f"  - props:")
                had_any = True

            if isinstance(v, str):
                if k != "cycles":
                    desc.write(f" str:{k}='{v}'")
                else:
                    desc.write(f" str:{k}=<cyclesval>")
            elif isinstance(v, int):
                desc.write(f" int:{k}={v}")
            elif isinstance(v, float):
                desc.write(f" fl:{k}={v:.3f}")
            elif len(v) == 3:
                desc.write(f" f3:{k}=({v[0]:.3f}, {v[1]:.3f}, {v[2]:.3f})")
            else:
                desc.write(f" o:{k}={str(v)}")
        if had_any:
            desc.write(f"\n")

    def _node_shader_image_desc(self, tex: bpy_extras.node_shader_utils.ShaderImageTextureWrapper) -> str:
        if not tex or not tex.image:
            return ""
        # Get relative path of the image
        tex_path = pathlib.Path(tex.image.filepath)
        if tex.image.filepath.startswith('//'):  # already relative
            rel_path = tex.image.filepath.replace('\\', '/')
        elif tex_path.root == '':
            rel_path = tex_path.as_posix()  # use just the filename
        else:
            try:
                # note: we can't use Path.relative_to since walk_up parameter is only since Python 3.12
                rel_path = pathlib.Path(os.path.relpath(tex_path, self.input_dir)).as_posix()
            except ValueError:
                rel_path = f"<outside of test folder>"
        if rel_path.startswith('../../..'):  # if relative path is too high up, just emit filename
            rel_path = tex_path.name

        desc = f" tex:'{tex.image.name}' ({rel_path}) a:{tex.use_alpha}"
        if str(tex.colorspace_is_data) == "True":  # unset value is "Ellipsis"
            desc += f" data"
        if tex.texcoords != 'UV':
            desc += f" uv:{tex.texcoords}"
        if tex.extension != 'REPEAT':
            desc += f" ext:{tex.extension}"
        if tuple(tex.translation) != (0.0, 0.0, 0.0):
            desc += f" tr:({tex.translation[0]:.3f}, {tex.translation[1]:.3f}, {tex.translation[2]:.3f})"
        if tuple(tex.rotation) != (0.0, 0.0, 0.0):
            desc += f" rot:({tex.rotation[0]:.3f}, {tex.rotation[1]:.3f}, {tex.rotation[2]:.3f})"
        if tuple(tex.scale) != (1.0, 1.0, 1.0):
            desc += f" scl:({tex.scale[0]:.3f}, {tex.scale[1]:.3f}, {tex.scale[2]:.3f})"
        return desc

    @staticmethod
    def _write_animdata_desc(adt: bpy.types.AnimData, desc: StringIO) -> None:
        if adt:
            if adt.action:
                desc.write(f"  - anim act:{adt.action.name}")
                if adt.action_slot:
                    desc.write(f" slot:{adt.action_slot.identifier}")
                desc.write(f" blend:{adt.action_blend_type} drivers:{len(adt.drivers)}\n")

    def generate_main_data_desc(self) -> str:
        """Generates textual description of the current state of the
        Blender main data."""

        desc = StringIO()

        # meshes
        if len(bpy.data.meshes):
            desc.write(f"==== Meshes: {len(bpy.data.meshes)}\n")
            for mesh in bpy.data.meshes:
                # mesh overview
                desc.write(
                    f"- Mesh '{mesh.name}' vtx:{len(mesh.vertices)} face:{len(mesh.polygons)} loop:{len(mesh.loops)} edge:{len(mesh.edges)}\n")
                if len(mesh.loops) > 0:
                    Report._write_collection_single(mesh.loops, desc)
                if len(mesh.edges) > 0:
                    Report._write_collection_single(mesh.edges, desc)
                # attributes
                for attr in mesh.attributes:
                    if not attr.is_internal:
                        Report._write_attr(attr, desc)
                # skinning / vertex groups
                has_skinning = any(v.groups for v in mesh.vertices)
                if has_skinning:
                    desc.write(f"  - vertex groups:\n")
                    for vtx in mesh.vertices[:5]:
                        desc.write(f"    -")
                        # emit vertex group weights in decreasing weight order
                        sorted_groups = sorted(vtx.groups, key=lambda g: g.weight, reverse=True)
                        for grp in sorted_groups:
                            desc.write(f" {grp.group}={grp.weight:.3f}")
                        desc.write(f"\n")
                # materials
                if mesh.materials:
                    desc.write(f"  - {len(mesh.materials)} materials\n")
                    Report._write_collection_single(mesh.materials, desc)
                # blend shapes
                key = mesh.shape_keys
                if key:
                    for kb in key.key_blocks:
                        desc.write(f"  - shape key '{kb.name}' w:{kb.value:.3f} vgrp:'{kb.vertex_group}'")
                        # print first several deltas that are not zero from the key
                        count = 0
                        idx = 0
                        for pt in kb.points:
                            if pt.co.length_squared > 0:
                                desc.write(f" {idx}:({pt.co[0]:.3f}, {pt.co[1]:.3f}, {pt.co[2]:.3f})")
                                count += 1
                                if count >= 3:
                                    break
                            idx += 1
                        desc.write(f"\n")
                Report._write_animdata_desc(mesh.animation_data, desc)
                Report._write_custom_props(mesh, desc)
                desc.write(f"\n")

        # curves
        if len(bpy.data.curves):
            desc.write(f"==== Curves: {len(bpy.data.curves)}\n")
            for curve in bpy.data.curves:
                # overview
                desc.write(
                    f"- Curve '{curve.name}' dim:{curve.dimensions} resu:{curve.resolution_u} resv:{curve.resolution_v} splines:{len(curve.splines)}\n")
                for spline in curve.splines[:5]:
                    desc.write(
                        f"  - spline type:{spline.type} pts:{spline.point_count_u}x{spline.point_count_v} order:{spline.order_u}x{spline.order_v} cyclic:{spline.use_cyclic_u},{spline.use_cyclic_v} endp:{spline.use_endpoint_u},{spline.use_endpoint_v}\n")
                    Report._write_collection_multi(spline.points, desc)
                # materials
                if curve.materials:
                    desc.write(f"  - {len(curve.materials)} materials\n")
                    Report._write_collection_single(curve.materials, desc)
                Report._write_animdata_desc(curve.animation_data, desc)
                Report._write_custom_props(curve, desc)
                desc.write(f"\n")

        # objects
        if len(bpy.data.objects):
            desc.write(f"==== Objects: {len(bpy.data.objects)}\n")
            for obj in bpy.data.objects:
                desc.write(f"- Obj '{obj.name}' {obj.type}")
                if obj.data:
                    desc.write(f" data:'{obj.data.name}'")
                if obj.parent:
                    desc.write(f" par:'{obj.parent.name}'")
                desc.write(f"\n")
                desc.write(f"  - pos {fmtf(obj.location[0])}, {fmtf(obj.location[1])}, {fmtf(obj.location[2])}\n")
                desc.write(
                    f"  - rot {fmtf(obj.rotation_euler[0])}, {fmtf(obj.rotation_euler[1])}, {fmtf(obj.rotation_euler[2])} ({obj.rotation_mode})\n")
                desc.write(f"  - scl {obj.scale[0]:.3f}, {obj.scale[1]:.3f}, {obj.scale[2]:.3f}\n")
                if obj.vertex_groups:
                    desc.write(f"  - {len(obj.vertex_groups)} vertex groups\n")
                    Report._write_collection_single(obj.vertex_groups, desc)
                if obj.material_slots:
                    has_object_link = any(slot.link == 'OBJECT' for slot in obj.material_slots if slot.link)
                    if has_object_link:
                        desc.write(f"  - {len(obj.material_slots)} object materials\n")
                        Report._write_collection_single(obj.material_slots, desc)
                if obj.modifiers:
                    desc.write(f"  - {len(obj.modifiers)} modifiers\n")
                    for mod in obj.modifiers:
                        desc.write(f"    - {mod.type} '{mod.name}'")
                        if isinstance(mod, bpy.types.SubsurfModifier):
                            desc.write(
                                f" levels:{mod.levels}/{mod.render_levels} type:{mod.subdivision_type} crease:{mod.use_creases}")
                        desc.write(f"\n")
                Report._write_animdata_desc(obj.animation_data, desc)
                Report._write_custom_props(obj, desc)
            desc.write(f"\n")

        # cameras
        if len(bpy.data.cameras):
            desc.write(f"==== Cameras: {len(bpy.data.cameras)}\n")
            for cam in bpy.data.cameras:
                desc.write(
                    f"- Cam '{cam.name}' {cam.type} lens:{cam.lens:.1f} {cam.lens_unit} near:{cam.clip_start:.3f} far:{cam.clip_end:.1f} orthosize:{cam.ortho_scale:.1f}\n")
                desc.write(f"  - fov {cam.angle:.3f} (h {cam.angle_x:.3f} v {cam.angle_y:.3f})\n")
                desc.write(
                    f"  - sensor {cam.sensor_width:.1f}x{cam.sensor_height:.1f} shift {cam.shift_x:.3f},{cam.shift_y:.3f}\n")
                if cam.dof.use_dof:
                    desc.write(
                        f"  - dof dist:{cam.dof.focus_distance:.3f} fstop:{cam.dof.aperture_fstop:.1f} blades:{cam.dof.aperture_blades}\n")
                Report._write_animdata_desc(cam.animation_data, desc)
                Report._write_custom_props(cam, desc)
            desc.write(f"\n")

        # lights
        if len(bpy.data.lights):
            desc.write(f"==== Lights: {len(bpy.data.lights)}\n")
            for light in bpy.data.lights:
                desc.write(
                    f"- Light '{light.name}' {light.type} col:({light.color[0]:.3f}, {light.color[1]:.3f}, {light.color[2]:.3f}) energy:{light.energy:.3f}\n")
                if isinstance(light, bpy.types.SpotLight):
                    desc.write(f"  - spot {light.spot_size:.3f} blend {light.spot_blend:.3f}\n")
                Report._write_animdata_desc(light.animation_data, desc)
                Report._write_custom_props(light, desc)
            desc.write(f"\n")

        # materials
        if len(bpy.data.materials):
            desc.write(f"==== Materials: {len(bpy.data.materials)}\n")
            for mat in bpy.data.materials:
                desc.write(f"- Mat '{mat.name}'\n")
                wrap = bpy_extras.node_shader_utils.PrincipledBSDFWrapper(mat)
                desc.write(
                    f"  - base color ({wrap.base_color[0]:.3f}, {wrap.base_color[1]:.3f}, {wrap.base_color[2]:.3f}){self._node_shader_image_desc(wrap.base_color_texture)}\n")
                desc.write(
                    f"  - specular ior {wrap.specular:.3f}{self._node_shader_image_desc(wrap.specular_texture)}\n")
                desc.write(
                    f"  - specular tint ({wrap.specular_tint[0]:.3f}, {wrap.specular_tint[1]:.3f}, {wrap.specular_tint[2]:.3f}){self._node_shader_image_desc(wrap.specular_tint_texture)}\n")
                desc.write(
                    f"  - roughness {wrap.roughness:.3f}{self._node_shader_image_desc(wrap.roughness_texture)}\n")
                desc.write(
                    f"  - metallic {wrap.metallic:.3f}{self._node_shader_image_desc(wrap.metallic_texture)}\n")
                desc.write(f"  - ior {wrap.ior:.3f}{self._node_shader_image_desc(wrap.ior_texture)}\n")
                if wrap.transmission > 0.0 or (wrap.transmission_texture and wrap.transmission_texture.image):
                    desc.write(
                        f"  - transmission {wrap.transmission:.3f}{self._node_shader_image_desc(wrap.transmission_texture)}\n")
                if wrap.alpha < 1.0 or (wrap.alpha_texture and wrap.alpha_texture.image):
                    desc.write(
                        f"  - alpha {wrap.alpha:.3f}{self._node_shader_image_desc(wrap.alpha_texture)}\n")
                if (wrap.emission_strength > 0.0 and wrap.emission_color[0] > 0.0 and wrap.emission_color[1] > 0.0 and wrap.emission_color[2] > 0.0) or (
                        wrap.emission_strength_texture and wrap.emission_strength_texture.image):
                    desc.write(
                        f"  - emission color ({wrap.emission_color[0]:.3f}, {wrap.emission_color[1]:.3f}, {wrap.emission_color[2]:.3f}){self._node_shader_image_desc(wrap.emission_color_texture)}\n")
                    desc.write(
                        f"  - emission strength {wrap.emission_strength:.3f}{self._node_shader_image_desc(wrap.emission_strength_texture)}\n")
                if (wrap.normalmap_texture and wrap.normalmap_texture.image):
                    desc.write(
                        f"  - normalmap {wrap.normalmap_strength:.3f}{self._node_shader_image_desc(wrap.normalmap_texture)}\n")
                if mat.alpha_threshold != 0.5:
                    desc.write(f"  - alpha_threshold {fmtf(mat.alpha_threshold)}\n")
                if mat.surface_render_method != 'DITHERED':
                    desc.write(f"  - surface_render_method {mat.surface_render_method}\n")
                if mat.displacement_method != 'BUMP':
                    desc.write(f"  - displacement {mat.displacement_method}\n")
                desc.write(
                    f"  - viewport diffuse ({fmtf(mat.diffuse_color[0])}, {fmtf(mat.diffuse_color[1])}, {fmtf(mat.diffuse_color[2])}, {fmtf(mat.diffuse_color[3])})\n")
                desc.write(
                    f"  - viewport specular ({fmtf(mat.specular_color[0])}, {fmtf(mat.specular_color[1])}, {fmtf(mat.specular_color[2])}), intensity {fmtf(mat.specular_intensity)}\n")
                desc.write(f"  - viewport metallic {fmtf(mat.metallic)}, roughness {fmtf(mat.roughness)}\n")
                desc.write(
                    f"  - backface {mat.use_backface_culling} probe {mat.use_backface_culling_lightprobe_volume} shadow {mat.use_backface_culling_shadow}\n")
                Report._write_animdata_desc(mat.animation_data, desc)
                Report._write_custom_props(mat, desc)
                desc.write(f"\n")

        # actions
        if len(bpy.data.actions):
            desc.write(f"==== Actions: {len(bpy.data.actions)}\n")
            for act in sorted(bpy.data.actions, key=lambda a: a.name):
                desc.write(
                    f"- Action '{act.name}' curverange:({act.curve_frame_range[0]:.1f} .. {act.curve_frame_range[1]:.1f}) curves:{len(act.fcurves)}\n")
                for fcu in act.fcurves[:15]:
                    desc.write(
                        f"  - fcu '{fcu.data_path}[{fcu.array_index}]' smooth:{fcu.auto_smoothing} extra:{fcu.extrapolation} keyframes:{len(fcu.keyframe_points)}\n")
                    Report._write_collection_multi(fcu.keyframe_points, desc)
                Report._write_custom_props(act, desc)
                desc.write(f"\n")

        # armatures
        if len(bpy.data.armatures):
            desc.write(f"==== Armatures: {len(bpy.data.armatures)}\n")
            for arm in bpy.data.armatures:
                desc.write(f"- Armature '{arm.name}' {len(arm.bones)} bones")
                if arm.display_type != 'OCTAHEDRAL':
                    desc.write(f" display:{arm.display_type}")
                desc.write("\n")
                for bone in arm.bones:
                    desc.write(f"  - bone '{bone.name}'")
                    if bone.parent:
                        desc.write(f" parent:'{bone.parent.name}'")
                    desc.write(
                        f" h:({fmtf(bone.head[0])}, {fmtf(bone.head[1])}, {fmtf(bone.head[2])}) t:({fmtf(bone.tail[0])}, {fmtf(bone.tail[1])}, {fmtf(bone.tail[2])})")
                    if bone.inherit_scale != 'FULL':
                        desc.write(f" inh_scale:{bone.inherit_scale}")
                    if bone.head_radius > 0.0 or bone.tail_radius > 0.0:
                        desc.write(f" radius h:{bone.head_radius:.3f} t:{bone.tail_radius:.3f}")
                    desc.write(f"\n")
                    mtx = bone.matrix_local
                    desc.write(f"      {fmtf(mtx[0][0])} {fmtf(mtx[0][1])} {fmtf(mtx[0][2])} {fmtf(mtx[0][3])}\n")
                    desc.write(f"      {fmtf(mtx[1][0])} {fmtf(mtx[1][1])} {fmtf(mtx[1][2])} {fmtf(mtx[1][3])}\n")
                    desc.write(f"      {fmtf(mtx[2][0])} {fmtf(mtx[2][1])} {fmtf(mtx[2][2])} {fmtf(mtx[2][3])}\n")
                    # mtx[3] is always 0,0,0,1, not worth printing it
                desc.write(f"\n")

        # images
        if len(bpy.data.images):
            desc.write(f"==== Images: {len(bpy.data.images)}\n")
            for img in bpy.data.images:
                desc.write(f"- Image '{img.name}' {img.size[0]}x{img.size[1]} {img.depth}bpp\n")
                Report._write_custom_props(img, desc)
            desc.write(f"\n")

        text = desc.getvalue()
        desc.close()
        return text

    def import_and_check(self, input_file: pathlib.Path, import_func: Callable[[str, dict], None]) -> bool:
        """
        Imports a single file using the provided import function, and
        checks whether it matches with expected template, returns
        comparison result.

        If there is a .json file next to the input file, the parameters from
        that one file will be passed as extra parameters to the import function.

        When working in template update mode (environment variable
        BLENDER_TEST_UPDATE=1), updates the template with new result
        and always returns true.
        """
        self.tested_count += 1
        input_basename = pathlib.Path(input_file).stem
        print(f"Importing {input_file}...", flush=True)

        # load json parameters if they exist
        params = {}
        input_params_file = input_file.with_suffix(".json")
        if input_params_file.exists():
            try:
                with input_params_file.open('r', encoding='utf-8') as file:
                    params = json.load(file)
            except:
                pass

        # import
        try:
            import_func(str(input_file), params)
            got_desc = self.generate_main_data_desc()
        except RuntimeError as ex:
            got_desc = f"Error during import: {ex}"

        ref_path: pathlib.Path = self.reference_dir / f"{input_basename}.txt"
        if ref_path.exists():
            ref_desc = ref_path.read_text(encoding="utf-8").replace("\r\n", "\n")
        else:
            ref_desc = ""

        ok = True
        if self.update_templates:
            # write out newly got result as reference
            if ref_desc != got_desc:
                ref_path.write_text(got_desc, encoding="utf-8", newline="\n")
                self.updated_list.append(input_basename)
        else:
            # compare result with expected reference
            self._add_test_result(input_basename, got_desc, ref_desc)
            if ref_desc == got_desc:
                self.passed_list.append(input_basename)
            else:
                self.failed_list.append(input_basename)
                ok = False
        return ok
