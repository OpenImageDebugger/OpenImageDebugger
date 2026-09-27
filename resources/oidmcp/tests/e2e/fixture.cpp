#include <Eigen/Dense>
#include <array>

// No builtin entry matches this struct, so only the custom_types.json this
// test points at can resolve it. Rows are padded: a wrong row_stride still
// reads row 0 correctly and corrupts only the rows after it.
struct RgbFrame {
    unsigned char* pixels;
    int cols;
    int rows;
    int channel_count;
    int stride_bytes;
};

int main() {
    constexpr int COLS = 16;
    constexpr int ROWS = 8;
    constexpr int CHANNELS = 3;
    constexpr int STRIDE_BYTES = 60; // 48 bytes of pixels, 12 of padding

    Eigen::MatrixXf gradient(8, 16);
    for (int row = 0; row < gradient.rows(); ++row) {
        for (int col = 0; col < gradient.cols(); ++col) {
            gradient(row, col) = static_cast<float>(col) / 15.0f;
        }
    }

    // Local rather than global: the resolver only needs an address that is
    // live at the breakpoint, and the buffer has to stay writable.
    std::array<unsigned char, ROWS * STRIDE_BYTES> frame_storage{};

    // Padding is filled first and left saturated, so a stride error surfaces
    // as 0xff rather than as plausible-looking neighbouring pixels.
    frame_storage.fill(0xff);
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            unsigned char* pixel =
                frame_storage.data() + row * STRIDE_BYTES + col * CHANNELS;
            pixel[0] = static_cast<unsigned char>(col * 16);
            pixel[1] = static_cast<unsigned char>(row * 32);
            pixel[2] = static_cast<unsigned char>((row + col) * 8);
        }
    }
    RgbFrame frame{frame_storage.data(), COLS, ROWS, CHANNELS, STRIDE_BYTES};

    return static_cast<int>(gradient(0, 15)) + frame.pixels[0]; // BREAK
}
