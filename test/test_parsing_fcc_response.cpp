#include "aristarchos.hpp"
#include <cstdio>
#include <fstream>

#define BZ2_BUFFER_SIZE 16384

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr,
            "ERROR! Usage: testParsingFCCResponse <BINARY HEADER FILE>\n");
    return 1;
  }

  std::ifstream fin(argv[1], std::ios::binary);

  char buffer[ARISTARCHOS_MAX_HEADER_SIZE];
  char ascii[BZ2_BUFFER_SIZE];

  fin.read(buffer, ARISTARCHOS_MAX_HEADER_SIZE);
  fin.close();

  unsigned ascii_len;
  char *ascii_str = decode_message(buffer, ascii, BZ2_BUFFER_SIZE, ascii_len);

  if (ascii_str == nullptr) {
    fprintf(stderr, "Error! Failed to decode binary stream!\n");
    return 1;
  }

  std::vector<FitsHeader> headers;
  int num_headers = decoded_str_to_header(ascii, ascii_len - 1, headers);

  return 0;
}
