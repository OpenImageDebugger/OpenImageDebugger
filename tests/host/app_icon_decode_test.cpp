#include <gtest/gtest.h>

#include <cstddef>

#include "host/app_icon_decode.h"
#include "host/ui/icons/app_icon_png.h"

TEST(AppIconDecode, DecodesEmbeddedIconToRgba) {
    const oid::host::DecodedImage img = oid::host::decode_png_rgba(
        oid::host::icons::APP_ICON_PNG, oid::host::icons::APP_ICON_PNG_SIZE);

    EXPECT_EQ(img.width, 256);
    EXPECT_EQ(img.height, 256);
    EXPECT_EQ(img.rgba.size(), static_cast<std::size_t>(256) * 256 * 4);
}

TEST(AppIconDecode, ReturnsEmptyOnGarbage) {
    const unsigned char garbage[] = {0x00, 0x01, 0x02, 0x03};
    const oid::host::DecodedImage img =
        oid::host::decode_png_rgba(garbage, sizeof(garbage));

    EXPECT_EQ(img.width, 0);
    EXPECT_EQ(img.height, 0);
    EXPECT_TRUE(img.rgba.empty());
}
