#include "parameter_file.hpp"
#include <fstream>
#include <iostream>

constexpr std::size_t LINE_SZ = 256;

/// @brief Find an return the first non-whitespace character in a string
/// @param[in] str A null-terminating C-string
/// @param[out] index Index of the first non-whitespace character in str; if no
///             such char exists (aka if str is a string of whitespaces), index
///             will be set to -1.
/// @return The first non-whitespace character in str
char aristarchos::first_non_wspace_char(const char *str, int &index) noexcept {
  std::size_t idx = 0;
  while (*(str + idx) && *(str + idx) == ' ')
    ++idx;
  index = *(str + idx) ? idx : -1;
  return *(str + idx);
}

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
int aristarchos::resolve_parameter_line(const char *str,
                                        std::string &param_name,
                                        std::string &param_value) noexcept {
  std::string line(str);
  // parsing parameter name; from first non-whitespace character up to the
  // last non-whitespace character before the equal sign
  auto equal_sign_at = line.find_first_of('='); // index of equal sign
  if (equal_sign_at == std::string::npos)
    return -1;
  auto start_idx = line.find_first_not_of(' '); // index of first non-ws char
  // index of first non-ws char left of equal char
  auto stop_idx = line.find_last_not_of(' ', equal_sign_at - 1);
  if (stop_idx <= start_idx)
    return -2;
  param_name = std::string(line, start_idx, stop_idx - start_idx + 1);

  // parsing parameter value; from first non-whitespace character after the
  // equal sign, up to the last non-whitespace character before the semicolumn
  // char (or the end of line).
  // index of first non-ws char after equal car
  start_idx = line.find_first_not_of(' ', equal_sign_at + 1);
  // index of ';' char (could be npos)
  equal_sign_at = line.find_first_of(';');
  // index of first non-ws char left of semicolumn char
  stop_idx = line.find_last_not_of(' ', equal_sign_at - 1);
  param_value = std::string(line, start_idx, stop_idx - start_idx + 1);

  return 0;
}

/// @brief Parse a parameter file and return of map<string,string> of
///        parameter_name, parameter_value type containing all parsed lines.
///        An example of such a file is given at the end of this file.
/// @param[in] fn The filename of the parameter file
/// @note The (in/out) parameter parameter_map will be cleared of any elements
///       at the start of the function.
/// @note The function will override entries with the same key. That is, if
///       the file contains the lines:
///       PARAM1 = val1
///       ...      ...
///       PARAM1 = val2
///       PARAM1 = val3
///       the map returned will only hold one entry for parameter PARAM1 with
///       the value val3.
int aristarchos::read_parameter_file(
    const char *fn,
    std::map<std::string, std::string> &parameter_map) noexcept {
  parameter_map.clear();

  // open parameter file
  std::ifstream fin(fn, std::ios::in);
  if (!fin.is_open()) {
    std::cerr << "\n[ERROR] Could not open parameter file \"" << fn << "\"";
    return -1;
  }

  // read parameter file, line by line
  char line[LINE_SZ], c;
  int i;
  std::string param_name, param_value;
  while (fin.getline(line, LINE_SZ)) {
    c = first_non_wspace_char(line, i);
    // skip empty line or lines starting with '#'
    if (!(i == -1 || c == '#')) {
      // seeking lines of type: 'PARAM_NAME = PARAM_VALUE;'
      if (resolve_parameter_line(line, param_name, param_value)) {
        std::cerr << "\n[ERROR] Failed to resolve parameter line: \"" << line
                  << "\"";
      } else {
        parameter_map.insert_or_assign(param_name, param_value);
      }
    }
  }

  return 0;
}
