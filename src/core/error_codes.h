#pragma once

namespace act::core::errors
{

// Provider errors
constexpr auto PROVIDER_TIMEOUT  = "PROVIDER_TIMEOUT";
constexpr auto AUTH_ERROR        = "AUTH_ERROR";
constexpr auto RATE_LIMIT        = "RATE_LIMIT";
constexpr auto NO_PROVIDER       = "NO_PROVIDER";

// File errors
constexpr auto FILE_NOT_FOUND    = "FILE_NOT_FOUND";
constexpr auto OUTSIDE_WORKSPACE = "OUTSIDE_WORKSPACE";
constexpr auto BINARY_FILE       = "BINARY_FILE";
constexpr auto PERMISSION_DENIED = "PERMISSION_DENIED";

// Tool errors
constexpr auto TOOL_NOT_FOUND    = "TOOL_NOT_FOUND";
constexpr auto INVALID_PARAMS    = "INVALID_PARAMS";
constexpr auto INVALID_PATTERN   = "INVALID_PATTERN";
constexpr auto STRING_NOT_FOUND  = "STRING_NOT_FOUND";
constexpr auto AMBIGUOUS_MATCH   = "AMBIGUOUS_MATCH";

// Shell errors
constexpr auto TIMEOUT           = "TIMEOUT";
constexpr auto COMMAND_BLOCKED   = "COMMAND_BLOCKED";

// Git errors
constexpr auto NOT_GIT_REPO      = "NOT_GIT_REPO";

} // namespace act::core::errors
