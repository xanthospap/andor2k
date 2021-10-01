#ifndef __ARISTARCHOS_DECODE_HPP__
#define __ARISTARCHOS_DECODE_HPP__

int base64decode_len(const char *bufcoded) noexcept;
int base64decode(char *bufplain, const char *bufcoded) noexcept;
int base64encode_len(int len) noexcept;
int base64encode(char *encoded, const char *string, int len) noexcept;

#endif
