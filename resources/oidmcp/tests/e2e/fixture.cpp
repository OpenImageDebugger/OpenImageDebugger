#include <Eigen/Dense>

int main() {
  Eigen::MatrixXf gradient(8, 16);
  for (int row = 0; row < gradient.rows(); ++row) {
    for (int col = 0; col < gradient.cols(); ++col) {
      gradient(row, col) = static_cast<float>(col) / 15.0f;
    }
  }
  return static_cast<int>(gradient(0, 15)); // BREAK
}
