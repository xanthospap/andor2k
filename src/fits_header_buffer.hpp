#ifndef __ARISTARCHOS_FITS_HEADER_BUF_HPP__
#define __ARISTARCHOS_FITS_HEADER_BUF_HPP__

#include <cstdio>
#include <string>

namespace aristarchos {

namespace fh_buffer_details {
constexpr std::size_t KEYWORD_SZ = 128;
constexpr std::size_t VALUE_SZ = 128;
constexpr std::size_t COMMENT_SZ = 256;
constexpr int UNDEF_VAL = -9999;
template <typename T> void to_char(T t, char *str) noexcept {
  std::sprintf(str, "%d", UNDEF_VAL);
}
template <> void to_char(const char *t, char *str) noexcept {
  std::memcpy(str, t, std::strlen(t));
}
template <> void to_char(unsigned int t, char *str) noexcept {
  std::sprintf(str, "%u", t);
}
template <> void to_char(int t, char *str) noexcept {
  std::sprintf(str, "%d", t);
}
template <> void to_char(double t, char *str) noexcept {
  std::sprintf(str, "%f", t);
}
} // namespace fh_buffer_details

class FitsHeaderEntry {
private:
  char m_value[fh_buffer_details::VALUE_SZ];
  char m_comment[fh_buffer_details::COMMENT_SZ];

public:
  template <typename T>
  FitsHeaderEntry(T hrdata, const char *comment) noexcept {
#ifdef DEBUG
    assert(std::strlen(comment < fh_buffer_details::COMMENT_SZ));
#endif
    std::strcpy(m_comment, comment);
    fh_buffer_details::to_char(hrdata, m_value);
  }

  char *value() noexcept { return m_value[0]; }
  char *comment() noexcept { return m_comment[0]; }

}; // FitsHeaderEntry

class FitsHeaderBuffer {
public:
  /// @brief null constructor (does nothing)
  FitsHeaderBuffer() noexcept {};

  template <typename T>
  void update(const char *hdrname, T hrdata, const char *comment) noexcept {
    m_map[std::string(hdrname)] = FitsHeaderEntry<T>(hrdata, comment);
  }

private:
  std::map<std::string, FitsHeaderEntry> m_map;
}; // FitsHeaderBuffer
} // namespace aristarchos

#endif
