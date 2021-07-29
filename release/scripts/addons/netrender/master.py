# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#s
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
import http, http.client, http.server, socket, socketserver
import shutil, time, hashlib
import pickle
import zipfile
import select # for select.error
import json


from netrender.utils import *
import netrender.model
import netrender.balancing
import netrender.master_html
import netrender.thumbnail as thumbnail

class MRenderFile(netrender.model.RenderFile):
    def __init__(self, filepath, index, start, end, signature):
        super().__init__(filepath, index, start, end, signature)
        self.found = False

    def updateStatus(self):
        self.found = os.path.exists(self.filepath)

        if self.found and self.signature is not None:
            found_signature = hashFile(self.filepath)
            self.found = self.signature == found_signature
            if not self.found:
                print("Signature mismatch", self.signature, found_signature)

        return self.found

    def test(self):
        # don't check when forcing upload and only until found
        if not self.force and not self.found:
            self.updateStatus()

        return self.found


class MRenderSlave(netrender.model.RenderSlave):
    def __init__(self, slave_info):
        super().__init__(slave_info)
        self.id = hashlib.md5(bytes(repr(slave_info.name) + repr(slave_info.address), encoding='utf8')).hexdigest()
        self.last_seen = time.time()


        self.job = None
        self.job_frames = []

        netrender.model.RenderSlave._slave_map[self.id] = self

    def seen(self):
        self.last_seen = time.time()

    def finishedFrame(self, frame_number):
        try:
            self.job_frames.remove(frame_number)
        except ValueError as e:
            print("Internal error: Frame %i not in job frames list" % frame_number)
            print(self.job_frames)
        if not self.job_frames:
            self.job = None

class MRenderJob(netrender.model.RenderJob):
    def __init__(self, job_id, job_info):
        super().__init__(job_info)
        self.id = job_id
        self.last_dispatched = time.time()
        self.start_time = time.time()
        self.finish_time = self.start_time
        # force one chunk for process jobs
        if self.type == netrender.model.JOB_PROCESS:
            self.chunks = 1

        # Force WAITING status on creation
        self.status = netrender.model.JOB_WAITING

        # special server properties
        self.last_update = 0
        self.save_path = ""
        self.files = [MRenderFile(rfile.filepath, rfile.index, rfile.start, rfile.end, rfile.signature) for rfile in job_info.files]

    def setForceUpload(self, force):
        for rfile in self.files:
            rfile.force = force

    def initInfo(self):
        if not self.resolution:
            self.resolution = tuple(getFileInfo(self.files[0].filepath, ["bpy.context.scene.render.resolution_x", "bpy.context.scene.render.resolution_y", "bpy.context.scene.render.resolution_percentage"]))

    def save(self):
        if self.save_path:
            f = open(os.path.join(self.save_path, "job.txt"), "w")
            f.write(json.dumps(self.serialize()))
            f.close()

    def edit(self, info_map):
        if "status" in info_map:
            self.status = info_map["status"]

        if "priority" in info_map:
            self.priority = info_map["priority"]

        if "chunks" in info_map:
            self.chunks = info_map["chunks"]

    def testStart(self):
        # Don't test files for versionned jobs
        if not self.version_info:
            for f in self.files:
                if not f.test():
                    return False

        self.start()
        self.initInfo()
        return True

    def testFinished(self):
        for f in self.frames:
            if f.status in {netrender.model.FRAME_QUEUED, netrender.model.FRAME_DISPATCHED, netrender.model.FRAME_ERROR}:
                break
        else:
            self.status = netrender.model.JOB_FINISHED
            self.finish_time=time.time()

    def pause(self, status = None):
        if self.status not in {netrender.model.JOB_PAUSED, netrender.model.JOB_QUEUED}:
            return

        if status is None:
            self.status = netrender.model.JOB_PAUSED if self.status == netrender.model.JOB_QUEUED else netrender.model.JOB_QUEUED
        elif status:
            self.status = netrender.model.JOB_QUEUED
        else:
            self.status = netrender.model.JOB_PAUSED

    def start(self):
        self.status = netrender.model.JOB_QUEUED


    def addLog(self, frames):
        frames = sorted(frames)
        log_name = "%06d_%06d.log" % (frames[0], frames[-1])
        log_path = os.path.join(self.save_path, log_name)

        for number in frames:
            frame = self[number]
            if frame:
                frame.log_path = log_path

    def addFrame(self, frame_number, command):
        frame = MRenderFrame(frame_number, command)
        self.frames.append(frame)
        return frame

    def reset(self, all):
        for f in self.frames:
            f.reset(all)

        if all:
            self.status = netrender.model.JOB_QUEUED

    def getFrames(self):
        frames = []
        for f in self.frames:
            if f.status == netrender.model.FRAME_QUEUED:
                self.last_dispatched = time.time()
                frames.append(f)
                if len(frames) >= self.chunks:
                    break

        return frames

    def getResultPath(self, filename):
        return os.path.join(self.save_path, filename)

class MRenderFrame(netrender.model.RenderFrame):
    def __init__(self, frame, command):
        super().__init__()
        self.number = frame
        self.slave = None
        self.time = 0
        self.status = netrender.model.FRAME_QUEUED
        self.command = command

        self.log_path = None

    def addDefaultRenderResult(self):
        self.results.append(self.getRenderFilename())

    def getRenderFilename(self):
        return "%06d.exr" % self.number

    def reset(self, all):
        if all or self.status == netrender.model.FRAME_ERROR:
            self.log_path = None
            self.slave = None
            self.time = 0
            self.status = netrender.model.FRAME_QUEUED


# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
file_pattern = re.compile("/file_([a-zA-Z0-9]+)_([0-9]+)")
render_pattern = re.compile("/render_([a-zA-Z0-9]+)_([0-9]+).exr")
result_pattern = re.compile("/result_([a-zA-Z0-9]+).zip")
thumb_pattern = re.compile("/thumb_([a-zA-Z0-9]+)_([0-9]+).jpg")
log_pattern = re.compile("/log_([a-zA-Z0-9]+)_([0-9]+).log")
reset_pattern = re.compile("/reset(all|)_([a-zA-Z0-9]+)_([0-9]+)")
cancel_pattern = re.compile("/cancel_([a-zA-Z0-9]+)")
pause_pattern = re.compile("/pause_([a-zA-Z0-9]+)")
edit_pattern = re.compile("/edit_([a-zA-Z0-9]+)")

class RenderHandler(http.server.BaseHTTPRequestHandler):
    def write_file(self, file_path, mode = 'wb'):
        length = int(self.headers['content-length'])
        f = open(file_path, mode)
        buf = self.rfile.read(length)
        f.write(buf)
        f.close()
        del buf

    def log_message(self, format, *args):
        # override because the original calls self.address_string(), which
        # is extremely slow due to some timeout..
        sys.stderr.write("[%s] %s\n" % (self.log_date_time_string(), format%args))

    def getInfoMap(self):
        length = int(self.headers['content-length'])

        if length > 0:
            msg = str(self.rfile.read(length), encoding='utf8')
            return json.loads(msg)
        else:
            return {}

    def send_head(self, code = http.client.OK, headers = {}, content = "application/octet-stream"):
        self.send_response(code)

        if code == http.client.OK and content:
            self.send_header("Content-type", content)

        for key, value in headers.items():
            self.send_header(key, value)

        self.end_headers()

    def do_HEAD(self):

        if self.path == "/status":
            job_id = self.headers.get('job-id', "")
            job_frame = int(self.headers.get('job-frame', -1))

            job = self.server.getJobID(job_id)
            if job:
                frame = job[job_frame]


                if frame:
                    self.send_head(http.client.OK)
                else:
                    # no such frame
                    self.send_head(http.client.NO_CONTENT)
            else:
                # no such job id
                self.send_head(http.client.NO_CONTENT)

    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    def do_GET(self):

        if self.path == "/version":
            self.send_head()
            self.server.stats("", "Version check")
            self.wfile.write(VERSION)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/render"):
            match = render_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]
                frame_number = int(match.groups()[1])

                job = self.server.getJobID(job_id)

                if job:
                    frame = job[frame_number]

                    if frame:
                        if frame.status in {netrender.model.FRAME_QUEUED, netrender.model.FRAME_DISPATCHED}:
                            self.send_head(http.client.ACCEPTED)
                        elif frame.status == netrender.model.FRAME_DONE:
                            self.server.stats("", "Sending result to client")

                            filename = job.getResultPath(frame.getRenderFilename())

                            f = open(filename, 'rb')
                            self.send_head(content = "image/x-exr")
                            shutil.copyfileobj(f, self.wfile)
                            f.close()
                        elif frame.status == netrender.model.FRAME_ERROR:
                            self.send_head(http.client.PARTIAL_CONTENT)
                    else:
                        # no such frame
                        self.send_head(http.client.NO_CONTENT)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/result"):
            match = result_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]

                job = self.server.getJobID(job_id)

                if job:
                    self.server.stats("", "Sending result to client")

                    zip_filepath = job.getResultPath("results.zip")
                    with zipfile.ZipFile(zip_filepath, "w") as zfile:
                        for frame in job.frames:
                            if frame.status == netrender.model.FRAME_DONE:
                                for filename in frame.results:
                                    filepath = job.getResultPath(filename)

                                    zfile.write(filepath, filename)


                    f = open(zip_filepath, 'rb')
                    self.send_head(content = "application/x-zip-compressed")
                    shutil.copyfileobj(f, self.wfile)
                    f.close()
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/thumb"):
            match = thumb_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]
                frame_number = int(match.groups()[1])

                job = self.server.getJobID(job_id)

                if job:
                    frame = job[frame_number]

                    if frame:
                        if frame.status in {netrender.model.FRAME_QUEUED, netrender.model.FRAME_DISPATCHED}:
                            self.send_head(http.client.ACCEPTED)
                        elif frame.status == netrender.model.FRAME_DONE:
                            filename = job.getResultPath(frame.getRenderFilename())

                            thumbname = thumbnail.generate(filename)

                            if thumbname:
                                f = open(thumbname, 'rb')
                                self.send_head(content = "image/jpeg")
                                shutil.copyfileobj(f, self.wfile)
                                f.close()
                            else: # thumbnail couldn't be generated
                                self.send_head(http.client.PARTIAL_CONTENT)
                                return
                        elif frame.status == netrender.model.FRAME_ERROR:
                            self.send_head(http.client.PARTIAL_CONTENT)
                    else:
                        # no such frame
                        self.send_head(http.client.NO_CONTENT)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/log"):
            match = log_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]
                frame_number = int(match.groups()[1])

                job = self.server.getJobID(job_id)

                if job:
                    frame = job[frame_number]

                    if frame:
                        if not frame.log_path or frame.status in {netrender.model.FRAME_QUEUED, netrender.model.FRAME_DISPATCHED}:
                            self.send_head(http.client.PROCESSING)
                        else:
                            self.server.stats("", "Sending log to client")
                            f = open(frame.log_path, 'rb')

                            self.send_head(content = "text/plain")

                            shutil.copyfileobj(f, self.wfile)

                            f.close()
                    else:
                        # no such frame
                        self.send_head(http.client.NO_CONTENT)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid URL
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/status":
            job_id = self.headers.get('job-id', "")
            job_frame = int(self.headers.get('job-frame', -1))

            if job_id:

                job = self.server.getJobID(job_id)
                if job:
                    if job_frame != -1:
                        frame = job[frame]

                        if frame:
                            message = frame.serialize()
                        else:
                            # no such frame
                            self.send_head(http.client.NO_CONTENT)
                            return
                    else:
                        message = job.serialize()
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
                    return
            else: # status of all jobs
                message = []

                for job in self.server:
                    message.append(job.serialize())


            self.server.stats("", "Sending status")
            self.send_head()
            self.wfile.write(bytes(json.dumps(message), encoding='utf8'))

        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/job":
            self.server.balance()

            slave_id = self.headers['slave-id']

            slave = self.server.getSeenSlave(slave_id)

            if slave: # only if slave id is valid
                job, frames = self.server.newDispatch(slave)

                if job and frames:
                    for f in frames:
                        print("dispatch", f.number)
                        f.status = netrender.model.FRAME_DISPATCHED
                        f.slave = slave

                    slave.job = job
                    slave.job_frames = [f.number for f in frames]

                    self.send_head(headers={"job-id": job.id})

                    message = job.serialize(frames)
                    self.wfile.write(bytes(json.dumps(message), encoding='utf8'))

                    self.server.stats("", "Sending job to slave")
                else:
                    # no job available, return error code
                    slave.job = None
                    slave.job_frames = []

                    self.send_head(http.client.ACCEPTED)
            else: # invalid slave id
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/file"):
            match = file_pattern.match(self.path)

            if match:
                slave_id = self.headers['slave-id']
                slave = self.server.getSeenSlave(slave_id)

                if not slave:
                    # invalid slave id
                    print("invalid slave id")

                job_id = match.groups()[0]
                file_index = int(match.groups()[1])

                job = self.server.getJobID(job_id)

                if job:
                    render_file = job.files[file_index]

                    if render_file:
                        self.server.stats("", "Sending file to slave")
                        f = open(render_file.filepath, 'rb')

                        self.send_head()
                        shutil.copyfileobj(f, self.wfile)

                        f.close()
                    else:
                        # no such file
                        self.send_head(http.client.NO_CONTENT)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/slaves":
            message = []

            self.server.stats("", "Sending slaves status")

            for slave in self.server.slaves:
                message.append(slave.serialize())

            self.send_head()

            self.wfile.write(bytes(json.dumps(message), encoding='utf8'))
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        else:
            # hand over the rest to the html section
            netrender.master_html.get(self)

    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    def do_POST(self):

        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        if self.path == "/job":

            length = int(self.headers['content-length'])

            job_info = netrender.model.RenderJob.materialize(json.loads(str(self.rfile.read(length), encoding='utf8')))
            job_id = self.server.nextJobID()

            job = MRenderJob(job_id, job_info)

            job.setForceUpload(self.server.force)

            for frame in job_info.frames:
                frame = job.addFrame(frame.number, frame.command)

            self.server.addJob(job)

            headers={"job-id": job_id}

            if job.testStart():
                self.server.stats("", "New job, started")
                self.send_head(headers=headers, content = None)
            else:
                self.server.stats("", "New job, missing files (%i total)" % len(job.files))
                self.send_head(http.client.ACCEPTED, headers=headers)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/edit"):
            match = edit_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]

                job = self.server.getJobID(job_id)

                if job:
                    info_map = self.getInfoMap()

                    job.edit(info_map)
                    self.send_head(content = None)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/balance_limit":
            info_map = self.getInfoMap()
            for rule_id, limit in info_map.items():
                try:
                    rule = self.server.balancer.ruleByID(rule_id)
                    if rule:
                        rule.setLimit(limit)
                except:
                    pass # invalid type

            self.send_head(content = None)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/balance_enable":
            info_map = self.getInfoMap()
            for rule_id, enabled in info_map.items():
                rule = self.server.balancer.ruleByID(rule_id)
                if rule:
                    rule.enabled = enabled

            self.send_head(content = None)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/cancel"):
            match = cancel_pattern.match(self.path)

            if match:
                info_map = self.getInfoMap()
                clear = info_map.get("clear", False)

                job_id = match.groups()[0]

                job = self.server.getJobID(job_id)

                if job:
                    self.server.stats("", "Cancelling job")
                    self.server.removeJob(job, clear)
                    self.send_head(content = None)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/pause"):
            match = pause_pattern.match(self.path)

            if match:
                info_map = self.getInfoMap()
                status = info_map.get("status", None)

                job_id = match.groups()[0]

                job = self.server.getJobID(job_id)

                if job:
                    self.server.stats("", "Pausing job")
                    job.pause(status)
                    self.send_head(content = None)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else:
                # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/clear":
            # cancel all jobs
            info_map = self.getInfoMap()
            clear = info_map.get("clear", False)

            self.server.stats("", "Clearing jobs")
            self.server.clear(clear)

            self.send_head(content = None)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/reset"):
            match = reset_pattern.match(self.path)

            if match:
                all = match.groups()[0] == 'all'
                job_id = match.groups()[1]
                job_frame = int(match.groups()[2])

                job = self.server.getJobID(job_id)

                if job:
                    if job_frame != 0:

                        frame = job[job_frame]
                        if frame:
                            self.server.stats("", "Reset job frame")
                            frame.reset(all)
                            self.send_head(content = None)
                        else:
                            # no such frame
                            self.send_head(http.client.NO_CONTENT)

                    else:
                        self.server.stats("", "Reset job")
                        job.reset(all)
                        self.send_head(content = None)

                else: # job not found
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid url
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/slave":
            length = int(self.headers['content-length'])
            # job_frame_string = self.headers['job-frame']  # UNUSED

            self.server.stats("", "New slave connected")

            slave_info = netrender.model.RenderSlave.materialize(json.loads(str(self.rfile.read(length), encoding='utf8')), cache = False)

            slave_info.address = self.client_address

            slave_id = self.server.addSlave(slave_info)

            self.send_head(headers = {"slave-id": slave_id}, content = None)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/log":
            length = int(self.headers['content-length'])

            log_info = netrender.model.LogFile.materialize(json.loads(str(self.rfile.read(length), encoding='utf8')))

            slave_id = log_info.slave_id

            slave = self.server.getSeenSlave(slave_id)

            if slave: # only if slave id is valid
                job = self.server.getJobID(log_info.job_id)

                if job:
                    self.server.stats("", "Log announcement")
                    job.addLog(log_info.frames)
                    self.send_head(content = None)
                else:
                    # no such job id
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid slave id
                self.send_head(http.client.NO_CONTENT)
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    # -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    def do_PUT(self):

        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        if self.path.startswith("/file"):
            match = file_pattern.match(self.path)

            if match:
                self.server.stats("", "Receiving job file")

                job_id = match.groups()[0]
                file_index = int(match.groups()[1])

                job = self.server.getJobID(job_id)

                if job:

                    rfile = job.files[file_index]

                    if rfile:
                        main_file = job.files[0].original_path # original path of the first file

                        main_path, main_name = os.path.split(main_file)

                        if file_index > 0:
                            file_path = createLocalPath(rfile, job.save_path, main_path, True)
                        else:
                            file_path = os.path.join(job.save_path, main_name)

                        # add same temp file + renames as slave

                        self.write_file(file_path)

                        rfile.filepath = file_path # set the new path
                        found = rfile.updateStatus() # make sure we have the right file

                        if not found: # checksum mismatch
                            self.server.stats("", "File upload but checksum mismatch, this shouldn't happen")
                            self.send_head(http.client.CONFLICT)
                        elif job.testStart(): # started correctly
                            self.server.stats("", "File upload, starting job")
                            self.send_head(content = None)
                        else:
                            self.server.stats("", "File upload, dependency files still missing")
                            self.send_head(http.client.ACCEPTED)
                    else: # invalid file
                        print("file not found", job_id, file_index)
                        self.send_head(http.client.NO_CONTENT)
                else: # job not found
                    print("job not found", job_id, file_index)
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid url
                print("no match")
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/render":
            self.server.stats("", "Receiving render result")

            slave_id = self.headers['slave-id']

            slave = self.server.getSeenSlave(slave_id)

            if slave: # only if slave id is valid
                job_id = self.headers['job-id']

                job = self.server.getJobID(job_id)

                if job:
                    job_frame = int(self.headers['job-frame'])
                    job_result = int(self.headers['job-result'])
                    job_time = float(self.headers['job-time'])

                    frame = job[job_frame]

                    if frame:
                        self.send_head(content = None)

                        if job.hasRenderResult():
                            if job_result == netrender.model.FRAME_DONE:
                                frame.addDefaultRenderResult()
                                self.write_file(job.getResultPath(frame.getRenderFilename()))

                            elif job_result == netrender.model.FRAME_ERROR:
                                # blacklist slave on this job on error
                                # slaves might already be in blacklist if errors on the whole chunk
                                if not slave.id in job.blacklist:
                                    job.blacklist.append(slave.id)

                        slave.finishedFrame(job_frame)

                        frame.status = job_result
                        frame.time = job_time

                        job.testFinished()

                    else: # frame not found
                        self.send_head(http.client.NO_CONTENT)
                else: # job not found
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid slave id
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/result":
            self.server.stats("", "Receiving job result")

            slave_id = self.headers['slave-id']

            slave = self.server.getSeenSlave(slave_id)

            if slave: # only if slave id is valid
                job_id = self.headers['job-id']

                job = self.server.getJobID(job_id)

                if job:
                    job_frame = int(self.headers['job-frame'])

                    frame = job[job_frame]

                    if frame:
                        job_result = int(self.headers['job-result'])
                        job_finished = self.headers['job-finished'] == str(True)

                        self.send_head(content = None)

                        if job_result == netrender.model.FRAME_DONE:
                            result_filename = self.headers['result-filename']

                            frame.results.append(result_filename)
                            self.write_file(job.getResultPath(result_filename))

                        if job_finished:
                            job_time = float(self.headers['job-time'])
                            slave.finishedFrame(job_frame)

                            frame.status = job_result
                            frame.time = job_time

                            job.testFinished()
                    else: # frame not found
                        self.send_head(http.client.NO_CONTENT)
                else: # job not found
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid slave id
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path == "/thumb":
            self.server.stats("", "Receiving thumbnail result")

            slave_id = self.headers['slave-id']

            slave = self.server.getSeenSlave(slave_id)

            if slave: # only if slave id is valid
                job_id = self.headers['job-id']

                job = self.server.getJobID(job_id)

                if job:
                    job_frame = int(self.headers['job-frame'])

                    frame = job[job_frame]

                    if frame:
                        self.send_head(content = None)

                        if job.hasRenderResult():
                            self.write_file(os.path.join(os.path.join(job.save_path, "%06d.jpg" % job_frame)))

                    else: # frame not found
                        self.send_head(http.client.NO_CONTENT)
                else: # job not found
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid slave id
                self.send_head(http.client.NO_CONTENT)
        # =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        elif self.path.startswith("/log"):
            self.server.stats("", "Receiving log file")

            match = log_pattern.match(self.path)

            if match:
                job_id = match.groups()[0]

                job = self.server.getJobID(job_id)

                if job:
                    job_frame = int(match.groups()[1])

                    frame = job[job_frame]

                    if frame and frame.log_path:
                        self.send_head(content = None)

                        self.write_file(frame.log_path, 'ab')

                        self.server.getSeenSlave(self.headers['slave-id'])

                    else: # frame not found
                        self.send_head(http.client.NO_CONTENT)
                else: # job not found
                    self.send_head(http.client.NO_CONTENT)
            else: # invalid url
                self.send_head(http.client.NO_CONTENT)

class RenderMasterServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    def __init__(self, address, handler_class, path, force=False, subdir=True):
        self.jobs = []
        self.jobs_map = {}
        self.slaves = []
        self.slaves_map = {}
        self.job_id = 0
        self.force = force

        if subdir:
            self.path = os.path.join(path, "master_" + str(os.getpid()))
        else:
            self.path = path

        verifyCreateDir(self.path)

        self.slave_timeout = 5 # 5 mins: need a parameter for that

        self.balancer = netrender.balancing.Balancer()
        self.balancer.addRule(netrender.balancing.RatingUsageByCategory(self.getJobs))
        self.balancer.addRule(netrender.balancing.RatingUsage())
        self.balancer.addException(netrender.balancing.ExcludeQueuedEmptyJob())
        self.balancer.addException(netrender.balancing.ExcludeSlavesLimit(self.countJobs, self.countSlaves, limit = 0.9))
        self.balancer.addPriority(netrender.balancing.NewJobPriority())
        self.balancer.addPriority(netrender.balancing.MinimumTimeBetweenDispatchPriority(limit = 2))

        super().__init__(address, handler_class)

    def restore(self, jobs, slaves, balancer = None):
        self.jobs = jobs
        self.jobs_map = {}

        for job in self.jobs:
            self.jobs_map[job.id] = job
            self.job_id = max(self.job_id, int(job.id))

        self.slaves = slaves
        for slave in self.slaves:
            self.slaves_map[slave.id] = slave

        if balancer:
            self.balancer = balancer


    def nextJobID(self):
        self.job_id += 1
        return str(self.job_id)

    def addSlave(self, slave_info):
        slave = MRenderSlave(slave_info)
        self.slaves.append(slave)
        self.slaves_map[slave.id] = slave

        return slave.id

    def removeSlave(self, slave):
        self.slaves.remove(slave)
        self.slaves_map.pop(slave.id)

    def getSlave(self, slave_id):
        return self.slaves_map.get(slave_id)

    def getSeenSlave(self, slave_id):
        slave = self.getSlave(slave_id)
        if slave:
            slave.seen()

        return slave

    def timeoutSlaves(self):
        removed = []

        t = time.time()

        for slave in self.slaves:
            if (t - slave.last_seen) / 60 > self.slave_timeout:
                removed.append(slave)

                if slave.job:
                    for f in slave.job_frames:
                        slave.job[f].status = netrender.model.FRAME_ERROR

        for slave in removed:
            self.removeSlave(slave)

    def updateUsage(self):
        blend = 0.5
        for job in self.jobs:
            job.usage *= (1 - blend)

        if self.slaves:
            slave_usage = blend / self.countSlaves()

            for slave in self.slaves:
                if slave.job:
                    slave.job.usage += slave_usage


    def clear(self, clear_files = False):
        removed = self.jobs[:]

        for job in removed:
            self.removeJob(job, clear_files)

    def balance(self):
        self.balancer.balance(self.jobs)

    def getJobs(self):
        return self.jobs

    def countJobs(self, status = netrender.model.JOB_QUEUED):
        total = 0
        for j in self.jobs:
            if j.status == status:
                total += 1

        return total

    def countSlaves(self):
        return len(self.slaves)

    def removeJob(self, job, clear_files = False):
        self.jobs.remove(job)
        self.jobs_map.pop(job.id)

        if clear_files:
            shutil.rmtree(job.save_path)

        for slave in self.slaves:
            if slave.job == job:
                slave.job = None
                slave.job_frames = []

    def addJob(self, job):
        self.jobs.append(job)
        self.jobs_map[job.id] = job

        # create job directory
        job.save_path = os.path.join(self.path, "job_" + job.id)
        verifyCreateDir(job.save_path)

        job.save()

    def getJobID(self, id):
        return self.jobs_map.get(id)

    def __iter__(self):
        for job in self.jobs:
            yield job

    def newDispatch(self, slave):
        if self.jobs:
            for job in self.jobs:
                if (
                    not self.balancer.applyExceptions(job)      # No exceptions
                    and slave.id not in job.blacklist           # slave is not blacklisted
                    and (not slave.tags or job.tags.issubset(slave.tags))  # slave doesn't use tags or slave has all job tags
                         ):

                    return job, job.getFrames()

        return None, None

def clearMaster(path):
    shutil.rmtree(path)

def createMaster(address, clear, force, path):
    filepath = os.path.join(path, "blender_master.data")

    if not clear and os.path.exists(filepath):
        print("loading saved master:", filepath)
        with open(filepath, 'rb') as f:
            path, jobs, slaves = pickle.load(f)

            httpd = RenderMasterServer(address, RenderHandler, path, force=force, subdir=False)
            httpd.restore(jobs, slaves)

            return httpd

    return RenderMasterServer(address, RenderHandler, path, force=force)

def saveMaster(path, httpd):
    filepath = os.path.join(path, "blender_master.data")

    with open(filepath, 'wb') as f:
        pickle.dump((httpd.path, httpd.jobs, httpd.slaves), f, pickle.HIGHEST_PROTOCOL)

def runMaster(address, broadcast, clear, force, path, update_stats, test_break,use_ssl=False,cert_path="",key_path=""):
    httpd = createMaster(address, clear, force, path)
    httpd.timeout = 1
    httpd.stats = update_stats
    if use_ssl:
        import ssl
        httpd.socket = ssl.wrap_socket(
                httpd.socket,
                certfile=cert_path,
                server_side=True,
                keyfile=key_path,
                ciphers="ALL",
                ssl_version=ssl.PROTOCOL_SSLv23,
                )
    if broadcast:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    start_time = time.time() - 2

    while not test_break():
        try:
            httpd.handle_request()
        except select.error:
            pass

        if time.time() - start_time >= 2: # need constant here
            httpd.timeoutSlaves()

            httpd.updateUsage()

            if broadcast:
                print("broadcasting address")
                s.sendto(bytes("%i" % address[1], encoding='utf8'), 0, ('<broadcast>', 8000))
                start_time = time.time()

    httpd.server_close()
    if clear:
        clearMaster(httpd.path)
    else:
        saveMaster(path, httpd)

