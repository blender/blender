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

import bpy
import os
import tempfile
import shutil
import tarfile
import time
import stat


bl_info = {
    "name": "Game Engine Publishing",
    "author": "Mitchell Stokes (Moguri), Oren Titane (Genome36)",
    "version": (0, 1, 0),
    "blender": (2, 75, 0),
    "location": "Render Properties > Publishing Info",
    "description": "Publish .blend file as game engine runtime, manage versions and platforms",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Game_Engine/Publishing",
    "category": "Game Engine",
}


def WriteRuntime(player_path, output_path, asset_paths, copy_python, overwrite_lib, copy_dlls, make_archive, report=print):
    import struct

    player_path = bpy.path.abspath(player_path)
    ext = os.path.splitext(player_path)[-1].lower()
    output_path = bpy.path.abspath(output_path)
    output_dir = os.path.dirname(output_path)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    python_dir = os.path.join(os.path.dirname(player_path),
                              bpy.app.version_string.split()[0],
                              "python",
                              "lib")

    # Check the paths
    if not os.path.isfile(player_path) and not(os.path.exists(player_path) and player_path.endswith('.app')):
        report({'ERROR'}, "The player could not be found! Runtime not saved")
        return

    # Check if we're bundling a .app
    if player_path.lower().endswith('.app'):
        # Python doesn't need to be copied for OS X since it's already inside blenderplayer.app
        copy_python = False

        output_path = bpy.path.ensure_ext(output_path, '.app')

        if os.path.exists(output_path):
            shutil.rmtree(output_path)

        shutil.copytree(player_path, output_path)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(output_path, 'Contents', 'Resources', 'game.blend'),
                                    relative_remap=False,
                                    compress=False,
                                    copy=True,
                                    )
    else:
        # Enforce "exe" extension on Windows
        if player_path.lower().endswith('.exe'):
            output_path = bpy.path.ensure_ext(output_path, '.exe')

        # Get the player's binary and the offset for the blend
        with open(player_path, "rb") as file:
            player_d = file.read()
            offset = file.tell()

        # Create a tmp blend file (Blenderplayer doesn't like compressed blends)
        tempdir = tempfile.mkdtemp()
        blend_path = os.path.join(tempdir, bpy.path.clean_name(output_path))
        bpy.ops.wm.save_as_mainfile(filepath=blend_path,
                                    relative_remap=False,
                                    compress=False,
                                    copy=True,
                                    )

        # Get the blend data
        with open(blend_path, "rb") as blend_file:
            blend_d = blend_file.read()

        # Get rid of the tmp blend, we're done with it
        os.remove(blend_path)
        os.rmdir(tempdir)

        # Create a new file for the bundled runtime
        with open(output_path, "wb") as output:
            # Write the player and blend data to the new runtime
            print("Writing runtime...", end=" ", flush=True)
            output.write(player_d)
            output.write(blend_d)

            # Store the offset (an int is 4 bytes, so we split it up into 4 bytes and save it)
            output.write(struct.pack('BBBB', (offset >> 24) & 0xFF,
                                     (offset >> 16) & 0xFF,
                                     (offset >> 8) & 0xFF,
                                     (offset >> 0) & 0xFF))

            # Stuff for the runtime
            output.write(b'BRUNTIME')

        print("done", flush=True)

    # Make sure the runtime is executable
    os.chmod(output_path, 0o755)

    # Copy bundled Python
    blender_dir = os.path.dirname(player_path)

    if copy_python:
        print("Copying Python files...", end=" ", flush=True)
        py_folder = os.path.join(bpy.app.version_string.split()[0], "python", "lib")
        dst = os.path.join(output_dir, py_folder)
        src = python_dir

        if os.path.exists(dst) and overwrite_lib:
            shutil.rmtree(dst)

        if not os.path.exists(dst):
            shutil.copytree(src, dst, ignore=lambda dir, contents: [i for i in contents if i == '__pycache__'])
            print("done", flush=True)
        else:
            print("used existing Python folder", flush=True)

    # And DLLs if we're doing a Windows runtime)
    if copy_dlls and ext == ".exe":
        print("Copying DLLs...", end=" ", flush=True)
        for file in [i for i in os.listdir(blender_dir) if i.lower().endswith('.dll')]:
            src = os.path.join(blender_dir, file)
            dst = os.path.join(output_dir, file)
            shutil.copy2(src, dst)

        print("done", flush=True)

    # Copy assets
    for ap in asset_paths:
        src = bpy.path.abspath(ap.name)
        dst = os.path.join(output_dir, ap.name[2:] if ap.name.startswith('//') else ap.name)

        if os.path.exists(src):
            if os.path.isdir(src):
                if ap.overwrite and os.path.exists(dst):
                    shutil.rmtree(dst)
                elif not os.path.exists(dst):
                    shutil.copytree(src, dst)
            else:
                if ap.overwrite or not os.path.exists(dst):
                    shutil.copy2(src, dst)
        else:
            report({'ERROR'}, "Could not find asset path: '%s'" % src)

    # Make archive
    if make_archive:
        print("Making archive...", end=" ", flush=True)

        arctype = ''
        if player_path.lower().endswith('.exe'):
            arctype = 'zip'
        elif player_path.lower().endswith('.app'):
            arctype = 'zip'
        else: # Linux
            arctype = 'gztar'

        basedir = os.path.normpath(os.path.join(os.path.dirname(output_path), '..'))
        afilename = os.path.join(basedir, os.path.basename(output_dir))

        if arctype == 'gztar':
            # Create the tarball ourselves instead of using shutil.make_archive
            # so we can handle permission bits.

            # The runtimename needs to use forward slashes as a path separator
            # since this is what tarinfo.name is using.
            runtimename = os.path.relpath(output_path, basedir).replace('\\', '/')

            def _set_ex_perm(tarinfo):
                if tarinfo.name == runtimename:
                    tarinfo.mode = 0o755
                return tarinfo

            with tarfile.open(afilename + '.tar.gz', 'w:gz') as tf:
                tf.add(output_dir, os.path.relpath(output_dir, basedir), filter=_set_ex_perm)
        elif arctype == 'zip':
            shutil.make_archive(afilename, 'zip', output_dir)
        else:
            report({'ERROR'}, "Unknown archive type %s for runtime %s\n" % (arctype, player_path))

        print("done", flush=True)


class PublishAllPlatforms(bpy.types.Operator):
    bl_idname = "wm.publish_platforms"
    bl_label = "Exports a runtime for each listed platform"

    def execute(self, context):
        ps = context.scene.ge_publish_settings

        if ps.publish_default_platform:
            print("Publishing default platform")
            blender_bin_path = bpy.app.binary_path
            blender_bin_dir = os.path.dirname(blender_bin_path)
            ext = os.path.splitext(blender_bin_path)[-1].lower()
            WriteRuntime(os.path.join(blender_bin_dir, 'blenderplayer' + ext),
                         os.path.join(ps.output_path, 'default', ps.runtime_name),
                         ps.asset_paths,
                         True,
                         True,
                         True,
                         ps.make_archive,
                         self.report
                         )
        else:
            print("Skipping default platform")

        for platform in ps.platforms:
            if platform.publish:
                print("Publishing", platform.name)
                WriteRuntime(platform.player_path,
                            os.path.join(ps.output_path, platform.name, ps.runtime_name),
                            ps.asset_paths,
                            True,
                            True,
                            True,
                            ps.make_archive,
                            self.report
                            )
            else:
                print("Skipping", platform.name)

        return {'FINISHED'}


class RENDER_UL_assets(bpy.types.UIList):
    bl_label = "Asset Paths Listing"

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        layout.prop(item, "name", text="", emboss=False)


class RENDER_UL_platforms(bpy.types.UIList):
    bl_label = "Platforms Listing"

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        row = layout.row()
        row.label(item.name)
        row.prop(item, "publish", text="")


class RENDER_PT_publish(bpy.types.Panel):
    bl_label = "Publishing Info"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.render.engine == "BLENDER_GAME")

    def draw(self, context):
        ps = context.scene.ge_publish_settings
        layout = self.layout

        # config
        layout.prop(ps, 'output_path')
        layout.prop(ps, 'runtime_name')
        layout.prop(ps, 'lib_path')
        layout.prop(ps, 'make_archive')

        layout.separator()

        # assets list
        layout.label("Asset Paths")

        # UI_UL_list
        row = layout.row()
        row.template_list("RENDER_UL_assets", "assets_list", ps, 'asset_paths', ps, 'asset_paths_active')

        # operators
        col = row.column(align=True)
        col.operator(PublishAddAssetPath.bl_idname, icon='ZOOMIN', text="")
        col.operator(PublishRemoveAssetPath.bl_idname, icon='ZOOMOUT', text="")

        # indexing
        if len(ps.asset_paths) > ps.asset_paths_active >= 0:
            ap = ps.asset_paths[ps.asset_paths_active]
            row = layout.row()
            row.prop(ap, 'overwrite')

        layout.separator()

        # publishing list
        row = layout.row(align=True)
        row.label("Platforms")
        row.prop(ps, 'publish_default_platform')

        # UI_UL_list
        row = layout.row()
        row.template_list("RENDER_UL_platforms", "platforms_list", ps, 'platforms', ps, 'platforms_active')

        # operators
        col = row.column(align=True)
        col.operator(PublishAddPlatform.bl_idname, icon='ZOOMIN', text="")
        col.operator(PublishRemovePlatform.bl_idname, icon='ZOOMOUT', text="")
        col.menu("PUBLISH_MT_platform_specials", icon='DOWNARROW_HLT', text="")

        # indexing
        if len(ps.platforms) > ps.platforms_active >= 0:
            platform = ps.platforms[ps.platforms_active]
            layout.prop(platform, 'name')
            layout.prop(platform, 'player_path')

        layout.operator(PublishAllPlatforms.bl_idname, 'Publish Platforms')


class PublishAutoPlatforms(bpy.types.Operator):
    bl_idname = "scene.publish_auto_platforms"
    bl_label = "Auto Add Platforms"

    def execute(self, context):
        ps = context.scene.ge_publish_settings

        # verify lib folder
        lib_path = bpy.path.abspath(ps.lib_path)
        if not os.path.exists(lib_path):
            self.report({'ERROR'}, "Could not add platforms, lib folder (%s) does not exist" % lib_path)
            return {'CANCELLED'}

        for lib in [i for i in os.listdir(lib_path) if os.path.isdir(os.path.join(lib_path, i))]:
            print("Found folder:", lib)
            player_found = False
            for root, dirs, files in os.walk(os.path.join(lib_path, lib)):
                if "__MACOSX" in root:
                    continue

                for f in dirs + files:
                    if f.startswith("blenderplayer.app") or f.startswith("blenderplayer"):
                        a = ps.platforms.add()
                        if lib.startswith('blender-'):
                            # Clean up names for packages from blender.org
                            # example: blender-2.71-RC2-OSX_10.6-x86_64.zip => OSX_10.6-x86_64.zip
                            # We're pretty consistent on naming, so this should hold up.
                            a.name = '-'.join(lib.split('-')[3 if 'rc' in lib.lower() else 2:])
                        else:
                            a.name = lib
                        a.player_path = bpy.path.relpath(os.path.join(root, f))
                        player_found = True
                        break

                if player_found:
                    break

        return {'FINISHED'}

# TODO This operator takes a long time to run, which is bad for UX. Could this instead be done as some sort of
# modal dialog? This could also allow users to select which platforms to download and give a better progress
# indicator.
class PublishDownloadPlatforms(bpy.types.Operator):
    bl_idname = "scene.publish_download_platforms"
    bl_label = "Download Platforms"

    def execute(self, context):
        import html.parser
        import urllib.request

        remote_platforms = []

        ps = context.scene.ge_publish_settings

        # create lib folder if not already available
        lib_path = bpy.path.abspath(ps.lib_path)
        if not os.path.exists(lib_path):
            os.makedirs(lib_path)

        print("Retrieving list of platforms from blender.org...", end=" ", flush=True)

        class AnchorParser(html.parser.HTMLParser):
            def handle_starttag(self, tag, attrs):
                if tag == 'a':
                    for key, value in attrs:
                        if key == 'href' and value.startswith('blender'):
                            remote_platforms.append(value)

        url = 'http://download.blender.org/release/Blender' + bpy.app.version_string.split()[0]
        parser = AnchorParser()
        data = urllib.request.urlopen(url).read()
        parser.feed(str(data))

        print("done", flush=True)

        print("Downloading files (this will take a while depending on your internet connection speed).", flush=True)
        for i in remote_platforms:
            src = '/'.join((url, i))
            dst = os.path.join(lib_path, i)

            dst_dir = '.'.join([i for i in dst.split('.') if i not in {'zip', 'tar', 'bz2'}])
            if not os.path.exists(dst) and not os.path.exists(dst.split('.')[0]):
                print("Downloading " + src + "...", end=" ", flush=True)
                urllib.request.urlretrieve(src, dst)
                print("done", flush=True)
            else:
                print("Reusing existing file: " + dst, flush=True)

            print("Unpacking " + dst + "...", end=" ", flush=True)
            if os.path.exists(dst_dir):
                shutil.rmtree(dst_dir)
            shutil.unpack_archive(dst, dst_dir)
            print("done", flush=True)

        print("Creating platform from libs...", flush=True)
        bpy.ops.scene.publish_auto_platforms()
        return {'FINISHED'}


class PublishAddPlatform(bpy.types.Operator):
    bl_idname = "scene.publish_add_platform"
    bl_label = "Add Publish Platform"

    def execute(self, context):
        a = context.scene.ge_publish_settings.platforms.add()
        a.name = a.name
        return {'FINISHED'}


class PublishRemovePlatform(bpy.types.Operator):
    bl_idname = "scene.publish_remove_platform"
    bl_label = "Remove Publish Platform"

    def execute(self, context):
        ps = context.scene.ge_publish_settings
        if ps.platforms_active < len(ps.platforms):
            ps.platforms.remove(ps.platforms_active)
            return {'FINISHED'}
        return {'CANCELLED'}


# TODO maybe this should display a file browser?
class PublishAddAssetPath(bpy.types.Operator):
    bl_idname = "scene.publish_add_assetpath"
    bl_label = "Add Asset Path"

    def execute(self, context):
        a = context.scene.ge_publish_settings.asset_paths.add()
        a.name = a.name
        return {'FINISHED'}


class PublishRemoveAssetPath(bpy.types.Operator):
    bl_idname = "scene.publish_remove_assetpath"
    bl_label = "Remove Asset Path"

    def execute(self, context):
        ps = context.scene.ge_publish_settings
        if ps.asset_paths_active < len(ps.asset_paths):
            ps.asset_paths.remove(ps.asset_paths_active)
            return {'FINISHED'}
        return {'CANCELLED'}


class PUBLISH_MT_platform_specials(bpy.types.Menu):
    bl_label = "Platform Specials"

    def draw(self, context):
        layout = self.layout
        layout.operator(PublishAutoPlatforms.bl_idname)
        layout.operator(PublishDownloadPlatforms.bl_idname)


class PlatformSettings(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty(
            name = "Platform Name",
            description = "The name of the platform",
            default = "Platform",
            )

    player_path = bpy.props.StringProperty(
            name = "Player Path",
            description = "The path to the Blenderplayer to use for this platform",
            default = "//lib/platform/blenderplayer",
            subtype = 'FILE_PATH',
            )

    publish = bpy.props.BoolProperty(
            name = "Publish",
            description = "Whether or not to publish to this platform",
            default = True,
            )


class AssetPath(bpy.types.PropertyGroup):
    # TODO This needs a way to be a FILE_PATH or a DIR_PATH
    name = bpy.props.StringProperty(
            name = "Asset Path",
            description = "Path to the asset to be copied",
            default = "//src",
            subtype = 'FILE_PATH',
            )

    overwrite = bpy.props.BoolProperty(
            name = "Overwrite Asset",
            description = "Overwrite the asset if it already exists in the destination folder",
            default = True,
            )


class PublishSettings(bpy.types.PropertyGroup):
    output_path = bpy.props.StringProperty(
            name = "Publish Output",
            description = "Where to publish the game",
            default = "//bin/",
            subtype = 'DIR_PATH',
            )

    runtime_name = bpy.props.StringProperty(
            name = "Runtime name",
            description = "The filename for the created runtime",
            default = "game",
            )

    lib_path = bpy.props.StringProperty(
            name = "Library Path",
            description = "Directory to search for platforms",
            default = "//lib/",
            subtype = 'DIR_PATH',
            )

    publish_default_platform = bpy.props.BoolProperty(
            name = "Publish Default Platform",
            description = "Whether or not to publish the default platform (the Blender install running this addon) when publishing platforms",
            default = True,
            )


    platforms = bpy.props.CollectionProperty(type=PlatformSettings, name="Platforms")
    platforms_active = bpy.props.IntProperty()

    asset_paths = bpy.props.CollectionProperty(type=AssetPath, name="Asset Paths")
    asset_paths_active = bpy.props.IntProperty()

    make_archive = bpy.props.BoolProperty(
            name = "Make Archive",
            description = "Create a zip archive of the published game",
            default = True,
            )


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.ge_publish_settings = bpy.props.PointerProperty(type=PublishSettings)


def unregister():
    bpy.utils.unregister_module(__name__)
    del bpy.types.Scene.ge_publish_settings


if __name__ == "__main__":
    register()
