#include "fits_header.hpp"
#include <cstring>
#include <cstdio>

void print_headers(const FitsHeaders& hdrs) noexcept {
    int count = 0;
    printf("<--- Printing Headers --->\n");
    for (auto const& h: hdrs.mvec) {
        printf("[%d] %8s = %32s / %s\n", count, h.key, h.val, h.comment);
        ++count;
    }
    return;
}

int main() {
    FitsHeaders headers;

    headers.update("KEY1", "VALUE1", "my first comment!");
    headers.update("  KEY2", "VALUE2", "my second comment!");
    headers.update("  KEY3 ", "VALUE3", "my third comment!");
    headers.update("KEY4  ", "VALUE4", "my fourth comment!");
    headers.update(" KEY5 ", "VALUE5", "my fifth comment!");

    char k[64], v[64], c[64];
    std::strcpy(k, " KEY6  ");
    std::strcpy(v, "29873498237");
    std::strcpy(c, "yet another comment");
    headers.update(k, v, c);

    print_headers(headers);

    std::memset(k, 0, 64);
    std::memset(v, 0, 64);
    std::memset(c, 0, 64);
    std::strcpy(k, " KEY1  ");
    std::strcpy(v, "1");
    std::strcpy(c, "should replace comment 1");
    headers.update(k, v, c);
    
    print_headers(headers);

    std::memset(k, 0, 64);
    std::memset(v, 0, 64);
    std::memset(c, 0, 64);
    std::strcpy(k, " KEY1123  ");
    std::strcpy(v, "1123");
    std::strcpy(c, "comment lost count!!");
    headers.update(k, v, c);

    print_headers(headers);

    std::memset(k, 0, 64);
    std::memset(v, 0, 64);
    std::memset(c, 0, 64);
    std::strcpy(k, " KEY5  ");
    std::strcpy(v, "1123");
    std::strcpy(c, "comment lost count!!");
    headers.update(k, v, c);

    print_headers(headers);

    printf("\n");
    return 0;
}