#ifndef __ARISTARCHOS_PARAMETER_FILE_PARSER_HPP__
#define __ARISTARCHOS_PARAMETER_FILE_PARSER_HPP__

#include <map>
#include <string>

namespace aristarchos {

/// @brief Find an return the first non-whitespace character in a string
/// @param[in] str A null-terminating C-string
/// @param[out] index Index of the first non-whitespace character in str; if no
///             such char exists (aka if str is a string of whitespaces), index
///             will be set to -1.
/// @return The first non-whitespace character in str
char first_non_wspace_char(const char *str, int &index) noexcept;

/// @brief Resolve a parameter line of type: 'PARAMETER_NAME = PARAMETER_VALUE;'
/// @param[in] str The parameter line
/// @param[out] param_name At successefult output, the PARAMETER_NAME trimmed
///                        of whitespace characters
/// @param[out] param_value At successefult output, the PARAMETER_VALUE trimmed
///                        of whitespace characters
/// @return Anything other than 0 denotes an error
/// @example:
/// Str: "PARAM_NAME = 1234567;"
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "   PARAM_NAME = 1234567;  "
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "  PARAM_NAME    = 1234567;"
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "PARAM_NAME =        1234567  "
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "PARAM_NAME = 1234567;some comment string here"
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "PARAM_NAME=1234567;"
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "PARAM_NAME=1234567"
/// Name: "PARAM_NAM", value: "123456"
///
/// Str: "PARAM_NAME=1234567  ; "
/// Name: "PARAM_NAM", value: "123456"
int resolve_parameter_line(const char *str, std::string &param_name,
                           std::string &param_value) noexcept;

/// @brief Parse a parameter file and return of map<string,string> of
///        parameter_name, parameter_value type containing all parsed lines.
///        An example of such a file is given at the end of this file.
/// @param[in] fn The filename of the parameter file
/// @note The function will override entries with the same key. That is, if
///       the file contains the lines:
///       PARAM1 = val1
///       ...      ...
///       PARAM1 = val2
///       PARAM1 = val3
///       the map returned will only hold one entry for parameter PARAM1 with
///       the value val3.
int read_parameter_file(
    const char *fn, std::map<std::string, std::string> &parameter_map) noexcept;

} // namespace aristarchos

#endif
