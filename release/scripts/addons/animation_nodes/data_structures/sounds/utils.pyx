cdef findStartAndEndFrame(list sequences):
    startFrame = min(sequence.frame_final_start for sequence in sequences)
    endFrame = max(sequence.frame_final_end for sequence in sequences)
    return startFrame, endFrame

# expects float list to be not empty and 0 <= x <= 1
cdef float getNearestFloatListValue(float *data, int length, float x):
    cdef float pos = <float>(length - 1) * x
    cdef int beforeIndex = <int>pos

    if pos - <float>beforeIndex <= 0.5:
        return data[beforeIndex]
    else:
        if beforeIndex == length:
             return data[beforeIndex]
        else:
            return data[beforeIndex + 1]
