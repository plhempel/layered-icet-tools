#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <png.hpp>


namespace deep_icet {

// Logging constants.
std::string_view constexpr log_sev_info  {      "\x1b[1m[info]\x1b[m  "};
std::string_view constexpr log_sev_warn  {   "\x1b[1;33m[warn]\x1b[m  "};
std::string_view constexpr log_sev_error {   "\x1b[1;31m[error]\x1b[m "};
std::string_view constexpr log_sev_fatal {"\x1b[1;30;41m[fatal]\x1b[m "};

std::string_view constexpr log_tag_egl    {"\x1b[1m[egl]\x1b[m "};
std::string_view constexpr log_tag_glfw   {"\x1b[1m[glfw]\x1b[m "};
std::string_view constexpr log_tag_opengl {"\x1b[1m[opengl]\x1b[m "};

// Image format.
using Channel   = uint8_t;
using Pixel     = png::basic_rgba_pixel<Channel>;
using PixelData = std::array<Channel, Pixel::traits::channels>;
using Image     = png::image<Pixel, png::solid_pixel_buffer<Pixel>>;
using ImageSize = decltype(std::declval<Image>().get_width());

auto constexpr alpha_channel {Pixel::traits::channels - 1};
auto constexpr channel_max   {std::numeric_limits<Channel>::max()};

} // namespace deep_icet
