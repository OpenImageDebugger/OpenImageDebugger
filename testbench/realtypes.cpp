/*
 * One live instance of every buffer type in builtin_types.json, against the
 * REAL OpenCV and Eigen; built only when both are found (see CMakeLists).
 * CvMat and IplImage are hand-rolled: modern OpenCV no longer ships the
 * legacy C API, and OID matches on the type NAME and reads members by name.
 */
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

#include <Eigen/Dense>

// CvMat and IplImage are at GLOBAL scope, not in the anonymous namespace: OID
// matches buffer types by debug-info type name, and an anonymous namespace
// would defeat the builtin_types.json "^CvMat$" / "^IplImage$" matches.

// Reads: .type (channels/dtype bit-fields), .step, .rows, .cols, .data.ptr.
struct CvMat {
    int type;
    int step;
    int* refcount; // unused; kept for member-layout fidelity
    int hdr_refcount;
    union {
        unsigned char* ptr;
        short* s;
        int* i;
        float* fl;
        double* db;
    } data;
    int rows;
    int cols;
};

// Legacy IPL depth constants (from opencv2/core/types_c.h). The signed flag is
// bit 31; the declarative entry recovers the value with `depth & 0xffffffff`.
constexpr int IPL_DEPTH_8U = 8;
constexpr int IPL_DEPTH_16S = static_cast<int>(0x80000010u);

// Described only by testbench/.oid/types.json. Global scope for the same
// reason as CvMat above.

// Five required fields only: channels, row_stride and pixel_layout default.
struct PackedGray8 {
    unsigned char* data;
    int w;
    int h;
};

// stride_bytes is in bytes, so the entry divides it down to pixels. Declares
// bgra, so the blue and red ramps swap if the layout is ignored.
struct PaddedBgr8 {
    unsigned char* data;
    int w;
    int h;
    int channels;
    int stride_bytes;
};

// Packed with alpha; its entry states channels as a literal 4.
struct PackedRgba8 {
    unsigned char* data;
    int w;
    int h;
};

// Double precision, single channel.
struct DepthF64 {
    double* samples;
    int w;
    int h;
};

// Reads: .imageData, .width, .height, .nChannels, .depth, .widthStep.
struct IplImage {
    int nChannels;
    int depth;
    int width;
    int height;
    int widthStep;
    char* imageData;
};

namespace {

// A debugger names a base-class subobject after the base TYPE; C++ has no such
// path segment. NamesABase and HidesInherited are the shapes that defeat a
// name-only or first-wins walker. 48x32 because a 3x3 matrix is a speck.
constexpr int kInheritW = 48;
constexpr int kInheritH = 32;

struct BufferBase {
    Eigen::MatrixXd inherited;
};

struct InheritedOnly : BufferBase {
    int tag;
};

struct NamesABase : BufferBase {
    Eigen::MatrixXd BufferBase;
};

struct HidesInherited : BufferBase {
    Eigen::MatrixXd inherited;
};

// Shifted per fixture: a flat fill would make every wrong path look right.
Eigen::MatrixXd banded(int shift) {
    Eigen::MatrixXd m(kInheritH, kInheritW);
    for (int r = 0; r < kInheritH; ++r) {
        for (int c = 0; c < kInheritW; ++c) {
            const double ramp = static_cast<double>(c) / kInheritW;
            const bool band = (r + c + shift * 5) % kInheritH < 4;
            m(r, c) = band ? ramp + 1.0 : ramp;
        }
    }
    return m;
}

// Members of `this` surface BARE (`inherited`, not `probe.inherited`).
struct InheritingProbe : BufferBase {
    Eigen::MatrixXd own;

    void probe() const {
        // >>> SET A BREAKPOINT ON THE NEXT LINE FOR THE INHERITED CASE <<<
        volatile int oid_breakpoint_inherited = 0;
        (void)oid_breakpoint_inherited;
    }
};

constexpr int kW = 160;
constexpr int kH = 120;

// Dimensions of the oversized fixture; see make_mat_chunked_64f().
constexpr int kBigW = 1024;
constexpr int kBigH = 1025;

// Side of the many-chunk fixture; see make_mat_many_chunks_8uc3().
constexpr int kHugeSide = 4096;

// 3-channel 8-bit: exercises the flags channel/depth bit-fields and the
// byte-step -> pixel-stride division in the cv::Mat entry.
cv::Mat make_mat_8uc3() {
    cv::Mat m(kH, kW, CV_8UC3);
    for (int y = 0; y < m.rows; ++y) {
        for (int x = 0; x < m.cols; ++x) {
            m.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uchar>(x),
                                              static_cast<uchar>(y),
                                              static_cast<uchar>(x + y));
        }
    }
    return m;
}

// Single-channel float: exercises the float dtype path.
cv::Mat make_mat_32fc1() {
    cv::Mat m(kH, kW, CV_32FC1);
    for (int y = 0; y < m.rows; ++y) {
        for (int x = 0; x < m.cols; ++x) {
            m.at<float>(y, x) = std::sin(x * 0.05f) * std::cos(y * 0.05f);
        }
    }
    return m;
}

// 1025 x 1024 float64 = 8,396,800 bytes, one row past the 8 MiB per-message
// budget; float64 also makes bytes-per-row 8x the element stride. Values ramp
// with the row, so a dropped tail reads as a flat band, not plausible data.
cv::Mat make_mat_chunked_64f() {
    cv::Mat m(kBigH, kBigW, CV_64FC1);
    for (int y = 0; y < m.rows; ++y) {
        for (int x = 0; x < m.cols; ++x) {
            m.at<double>(y, x) =
                static_cast<double>(y) + static_cast<double>(x) / kBigW;
        }
    }
    return m;
}

// 1024 x 1024 float64 = 8,388,608 bytes: the per-message budget exactly (the
// sender tests payload <= budget) and an exact multiple of the read page size.
cv::Mat make_mat_budget_edge_64f() {
    cv::Mat m(kBigW, kBigW, CV_64FC1);
    for (int y = 0; y < m.rows; ++y) {
        for (int x = 0; x < m.cols; ++x) {
            m.at<double>(y, x) =
                static_cast<double>(x) + static_cast<double>(y) / kBigW;
        }
    }
    return m;
}

// 4096 x 4096 x 3 = 50,331,648 bytes: six messages and a remainder, so a
// chunked run has a middle here. Channel 0 is flat: colour means a bad stride.
cv::Mat make_mat_many_chunks_8uc3() {
    cv::Mat m(kHugeSide, kHugeSide, CV_8UC3);
    for (int y = 0; y < m.rows; ++y) {
        auto* row = m.ptr<unsigned char>(y);
        const auto down = static_cast<unsigned char>(y * 255 / (kHugeSide - 1));
        for (int x = 0; x < m.cols; ++x) {
            row[x * 3 + 0] = 0;
            row[x * 3 + 1] =
                static_cast<unsigned char>(x * 255 / (kHugeSide - 1));
            row[x * 3 + 2] = down;
        }
    }
    return m;
}

std::atomic<bool> worker_ready{false};
std::atomic<bool> worker_done{false};

// Parks a second thread whose top frame is also frame 0, so following the
// selected thread cannot be mistaken for following the frame index.
void worker_thread_main() {
    cv::Mat worker_mat(kH, kW, CV_8UC1);
    for (int y = 0; y < worker_mat.rows; ++y) {
        for (int x = 0; x < worker_mat.cols; ++x) {
            // Diagonal bars: unmistakably not one of main's ramps.
            worker_mat.at<unsigned char>(y, x) =
                static_cast<unsigned char>(((x + 2 * y) & 0x1f) * 8);
        }
    }
    worker_ready.store(true);
    worker_ready.notify_one();
    worker_done.wait(false);
    (void)worker_mat;
}

} // namespace

int main() {
    // Joined by a guard, not at the end of main: destroying a joinable
    // std::thread terminates, and an exception could unwind past the fixtures.
    struct WorkerGuard {
        std::thread thread;
        ~WorkerGuard() {
            worker_done.store(true);
            worker_done.notify_one();
            if (thread.joinable()) {
                thread.join();
            }
        }
    } worker{std::thread(worker_thread_main)};

    cv::Mat mat_8uc3 = make_mat_8uc3();
    cv::Mat mat_32fc1 = make_mat_32fc1();
    cv::Mat mat_chunked_64f = make_mat_chunked_64f();
    cv::Mat mat_budget_edge_64f = make_mat_budget_edge_64f();
    cv::Mat mat_many_chunks_8uc3 = make_mat_many_chunks_8uc3();

    std::vector<unsigned char> cvmat_backing(static_cast<std::size_t>(kW) * kH);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            cvmat_backing[static_cast<std::size_t>(y) * kW + x] =
                static_cast<unsigned char>((x ^ y) & 0xff);
        }
    }
    CvMat cvmat{};
    cvmat.type = CV_8U; // depth 0, channels 1 -> (0<<3)|0
    cvmat.step = kW;    // bytes per row = cols * channels * elemsize
    cvmat.rows = kH;
    cvmat.cols = kW;
    cvmat.data.ptr = cvmat_backing.data();

    // The real cv::Mat fixtures run on OpenCV 5 (CV_CN_SHIFT=5); the C-API
    // CvMat lived in <=4, shift 3, so build it by hand: the macro gives 64.
    // Only this reaches the <=4 branch of the entry's adaptive `channels`.
    constexpr int kCvLegacy8UC3 = (3 - 1) << 3; // == 16
    std::vector<unsigned char> cvmat3_backing(static_cast<std::size_t>(kW) *
                                              kH * 3);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * kW + x) * 3;
            cvmat3_backing[i + 0] = static_cast<unsigned char>(x); // B
            cvmat3_backing[i + 1] = static_cast<unsigned char>(y); // G
            cvmat3_backing[i + 2] = 128;                           // R
        }
    }
    CvMat cvmat_8uc3{};
    cvmat_8uc3.type = kCvLegacy8UC3; // depth 0, channels 3 -> (2<<3)|0
    cvmat_8uc3.step = kW * 3;        // cols * channels * elemsize
    cvmat_8uc3.rows = kH;
    cvmat_8uc3.cols = kW;
    cvmat_8uc3.data.ptr = cvmat3_backing.data();

    std::vector<char> ipl8_backing(static_cast<std::size_t>(kW) * kH * 3);
    IplImage ipl_8u{};
    ipl_8u.nChannels = 3;
    ipl_8u.depth = IPL_DEPTH_8U;
    ipl_8u.width = kW;
    ipl_8u.height = kH;
    ipl_8u.widthStep = kW * 3;
    ipl_8u.imageData = ipl8_backing.data();
    for (int y = 0; y < kH; ++y) {
        auto* row = reinterpret_cast<unsigned char*>(ipl_8u.imageData +
                                                     y * ipl_8u.widthStep);
        for (int x = 0; x < kW; ++x) {
            row[x * 3 + 0] = static_cast<unsigned char>(x);
            row[x * 3 + 1] = static_cast<unsigned char>(y);
            row[x * 3 + 2] = 128;
        }
    }

    // Signed depth (bit 31 set) exercises the dtype expression's
    // & 0xffffffff mask, not the row-stride division.
    std::vector<char> ipl16_backing(static_cast<std::size_t>(kW) * kH *
                                    sizeof(short));
    IplImage ipl_16s{};
    ipl_16s.nChannels = 1;
    ipl_16s.depth = IPL_DEPTH_16S;
    ipl_16s.width = kW;
    ipl_16s.height = kH;
    ipl_16s.widthStep = kW * static_cast<int>(sizeof(short));
    ipl_16s.imageData = ipl16_backing.data();
    for (int y = 0; y < kH; ++y) {
        auto* row =
            reinterpret_cast<short*>(ipl_16s.imageData + y * ipl_16s.widthStep);
        for (int x = 0; x < kW; ++x) {
            row[x] = static_cast<short>((x - y) * 100);
        }
    }

    // Dimensions come from the template args ({targ:1}/{targ:2}); default
    // storage is column-major, so the entry transposes.
    Eigen::Matrix<float, 3, 4> eig_fixed;
    for (int r = 0; r < eig_fixed.rows(); ++r) {
        for (int c = 0; c < eig_fixed.cols(); ++c) {
            eig_fixed(r, c) = static_cast<float>(r * 10 + c);
        }
    }

    // Dynamic double: template dims are Dynamic (-1), so the entry must fall
    // back to the runtime storage dims (m_storage.m_rows/m_cols).
    Eigen::MatrixXd eig_dyn(5, 7);
    for (int r = 0; r < eig_dyn.rows(); ++r) {
        for (int c = 0; c < eig_dyn.cols(); ++c) {
            eig_dyn(r, c) =
                static_cast<double>(r) + static_cast<double>(c) / 10.0;
        }
    }

    // Dynamic float, row-major: the RowMajor option bit flips the transpose
    // branch relative to the column-major matrices above.
    Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
        eig_rowmajor(4, 6);
    for (int r = 0; r < eig_rowmajor.rows(); ++r) {
        for (int c = 0; c < eig_rowmajor.cols(); ++c) {
            eig_rowmajor(r, c) = static_cast<float>(r * 100 + c);
        }
    }

    // Eigen::Map over a contiguous buffer: nested template args ({targ:0.*})
    // and the map's runtime m_rows/m_cols (.m_value).
    std::vector<float> map_backing(6 * 8);
    for (std::size_t i = 0; i < map_backing.size(); ++i) {
        map_backing[i] = static_cast<float>(i);
    }
    Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic>> eig_map(
        map_backing.data(), 6, 8);

    // Outer stride 12 on a 6x8 view, so the row stride must come from the
    // runtime stride, not the width. Backing is 12*8 = 96; index 89 is written.
    std::vector<float> stride_backing(12 * 8, 0.0f);
    for (int c = 0; c < 8; ++c) {
        for (int r = 0; r < 6; ++r) {
            stride_backing[static_cast<std::size_t>(c) * 12 + r] =
                static_cast<float>(r * 10 + c);
        }
    }
    Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic>,
               0,
               Eigen::OuterStride<>>
        eig_map_strided(stride_backing.data(), 6, 8, Eigen::OuterStride<>(12));

    InheritedOnly inherited_only;
    inherited_only.inherited = banded(0);
    inherited_only.tag = 1;

    NamesABase names_a_base;
    names_a_base.inherited = banded(1);
    names_a_base.BufferBase = banded(2);

    HidesInherited hides_inherited;
    hides_inherited.inherited = banded(3);
    hides_inherited.BufferBase::inherited = banded(4);

    InheritingProbe inheriting_probe;
    inheriting_probe.inherited = banded(5);
    inheriting_probe.own = banded(6);

    // Custom types from testbench/.oid/types.json: if these fail to plot, the
    // Debug Console names the file or the offending key.
    constexpr int kCustomW = 64;
    constexpr int kCustomH = 48;

    // Horizontal ramp.
    std::vector<unsigned char> gray_backing(static_cast<std::size_t>(kCustomW) *
                                            kCustomH);
    for (int y = 0; y < kCustomH; ++y) {
        for (int x = 0; x < kCustomW; ++x) {
            gray_backing[static_cast<std::size_t>(y) * kCustomW + x] =
                static_cast<unsigned char>(x * 4);
        }
    }
    PackedGray8 custom_gray{gray_backing.data(), kCustomW, kCustomH};

    // 192 bytes of pixels per row, padded to 204 (68 whole pixels). Padding
    // is saturated, so a wrong stride shows as a white band.
    constexpr int kBgrStrideBytes = 204;
    std::vector<unsigned char> bgr_backing(
        static_cast<std::size_t>(kBgrStrideBytes) * kCustomH, 0xff);
    for (int y = 0; y < kCustomH; ++y) {
        for (int x = 0; x < kCustomW; ++x) {
            unsigned char* px = bgr_backing.data() +
                                static_cast<std::size_t>(y) * kBgrStrideBytes +
                                x * 3;
            px[0] = static_cast<unsigned char>(x * 4);       // B
            px[1] = static_cast<unsigned char>(y * 5);       // G
            px[2] = static_cast<unsigned char>(255 - x * 4); // R
        }
    }
    PaddedBgr8 custom_bgr{
        bgr_backing.data(), kCustomW, kCustomH, 3, kBgrStrideBytes};

    // Red rises left to right, green top to bottom.
    std::vector<unsigned char> rgba_backing(static_cast<std::size_t>(kCustomW) *
                                            kCustomH * 4);
    for (int y = 0; y < kCustomH; ++y) {
        for (int x = 0; x < kCustomW; ++x) {
            unsigned char* px =
                rgba_backing.data() +
                (static_cast<std::size_t>(y) * kCustomW + x) * 4;
            px[0] = static_cast<unsigned char>(x * 4); // R
            px[1] = static_cast<unsigned char>(y * 5); // G
            px[2] = 128;                               // B
            px[3] = 255;                               // A
        }
    }
    PackedRgba8 custom_rgba{rgba_backing.data(), kCustomW, kCustomH};

    // Metres, ramped so the min/max readout is worth reading.
    std::vector<double> depth_backing(static_cast<std::size_t>(kCustomW) *
                                      kCustomH);
    for (int y = 0; y < kCustomH; ++y) {
        for (int x = 0; x < kCustomW; ++x) {
            depth_backing[static_cast<std::size_t>(y) * kCustomW + x] =
                0.5 + static_cast<double>(x) / 100.0 +
                static_cast<double>(y) / 1000.0;
        }
    }
    DepthF64 custom_depth{depth_backing.data(), kCustomW, kCustomH};

    // Unbounded by design: a stop before worker_ready would show that thread
    // mid-construction, and any deadline would expire while a human is paused.
    worker_ready.wait(false);

    std::cout << "Fixtures live: mat_8uc3 " << mat_8uc3.cols << "x"
              << mat_8uc3.rows << ", eig_dyn " << eig_dyn.rows() << "x"
              << eig_dyn.cols() << "; worker thread parked holding worker_mat."
              << " Break on the next line and plot each.\n";

    // >>> SET YOUR BREAKPOINT ON THE NEXT LINE <<<
    // Every fixture above is in scope here; plot each by name, then continue.
    volatile int oid_breakpoint = 0;
    (void)oid_breakpoint;
    inheriting_probe.probe();
    (void)cvmat;
    (void)cvmat_8uc3;
    (void)ipl_8u;
    (void)ipl_16s;
    (void)eig_fixed;
    (void)eig_rowmajor;
    (void)eig_map;
    (void)eig_map_strided;
    (void)custom_gray;
    (void)custom_bgr;
    (void)custom_rgba;
    (void)custom_depth;

    // Pixels differ at every stop: a viewer that redraws nothing looks exactly
    // like one that redraws correctly until something moves on screen.
    for (int step = 1; step <= 8; ++step) {
        for (int y = 0; y < kCustomH; ++y) {
            for (int x = 0; x < kCustomW; ++x) {
                const std::size_t i =
                    static_cast<std::size_t>(y) * kCustomW + x;
                gray_backing[i] =
                    static_cast<unsigned char>((x + step * 16) * 4);
                depth_backing[i] += 0.25;
                unsigned char* px = bgr_backing.data() +
                                    static_cast<std::size_t>(y) *
                                        kBgrStrideBytes +
                                    x * 3;
                px[1] = static_cast<unsigned char>((y * 5 + step * 24) & 0xff);
            }
        }
        // >>> SET A BREAKPOINT ON THE NEXT LINE TO WATCH BUFFERS UPDATE <<<
        volatile int oid_breakpoint_step = step;
        (void)oid_breakpoint_step;
    }

    return 0;
}
