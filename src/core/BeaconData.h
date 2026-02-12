#pragma once

#include <string>
#include <vector>

struct Beacon {
  std::string callsign;
  std::string location;
  double lat;
  double lon;
};

const std::vector<Beacon> NCDXF_BEACONS = {
    {"4U1UN", "United Nations", 40.7, -74.0},
    {"VE8AT", "Canada", 62.5, -114.4},
    {"W6WX", "United States", 37.4, -122.2},
    {"KH6WO", "Hawaii", 21.3, -157.8},
    {"ZL6B", "New Zealand", -41.3, 174.8},
    {"VK6RBP", "Australia", -32.0, 115.9},
    {"JA2IGY", "Japan", 34.5, 136.7},
    {"RR9O", "Russia", 55.0, 82.9},
    {"VR2B", "Hong Kong", 22.3, 114.2},
    {"4S7B", "Sri Lanka", 6.9, 79.9},
    {"ZS6DN", "South Africa", -25.8, 28.2},
    {"5Z4B", "Kenya", -1.3, 36.8},
    {"4X6TU", "Israel", 32.1, 34.8},
    {"OH2B", "Finland", 60.2, 24.9},
    {"CS3B", "Madeira", 32.7, -16.9},
    {"LU4AA", "Argentina", -34.6, -58.4},
    {"OA4B", "Peru", -12.1, -77.0},
    {"YV5B", "Venezuela", 10.5, -66.9}};

const std::vector<int> BEACON_BANDS = {14100, 18110, 21150, 24930, 28200};

enum class BeaconStatus { OFF, TRANSMITTING, WAITING };
