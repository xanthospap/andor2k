#include "andor2k.hpp"
#include <cstdio>

void usage(const char *prog_name) noexcept {
  printf("Usage: %s [OPTIONS]\n", prog_name);
  printf("Options can be any of the following:\n");
  printf("--vbin VBIN\n");
  printf("\tVBIN must be an integer value that defines vertical binning\n."
         "\tDefault is 1\n");
  printf("--hbin HBIN\n");
  printf("\tHBIN must be an integer value that defines horizontal binning\n."
         "\tDefault is 1\n");
  printf("--bin BIN\n");
  printf(
      "\tIn case you want to define the vertical and horizontal binning\n"
      "\tto the same value, you can use this switch. E.g. setting \"--bin 1\"\n"
      "\twould have the same affect as setting: \"--vbin 2 --hbin 2\"\n");
  printf("--hstart HSTART\n");
  printf("\tSpecify the horizontal start pixel (inclusive); valid values "
         "[1,2048]\n"
         "\tDefault value is 1\n");
  printf("--hend HEND\n");
  printf("\tSpecify the ending horizontal pixel (inclusive); valid values "
         "[1,2048]\n"
         "\tDefault value is 2048\n");
  printf("--vstart VSTART\n");
  printf(
      "\tSpecify the vertical start pixel (inclusive); valid values [1,2048]\n"
      "\tDefault value is 2048\n");
  printf("--vend VEND\n");
  printf(
      "\tSpecify the ending vertical pixel (inclusive); valid values [1,2048]\n"
      "\tDefault value is 2048\n");
  printf("--filename FILENAME\n");
  printf(
      "\tSpecify the filename of the exposure; do not prepend path. The\n"
      "\t\".fits\" extension will be added to the filename (do not provide it\n"
      "\there). An index nmber will also be appended to the provided "
      "filename.\n");
}