#pragma once

#include <optional>
#include <string>

struct IonosondeStation {
  std::string code;
  std::string name;
  double lat;
  double lon;
  double foF2;
  std::optional<double> mufd; // MUF(3000)F2
  std::optional<double> hmF2;
  double md = 3.0; // M(3000)F2 factor
  int confidence;
  double timestamp;
};

struct InterpolatedIonosonde {
  double foF2 = 0.0;
  std::optional<double> mufd;
  std::optional<double> hmF2;
  double md = 3.0;

  // Quality markers
  int stationsUsed = 0;
  double nearestDistance = 1e9;
};
