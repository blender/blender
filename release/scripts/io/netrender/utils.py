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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import sys, os
import re
import http, http.client, http.server, urllib, socket
import subprocess, shutil, time, hashlib

import netrender.model

try:
  import bpy
except:
  bpy = None

VERSION = bytes("0.7", encoding='utf8')

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

def rnaType(rna_type):
    if bpy: bpy.types.register(rna_type)
    return rna_type

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

def clientConnection(address, port, report = None):
    if address == "[default]":
#            calling operator from python is fucked, scene isn't in context
#			if bpy:
#				bpy.ops.render.netclientscan()
#			else:
            address, port = clientScan()
            if address == "":
                return None

    try:
        conn = http.client.HTTPConnection(address, port)

        if conn:
            if clientVerifyVersion(conn):
                return conn
            else:
                conn.close()
                reporting(report, "Incorrect master version", ValueError)
    except Exception as err:
        if report:
            report('ERROR', str(err))
            return None
        else:
            raise

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

def prefixPath(prefix_directory, file_path, prefix_path):
    if os.path.isabs(file_path):
        # if an absolute path, make sure path exists, if it doesn't, use relative local path
        full_path = file_path
        if not os.path.exists(full_path):
            p, n = os.path.split(full_path)

            if prefix_path and p.startswith(prefix_path):
                directory = prefix_directory + p[len(prefix_path):]
                full_path = directory + os.sep + n
                if not os.path.exists(directory):
                    os.mkdir(directory)
            else:
                full_path = prefix_directory + n
    else:
        full_path = prefix_directory + file_path

    return full_path
