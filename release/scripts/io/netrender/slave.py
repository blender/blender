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
import http, http.client, http.server, urllib
import subprocess, time

from netrender.utils import *
import netrender.model

BLENDER_PATH = sys.argv[0]

CANCEL_POLL_SPEED = 2
MAX_TIMEOUT = 10
INCREMENT_TIMEOUT = 1

if platform.system() == 'Windows' and platform.version() >= '5': # Error mode is only available on Win2k or higher, that's version 5
    import ctypes
    def SetErrorMode():
        val = ctypes.windll.kernel32.SetErrorMode(0x0002)
        ctypes.windll.kernel32.SetErrorMode(val | 0x0002)
        return val

    def RestoreErrorMode(val):
        ctypes.windll.kernel32.SetErrorMode(val)
else:
    def SetErrorMode():
        return 0

    def RestoreErrorMode(val):
        pass

def clearSlave(path):
    shutil.rmtree(path)

def slave_Info():
    sysname, nodename, release, version, machine, processor = platform.uname()
    slave = netrender.model.RenderSlave()
    slave.name = nodename
    slave.stats = sysname + " " + release + " " + machine + " " + processor
    return slave

def testCancel(conn, job_id, frame_number):
        conn.request("HEAD", "/status", headers={"job-id":job_id, "job-frame": str(frame_number)})

        # cancelled if job isn't found anymore
        if conn.getresponse().status == http.client.NO_CONTENT:
            return True
        else:
            return False

def testFile(conn, job_id, slave_id, rfile, JOB_PREFIX, main_path = None):
    job_full_path = prefixPath(JOB_PREFIX, rfile.filepath, main_path)
    
    found = os.path.exists(job_full_path)
    
    if found:
        found_signature = hashFile(job_full_path)
        found = found_signature == rfile.signature
        
        if not found:
            print("Found file %s at %s but signature mismatch!" % (rfile.filepath, job_full_path))

    if not found:
        temp_path = JOB_PREFIX + "slave.temp.blend"
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

    return job_full_path

def render_slave(engine, netsettings, threads):
    timeout = 1

    engine.update_stats("", "Network render node initiation")

    conn = clientConnection(netsettings.server_address, netsettings.server_port)

    if conn:
        conn.request("POST", "/slave", repr(slave_Info().serialize()))
        response = conn.getresponse()

        slave_id = response.getheader("slave-id")

        NODE_PREFIX = netsettings.path + "slave_" + slave_id + os.sep
        if not os.path.exists(NODE_PREFIX):
            os.mkdir(NODE_PREFIX)

        engine.update_stats("", "Network render connected to master, waiting for jobs")

        while not engine.test_break():
            conn.request("GET", "/job", headers={"slave-id":slave_id})
            response = conn.getresponse()

            if response.status == http.client.OK:
                timeout = 1 # reset timeout on new job

                job = netrender.model.RenderJob.materialize(eval(str(response.read(), encoding='utf8')))
                engine.update_stats("", "Network render processing job from master")

                JOB_PREFIX = NODE_PREFIX + "job_" + job.id + os.sep
                if not os.path.exists(JOB_PREFIX):
                    os.mkdir(JOB_PREFIX)


                if job.type == netrender.model.JOB_BLENDER:
                    job_path = job.files[0].filepath # path of main file
                    main_path, main_file = os.path.split(job_path)

                    job_full_path = testFile(conn, job.id, slave_id, job.files[0], JOB_PREFIX)
                    print("Fullpath", job_full_path)
                    print("File:", main_file, "and %i other files" % (len(job.files) - 1,))
                    engine.update_stats("", "Render File "+ main_file+ " for job "+ job.id)

                    for rfile in job.files[1:]:
                        print("\t", rfile.filepath)
                        testFile(conn, job.id, slave_id, rfile, JOB_PREFIX, main_path)

                # announce log to master
                logfile = netrender.model.LogFile(job.id, slave_id, [frame.number for frame in job.frames])
                conn.request("POST", "/log", bytes(repr(logfile.serialize()), encoding='utf8'))
                response = conn.getresponse()


                first_frame = job.frames[0].number

                # start render
                start_t = time.time()

                if job.type == netrender.model.JOB_BLENDER:
                    frame_args = []

                    for frame in job.frames:
                        print("frame", frame.number)
                        frame_args += ["-f", str(frame.number)]

                    val = SetErrorMode()
                    process = subprocess.Popen([BLENDER_PATH, "-b", "-noaudio", job_full_path, "-t", str(threads), "-o", JOB_PREFIX + "######", "-E", "BLENDER_RENDER", "-F", "MULTILAYER"] + frame_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    RestoreErrorMode(val)
                elif job.type == netrender.model.JOB_PROCESS:
                    command = job.frames[0].command
                    val = SetErrorMode()
                    process = subprocess.Popen(command.split(" "), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    RestoreErrorMode(val)

                headers = {"slave-id":slave_id}

                cancelled = False
                stdout = bytes()
                run_t = time.time()
                while not cancelled and process.poll() == None:
                    stdout += process.stdout.read(1024)
                    current_t = time.time()
                    cancelled = engine.test_break()
                    if current_t - run_t > CANCEL_POLL_SPEED:

                        # update logs if needed
                        if stdout:
                            # (only need to update on one frame, they are linked
                            conn.request("PUT", logURL(job.id, first_frame), stdout, headers=headers)
                            response = conn.getresponse()
                            
                            # Also output on console
                            if netsettings.slave_thumb:
                                print(str(stdout, encoding='utf8'), end="")

                            stdout = bytes()

                        run_t = current_t
                        if testCancel(conn, job.id, first_frame):
                            cancelled = True

                # read leftovers if needed
                stdout += process.stdout.read()

                if cancelled:
                    # kill process if needed
                    if process.poll() == None:
                        process.terminate()
                    continue # to next frame

                # flush the rest of the logs
                if stdout:
                    # Also output on console
                    if netsettings.slave_thumb:
                        print(str(stdout, encoding='utf8'), end="")
                    
                    # (only need to update on one frame, they are linked
                    conn.request("PUT", logURL(job.id, first_frame), stdout, headers=headers)
                    if conn.getresponse().status == http.client.NO_CONTENT:
                        continue

                total_t = time.time() - start_t

                avg_t = total_t / len(job.frames)

                status = process.returncode

                print("status", status)

                headers = {"job-id":job.id, "slave-id":slave_id, "job-time":str(avg_t)}


                if status == 0: # non zero status is error
                    headers["job-result"] = str(DONE)
                    for frame in job.frames:
                        headers["job-frame"] = str(frame.number)
                        if job.type == netrender.model.JOB_BLENDER:
                            # send image back to server

                            filename = JOB_PREFIX + "%06d" % frame.number + ".exr"

                            # thumbnail first
                            if netsettings.slave_thumb:
                                thumbname = thumbnail(filename)

                                f = open(thumbname, 'rb')
                                conn.request("PUT", "/thumb", f, headers=headers)
                                f.close()
                                conn.getresponse()

                            f = open(filename, 'rb')
                            conn.request("PUT", "/render", f, headers=headers)
                            f.close()
                            if conn.getresponse().status == http.client.NO_CONTENT:
                                continue

                        elif job.type == netrender.model.JOB_PROCESS:
                            conn.request("PUT", "/render", headers=headers)
                            if conn.getresponse().status == http.client.NO_CONTENT:
                                continue
                else:
                    headers["job-result"] = str(ERROR)
                    for frame in job.frames:
                        headers["job-frame"] = str(frame.number)
                        # send error result back to server
                        conn.request("PUT", "/render", headers=headers)
                        if conn.getresponse().status == http.client.NO_CONTENT:
                            continue

                engine.update_stats("", "Network render connected to master, waiting for jobs")
            else:
                if timeout < MAX_TIMEOUT:
                    timeout += INCREMENT_TIMEOUT

                for i in range(timeout):
                    time.sleep(1)
                    if engine.test_break():
                        break

        conn.close()

        if netsettings.slave_clear:
            clearSlave(NODE_PREFIX)

if __name__ == "__main__":
    pass
