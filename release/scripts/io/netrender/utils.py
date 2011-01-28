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

import sys, os
import re
import http, http.client, http.server, urllib, socket
import subprocess, shutil, time, hashlib, zlib

import netrender.model

try:
  import bpy
except:
  bpy = None

VERSION = bytes("1.2", encoding='utf8')

# Jobs status
JOB_WAITING = 0 # before all data has been entered
JOB_PAUSED = 1 # paused by user
JOB_FINISHED = 2 # finished rendering
JOB_QUEUED = 3 # ready to be dispatched

JOB_STATUS_TEXT = {
        JOB_WAITING: "Waiting",
        JOB_PAUSED: "Paused",
        JOB_FINISHED: "Finished",
        JOB_QUEUED: "Queued"
        }


# Frames status
QUEUED = 0
DISPATCHED = 1
DONE = 2
ERROR = 3

FRAME_STATUS_TEXT = {
        QUEUED: "Queued",
        DISPATCHED: "Dispatched",
        DONE: "Done",
        ERROR: "Error"
        }

class DirectoryContext:
    def __init__(self, path):
        self.path = path
        
    def __enter__(self):
        self.curdir = os.path.abspath(os.curdir)
        os.chdir(self.path)

    def __exit__(self, exc_type, exc_value, traceback):
        os.chdir(self.curdir)

class BreakableIncrementedSleep:
    def __init__(self, increment, default_timeout, max_timeout, break_fct):
        self.increment = increment
        self.default = default_timeout
        self.max = max_timeout
        self.current = self.default
        self.break_fct = break_fct
        
    def reset(self):
        self.current = self.default

    def increase(self):
        self.current = min(self.current + self.increment, self.max)
        
    def sleep(self):
        for i in range(self.current):
            time.sleep(1)
            if self.break_fct():
                break
            
        self.increase()

def responseStatus(conn):
    response = conn.getresponse()
    response.read()
    return response.status

def reporting(report, message, errorType = None):
    if errorType:
        t = 'ERROR'
    else:
        t = 'INFO'

    if report:
        report(t, message)
        return None
    elif errorType:
        raise errorType(message)
    else:
        return None

def clientScan(report = None):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.settimeout(30)

        s.bind(('', 8000))

        buf, address = s.recvfrom(64)

        address = address[0]
        port = int(str(buf, encoding='utf8'))

        reporting(report, "Master server found")

        return (address, port)
    except socket.timeout:
        reporting(report, "No master server on network", IOError)

        return ("", 8000) # return default values

def clientConnection(address, port, report = None, scan = True, timeout = 5):
    if address == "[default]":
#            calling operator from python is fucked, scene isn't in context
#			if bpy:
#				bpy.ops.render.netclientscan()
#			else:
        if not scan:
            return None

        address, port = clientScan()
        if address == "":
            return None

    try:
        conn = http.client.HTTPConnection(address, port, timeout = timeout)

        if conn:
            if clientVerifyVersion(conn):
                return conn
            else:
                conn.close()
                reporting(report, "Incorrect master version", ValueError)
    except BaseException as err:
        if report:
            report('ERROR', str(err))
            return None
        else:
            print(err)
            return None

def clientVerifyVersion(conn):
    conn.request("GET", "/version")
    response = conn.getresponse()

    if response.status != http.client.OK:
        conn.close()
        return False

    server_version = response.read()

    if server_version != VERSION:
        print("Incorrect server version!")
        print("expected", str(VERSION, encoding='utf8'), "received", str(server_version, encoding='utf8'))
        return False

    return True

def fileURL(job_id, file_index):
    return "/file_%s_%i" % (job_id, file_index)

def logURL(job_id, frame_number):
    return "/log_%s_%i.log" % (job_id, frame_number)

def renderURL(job_id, frame_number):
    return "/render_%s_%i.exr" % (job_id, frame_number)

def cancelURL(job_id):
    return "/cancel_%s" % (job_id)

def hashFile(path):
    f = open(path, "rb")
    value = hashData(f.read())
    f.close()
    return value
    
def hashData(data):
    m = hashlib.md5()
    m.update(data)
    return m.hexdigest()
    

def prefixPath(prefix_directory, file_path, prefix_path, force = False):
    if os.path.isabs(file_path):
        # if an absolute path, make sure path exists, if it doesn't, use relative local path
        full_path = file_path
        if force or not os.path.exists(full_path):
            p, n = os.path.split(os.path.normpath(full_path))

            if prefix_path and p.startswith(prefix_path):
                if len(prefix_path) < len(p):
                    directory = os.path.join(prefix_directory, p[len(prefix_path)+1:]) # +1 to remove separator
                    if not os.path.exists(directory):
                        os.mkdir(directory)
                else:
                    directory = prefix_directory
                full_path = os.path.join(directory, n)
            else:
                full_path = os.path.join(prefix_directory, n)
    else:
        full_path = os.path.join(prefix_directory, file_path)

    return full_path

def getResults(server_address, server_port, job_id, resolution_x, resolution_y, resolution_percentage, frame_ranges):
    frame_arguments = []
    for r in frame_ranges:
        if len(r) == 2:
            frame_arguments.extend(["-s", str(r[0]), "-e", str(r[1]), "-a"])
        else:
            frame_arguments.extend(["-f", str(r[0])])
            
    filepath = os.path.join(bpy.app.tempdir, "netrender_temp.blend")
    bpy.ops.wm.save_as_mainfile(filepath=filepath, copy=True, check_existing=False)
            
    process = subprocess.Popen([sys.argv[0], "-b", "-noaudio", filepath, "-P", __file__] + frame_arguments + ["--", "GetResults", server_address, str(server_port), job_id, str(resolution_x), str(resolution_y), str(resolution_percentage)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while process.poll() is None:
        process.stdout.read(1024) # empty buffer to be sure
    process.stdout.read()
    
    os.remove(filepath)
    
    return

def _getResults(server_address, server_port, job_id, resolution_x, resolution_y, resolution_percentage):
    render = bpy.context.scene.render
    
    netsettings = bpy.context.scene.network_render

    netsettings.server_address = server_address
    netsettings.server_port = int(server_port)
    netsettings.job_id = job_id

    render.engine = 'NET_RENDER'
    render.resolution_x = int(resolution_x)
    render.resolution_y = int(resolution_y)
    render.resolution_percentage = int(resolution_percentage)
    

def getFileInfo(filepath, infos):
    process = subprocess.Popen([sys.argv[0], "-b", "-noaudio", filepath, "-P", __file__, "--", "FileInfo"] + infos, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout = bytes()
    while process.poll() is None:
        stdout += process.stdout.read(1024)

    # read leftovers if needed
    stdout += process.stdout.read()

    stdout = str(stdout, encoding="utf8")

    values = [eval(v[1:].strip()) for v in stdout.split("\n") if v.startswith("$")]

    return values
  

if __name__ == "__main__":
    try:
        start = sys.argv.index("--") + 1
    except ValueError:
        start = 0
    action, *args = sys.argv[start:]
    
    if action == "FileInfo": 
        for info in args:
            print("$", eval(info))
    elif action == "GetResults":
        _getResults(args[0], args[1], args[2], args[3], args[4], args[5])
