# WebM container, VP9 video, Opus audio.
import bpy

ffmpeg = bpy.context.scene.render.ffmpeg
ffmpeg.format = "WEBM"
ffmpeg.codec = "WEBM"
ffmpeg.audio_codec = "OPUS"
ffmpeg.constant_rate_factor = 'MEDIUM'
ffmpeg.use_max_b_frames = False
