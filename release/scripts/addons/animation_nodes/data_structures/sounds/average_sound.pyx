import bpy
from . utils cimport findStartAndEndFrame

cdef class AverageSound(Sound):
    def __cinit__(self, FloatList samples not None, int startFrame):
        self.type = "AVERAGE"
        self.samples = samples
        self.startFrame = startFrame
        self.endFrame = startFrame + len(samples) - 1

    @classmethod
    def fromSequences(cls, list sequences not None, int index):
        return createAverageSound(sequences, index)

    cpdef float evaluate(self, float frame):
        cdef int intFrame = <int>frame
        cdef float before = self.evaluateInt(intFrame)
        cdef float after = self.evaluateInt(intFrame + 1)
        cdef float influence = frame - intFrame
        return before * (1 - influence) + after * influence

    cdef float evaluateInt(self, int frame):
        if self.startFrame <= frame <= self.endFrame:
            return self.samples.data[frame - self.startFrame]
        return 0

def createAverageSound(list sequences not None, int index):
    checkAverageSoundInput(sequences, index)
    return createValidAverageSound(sequences, index)

cdef checkAverageSoundInput(list sequences, int index):
    if len(sequences) == 0:
        raise ValueError("at least one sequence required")
    for sequence in sequences:
        if not isinstance(getattr(sequence, "sound", None), bpy.types.Sound):
            raise TypeError("at least one sequence has no sound")
        if index >= len(sequence.sound.bakedData.average):
            raise IndexError("at least one sequence does not have the given bake index")

cdef createValidAverageSound(list sequences, int index):
    cdef int startFrame, endFrame
    cdef FloatList mixedSamples
    startFrame, endFrame = findStartAndEndFrame(sequences)
    mixedSamples = addSequenceSamplesInRange(sequences, index, startFrame, endFrame)
    return AverageSound(mixedSamples, startFrame)

cdef FloatList addSequenceSamplesInRange(list sequences, int index, int rangeStart, int rangeEnd):
    cdef FloatList mixedSamples = FloatList(length = rangeEnd - rangeStart)
    cdef FloatList samples
    mixedSamples.fill(0)

    cdef int frame, finalStartFrame, finalEndFrame, sequenceStart
    for sequence in sequences:
        samples = getAverageSoundSamples(sequence.sound, index)
        sequenceStart = sequence.frame_start
        finalStartFrame = max(rangeStart, sequence.frame_final_start)
        finalEndFrame = min(rangeEnd, sequence.frame_final_end, len(samples) + sequenceStart)

        for frame in range(finalStartFrame, finalEndFrame):
            mixedSamples.data[frame - rangeStart] += samples.data[frame - sequenceStart]

    return mixedSamples

cdef dict cachedAverageSounds = dict()
cdef getAverageSoundSamples(sound, index):
    cdef str identifier = sound.bakedData.average[index].identifier
    if identifier not in cachedAverageSounds:
        cachedAverageSounds[identifier] = sound.bakedData.average[index].getSamples()
    return cachedAverageSounds[identifier]
