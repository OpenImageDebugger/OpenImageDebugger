"""Write a colourful test .npy (size x size x 3 uint8) for the acceptance viewer.

The pattern -- red/green ramps, a blue checkerboard, and a white diagonal
stripe -- makes rotation and zoom unmistakable when driving spin.py.
"""
import argparse

import numpy as np


def make_image(size: int = 256) -> np.ndarray:
    y, x = np.mgrid[0:size, 0:size]
    img = np.zeros((size, size, 3), np.uint8)
    img[..., 0] = x.astype(np.uint8)                                 # red ramp L->R
    img[..., 1] = y.astype(np.uint8)                                 # green ramp T->B
    img[..., 2] = (((x // 32 + y // 32) % 2) * 255).astype(np.uint8)  # blue checker
    img[np.abs(x - y) < 4] = 255                                     # white diagonal
    return img


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', help='output .npy path, e.g. /tmp/oid_vis.npy')
    parser.add_argument('--size', type=int, default=256)
    args = parser.parse_args()
    img = make_image(args.size)
    np.save(args.path, img)
    print(f'wrote {args.path} {img.shape} {img.dtype}')


if __name__ == '__main__':
    main()
