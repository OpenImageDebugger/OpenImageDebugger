/*
 * Live-debugger fixture for the declarative built-in buffer types.
 *
 * Unlike testbench/main.cpp (a dependency-free cv::Mat *simulator*), this file
 * holds one live instance of every buffer type declared in
 * resources/oidscripts/oidtypes/builtin_types.json, so the declarative JSON
 * entries can be checked against real memory under gdb and lldb.
 *
 *   - cv::Mat and all the Eigen types use the REAL current libraries.
 *   - CvMat and IplImage are the legacy OpenCV C API, which modern OpenCV
 *     (5, and 4 with the C API disabled) no longer ships. OID matches on the
 *     type NAME and reads members by name, so faithful local structs with the
 *     same member names and legacy constant values exercise those two entries
 *     exactly as the real types would.
 *   - One deliberately oversized cv::Mat crosses the viewer's 8 MiB
 *     per-message budget, so plotting it exercises the chunked transfer path
 *     that every other fixture here is too small to reach.
 *
 * This target is built only when OpenCV and Eigen are found (see CMakeLists).
 * Set a breakpoint on the marked line in main(), run under a debugger with OID
 * active, and plot each variable. Each fixture notes what it is meant to check.
 */
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include <opencv2/core.hpp>

#include <Eigen/Dense>

// --- Legacy OpenCV C-API reproductions (see file header) -------------------
// These are defined at GLOBAL scope, not inside the anonymous namespace below:
// OID matches buffer types by their debug-info type name, and an anonymous
// namespace would make them "(anonymous namespace)::CvMat" / "::IplImage",
// which the builtin_types.json "^CvMat$" / "^IplImage$" matches would miss.

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

constexpr int kW = 160;
constexpr int kH = 120;

// Dimensions of the oversized fixture; see make_mat_chunked_64f().
constexpr int kBigW = 1024;
constexpr int kBigH = 1025;

// CvMat.type packs the depth (which doubles as the OID dtype code) in the low
// 3 bits and (channels - 1) in the CV_CN_SHIFT (=3) field. CV_8U is OpenCV's
// own macro (== 0) from <opencv2/core.hpp>.

// --- Real cv::Mat fixtures -------------------------------------------------

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

// Oversized single-channel float64: the only fixture here that is about the
// transfer rather than a type entry. 1025 rows x 1024 float64 = 8,396,800
// bytes, just past the viewer's 8 MiB per-message budget, so it crosses the
// wire as a begin + one full 1024-row strip + a 1-row remainder instead of a
// single message. float64 also makes bytes-per-row eight times the element
// stride, which is where a transfer that measures strips in the wrong unit
// goes wrong. Values ramp with the row, so a dropped tail reads as a flat
// band rather than as plausible data.
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

} // namespace

int main() {
    // --- OpenCV cv::Mat (real library) ---
    cv::Mat mat_8uc3 = make_mat_8uc3();
    cv::Mat mat_32fc1 = make_mat_32fc1();
    cv::Mat mat_chunked_64f = make_mat_chunked_64f();

    // --- CvMat (legacy struct), single-channel 8-bit ---
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

    // --- CvMat (legacy struct), 3-channel 8-bit, OpenCV <=4 bit packing ---
    // The real cv::Mat fixtures run against OpenCV 5, whose flags pack the
    // channel count at CV_CN_SHIFT=5, so they only exercise that layout. The
    // C-API CvMat actually lived in OpenCV <=4, which packs it at
    // CV_CN_SHIFT=3, making CV_8UC3 == (3-1)<<3 | CV_8U(0) == 16. Build that
    // value by hand: the CV_8UC3 macro on an OpenCV-5 build would use shift 5
    // and yield 64. This drives the <=4 branch of the version-adaptive
    // `channels` expression that no real cv::Mat here can reach.
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

    // --- IplImage (legacy struct), unsigned 8-bit, 3 channels ---
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

    // --- IplImage (legacy struct), signed 16-bit: the signed depth exercises
    // the row-stride computation for a signed IPL depth (a case the old Python
    // arithmetic collapsed to zero). ---
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

    // --- Eigen fixtures (real library) ---
    // Fixed-size float: compile-time dimensions come from the template args
    // ({targ:1}/{targ:2}); default storage is column-major (transposed).
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

    // Eigen::Map with an explicit outer stride: a 6x8 view whose columns are
    // spaced 12 apart in a column-major backing store (6 real rows + 6 padding
    // per column), so the row stride must come from the runtime outer stride
    // rather than the width. Backing = stride(12) * cols(8) = 96 floats; the
    // last written index is (7*12 + 5) = 89, so a 6*12 = 72 store overflows.
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

    std::cout << "Fixtures live: mat_8uc3 " << mat_8uc3.cols << "x"
              << mat_8uc3.rows << ", eig_dyn " << eig_dyn.rows() << "x"
              << eig_dyn.cols() << ". Break on the next line and plot each.\n";

    // >>> SET YOUR BREAKPOINT ON THE NEXT LINE <<<
    // Every fixture above is in scope here; plot each by name, then continue.
    volatile int oid_breakpoint = 0;
    (void)oid_breakpoint;
    (void)cvmat;
    (void)cvmat_8uc3;
    (void)ipl_8u;
    (void)ipl_16s;
    (void)eig_fixed;
    (void)eig_rowmajor;
    (void)eig_map;
    (void)eig_map_strided;

    return 0;
}
