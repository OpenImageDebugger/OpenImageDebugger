import gdb

GIW_TYPES_FLOAT32 = 0
GIW_TYPES_UINT8 = 1

##
# Default values created for OpenCV Mat structures. Change it according to your
# needs.
def get_buffer_info(picked_obj):
    # OpenCV constants
    CV_CN_MAX = 512
    CV_CN_SHIFT = 3
    CV_MAT_CN_MASK = ((CV_CN_MAX - 1) << CV_CN_SHIFT)
    CV_DEPTH_MAX = (1 << CV_CN_SHIFT)
    CV_MAT_TYPE_MASK = (CV_DEPTH_MAX*CV_CN_MAX - 1)
    CV_8U = 0
    CV_32F = 5

    char_type = gdb.lookup_type("char")
    char_pointer_type = char_type.pointer()
    buffer = picked_obj['data'].cast(char_pointer_type)
    if buffer==0x0:
        raise Exception('Received null buffer!')

    width = int(picked_obj['cols'])
    height = int(picked_obj['rows'])
    flags = int(picked_obj['flags'])

    channels = ((((flags) & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1)
    print('channels!!',channels)
    step = int(int(picked_obj['step']['buf'][0])/channels)

    cvtype = ((flags) & CV_MAT_TYPE_MASK)

    if (cvtype&7) == CV_8U:
        type = GIW_TYPES_UINT8
    elif (cvtype&7) == (CV_32F):
        type = GIW_TYPES_FLOAT32
        step = int(step/4)
        pass

    return (buffer, width, height, channels, type, step)

##
# Returns true if the given symbol is of observable type (the type of the
# buffer you are working with). Make sure to check for pointers of your type as
# well
def is_symbol_observable(symbol):
    symbol_type = str(symbol.type)
    return symbol_type == 'cv::Mat' or symbol_type == 'cv::Mat *'

