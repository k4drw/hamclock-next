#pragma once

#include <cstdio>
#include <string>

enum class WidgetType {
  SOLAR,
  DX_CLUSTER,
  LIVE_SPOTS,
  BAND_CONDITIONS,
  CONTESTS,
  ON_THE_AIR,
  GIMBAL,
  MOON,
  CLOCK_AUX,
  DX_PEDITIONS,
  DE_WEATHER,
  DX_WEATHER,
  NCDXF,
  SDO,
  HISTORY_FLUX,
  HISTORY_KP,
  HISTORY_SSN,
  HISTORY_XRAY,
  DRAP,
  AURORA,
  AURORA_GRAPH,
  ADIF,
  COUNTDOWN,
  CALLBOOK,
  DST_INDEX,
  WATCHLIST,
  EME_TOOL,
  SANTA_TRACKER,
  CPU_TEMP,
};

inline const char *widgetTypeToString(WidgetType t) {
  switch (t) {
  case WidgetType::SOLAR:
    return "solar";
  case WidgetType::DX_CLUSTER:
    return "dx_cluster";
  case WidgetType::LIVE_SPOTS:
    return "live_spots";
  case WidgetType::BAND_CONDITIONS:
    return "band_conditions";
  case WidgetType::CONTESTS:
    return "contests";
  case WidgetType::ON_THE_AIR:
    return "on_the_air";
  case WidgetType::GIMBAL:
    return "gimbal";
  case WidgetType::MOON:
    return "moon";
  case WidgetType::CLOCK_AUX:
    return "clock_aux";
  case WidgetType::DX_PEDITIONS:
    return "dx_peditions";
  case WidgetType::DE_WEATHER:
    return "de_weather";
  case WidgetType::DX_WEATHER:
    return "dx_weather";
  case WidgetType::NCDXF:
    return "ncdxf";
  case WidgetType::SDO:
    return "sdo";
  case WidgetType::HISTORY_FLUX:
    return "history_flux";
  case WidgetType::HISTORY_KP:
    return "history_kp";
  case WidgetType::HISTORY_SSN:
    return "history_ssn";
  case WidgetType::HISTORY_XRAY:
    return "history_xray";
  case WidgetType::DRAP:
    return "drap";
  case WidgetType::AURORA:
    return "aurora";
  case WidgetType::AURORA_GRAPH:
    return "aurora_graph";
  case WidgetType::ADIF:
    return "adif";
  case WidgetType::COUNTDOWN:
    return "countdown";
  case WidgetType::CALLBOOK:
    return "callbook";
  case WidgetType::DST_INDEX:
    return "dst_index";
  case WidgetType::WATCHLIST:
    return "watchlist";
  case WidgetType::EME_TOOL:
    return "eme_tool";
  case WidgetType::SANTA_TRACKER:
    return "santa_tracker";
  case WidgetType::CPU_TEMP:
    return "cpu_temp";
  }
  return "solar";
}

inline const char *widgetTypeDisplayName(WidgetType t) {
  switch (t) {
  case WidgetType::SOLAR:
    return "Solar";
  case WidgetType::DX_CLUSTER:
    return "DX Cluster";
  case WidgetType::LIVE_SPOTS:
    return "Live Spots";
  case WidgetType::BAND_CONDITIONS:
    return "Band Cond";
  case WidgetType::CONTESTS:
    return "Contests";
  case WidgetType::ON_THE_AIR:
    return "On The Air";
  case WidgetType::GIMBAL:
    return "Gimbal";
  case WidgetType::MOON:
    return "Moon";
  case WidgetType::CLOCK_AUX:
    return "Clock Aux";
  case WidgetType::DX_PEDITIONS:
    return "DX Peditions";
  case WidgetType::DE_WEATHER:
    return "DE Weather";
  case WidgetType::DX_WEATHER:
    return "DX Weather";
  case WidgetType::NCDXF:
    return "NCDXF";
  case WidgetType::SDO:
    return "SDO";
  case WidgetType::HISTORY_FLUX:
    return "Solar Flux";
  case WidgetType::HISTORY_KP:
    return "K-Index";
  case WidgetType::HISTORY_SSN:
    return "Sunspots";
  case WidgetType::HISTORY_XRAY:
    return "X-Ray Flux";
  case WidgetType::DRAP:
    return "DRAP";
  case WidgetType::AURORA:
    return "Aurora";
  case WidgetType::AURORA_GRAPH:
    return "Aurora Graph";
  case WidgetType::ADIF:
    return "ADIF Log";
  case WidgetType::COUNTDOWN:
    return "Countdown";
  case WidgetType::CALLBOOK:
    return "Callbook";
  case WidgetType::DST_INDEX:
    return "Dst Index";
  case WidgetType::WATCHLIST:
    return "Watchlist";
  case WidgetType::EME_TOOL:
    return "EME Tool";
  case WidgetType::SANTA_TRACKER:
    return "Santa Tracker";
  case WidgetType::CPU_TEMP:
    return "CPU Temp";
  }
  return "Solar";
}

inline WidgetType widgetTypeFromString(const std::string &s,
                                       WidgetType fallback) {
  if (s == "solar")
    return WidgetType::SOLAR;
  if (s == "dx_cluster")
    return WidgetType::DX_CLUSTER;
  if (s == "live_spots")
    return WidgetType::LIVE_SPOTS;
  if (s == "band_conditions")
    return WidgetType::BAND_CONDITIONS;
  if (s == "contests")
    return WidgetType::CONTESTS;
  if (s == "on_the_air")
    return WidgetType::ON_THE_AIR;
  if (s == "gimbal")
    return WidgetType::GIMBAL;
  if (s == "moon")
    return WidgetType::MOON;
  if (s == "clock_aux")
    return WidgetType::CLOCK_AUX;
  if (s == "dx_peditions")
    return WidgetType::DX_PEDITIONS;
  if (s == "de_weather")
    return WidgetType::DE_WEATHER;
  if (s == "dx_weather")
    return WidgetType::DX_WEATHER;
  if (s == "ncdxf")
    return WidgetType::NCDXF;
  if (s == "sdo")
    return WidgetType::SDO;
  if (s == "history_flux")
    return WidgetType::HISTORY_FLUX;
  if (s == "history_kp")
    return WidgetType::HISTORY_KP;
  if (s == "history_ssn")
    return WidgetType::HISTORY_SSN;
  if (s == "history_xray")
    return WidgetType::HISTORY_XRAY;
  if (s == "drap")
    return WidgetType::DRAP;
  if (s == "aurora")
    return WidgetType::AURORA;
  if (s == "aurora_graph")
    return WidgetType::AURORA_GRAPH;
  if (s == "adif")
    return WidgetType::ADIF;
  if (s == "countdown")
    return WidgetType::COUNTDOWN;
  if (s == "callbook")
    return WidgetType::CALLBOOK;
  if (s == "dst_index")
    return WidgetType::DST_INDEX;
  if (s == "watchlist")
    return WidgetType::WATCHLIST;
  if (s == "eme_tool")
    return WidgetType::EME_TOOL;
  if (s == "santa_tracker")
    return WidgetType::SANTA_TRACKER;
  if (s == "cpu_temp")
    return WidgetType::CPU_TEMP;
  std::fprintf(stderr, "WidgetType: unknown '%s', using fallback\n", s.c_str());
  return fallback;
}
