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

import sys, os, platform, shutil
import http, http.client, http.server
import subprocess, time, threading
import json

import bpy

from netrender.utils import *
import netrender.model
import netrender.repath
import netrender.baking
import netrender.thumbnail as thumbnail


CANCEL_POLL_SPEED = 2
MAX_TIMEOUT = 10
INCREMENT_TIMEOUT = 1
MAX_CONNECT_TRY = 10

def clearSlave(path):
    shutil.rmtree(path)

def slave_Info(netsettings):
    sysname, nodename, release, version, machine, processor = platform.uname()
    slave = netrender.model.RenderSlave()
    slave.name = nodename
    slave.stats = sysname + " " + release + " " + machine + " " + processor
    if netsettings.slave_tags:
        slave.tags = set(netsettings.slave_tags.split(";"))

    if netsettings.slave_bake:
        slave.tags.add(netrender.model.TAG_BAKING)

    if netsettings.slave_render:
        slave.tags.add(netrender.model.TAG_RENDER)

    return slave

def testCancel(conn, job_id, frame_number):
        with ConnectionContext():
            conn.request("HEAD", "/status", headers={"job-id":job_id, "job-frame": str(frame_number)})

        # canceled if job isn't found anymore
        if responseStatus(conn) == http.client.NO_CONTENT:
            return True
        else:
            return False

def testFile(conn, job_id, slave_id, rfile, job_prefix, main_path=None):
    job_full_path = createLocalPath(rfile, job_prefix, main_path, rfile.force)

    found = os.path.exists(job_full_path)

    if found and rfile.signature is not None:
        found_signature = hashFile(job_full_path)
        found = found_signature == rfile.signature

        if not found:
            print("Found file %s at %s but signature mismatch!" % (rfile.filepath, job_full_path))
            os.remove(job_full_path)

    if not found:
        # Force prefix path if not found
        job_full_path = createLocalPath(rfile, job_prefix, main_path, True)
        print("Downloading", job_full_path)
        temp_path = os.path.join(job_prefix, "slave.temp")
        with ConnectionContext():
            conn.request("GET", fileURL(job_id, rfile.index), headers={"slave-id":slave_id})
        response = conn.getresponse()

        if response.status != http.client.OK:
            return None # file for job not returned by server, need to return an error code to server

        f = open(temp_path, "wb")
        buf = response.read(1024)

        while buf:
            f.write(buf)
            buf = response.read(1024)

        f.close()

        os.renames(temp_path, job_full_path)

    rfile.filepath = job_full_path

    return job_full_path

def breakable_timeout(timeout):
    for i in range(timeout):
        time.sleep(1)
        if engine.test_break():
            break

def render_slave(engine, netsettings, threads):
    bisleep = BreakableIncrementedSleep(INCREMENT_TIMEOUT, 1, MAX_TIMEOUT, engine.test_break)

    engine.update_stats("", "Network render node initiation")

    slave_path = bpy.path.abspath(netsettings.path)

    if not os.path.exists(slave_path):
        print("Slave working path ( %s ) doesn't exist" % netsettings.path)
        return

    if not os.access(slave_path, os.W_OK):
        print("Slave working path ( %s ) is not writable" % netsettings.path)
        return

    conn = clientConnection(netsettings)

    if not conn:
        print("Connection failed, will try connecting again at most %i times" % MAX_CONNECT_TRY)
        bisleep.reset()

        for i in range(MAX_CONNECT_TRY):
            bisleep.sleep()

            conn = clientConnection(netsettings)

            if conn or engine.test_break():
                break

            print("Retry %i failed, waiting %is before retrying" % (i + 1, bisleep.current))

    if conn:
        with ConnectionContext():
            conn.request("POST", "/slave", json.dumps(slave_Info(netsettings).serialize()))
        response = conn.getresponse()
        response.read()

        slave_id = response.getheader("slave-id")

        NODE_PREFIX = os.path.join(slave_path, "slave_" + slave_id)
        verifyCreateDir(NODE_PREFIX)

        engine.update_stats("", "Network render connected to master, waiting for jobs")

        while not engine.test_break():
            with ConnectionContext():
                conn.request("GET", "/job", headers={"slave-id":slave_id})
            response = conn.getresponse()

            if response.status == http.client.OK:
                bisleep.reset()

                job = netrender.model.RenderJob.materialize(json.loads(str(response.read(), encoding='utf8')))
                engine.update_stats("", "Network render processing job from master")

                job_prefix = os.path.join(NODE_PREFIX, "job_" + job.id)
                verifyCreateDir(job_prefix)

                # set tempdir for fsaa temp files
                # have to set environ var because render is done in a subprocess and that's the easiest way to propagate the setting
                os.environ["TMP"] = job_prefix


                if job.type == netrender.model.JOB_BLENDER:
                    job_path = job.files[0].original_path # original path of the first file
                    main_path, main_file = os.path.split(job_path)

                    job_full_path = testFile(conn, job.id, slave_id, job.files[0], job_prefix)
                    print("Fullpath", job_full_path)
                    print("File:", main_file, "and %i other files" % (len(job.files) - 1,))

                    for rfile in job.files[1:]:
                        testFile(conn, job.id, slave_id, rfile, job_prefix, main_path)
                        print("\t", rfile.filepath)

                    netrender.repath.update(job)

                    engine.update_stats("", "Render File " + main_file + " for job " + job.id)
                elif job.type == netrender.model.JOB_VCS:
                    if not job.version_info:
                        # Need to return an error to server, incorrect job type
                        pass

                    job_path = job.files[0].filepath # path of main file
                    main_path, main_file = os.path.split(job_path)

                    job.version_info.update()

                    # For VCS jobs, file path is relative to the working copy path
                    job_full_path = os.path.join(job.version_info.wpath, job_path)

                    engine.update_stats("", "Render File " + main_file + " for job " + job.id)

                # announce log to master
                logfile = netrender.model.LogFile(job.id, slave_id, [frame.number for frame in job.frames])
                with ConnectionContext():
                    conn.request("POST", "/log", bytes(json.dumps(logfile.serialize()), encoding='utf8'))
                response = conn.getresponse()
                response.read()


                first_frame = job.frames[0].number

                # start render
                start_t = time.time()

                if job.rendersWithBlender():
                    frame_args = []

                    for frame in job.frames:
                        print("frame", frame.number)
                        frame_args += ["-f", str(frame.number)]

                    with NoErrorDialogContext():
                        process = subprocess.Popen(
                            [bpy.app.binary_path,
                             "-b",
                             "-y",
                             "-noaudio",
                             job_full_path,
                             "-t", str(threads),
                             "-o", os.path.join(job_prefix, "######"),
                             "-E", job.render,
                             "-F", "MULTILAYER",
                             ] + frame_args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            )

                elif job.subtype == netrender.model.JOB_SUB_BAKING:
                    tasks = []
                    for frame in job.frames:
                        tasks.append(netrender.baking.commandToTask(frame.command))

                    with NoErrorDialogContext():
                        process = netrender.baking.bake(job, tasks)

                elif job.type == netrender.model.JOB_PROCESS:
                    command = job.frames[0].command
                    with NoErrorDialogContext():
                        process = subprocess.Popen(command.split(" "), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

                headers = {"slave-id":slave_id}

                results = []

                line = ""

                class ProcessData:
                    def __init__(self):
                        self.lock = threading.Lock()
                        self.stdout = bytes()
                        self.cancelled = False
                        self.start_time = time.time()
                        self.last_time = time.time()

                data = ProcessData()

                def run_process(process, data):
                    while not data.cancelled and process.poll() is None:
                        buf = process.stdout.read(1024)

                        data.lock.acquire()
                        data.stdout += buf
                        data.lock.release()

                process_thread = threading.Thread(target=run_process, args=(process, data))

                process_thread.start()

                while not data.cancelled and process_thread.is_alive():
                    time.sleep(CANCEL_POLL_SPEED / 2)
                    current_time = time.time()
                    data.cancelled = engine.test_break()
                    if current_time - data.last_time > CANCEL_POLL_SPEED:

                        data.lock.acquire()

                        # update logs if needed
                        if data.stdout:
                            # (only need to update on one frame, they are linked
                            with ConnectionContext():
                                conn.request("PUT", logURL(job.id, first_frame), data.stdout, headers=headers)
                            responseStatus(conn)

                            stdout_text = str(data.stdout, encoding='utf8')

                            # Also output on console
                            if netsettings.use_slave_output_log:
                                print(stdout_text, end="")

                            lines = stdout_text.split("\n")
                            lines[0] = line + lines[0]
                            line = lines.pop()
                            if job.subtype == netrender.model.JOB_SUB_BAKING:
                                results.extend(netrender.baking.resultsFromOuput(lines))

                            data.stdout = bytes()

                        data.lock.release()

                        data.last_time = current_time
                        if testCancel(conn, job.id, first_frame):
                            engine.update_stats("", "Job canceled by Master")
                            data.cancelled = True

                process_thread.join()
                del process_thread

                if job.type == netrender.model.JOB_BLENDER:
                    netrender.repath.reset(job)

                if data.cancelled:
                    # kill process if needed
                    if process.poll() is None:
                        try:
                            process.terminate()
                        except OSError:
                            pass
                    continue # to next frame

                # read leftovers if needed
                data.stdout += process.stdout.read()

                # flush the rest of the logs
                if data.stdout:
                    stdout_text = str(data.stdout, encoding='utf8')

                    # Also output on console
                    if netsettings.use_slave_output_log:
                        print(stdout_text, end="")

                    lines = stdout_text.split("\n")
                    lines[0] = line + lines[0]
                    if job.subtype == netrender.model.JOB_SUB_BAKING:
                        results.extend(netrender.baking.resultsFromOuput(lines))

                    # (only need to update on one frame, they are linked
                    with ConnectionContext():
                        conn.request("PUT", logURL(job.id, first_frame), data.stdout, headers=headers)

                    if responseStatus(conn) == http.client.NO_CONTENT:
                        continue

                total_t = time.time() - data.start_time

                avg_t = total_t / len(job.frames)

                status = process.returncode

                print("status", status)

                headers = {"job-id":job.id, "slave-id":slave_id, "job-time":str(avg_t)}


                if status == 0: # non zero status is error
                    headers["job-result"] = str(netrender.model.FRAME_DONE)
                    for frame in job.frames:
                        headers["job-frame"] = str(frame.number)
                        if job.hasRenderResult():
                            # send image back to server

                            filename = os.path.join(job_prefix, "%06d.exr" % frame.number)

                            # thumbnail first
                            if netsettings.use_slave_thumb:
                                thumbname = thumbnail.generate(filename)

                                if thumbname:
                                    f = open(thumbname, 'rb')
                                    with ConnectionContext():
                                        conn.request("PUT", "/thumb", f, headers=headers)
                                    f.close()
                                    responseStatus(conn)

                            f = open(filename, 'rb')
                            with ConnectionContext():
                                conn.request("PUT", "/render", f, headers=headers)
                            f.close()
                            if responseStatus(conn) == http.client.NO_CONTENT:
                                continue

                        elif job.subtype == netrender.model.JOB_SUB_BAKING:
                            index = job.frames.index(frame)

                            frame_results = [result_filepath for task_index, result_filepath in results if task_index == index]

                            for result_filepath in frame_results:
                                result_path, result_filename = os.path.split(result_filepath)
                                headers["result-filename"] = result_filename
                                headers["job-finished"] = str(result_filepath == frame_results[-1])

                                f = open(result_filepath, 'rb')
                                with ConnectionContext():
                                    conn.request("PUT", "/result", f, headers=headers)
                                f.close()
                                if responseStatus(conn) == http.client.NO_CONTENT:
                                    continue

                        elif job.type == netrender.model.JOB_PROCESS:
                            with ConnectionContext():
                                conn.request("PUT", "/render", headers=headers)
                            if responseStatus(conn) == http.client.NO_CONTENT:
                                continue
                else:
                    headers["job-result"] = str(netrender.model.FRAME_ERROR)
                    for frame in job.frames:
                        headers["job-frame"] = str(frame.number)
                        # send error result back to server
                        with ConnectionContext():
                            conn.request("PUT", "/render", headers=headers)
                        if responseStatus(conn) == http.client.NO_CONTENT:
                            continue

                engine.update_stats("", "Network render connected to master, waiting for jobs")
            else:
                bisleep.sleep()

        conn.close()

        if netsettings.use_slave_clear:
            clearSlave(NODE_PREFIX)

if __name__ == "__main__":
    pass
