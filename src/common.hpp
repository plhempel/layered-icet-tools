#pragma once

#include <string_view>


namespace deep_icet {

// Logging constants.
std::string_view constexpr log_sev_info  {      "\x1b[1m[info]\x1b[m  "};
std::string_view constexpr log_sev_warn  {   "\x1b[1;33m[warn]\x1b[m  "};
std::string_view constexpr log_sev_error {   "\x1b[1;31m[error]\x1b[m "};
std::string_view constexpr log_sev_fatal {"\x1b[1;30;41m[fatal]\x1b[m "};

std::string_view constexpr log_tag_egl    {"\x1b[1m[egl]\x1b[m "};
std::string_view constexpr log_tag_glfw   {"\x1b[1m[glfw]\x1b[m "};
std::string_view constexpr log_tag_opengl {"\x1b[1m[opengl]\x1b[m "};

} // namespace deep_icet
