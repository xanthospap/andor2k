#ifndef __ARISTARCHOS_CCD_PARAMS_HPP__
#define __ARISTARCHOS_CCD_PARAMS_HPP__

#include <string>

namespace aristarchos {

namespace details {
// const char *parameter_file = "fitsimage.par";
constexpr int INT_UNDEF{9999};
} // namespace details

class CCDParams {
public:
  /// @brief Read and assign "CCD_TARGET_TEMP" value of from a CCD parameter
  ///        file
  int read_CCD_params(const char *filename) noexcept;

private:
  int m_target_temp{details::INT_UNDEF}, m_xsize{details::INT_UNDEF},
      m_ysize{details::INT_UNDEF};
  unsigned int m_last_temp_status_code{details::INT_UNDEF},
      m_last_CCD_status_code{details::INT_UNDEF};
}; // CCDParams

} // namespace aristarchos
#endif
