/*
 * Minimal OpenCV Mat simulator; for testing purposes only.
 */
#include <limits>
#include <iostream>
#include <type_traits>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <thread>
#include <cassert>
#include <cstring>
#include <functional>

using namespace std;

namespace cv {
struct Mat {
private:
    shared_ptr<void> dataMgr;

public:
    void* data;
    int cols; // Width
    int rows; // Height
    int flags; // OpenCV flags
    struct {
       int buf[2]; // Buf[0] = width of the containing
                   // buffer*channels; buff[1] = channels
    } step;

    Mat() : data(nullptr),
            cols(0),
            rows(0) {}

    Mat& operator=(Mat&& o) {
        data = o.data;
        dataMgr = std::move(o.dataMgr);
        cols = o.cols;
        rows = o.rows;

        return *this;
    }

    template<typename T>
    void create(int rows_, int cols_, int channels_)
    {
        cols = cols_;
        rows = rows_;
        step = {cols_ * channels_ * static_cast<int>(sizeof(T)),
                channels_};

        const int GIW_TYPES_UINT8 = 0;
        const int GIW_TYPES_UINT16 = 2;
        const int GIW_TYPES_INT16 = 3;
        const int GIW_TYPES_INT32 = 4;
        const int GIW_TYPES_FLOAT32 = 5;
        const int GIW_TYPES_FLOAT64 = 6;

        const int CV_CN_SHIFT = 3;

        flags = (channels_ - 1) << CV_CN_SHIFT;

        if(is_same<T, uint8_t>::value) {
            flags |= GIW_TYPES_UINT8;
        } else if(is_same<T, uint16_t>::value) {
            flags |= GIW_TYPES_UINT16;
        } else if(is_same<T, int16_t>::value) {
            flags |= GIW_TYPES_INT16;
        } else if(is_same<T, int32_t>::value) {
            flags |= GIW_TYPES_INT32;
        } else if(is_same<T, float>::value) {
            flags |= GIW_TYPES_FLOAT32;
        } else if(is_same<T, double>::value) {
            flags |= GIW_TYPES_FLOAT64;
        }

        dataMgr = shared_ptr<T>(new T[rows * cols * channels_],
                [](T* buf) {
            delete[] buf;
        });
        data = dataMgr.get();
    }

    void release() {
        // Intentionally, "data" is not changed so we are left with an invalid pointer.
        dataMgr.reset();
        data = (void*)rand();
    }

    template<typename T>
    struct Iterator {
        Iterator(Mat& m_) : m(m_) {}

        Iterator& operator=(Iterator& o) {
            m = o.m;

            return *this;
        }

        T& operator()(int row, int col, int chan) {
            return (static_cast<T*>(m.data))[row*m.step.buf[0]/sizeof(T) + col*m.step.buf[1] + chan];
        }

        Mat &m;
    };
};

}

using namespace cv;

template<typename T>
void fillBuffer(int W, int H, int C, Mat& matrix) {
    matrix.create<T>(H, W, C);

    Mat::Iterator<T> i(matrix);
    int nN = 1;
    for(int n = 0; n < nN; ++n) {
        float ni = n / static_cast<float>(nN) * M_PI_2;
        for(int y = 0; y < H; ++y) {
            for(int x = 0; x < W; ++x) {
                float r = sin(cos(ni) * 10 + x / static_cast<float>(W) * M_PI) * 127 + 128;
                float g = sin(cos(ni) * 20 + y / static_cast<float>(H) * M_PI) * 127 + 128;
                float b = cos(cos(ni) * 30 + x / static_cast<float>(W) * M_PI) * 127 + 128;
                r = x;
                g = y;

                i(y, x, 0) = r;

                if(C > 1)
                    i(y, x, 1) = g;

                if(C > 2)
                    i(y, x, 2) = b;
            }
            continue;
        }
        continue;
    }
    return;
}

template<typename T>
void ones(int W, int H, int C, Mat& matrix) {
    assert(W > 0 && H > 0 && C > 0);
    matrix.create<T>(H, W, C);

    Mat::Iterator<T> i(matrix);
    for(int y = 0; y < H; ++y) {
        for(int x = 0; x < W; ++x) {
            for(int c = 0; c < C; ++c) {
                i(y, x, c) = 1;
            }
        }
    }
    return;
}

int roundlog2(unsigned int n) {
    int result = 0;
    while((n >> result) > 0) {
        ++result;
    }

    return result - 1;
}

template<typename T>
void computeSumTable(int W, int H, int C, Mat& matrix) {
    assert(W > 0 && H > 0 && C > 0);
    Mat output;
    output.create<T>(H, W, C);

    Mat::Iterator<T> in(matrix);
    Mat::Iterator<T> out(output);
    auto kernelH = [&W, &H, &C](Mat::Iterator<T>& in, Mat::Iterator<T>& out, int iteration) {
        int step = 1 << iteration;
        for(int y = 0; y < H; ++y) {
            for(int x = 0; x < step; ++x) {
                for(int c = 0; c < C; ++c) {
                    out(y, x, c) = in(y, x, c);
                }
            }
            for(int x = step; x < W; ++x) {
                for(int c = 0; c < C; ++c) {
                    out(y, x, c) = in(y, x, c) + in(y, x - step, c);
                }
            }
        }
    };

    auto kernelV = [&W, &H, &C](Mat::Iterator<T>& in, Mat::Iterator<T>& out, int iteration) {
        int step = 1 << iteration;
        for(int y = 0; y < step; ++y) {
            for(int x = 0; x < W; ++x) {
                for(int c = 0; c < C; ++c) {
                    out(y, x, c) = in(y, x, c);
                }
            }
        }

        for(int y = step; y < H; ++y) {
            for(int x = 0; x < W; ++x) {
                for(int c = 0; c < C; ++c) {
                    out(y, x, c) = in(y, x, c) + in(y - step, x, c);
                }
            }
        }
    };

    bool isOutputSwapped = false;

    auto callKernels = [&W, &H, &in, &out, &kernelH, &kernelV, &isOutputSwapped](bool isHorizontal) {
        int numIterations;
        if(isHorizontal) {
            numIterations = roundlog2(W-1);
        } else {
            numIterations = roundlog2(H-1);
        }

        for(int i = 0; i <= numIterations; ++i) {
            if((i % 2) == 0) {
                if(isHorizontal)
                    kernelH(in, out, i);
                else
                    kernelV(in, out, i);
            } else {
                if(isHorizontal)
                    kernelH(out, in, i);
                else
                    kernelV(out, in, i);
            }

            isOutputSwapped = !isOutputSwapped;
            continue;
        }
    };

    callKernels(true);
    if(isOutputSwapped) {
        matrix = std::move(output);
        output.create<T>(H, W, C);
        isOutputSwapped = false;
    }
    callKernels(false);

    if(isOutputSwapped) {
        matrix = std::move(output);
    }

    return;
}

class TestFather {
protected:
    Mat TestField;
};

class Test : public TestFather {
public:
    function<void()> run() {
        auto lambda = [this]() {
            nest();
        };

        return lambda;
    }

private:

    int sampleField;

    void nest() {
        // Breakpoints should go here!
        int W = 1024;
        int H = 1024;
        int C = 3;
        Mat::Iterator<uint8_t> i(TestField);
        ones<uint8_t>(W, H, 1, TestField);
        i(0,0,0) = 255;
        Mat broken;
        Mat* pointer = &TestField;
        char tst;
        broken.data = &tst;
        broken.cols = 1024;
        broken.rows = 1024;
        broken.step.buf[2] = 3;
        broken.release();
        ones<uint8_t>(W, H, C, TestField);
        i(0,0,0) = 255;
        i(0,0,1) = 0;
        i(0,0,2) = 0;
        ones<uint8_t>(W, H, 1, TestField);
        i(0,0,0) = 255;
        computeSumTable<uint8_t>(W, H, C, TestField);
        return;
    }
};

void body() {
    Test tst;
    auto f = tst.run();

    thread t(f);
    t.join();

    return;
}

void bodyCaller() {
    thread t(body);
    t.join();
}

int main(int, char *[])
{
    bodyCaller();

    return 0;
}
