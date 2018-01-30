GIW_TYPES_UINT8 = 0
GIW_TYPES_UINT16 = 2
GIW_TYPES_INT16 = 3
GIW_TYPES_INT32 = 4
GIW_TYPES_FLOAT32 = 5
GIW_TYPES_FLOAT64 = 6

##
# Default values created for OpenCV Mat structures. Change it according to your
# needs.
def get_buffer_info(picked_obj):
    import gdb

    # OpenCV constants
    CV_CN_MAX = 512
    CV_CN_SHIFT = 3
    CV_MAT_CN_MASK = ((CV_CN_MAX - 1) << CV_CN_SHIFT)
    CV_DEPTH_MAX = (1 << CV_CN_SHIFT)
    CV_MAT_TYPE_MASK = (CV_DEPTH_MAX*CV_CN_MAX - 1)

    char_type = gdb.lookup_type("char")
    char_pointer_type = char_type.pointer()
    buffer = picked_obj['data'].cast(char_pointer_type)
    if buffer==0x0:
        raise Exception('Received null buffer!')

    width = int(picked_obj['cols'])
    height = int(picked_obj['rows'])
    flags = int(picked_obj['flags'])

    channels = ((((flags) & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1)
    step = int(int(picked_obj['step']['buf'][0])/channels)

    cvtype = ((flags) & CV_MAT_TYPE_MASK)

    type = (cvtype&7)

    if type == GIW_TYPES_UINT16 or \
       type == GIW_TYPES_INT16:
        step = int(step / 2)
    elif type == GIW_TYPES_INT32 or \
         type == GIW_TYPES_FLOAT32:
        step = int(step / 4)
    elif type == GIW_TYPES_FLOAT64:
        step = int(step / 8)

    return (buffer, width, height, channels, type, step)

##
# Returns true if the given symbol is of observable type (the type of the
# buffer you are working with). Make sure to check for pointers of your type as
# well
def is_symbol_observable(symbol):
    symbol_type = str(symbol.type)
    return symbol_type == 'cv::Mat' or symbol_type == 'cv::Mat *'

