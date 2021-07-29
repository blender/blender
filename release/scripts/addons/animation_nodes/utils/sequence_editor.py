import bpy
from . path import toAbsolutePath

def iterSoundSequencesInScene(scene):
    return (sequence for sequence in iterSequencesInScene(scene) if sequence.type == "SOUND")

def iterSequencesInScene(scene):
    if scene is None: return []
    if scene.sequence_editor is None: []
    yield from scene.sequence_editor.sequences

def getEmptyChannel(editor):
    channels = [False] * 32
    for sequence in editor.sequences:
        channels[sequence.channel - 1] = True
    for channel, isUsed in enumerate(channels):
        if not isUsed: return channel + 1
    return 32

def getOrCreateSequencer(scene):
    if not scene.sequence_editor:
        scene.sequence_editor_create()
    return scene.sequence_editor
