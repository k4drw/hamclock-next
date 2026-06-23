#pragma once

#include <vector>
#include <map>
#include <tuple>
#include <cstdint>

struct GribField {
  int discipline;
  int category;
  int number;
  int nx;
  int ny;
  std::vector<float> values;
};

class GribDecoder {
public:
  // Parses a GRIB2 file and returns all decoded fields.
  static std::vector<GribField> decode(const std::vector<uint8_t> &data);
};
