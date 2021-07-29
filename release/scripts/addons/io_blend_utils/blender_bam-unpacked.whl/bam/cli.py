#!/usr/bin/env python3

# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

"""
This is the entry point for command line access.
"""

import os
import sys
import json

# ------------------
# Ensure module path
path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "modules"))
if path not in sys.path:
    sys.path.append(path)
del path
# --------

import logging
log = logging.getLogger("bam_cli")


def fatal(msg):
    if __name__ == "__main__":
        sys.stderr.write("fatal: ")
        sys.stderr.write(msg)
        sys.stderr.write("\n")
        sys.exit(1)
    else:
        raise RuntimeError(msg)


class bam_config:
    # fake module
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    CONFIG_DIR = ".bam"
    # can infact be any file in the session
    SESSION_FILE = ".bam_paths_remap.json"

    @staticmethod
    def find_basedir(cwd=None, path_suffix=None, abort=False, test_subpath=CONFIG_DIR, descr="<unknown>"):
        """
        Return the config path (or None when not found)
        Actually should raise an error?
        """

        if cwd is None:
            cwd = os.getcwd()

        parent = (os.path.normpath(
                  os.path.abspath(
                  cwd)))

        parent_prev = None

        while parent != parent_prev:
            test_dir = os.path.join(parent, test_subpath)
            if os.path.exists(test_dir):
                if path_suffix is not None:
                    test_dir = os.path.join(test_dir, path_suffix)
                return test_dir

            parent_prev = parent
            parent = os.path.dirname(parent)

        if abort is True:
            fatal("Not a %s (or any of the parent directories): %s" % (descr, test_subpath))

        return None

    @staticmethod
    def find_rootdir(cwd=None, path_suffix=None, abort=False, test_subpath=CONFIG_DIR, descr="<unknown>"):
        """
        find_basedir(), without '.bam' suffix
        """
        path = bam_config.find_basedir(
                cwd=cwd,
                path_suffix=path_suffix,
                abort=abort,
                test_subpath=test_subpath,
                )

        return path[:-(len(test_subpath) + 1)]

    def find_sessiondir(cwd=None, abort=False):
        """
        from:  my_project/my_session/some/subdir
        to:    my_project/my_session
        where: my_project/.bam/  (is the basedir)
        """
        session_rootdir = bam_config.find_basedir(
                cwd=cwd,
                test_subpath=bam_config.SESSION_FILE,
                abort=abort,
                descr="bam session"
                )

        if session_rootdir is not None:
            return session_rootdir[:-len(bam_config.SESSION_FILE)]
        else:
            if abort:
                if not os.path.isdir(session_rootdir):
                    fatal("Expected a directory (%r)" % session_rootdir)
            return None

    @staticmethod
    def load(id_="config", cwd=None, abort=False):
        filepath = bam_config.find_basedir(
                cwd=cwd,
                path_suffix=id_,
                descr="bam repository",
                )
        if abort is True:
            if filepath is None:
                fatal("Not a bam repository (or any of the parent directories): .bam")

        with open(filepath, 'r') as f:
            return json.load(f)

    @staticmethod
    def write(id_="config", data=None, cwd=None):
        filepath = bam_config.find_basedir(
                cwd=cwd,
                path_suffix=id_,
                descr="bam repository",
                )

        from bam.utils.system import write_json_to_file
        write_json_to_file(filepath, data)

    @staticmethod
    def write_bamignore(cwd=None):
        path = bam_config.find_rootdir(cwd=cwd)
        if path:
            filepath = os.path.join(path, ".bamignore")
            with open(filepath, 'w') as f:
                f.write(r".*\.blend\d+$")

    @staticmethod
    def create_bamignore_filter(id_=".bamignore", cwd=None):
        path = bam_config.find_rootdir()
        bamignore = os.path.join(path, id_)
        if os.path.isfile(bamignore):
            with open(bamignore, 'r', encoding='utf-8') as f:
                compiled_patterns = []

                import re
                for i, l in enumerate(f):
                    l = l.rstrip()
                    if l:
                        try:
                            p = re.compile(l)
                        except re.error as e:
                            fatal("%s:%d file contains an invalid regular expression, %s" %
                                  (bamignore, i + 1, str(e)))
                        compiled_patterns.append(p)

                if compiled_patterns:
                    def filter_ignore(f):
                        for pattern in filter_ignore.compiled_patterns:
                            if re.match(pattern, f):
                                return False
                        return True
                    filter_ignore.compiled_patterns = compiled_patterns

                    return filter_ignore

        return None


class bam_session:
    # fake module
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    def session_path_to_cache(
            path,
            cachedir=None,
            session_rootdir=None,
            paths_remap_relbase=None,
            abort=True):
        """
        Given an absolute path, give us the cache-path on disk.
        """

        if session_rootdir is None:
            session_rootdir = bam_config.find_sessiondir(path, abort=abort)

        if paths_remap_relbase is None:
            with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
                paths_remap = json.load(fp)
                paths_remap_relbase = paths_remap.get(".", "")
                del fp, paths_remap

        cachedir = os.path.join(bam_config.find_rootdir(cwd=session_rootdir, abort=True), ".cache")
        path_rel = os.path.relpath(path, session_rootdir)
        if path_rel[0] == "_":
            path_cache = os.path.join(cachedir, path_rel[1:])
        else:
            path_cache = os.path.join(cachedir, paths_remap_relbase, path_rel)
        path_cache = os.path.normpath(path_cache)
        return path_cache

    @staticmethod
    def request_url(req_path):
        cfg = bam_config.load()
        result = "%s/%s" % (cfg['url'], req_path)
        return result

    @staticmethod
    def status(session_rootdir,
               paths_uuid_update=None):

        paths_add = {}
        paths_remove = {}
        paths_modified = {}

        from bam.utils.system import uuid_from_file

        session_rootdir = os.path.abspath(session_rootdir)

        # don't commit metadata
        paths_used = {
            os.path.join(session_rootdir, ".bam_paths_uuid.json"),
            os.path.join(session_rootdir, ".bam_paths_remap.json"),
            os.path.join(session_rootdir, ".bam_deps_remap.json"),
            os.path.join(session_rootdir, ".bam_paths_edit.data"),
            os.path.join(session_rootdir, ".bam_tmp.zip"),
            }

        paths_uuid = bam_session.load_paths_uuid(session_rootdir)

        for f_rel, sha1 in paths_uuid.items():
            f_abs = os.path.join(session_rootdir, f_rel)
            if os.path.exists(f_abs):
                sha1_modified = uuid_from_file(f_abs)
                if sha1_modified != sha1:
                    paths_modified[f_rel] = f_abs
                if paths_uuid_update is not None:
                    paths_uuid_update[f_rel] = sha1_modified
                paths_used.add(f_abs)
            else:
                paths_remove[f_rel] = f_abs

        # ----
        # find new files
        def iter_files(path, filename_check=None):
            for dirpath, dirnames, filenames in os.walk(path):

                # skip '.svn'
                if dirpath.startswith(".") and dirpath != ".":
                    continue

                for filename in filenames:
                    filepath = os.path.join(dirpath, filename)
                    if filename_check is None or filename_check(filepath):
                        yield filepath

        bamignore_filter = bam_config.create_bamignore_filter()

        for f_abs in iter_files(session_rootdir, bamignore_filter):
            if f_abs not in paths_used:
                # we should be clever - add the file to a useful location based on some rules
                # (category, filetype & tags?)

                f_rel = os.path.relpath(f_abs, session_rootdir)

                paths_add[f_rel] = f_abs

                if paths_uuid_update is not None:
                    paths_uuid_update[f_rel] = uuid_from_file(f_abs)

        return paths_add, paths_remove, paths_modified

    @staticmethod
    def load_paths_uuid(session_rootdir):
        with open(os.path.join(session_rootdir, ".bam_paths_uuid.json")) as f:
            return json.load(f)

    @staticmethod
    def is_dirty(session_rootdir):
        paths_add, paths_remove, paths_modified = bam_session.status(session_rootdir)
        return any((paths_add, paths_modified, paths_remove))

    @staticmethod
    def binary_edits_apply_single(
            blendfile_abs,  # str
            blendfile,  # bytes
            binary_edits,
            session_rootdir,
            paths_uuid_update=None,
            ):

        sys.stdout.write("  operating on: %r\n" % blendfile_abs)
        sys.stdout.flush()
        # we don't want to read, just edit whats there.
        with open(blendfile_abs, 'rb+') as fh_blend:
            for ofs, data in binary_edits:
                # sys.stdout.write("\n%r\n" % data)
                sys.stdout.flush()
                # ensure we're writing to the correct location.
                # fh_blend.seek(ofs)
                # sys.stdout.write(repr(b'existing data: ' + fh_blend.read(len(data) + 1)))
                fh_blend.seek(ofs)
                fh_blend.write(data)
        sys.stdout.write("\n")
        sys.stdout.flush()

        if paths_uuid_update is not None:
            # update hash!
            # we could do later, but the file is fresh in cache, so do now
            from bam.utils.system import uuid_from_file
            f_rel = os.path.relpath(blendfile_abs, session_rootdir)
            paths_uuid_update[f_rel] = uuid_from_file(blendfile_abs)
            del uuid_from_file

    @staticmethod
    def binary_edits_apply_all(
            session_rootdir,
            # collection of local paths or None (to apply all binary edits)
            paths=None,
            update_uuid=False,
            ):

        # sanity check
        if paths is not None:
            for path in paths:
                assert(type(path) is bytes)
                assert(not os.path.isabs(path))
                assert(os.path.exists(os.path.join(session_rootdir, path.decode('utf-8'))))

        with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
            paths_remap = json.load(fp)
            paths_remap_relbase = paths_remap.get(".", "")
            paths_remap_reverse = {v: k for k, v in paths_remap.items()}
            del paths_remap

        with open(os.path.join(session_rootdir, ".bam_paths_edit.data"), 'rb') as fh:
            import pickle
            binary_edits_all = pickle.load(fh)
            paths_uuid_update = {} if update_uuid else None
            for blendfile, binary_edits in binary_edits_all.items():
                if binary_edits:
                    if paths is not None and blendfile not in paths:
                        continue

                    # get the absolute path as it is in the main repo
                    # then remap back to our local checkout
                    blendfile_abs_remote = os.path.normpath(os.path.join(paths_remap_relbase, blendfile.decode('utf-8')))
                    blendfile_abs = os.path.join(session_rootdir, paths_remap_reverse[blendfile_abs_remote])

                    bam_session.binary_edits_apply_single(
                            blendfile_abs,
                            blendfile,
                            binary_edits,
                            session_rootdir,
                            paths_uuid_update,
                            )
            del pickle
            del binary_edits_all

        if update_uuid and paths_uuid_update:
            # freshen the UUID's based on the replayed binary_edits
            from bam.utils.system import write_json_to_file
            paths_uuid = bam_session.load_paths_uuid(session_rootdir)
            paths_uuid.update(paths_uuid_update)
            write_json_to_file(os.path.join(session_rootdir, ".bam_paths_uuid.json"), paths_uuid)
            del write_json_to_file
            del paths_uuid

    @staticmethod
    def binary_edits_update_single(
            blendfile_abs,
            binary_edits,
            # callback, takes a filepath
            remap_filepath_cb,
            ):
        """
        After committing a blend file, we need to re-create the binary edits.
        """
        from bam.blend import blendfile_path_walker
        for fp, (rootdir, fp_blend_basename) in blendfile_path_walker.FilePath.visit_from_blend(
                blendfile_abs,
                readonly=True,
                recursive=False,
                ):
            f_rel_orig = fp.filepath
            f_rel = remap_filepath_cb(f_rel_orig)
            fp.filepath_assign_edits(f_rel, binary_edits)


class bam_commands:
    """
    Sub-commands from the command-line map directly to these methods.
    """
    # fake module
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def init(url, directory_name=None):
        import urllib.parse

        if "@" in url:
            # first & last :)
            username, url = url.rpartition('@')[0::2]
        else:
            import getpass
            username = getpass.getuser()
            print("Using username:", username)
            del getpass

        parsed_url = urllib.parse.urlsplit(url)

        proj_dirname = os.path.basename(parsed_url.path)
        if directory_name:
            proj_dirname = directory_name
        proj_dirname_abs = os.path.join(os.getcwd(), proj_dirname)

        if os.path.exists(proj_dirname_abs):
            fatal("Cannot create project %r already exists" % proj_dirname_abs)

        # Create the project directory inside the current directory
        os.mkdir(proj_dirname_abs)
        # Create the .bam directory
        bam_basedir = os.path.join(proj_dirname_abs, bam_config.CONFIG_DIR)
        os.mkdir(bam_basedir)

        # Add a config file with project url, username and password
        bam_config.write(
                data={
                    "url": url,
                    "user": username,
                    "password": "",
                    "config_version": 1
                    },
                cwd=proj_dirname_abs)

        # Create the default .bamignore
        # TODO (fsiddi) get this data from the project config on the server
        bam_config.write_bamignore(cwd=proj_dirname_abs)

        print("Project %r initialized" % proj_dirname)

    @staticmethod
    def create(session_name):
        rootdir = bam_config.find_rootdir(abort=True)

        session_rootdir = os.path.join(rootdir, session_name)

        if os.path.exists(session_rootdir):
            fatal("session path exists %r" % session_rootdir)
        if rootdir != bam_config.find_rootdir(cwd=session_rootdir):
            fatal("session is located outside %r" % rootdir)

        def write_empty(f, data):
            with open(os.path.join(session_rootdir, f), 'wb') as f:
                f.write(data)

        os.makedirs(session_rootdir)

        write_empty(".bam_paths_uuid.json", b'{}')
        write_empty(".bam_paths_remap.json", b'{}')
        write_empty(".bam_deps_remap.json", b'{}')

        print("Session %r created" % session_name)

    @staticmethod
    def checkout(
            path,
            output_dir=None,
            session_rootdir_partial=None,
            all_deps=False,
            ):

        # ---------
        # constants
        CHUNK_SIZE = 1024

        cfg = bam_config.load(abort=True)

        if output_dir is None:
            # fallback to the basename
            session_rootdir = os.path.splitext(os.path.basename(path))[0]
        else:
            output_dir = os.path.realpath(output_dir)
            if os.sep in output_dir.rstrip(os.sep):
                # are we a subdirectory?
                # (we know this exists, since we have config already)
                project_rootdir = bam_config.find_rootdir(abort=True)
                if ".." in os.path.relpath(output_dir, project_rootdir).split(os.sep):
                    fatal("Output %r is outside the project path %r" % (output_dir, project_rootdir))
                del project_rootdir
            session_rootdir = output_dir
        del output_dir

        if bam_config.find_sessiondir(cwd=session_rootdir):
            fatal("Can't checkout in existing session. Use update.")

        payload = {
            "filepath": path,
            "command": "checkout",
            "arguments": json.dumps({
                "all_deps": all_deps,
                }),
            }

        # --------------------------------------------------------------------
        # First request we simply get a list of files to download
        #
        import requests
        r = requests.get(
                bam_session.request_url("file"),
                params=payload,
                auth=(cfg['user'], cfg['password']),
                stream=True,
                )

        if r.status_code not in {200, }:
            # TODO(cam), make into reusable function?
            print("Error %d:\n%s" % (r.status_code, next(r.iter_content(chunk_size=1024)).decode('utf-8')))
            return

        # TODO(cam) how to tell if we get back a message payload? or real data???
        dst_dir_data = payload['filepath'].split('/')[-1]

        if 1:
            dst_dir_data += ".zip"

        with open(dst_dir_data, 'wb') as f:
            import struct
            ID_MESSAGE = 1
            ID_PAYLOAD = 2
            head = r.raw.read(4)
            if head != b'BAM\0':
                fatal("bad header from server")

            while True:
                msg_type, msg_size = struct.unpack("<II", r.raw.read(8))
                if msg_type == ID_MESSAGE:
                    sys.stdout.write(r.raw.read(msg_size).decode('utf-8'))
                    sys.stdout.flush()
                elif msg_type == ID_PAYLOAD:
                    # payload
                    break

            tot_size = 0
            for chunk in r.iter_content(chunk_size=CHUNK_SIZE):
                if chunk:  # filter out keep-alive new chunks
                    tot_size += len(chunk)
                    f.write(chunk)
                    f.flush()

                    sys.stdout.write("\rdownload: [%03d%%]" % ((100 * tot_size) // msg_size))
                    sys.stdout.flush()
            del struct

        # ---------------
        # extract the zip
        import zipfile
        with open(dst_dir_data, 'rb') as zip_file:
            zip_handle = zipfile.ZipFile(zip_file)
            zip_handle.extractall(session_rootdir)
        del zipfile, zip_file

        os.remove(dst_dir_data)
        sys.stdout.write("\nwritten: %r\n" % session_rootdir)

        # ----
        # Update cache
        cachedir = os.path.join(bam_config.find_rootdir(cwd=session_rootdir, abort=True), ".cache")
        # os.makedirs(cachedir, exist_ok=True)

        # --------------------------------------------------------------------
        # Second request we simply download the files..
        #
        # which we don't have in cache,
        # note that its possible we have all in cache and don't need to make a second request.
        files = []
        with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
            from bam.utils.system import uuid_from_file
            paths_remap = json.load(fp)

            paths_uuid = bam_session.load_paths_uuid(session_rootdir)

            for f_src, f_dst in paths_remap.items():
                if f_src == ".":
                    continue

                uuid = paths_uuid.get(f_src)
                if uuid is not None:
                    f_dst_abs = os.path.join(cachedir, f_dst)
                    if os.path.exists(f_dst_abs):
                        # check if we need to download this file?
                        uuid_exists = uuid_from_file(f_dst_abs)
                        assert(type(uuid) is type(uuid_exists))
                        if uuid == uuid_exists:
                            continue

                files.append(f_dst)

            del uuid_from_file

        if files:
            payload = {
                "command": "checkout_download",
                "arguments": json.dumps({
                    "files": files,
                    }),
                }
            import requests
            r = requests.get(
                    bam_session.request_url("file"),
                    params=payload,
                    auth=(cfg['user'], cfg['password']),
                    stream=True,
                    )

            if r.status_code not in {200, }:
                # TODO(cam), make into reusable function?
                print("Error %d:\n%s" % (r.status_code, next(r.iter_content(chunk_size=1024)).decode('utf-8')))
                return

            # TODO(cam) how to tell if we get back a message payload? or real data???
            # needed so we don't read past buffer bounds
            def iter_content_size(r, size, chunk_size=CHUNK_SIZE):
                while size >= chunk_size:
                    size -= chunk_size
                    yield r.raw.read(chunk_size)
                if size:
                    yield r.raw.read(size)


            import struct
            ID_MESSAGE = 1
            ID_PAYLOAD = 2
            ID_PAYLOAD_APPEND = 3
            ID_PAYLOAD_EMPTY = 4
            ID_DONE = 5
            head = r.raw.read(4)
            if head != b'BAM\0':
                fatal("bad header from server")

            file_index = 0
            is_header_read = True
            while True:
                if is_header_read:
                    msg_type, msg_size = struct.unpack("<II", r.raw.read(8))
                else:
                    is_header_read = True

                if msg_type == ID_MESSAGE:
                    sys.stdout.write(r.raw.read(msg_size).decode('utf-8'))
                    sys.stdout.flush()
                elif msg_type == ID_PAYLOAD_EMPTY:
                    file_index += 1
                elif msg_type == ID_PAYLOAD:
                    f_rel = files[file_index]
                    f_abs = os.path.join(cachedir, files[file_index])
                    file_index += 1

                    # server also prints... we could do this a bit different...
                    sys.stdout.write("file: %r" % f_rel)
                    sys.stdout.flush()

                    os.makedirs(os.path.dirname(f_abs), exist_ok=True)

                    with open(f_abs, "wb") as f:
                        while True:
                            tot_size = 0
                            # No need to worry about filling memory,
                            # total chunk size is capped by the server
                            chunks = []
                            # for chunk in r.iter_content(chunk_size=CHUNK_SIZE):
                            for chunk in iter_content_size(r, msg_size, chunk_size=CHUNK_SIZE):
                                if chunk:  # filter out keep-alive new chunks
                                    tot_size += len(chunk)
                                    # f.write(chunk)
                                    # f.flush()
                                    chunks.append(chunk)

                                    sys.stdout.write("\rdownload: [%03d%%]" % ((100 * tot_size) // msg_size))
                                    sys.stdout.flush()
                            assert(tot_size == msg_size)

                            # decompress all chunks
                            import lzma
                            f.write(lzma.decompress(b''.join(chunks)))
                            f.flush()
                            del chunks

                            # take care! - re-reading the next header to see if
                            # we're appending to this file or not
                            msg_type, msg_size = struct.unpack("<II", r.raw.read(8))
                            if msg_type == ID_PAYLOAD_APPEND:
                                continue
                            # otherwise continue the outer loop, without re-reading the header

                            # don't re-read the header next iteration
                            is_header_read = False
                            break

                elif msg_type == ID_DONE:
                    break
                elif msg_type == ID_PAYLOAD_APPEND:
                    # Should only handle in a read-loop above
                    raise Exception("Invalid state for message-type %d" % msg_type)
                else:
                    raise Exception("Unknown message-type %d" % msg_type)
            del struct


        del files

        # ------------
        # Update Cache
        #
        # TODO, remove stale cache
        # we need this to map to project level paths
        #
        # Copy cache into our session before applying binary edits.
        with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
            paths_remap = json.load(fp)
            for f_dst, f_src in paths_remap.items():
                if f_dst == ".":
                    continue

                f_src_abs = os.path.join(cachedir, f_src)

                # this should 'almost' always be true
                if os.path.exists(f_src_abs):

                    f_dst_abs = os.path.join(session_rootdir, f_dst)
                    os.makedirs(os.path.dirname(f_dst_abs), exist_ok=True)

                    import shutil
                    # print("from        ", f_dst_abs, os.path.exists(f_dst_abs))
                    # print("to          ", f_src_abs, os.path.exists(f_src_abs))
                    # print("CREATING:   ", f_src_abs)
                    shutil.copyfile(f_src_abs, f_dst_abs)
                    del shutil
        # import time
        # time.sleep(10000)

        del paths_remap, cachedir
        # ...done updating cache
        # ----------------------

        # -------------------
        # replay binary edits
        #
        # We've downloaded the files pristine from their repo.
        # This means we can use local cache and avoid re-downloading.
        #
        # But for files to work locally we have to apply binary edits given to us by the server.

        sys.stdout.write("replaying edits...\n")
        bam_session.binary_edits_apply_all(session_rootdir, paths=None, update_uuid=True)

        # ...done with binary edits
        # -------------------------

    @staticmethod
    def update(paths):
        # Load project configuration
        # cfg = bam_config.load(abort=True)

        # TODO(cam) multiple paths
        session_rootdir = bam_config.find_sessiondir(paths[0], abort=True)
        # so as to avoid off-by-one errors string mangling
        session_rootdir = session_rootdir.rstrip(os.sep)

        paths_uuid = bam_session.load_paths_uuid(session_rootdir)

        if not paths_uuid:
            print("Nothing to update!")
            return

        if bam_session.is_dirty(session_rootdir):
            fatal("Local changes detected, commit before checking out!")

        # -------------------------------------------------------------------------------
        # TODO(cam) don't guess this important info
        files = [f for f in os.listdir(session_rootdir) if not f.startswith(".")]
        files_blend = [f for f in files if f.endswith(".blend")]
        if files_blend:
            f = files_blend[0]
        else:
            f = files[0]
        with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
            paths_remap = json.load(fp)
            paths_remap_relbase = paths_remap.get(".", "")
        path = os.path.join(paths_remap_relbase, f)
        # -------------------------------------------------------------------------------

        # merge sessions
        session_tmp = session_rootdir + ".tmp"
        bam_commands.checkout(
                path,
                output_dir=session_tmp,
                session_rootdir_partial=session_rootdir,
                )

        for dirpath, dirnames, filenames in os.walk(session_tmp):
            for filename in filenames:
                filepath = os.path.join(dirpath, filename)
                f_src = filepath
                f_dst = session_rootdir + filepath[len(session_tmp):]
                os.rename(f_src, f_dst)
        import shutil
        shutil.rmtree(session_tmp)

    @staticmethod
    def revert(paths):
        # Copy files back from the cache
        # a relatively lightweight operation

        def _get_from_path(session_rootdir, cachedir, paths_remap, path_abs):
            print("====================")
            print(path_abs)
            path_abs = os.path.normpath(path_abs)
            print(paths_remap)
            for f_src, f_dst in paths_remap.items():
                if f_src == ".":
                    continue
                print("-----------------")
                f_src_abs = os.path.join(session_rootdir, f_src)
                #if os.path.samefile(f_src_abs, path_abs):
                print(f_src_abs)
                print(f_src)
                print(f_dst)
                if f_src_abs == path_abs:
                    f_dst_abs = os.path.join(cachedir, f_dst)
                    return f_src, f_src_abs, f_dst_abs
            return None, None, None

        # 2 passes, once to check, another to execute
        for pass_ in range(2):
            for path in paths:
                path = os.path.normpath(os.path.abspath(path))
                if os.path.isdir(path):
                    fatal("Reverting a directory not yet supported (%r)" % path)

                # possible we try revert different session's files
                session_rootdir = bam_config.find_sessiondir(path, abort=True)
                cachedir = os.path.join(bam_config.find_rootdir(cwd=session_rootdir, abort=True), ".cache")
                if not os.path.exists(cachedir):
                    fatal("Local cache missing (%r)" %
                          cachedir)

                path_rel = os.path.relpath(path, session_rootdir)

                with open(os.path.join(session_rootdir, ".bam_paths_uuid.json")) as fp:
                    paths_uuid = json.load(fp)
                    if paths_uuid.get(path_rel) is None:
                        fatal("Given path isn't in the session, skipping (%s)" %
                              path_abs)

                # first pass is sanity check only
                if pass_ == 0:
                    continue

                with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as fp:
                    paths_remap = json.load(fp)
                    paths_remap_relbase = paths_remap.get(".", "")
                    del fp, paths_remap

                path_cache = bam_session.session_path_to_cache(
                        path,
                        cachedir=cachedir,
                        session_rootdir=session_rootdir,
                        paths_remap_relbase=paths_remap_relbase,
                        )

                if not os.path.exists(path_cache):
                    fatal("Given path missing cache disk (%s)" %
                          path_cache)

                if pass_ == 1:
                    # for real
                    print("  Reverting %r" % path)
                    os.makedirs(os.path.dirname(path), exist_ok=True)
                    import shutil
                    shutil.copyfile(path_cache, path)

                    bam_session.binary_edits_apply_all(
                            session_rootdir,
                            paths={path_rel.encode('utf-8')},
                            update_uuid=False,
                            )

    @staticmethod
    def commit(paths, message):
        from bam.utils.system import write_json_to_file, write_json_to_zip
        import requests

        # Load project configuration
        cfg = bam_config.load(abort=True)

        session_rootdir = bam_config.find_sessiondir(paths[0], abort=True)

        cachedir = os.path.join(bam_config.find_rootdir(cwd=session_rootdir, abort=True), ".cache")
        basedir = bam_config.find_basedir(
                cwd=session_rootdir,
                descr="bam repository",
                )
        basedir_temp = os.path.join(basedir, "tmp")

        if os.path.isdir(basedir_temp):
            fatal("Path found, "
                  "another commit in progress, or remove with path! (%r)" %
                  basedir_temp)

        if not os.path.exists(os.path.join(session_rootdir, ".bam_paths_uuid.json")):
            fatal("Path not a project session, (%r)" %
                  session_rootdir)


        # make a zipfile from session
        paths_uuid = bam_session.load_paths_uuid(session_rootdir)

        # No longer used
        """
        with open(os.path.join(session_rootdir, ".bam_deps_remap.json")) as f:
            deps_remap = json.load(f)
        """

        paths_uuid_update = {}

        paths_add, paths_remove, paths_modified = bam_session.status(session_rootdir, paths_uuid_update)

        if not any((paths_add, paths_modified, paths_remove)):
            print("Nothing to commit!")
            return

        # we need to update paths_remap as we go
        with open(os.path.join(session_rootdir, ".bam_paths_remap.json")) as f:
            paths_remap = json.load(f)
            paths_remap_relbase = paths_remap.get(".", "")
            paths_remap_relbase_bytes = paths_remap_relbase.encode("utf-8")

        def remap_filepath_bytes(f_rel):
            assert(type(f_rel) is bytes)
            f_rel_in_proj = paths_remap.get(f_rel.decode("utf-8"))
            if f_rel_in_proj is None:
                if paths_remap_relbase_bytes:
                    if f_rel.startswith(b'_'):
                        f_rel_in_proj = f_rel[1:]
                    else:
                        f_rel_in_proj = os.path.join(paths_remap_relbase_bytes, f_rel)
                else:
                    if f_rel.startswith(b'_'):
                        # we're already project relative
                        f_rel_in_proj = f_rel[1:]
                    else:
                        f_rel_in_proj = f_rel
            else:
                f_rel_in_proj = f_rel_in_proj.encode("utf-8")
            return f_rel_in_proj

        def remap_filepath(f_rel):
            assert(type(f_rel) is str)
            f_rel_in_proj = paths_remap.get(f_rel)
            if f_rel_in_proj is None:
                if paths_remap_relbase:
                    if f_rel.startswith("_"):
                        f_rel_in_proj = f_rel[1:]
                    else:
                        f_rel_in_proj = os.path.join(paths_remap_relbase, f_rel)
                else:
                    if f_rel.startswith("_"):
                        # we're already project relative
                        f_rel_in_proj = f_rel[1:]
                    else:
                        f_rel_in_proj = f_rel

            return f_rel_in_proj

        def remap_cb(f, data):
            # check for the absolute path hint
            if f.startswith(b'//_'):
                proj_base_b = data
                return b'//' + os.path.relpath(f[3:], proj_base_b)
            return None

        def remap_file(f_rel, f_abs):
            f_abs_remap = os.path.join(basedir_temp, f_rel)
            dir_remap = os.path.dirname(f_abs_remap)
            os.makedirs(dir_remap, exist_ok=True)

            # final location in the project
            f_rel_in_proj = remap_filepath(f_rel)
            proj_base_b = os.path.dirname(f_rel_in_proj).encode("utf-8")

            from bam.blend import blendfile_pack_restore
            blendfile_pack_restore.blendfile_remap(
                    f_abs.encode('utf-8'),
                    dir_remap.encode('utf-8'),
                    deps_remap_cb=remap_cb,
                    deps_remap_cb_userdata=proj_base_b,
                    )
            return f_abs_remap

        for f_rel, f_abs in list(paths_modified.items()):
            if f_abs.endswith(".blend"):
                f_abs_remap = remap_file(f_rel, f_abs)
                if os.path.exists(f_abs_remap):
                    paths_modified[f_rel] = f_abs_remap

        for f_rel, f_abs in list(paths_add.items()):
            if f_abs.endswith(".blend"):
                f_abs_remap = remap_file(f_rel, f_abs)
                if os.path.exists(f_abs_remap):
                    paths_add[f_rel] = f_abs_remap

        """
                deps = deps_remap.get(f_rel)
                if deps:
                    # ----
                    # remap!
                    f_abs_remap = os.path.join(basedir_temp, f_rel)
                    dir_remap = os.path.dirname(f_abs_remap)
                    os.makedirs(dir_remap, exist_ok=True)
                    import blendfile_pack_restore
                    blendfile_pack_restore.blendfile_remap(
                            f_abs.encode('utf-8'),
                            dir_remap.encode('utf-8'),
                            deps,
                            )
                    if os.path.exists(f_abs_remap):
                        f_abs = f_abs_remap
                        paths_modified[f_rel] = f_abs
        """

        # -------------------------
        print("Now make a zipfile")
        import zipfile
        temp_zip = os.path.join(session_rootdir, ".bam_tmp.zip")
        with zipfile.ZipFile(temp_zip, 'w', zipfile.ZIP_DEFLATED) as zip_handle:
            for paths_dict, op in ((paths_modified, 'M'), (paths_add, 'A')):
                for (f_rel, f_abs) in paths_dict.items():
                    print("  packing (%s): %r" % (op, f_abs))
                    zip_handle.write(f_abs, arcname=f_rel)

            # make a paths remap that only includes modified files
            # TODO(cam), from 'packer.py'

            paths_remap_subset = {
                    f_rel: f_rel_in_proj
                    for f_rel, f_rel_in_proj in paths_remap.items() if f_rel in paths_modified}
            paths_remap_subset.update({
                    f_rel: remap_filepath(f_rel)
                    for f_rel in paths_add})

            # paths_remap_subset.update(paths_remap_subset_add)
            write_json_to_zip(zip_handle, ".bam_paths_remap.json", paths_remap_subset)

            # build a list of path manipulation operations
            paths_ops = {}
            # paths_remove ...
            for f_rel, f_abs in paths_remove.items():
                # TODO
                f_abs_remote = paths_remap[f_rel]
                paths_ops[f_abs_remote] = 'D'

            write_json_to_zip(zip_handle, ".bam_paths_ops.json", paths_ops)
            log.debug(paths_ops)


        # --------------
        # Commit Request
        payload = {
            "command": "commit",
            "arguments": json.dumps({
                'message': message,
                }),
            }
        files = {
            "file": open(temp_zip, 'rb'),
            }

        with files["file"]:
            r = requests.put(
                    bam_session.request_url("file"),
                    params=payload,
                    auth=(cfg["user"], cfg["password"]),
                    files=files)

        os.remove(temp_zip)

        try:
            r_json = r.json()
            print(r_json.get("message", "<empty>"))
        except Exception:
            print(r.text)

        # TODO, handle error cases
        ok = True
        if ok:

            # ----------
            # paths_uuid
            paths_uuid.update(paths_uuid_update)
            write_json_to_file(os.path.join(session_rootdir, ".bam_paths_uuid.json"), paths_uuid_update)

            # -----------
            # paths_remap
            paths_remap.update(paths_remap_subset)
            for k in paths_remove:
                del paths_remap[k]
            write_json_to_file(os.path.join(session_rootdir, ".bam_paths_remap.json"), paths_remap)
            del write_json_to_file

            # ------------------
            # Update Local Cache
            #
            # We now have 'pristine' files in basedir_temp, the commit went fine.
            # So move these into local cache AND we have to remake the binary_edit data.
            # since files were modified, if we don't do this - we wont be able to revert or avoid
            # re-downloading the files later.
            binary_edits_all_update = {}
            binary_edits_all_remove = set()
            for paths_dict, op in ((paths_modified, 'M'), (paths_add, 'A')):
                for f_rel, f_abs in paths_dict.items():
                    print("  caching (%s): %r" % (op, f_abs))
                    f_dst_abs = os.path.join(cachedir, f_rel)
                    os.makedirs(os.path.dirname(f_dst_abs), exist_ok=True)
                    if f_abs.startswith(basedir_temp):
                        os.rename(f_abs, f_dst_abs)
                    else:
                        import shutil
                        shutil.copyfile(f_abs, f_dst_abs)
                        del shutil
                    binary_edits = binary_edits_all_update[f_rel.encode('utf-8')] = []

                    # update binary_edits
                    if f_rel.endswith(".blend"):
                        bam_session.binary_edits_update_single(
                                f_dst_abs,
                                binary_edits,
                                remap_filepath_cb=remap_filepath_bytes,
                                )
            for f_rel, f_abs in paths_remove.items():
                binary_edits_all_remove.add(f_rel)

            paths_edit_abs = os.path.join(session_rootdir, ".bam_paths_edit.data")
            if binary_edits_all_update or binary_edits_all_remove:
                if os.path.exists(paths_edit_abs):
                    with open(paths_edit_abs, 'rb') as fh:
                        import pickle
                        binary_edits_all = pickle.load(fh)
                        del pickle
                else:
                    binary_edits_all = {}

                if binary_edits_all_remove and binary_edits_all:
                    for f_rel in binary_edits_all_remove:
                        if f_rel in binary_edits_all:
                            try:
                                del binary_edits_all[f_rel]
                            except KeyError:
                                pass
                if binary_edits_all_update:
                    binary_edits_all.update(binary_edits_all_update)

            import pickle
            with open(paths_edit_abs, 'wb') as fh:
                print()
                pickle.dump(binary_edits_all, fh, pickle.HIGHEST_PROTOCOL)
            del binary_edits_all
            del paths_edit_abs
            del pickle

        # ------------------------------
        # Cleanup temp dir to finish off
        if os.path.exists(basedir_temp):
            import shutil
            shutil.rmtree(basedir_temp)
            del shutil

    @staticmethod
    def status(paths, use_json=False):
        # TODO(cam) multiple paths
        path = paths[0]
        del paths

        session_rootdir = bam_config.find_sessiondir(path, abort=True)
        paths_add, paths_remove, paths_modified = bam_session.status(session_rootdir)

        if not use_json:
            for f in sorted(paths_add):
                print("  A: %s" % f)
            for f in sorted(paths_modified):
                print("  M: %s" % f)
            for f in sorted(paths_remove):
                print("  D: %s" % f)
        else:
            ret = []
            for f in sorted(paths_add):
                ret.append(("A", f))
            for f in sorted(paths_modified):
                ret.append(("M", f))
            for f in sorted(paths_remove):
                ret.append(("D", f))

            print(json.dumps(ret))

    @staticmethod
    def list_dir(paths, use_full=False, use_json=False):
        import requests

        # Load project configuration
        cfg = bam_config.load(abort=True)

        # TODO(cam) multiple paths
        path = paths[0]
        del paths

        payload = {
            "path": path,
            }
        r = requests.get(
                bam_session.request_url("file_list"),
                params=payload,
                auth=(cfg['user'], cfg['password']),
                stream=True,
                )

        r_json = r.json()
        items = r_json.get("items_list")
        if items is None:
            fatal(r_json.get("message", "<empty>"))

        items.sort()

        if use_json:
            ret = []
            for (name_short, name_full, file_type) in items:
                ret.append((name_short, file_type))

            print(json.dumps(ret))
        else:
            def strip_dot_slash(f):
                return f[2:] if f.startswith("./") else f

            for (name_short, name_full, file_type) in items:
                if file_type == "dir":
                    print("  %s/" % (strip_dot_slash(name_full) if use_full else name_short))
            for (name_short, name_full, file_type) in items:
                if file_type != "dir":
                    print("  %s" % (strip_dot_slash(name_full) if use_full else name_short))

    @staticmethod
    def deps(paths, recursive=False, use_json=False):

        def deps_path_walker():
            from bam.blend import blendfile_path_walker
            for blendfile_src in paths:
                blendfile_src = blendfile_src.encode('utf-8')
                yield from blendfile_path_walker.FilePath.visit_from_blend(
                        blendfile_src,
                        readonly=True,
                        recursive=recursive,
                        )

        def status_walker():
            for fp, (rootdir, fp_blend_basename) in deps_path_walker():
                f_rel = fp.filepath
                f_abs = fp.filepath_absolute

                yield (
                    # blendfile-src
                    os.path.join(fp.basedir, fp_blend_basename).decode('utf-8'),
                    # fillepath-dst
                    f_rel.decode('utf-8'),
                    f_abs.decode('utf-8'),
                    # filepath-status
                    "OK" if os.path.exists(f_abs) else "MISSING FILE",
                    )

        if use_json:
            is_first = True
            # print in parts, so we don't block the output
            print("[")
            for f_src, f_dst, f_dst_abs, f_status in status_walker():
                if is_first:
                    is_first = False
                else:
                    print(",")

                print(json.dumps((f_src, f_dst, f_dst_abs, f_status)), end="")
            print("]")
        else:
            for f_src, f_dst, f_dst_abs, f_status in status_walker():
                print("  %r -> (%r = %r) %s" % (f_src, f_dst, f_dst_abs, f_status))

    @staticmethod
    def pack(
            paths,
            output,
            mode,
            repository_base_path=None,
            all_deps=False,
            use_quiet=False,
            warn_remap_externals=False,
            compress_level=-1,
            filename_filter=None,
            ):
        # Local packing (don't use any project/session stuff)
        from .blend import blendfile_pack

        # TODO(cam) multiple paths
        path = paths[0]
        del paths

        if output is None:
            fatal("Output path must be given when packing with: --mode=FILE")

        if os.path.isdir(output):
            if mode == "ZIP":
                output = os.path.join(output, os.path.splitext(path)[0] + ".zip")
            else:  # FILE
                output = os.path.join(output, os.path.basename(path))

        if use_quiet:
            report = lambda msg: None
        else:
            report = lambda msg: print(msg, end="")

        if repository_base_path is not None:
            repository_base_path = repository_base_path.encode('utf-8')

        # replace var with a pattern matching callback
        filename_filter_cb = blendfile_pack.exclusion_filter(filename_filter)

        for msg in blendfile_pack.pack(
                path.encode('utf-8'),
                output.encode('utf-8'),
                mode=mode,
                all_deps=all_deps,
                repository_base_path=repository_base_path,
                compress_level=compress_level,
                report=report,
                warn_remap_externals=warn_remap_externals,
                use_variations=True,
                filename_filter=filename_filter_cb,
                ):
            pass

    @staticmethod
    def copy(
            paths,
            output,
            base,
            all_deps=False,
            use_quiet=False,
            filename_filter=None,
            ):
        # Local packing (don't use any project/session stuff)
        from .blend import blendfile_copy
        from bam.utils.system import is_subdir

        paths = [os.path.abspath(path) for path in paths]
        base = os.path.abspath(base)
        output = os.path.abspath(output)

        # check all blends are in the base path
        for path in paths:
            if not is_subdir(path, base):
                fatal("Input blend file %r is not a sub directory of %r" % (path, base))

        if use_quiet:
            report = lambda msg: None
        else:
            report = lambda msg: print(msg, end="")

        # replace var with a pattern matching callback
        if filename_filter:
            # convert string into regex callback
            # "*.txt;*.png;*.rst" --> r".*\.txt$|.*\.png$|.*\.rst$"
            import re
            import fnmatch

            compiled_pattern = re.compile(
                    b'|'.join(fnmatch.translate(f).encode('utf-8')
                              for f in filename_filter.split(";") if f),
                    re.IGNORECASE,
                    )

            def filename_filter(f):
                return (not filename_filter.compiled_pattern.match(f))
            filename_filter.compiled_pattern = compiled_pattern

            del compiled_pattern
            del re, fnmatch

        for msg in blendfile_copy.copy_paths(
                [path.encode('utf-8') for path in paths],
                output.encode('utf-8'),
                base.encode('utf-8'),
                all_deps=all_deps,
                report=report,
                filename_filter=filename_filter,
                ):
            pass

    @staticmethod
    def remap_start(
            paths,
            use_json=False,
            ):
        filepath_remap = "bam_remap.data"

        for p in paths:
            if not os.path.exists(p):
                fatal("Path %r not found!" % p)
        paths = [p.encode('utf-8') for p in paths]

        if os.path.exists(filepath_remap):
            fatal("Remap in progress, run with 'finish' or remove %r" % filepath_remap)

        from bam.blend import blendfile_path_remap
        remap_data = blendfile_path_remap.start(
                paths,
                use_json=use_json,
                )

        with open(filepath_remap, 'wb') as fh:
            import pickle
            pickle.dump(remap_data, fh, pickle.HIGHEST_PROTOCOL)
            del pickle

    @staticmethod
    def remap_finish(
            paths,
            force_relative=False,
            dry_run=False,
            use_json=False,
            ):
        filepath_remap = "bam_remap.data"

        for p in paths:
            if not os.path.exists(p):
                fatal("Path %r not found!" % p)
        # bytes needed for blendfile_path_remap API
        paths = [p.encode('utf-8') for p in paths]

        if not os.path.exists(filepath_remap):
            fatal("Remap not started, run with 'start', (%r not found)" % filepath_remap)

        with open(filepath_remap, 'rb') as fh:
            import pickle
            remap_data = pickle.load(fh)
            del pickle

        from bam.blend import blendfile_path_remap
        blendfile_path_remap.finish(
                paths, remap_data,
                force_relative=force_relative,
                dry_run=dry_run,
                use_json=use_json,
                )

        if not dry_run:
            os.remove(filepath_remap)

    @staticmethod
    def remap_reset(
            use_json=False,
            ):
        filepath_remap = "bam_remap.data"
        if os.path.exists(filepath_remap):
            os.remove(filepath_remap)
        else:
            fatal("remapping not started, nothing to do!")


# -----------------------------------------------------------------------------
# Argument Parser

def init_argparse_common(
        subparse,
        use_json=False,
        use_all_deps=False,
        use_quiet=False,
        use_compress_level=False,
        use_exclude=False,
        ):
    import argparse

    if use_json:
        subparse.add_argument(
                "-j", "--json", dest="json", action='store_true',
                help="Generate JSON output",
                )
    if use_all_deps:
        subparse.add_argument(
                "-a", "--all-deps", dest="all_deps", action='store_true',
                help="Follow all dependencies (unused indirect dependencies too)",
                )
    if use_quiet:
        subparse.add_argument(
                "-q", "--quiet", dest="use_quiet", action='store_true',
                help="Suppress status output",
                )
    if use_compress_level:
        class ChoiceToZlibLevel(argparse.Action):
            def __call__(self, parser, namespace, value, option_string=None):
                setattr(namespace, self.dest, {"default": -1, "fast": 1, "best": 9, "store": 0}[value[0]])

        subparse.add_argument(
                "-c", "--compress", dest="compress_level", nargs=1, default=-1, metavar='LEVEL',
                action=ChoiceToZlibLevel,
                choices=('default', 'fast', 'best', 'store'),
                help="Compression level for resulting archive",
                )
    if use_exclude:
        subparse.add_argument(
                "-e", "--exclude", dest="exclude", metavar='PATTERN(S)', required=False,
                default="",
                help="""
                Optionally exclude files from the pack.

                Using Unix shell-style wildcards *(case insensitive)*.
                ``--exclude="*.png"``

                Multiple patterns can be passed using the  ``;`` separator.
                ``--exclude="*.txt;*.avi;*.wav"``
                """
                )


def create_argparse_init(subparsers):
    subparse = subparsers.add_parser("init",
            help="Initialize a new project directory")
    subparse.add_argument(
            dest="url",
            help="Project repository url",
            )
    subparse.add_argument(
            dest="directory_name", nargs="?",
            help="Directory name",
            )
    subparse.set_defaults(
            func=lambda args:
            bam_commands.init(args.url, args.directory_name),
            )


def create_argparse_create(subparsers):
    subparse = subparsers.add_parser(
            "create", aliases=("cr",),
            help="Create a new empty session directory",
            )
    subparse.add_argument(
            dest="session_name", nargs=1,
            help="Name of session directory",
            )
    subparse.set_defaults(
            func=lambda args:
            bam_commands.create(args.session_name[0]),
            )


def create_argparse_checkout(subparsers):
    subparse = subparsers.add_parser(
            "checkout", aliases=("co",),
            help="Checkout a remote path in an existing project",
            )
    subparse.add_argument(
            dest="path", type=str, metavar='REMOTE_PATH',
            help="Path to checkout on the server",
            )
    subparse.add_argument(
            "-o", "--output", dest="output", type=str, metavar='DIRNAME',
            help="Local name to checkout the session into (optional, falls back to path name)",
            )

    init_argparse_common(subparse, use_all_deps=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.checkout(args.path, args.output, args.all_deps),
            )


def create_argparse_update(subparsers):
    subparse = subparsers.add_parser(
            "update", aliases=("up",),
            help="Update a local session with changes from the remote project",
            )
    subparse.add_argument(
            dest="paths", nargs="*",
            help="Path(s) to operate on",
            )
    subparse.set_defaults(
            func=lambda args:
            bam_commands.update(args.paths or ["."]),
            )


def create_argparse_revert(subparsers):
    subparse = subparsers.add_parser(
            "revert", aliases=("rv",),
            help="Reset local changes back to the state at time of checkout",
            )
    subparse.add_argument(
            dest="paths", nargs="+",
            help="Path(s) to operate on",
            )
    subparse.set_defaults(
            func=lambda args:
            bam_commands.revert(args.paths or ["."]),
            )


def create_argparse_commit(subparsers):
    subparse = subparsers.add_parser(
            "commit", aliases=("ci",),
            help="Commit changes from a session to the remote project",
            )
    subparse.add_argument(
            "-m", "--message", dest="message", metavar='MESSAGE',
            required=True,
            help="Commit message",
            )
    subparse.add_argument(
            dest="paths", nargs="*",
            help="paths to commit",
            )
    subparse.set_defaults(
            func=lambda args:
            bam_commands.commit(args.paths or ["."], args.message),
            )


def create_argparse_status(subparsers):
    subparse = subparsers.add_parser(
            "status", aliases=("st",),
            help="Show any edits made in the local session",
            )
    subparse.add_argument(
            dest="paths", nargs="*",
            help="Path(s) to operate on",
            )

    init_argparse_common(subparse, use_json=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.status(args.paths or ["."], use_json=args.json),
            )


def create_argparse_list(subparsers):
    subparse = subparsers.add_parser(
            "list", aliases=("ls",),
            help="List the contents of a remote directory",
            )
    subparse.add_argument(
            dest="paths", nargs="*",
            help="Path(s) to operate on",
            )
    subparse.add_argument(
            "-f", "--full", dest="full", action='store_true',
            help="Show the full paths",
            )

    init_argparse_common(subparse, use_json=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.list_dir(
                    args.paths or ["."],
                    use_full=args.full,
                    use_json=args.json,
                    ),
                    )


def create_argparse_deps(subparsers):
    subparse = subparsers.add_parser(
            "deps", aliases=("dp",),
            help="List dependencies for file(s)",
            )
    subparse.add_argument(
            dest="paths", nargs="+",
            help="Path(s) to operate on",
            )
    subparse.add_argument(
            "-r", "--recursive", dest="recursive", action='store_true',
            help="Scan dependencies recursively",
            )

    init_argparse_common(subparse, use_json=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.deps(
                    args.paths, args.recursive,
                    use_json=args.json),
                    )


def create_argparse_pack(subparsers):
    import argparse
    subparse = subparsers.add_parser(
            "pack", aliases=("pk",),
            help="Pack a blend file and its dependencies into an archive",
            description=
    """
    You can simply pack a blend file like this to create a zip-file of the same name.

    .. code-block:: sh

       bam pack /path/to/scene.blend

    You may also want to give an explicit output directory.

    This command is used for packing a ``.blend`` file into a ``.zip`` file for redistribution.

    .. code-block:: sh

       # pack a blend with maximum compression for online downloads
       bam pack /path/to/scene.blend --output my_scene.zip --compress=best

    You may also pack a .blend while keeping your whole repository hierarchy by passing
    the path to the top directory of the repository, and ask to be warned about dependencies paths
    outside of that base path:

    .. code-block:: sh

       bam pack --repo="/path/to/repo" --warn-external /path/to/repo/path/to/scene.blend

    """,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            )
    subparse.add_argument(
            dest="paths", nargs="+",
            help="Path(s) to operate on",
            )
    subparse.add_argument(
            "-o", "--output", dest="output", metavar='FILE', required=False,
            help="Output file or a directory when multiple inputs are passed",
            )
    subparse.add_argument(
            "-m", "--mode", dest="mode", metavar='MODE', required=False,
            default='ZIP',
            choices=('ZIP', 'FILE'),
            help="Output file or a directory when multiple inputs are passed",
            )
    subparse.add_argument(
            "--repo", dest="repository_base_path", metavar='DIR', required=False,
            help="Base directory from which you want to keep existing hierarchy (usually to repository directory),"
                 "will default to packed blend file's directory if not specified",
            )
    subparse.add_argument(
            "--warn-external", dest="warn_remap_externals", action='store_true',
            help="Warn for every dependency outside of given repository base path",
            )

    init_argparse_common(subparse, use_all_deps=True, use_quiet=True, use_compress_level=True, use_exclude=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.pack(
                    args.paths,
                    args.output or
                    ((os.path.splitext(args.paths[0])[0] + ".zip")
                     if args.mode == 'ZIP' else None),
                    args.mode,
                    repository_base_path=args.repository_base_path or None,
                    all_deps=args.all_deps,
                    use_quiet=args.use_quiet,
                    warn_remap_externals=args.warn_remap_externals,
                    compress_level=args.compress_level,
                    filename_filter=args.exclude,
                    ),
            )


def create_argparse_copy(subparsers):
    import argparse
    subparse = subparsers.add_parser(
            "copy", aliases=("cp",),
            help="Copy blend file(s) and their dependencies to a new location (maintaining the directory structure).",
            description=
    """
    The line below will copy ``scene.blend`` to ``/destination/to/scene.blend``.

    .. code-block:: sh

       bam copy /path/to/scene.blend --base=/path --output=/destination

    .. code-block:: sh

       # you can also copy multiple files
       bam copy /path/to/scene.blend /path/other/file.blend --base=/path --output /other/destination
    """,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            )
    subparse.add_argument(
            dest="paths", nargs="+",
            help="Path(s) to blend files to operate on",
            )
    subparse.add_argument(
            "-o", "--output", dest="output", metavar='DIR', required=True,
            help="Output directory where where files will be copied to",
            )
    subparse.add_argument(
            "-b", "--base", dest="base", metavar='DIR', required=True,
            help="Base directory for input paths (files outside this path will be omitted)",
            )

    init_argparse_common(subparse, use_all_deps=True, use_quiet=True, use_exclude=True)

    subparse.set_defaults(
            func=lambda args:
            bam_commands.copy(
                    args.paths,
                    args.output,
                    args.base,
                    all_deps=args.all_deps,
                    use_quiet=args.use_quiet,
                    filename_filter=args.exclude,
                    ),
            )


def create_argparse_remap(subparsers):
    import argparse

    subparse = subparsers.add_parser(
            "remap",
            help="Remap blend file paths",
            description=
    """
    This command is a 3 step process:

    - first run ``bam remap start .`` which stores the current state of your project (recursively).
    - then re-arrange the files on the filesystem (rename, relocate).
    - finally run ``bam remap finish`` to apply the changes, updating the ``.blend`` files internal paths.


    .. code-block:: sh

       cd /my/project

       bam remap start .
       mv photos textures
       mv house_v14_library.blend house_libraray.blend
       bam remap finish

    .. note::

       Remapping creates a file called ``bam_remap.data`` in the current directory.
       You can relocate the entire project to a new location but on executing ``finish``,
       this file must be accessible from the current directory.

    .. note::

       This command depends on files unique contents,
       take care not to modify the files once remap is started.
    """,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            )

    subparse_remap_commands = subparse.add_subparsers(
            title="Remap commands",
            description='valid subcommands',
            help='additional help',
            )
    sub_subparse = subparse_remap_commands.add_parser(
            "start",
            help="Start remapping the blend files",
            )

    sub_subparse.add_argument(
            dest="paths", nargs="*",
            help="Path(s) to operate on",
            )
    init_argparse_common(sub_subparse, use_json=True)

    sub_subparse.set_defaults(
            func=lambda args:
            bam_commands.remap_start(
                    args.paths or ["."],
                    use_json=args.json,
                    ),
                    )

    sub_subparse = subparse_remap_commands.add_parser(
            "finish",
            help="Finish remapping the blend files",
            )
    sub_subparse.add_argument(
            dest="paths", nargs="*",
            help="Path(s) to operate on",
            )
    sub_subparse.add_argument(
            "-r", "--force-relative", dest="force_relative", action='store_true',
            help="Make all remapped paths relative (even if they were originally absolute)",
            )
    sub_subparse.add_argument(
            "-d", "--dry-run", dest="dry_run", action='store_true',
            help="Just print output as if the paths are being run",
            )
    init_argparse_common(sub_subparse, use_json=True)

    sub_subparse.set_defaults(
            func=lambda args:
            bam_commands.remap_finish(
                    args.paths or ["."],
                    force_relative=args.force_relative,
                    dry_run=args.dry_run,
                    use_json=args.json,
                    ),
                    )

    sub_subparse = subparse_remap_commands.add_parser(
            "reset",
            help="Cancel path remapping",
            )
    init_argparse_common(sub_subparse, use_json=True)

    sub_subparse.set_defaults(
            func=lambda args:
            bam_commands.remap_reset(
                    use_json=args.json,
                    ),
            )


def create_argparse():
    import argparse

    usage_text = (
        "BAM!\n" +
        __doc__
        )

    parser = argparse.ArgumentParser(
            prog="bam",
            description=usage_text,
            )

    subparsers = parser.add_subparsers(
            title='subcommands',
            description='valid subcommands',
            help='additional help',
            )

    create_argparse_init(subparsers)
    create_argparse_create(subparsers)
    create_argparse_checkout(subparsers)
    create_argparse_commit(subparsers)
    create_argparse_update(subparsers)
    create_argparse_revert(subparsers)
    create_argparse_status(subparsers)
    create_argparse_list(subparsers)

    # non-bam project commands
    create_argparse_deps(subparsers)
    create_argparse_pack(subparsers)
    create_argparse_copy(subparsers)
    create_argparse_remap(subparsers)

    return parser


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)-15s %(levelname)8s %(name)s %(message)s',
    )

    parser = create_argparse()
    args = parser.parse_args(argv)

    # call subparser callback
    if not hasattr(args, "func"):
        parser.print_help()
        return

    args.func(args)


if __name__ == "__main__":
    raise Exception("This module can't be executed directly, Call '../bam_cli.py'")
