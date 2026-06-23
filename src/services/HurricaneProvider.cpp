#include "HurricaneProvider.h"
#include "../core/Constants.h"
#include <SDL.h>
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <miniz.h>
#include <algorithm>
#include <functional>
#include "../core/StringUtils.h"
#include "../core/Logger.h"

using json = nlohmann::json;
using namespace tinyxml2;

HurricaneProvider::HurricaneProvider(NetworkManager &net,
                                     std::shared_ptr<HurricaneStore> store)
    : net_(net), store_(std::move(store)) {}

static std::vector<GeoPoint> parseCoordinates(const char* text) {
    std::vector<GeoPoint> pts;
    if (!text) return pts;
    std::string s(text);
    size_t start = 0;
    while (start < s.length()) {
        while (start < s.length() && std::isspace(s[start])) start++;
        if (start >= s.length()) break;

        size_t end = start;
        while (end < s.length() && !std::isspace(s[end])) end++;
        
        std::string chunk = s.substr(start, end - start);
        if (!chunk.empty()) {
            auto comma1 = chunk.find(',');
            if (comma1 != std::string::npos) {
                auto comma2 = chunk.find(',', comma1 + 1);
                std::string lon_s = chunk.substr(0, comma1);
                std::string lat_s = (comma2 != std::string::npos) ? chunk.substr(comma1 + 1, comma2 - comma1 - 1) : chunk.substr(comma1 + 1);
                pts.push_back({StringUtils::safe_stod(lat_s), StringUtils::safe_stod(lon_s)});
            }
        }
        start = end + 1;
    }
    return pts;
}

static std::vector<HurricaneStorm> parseGtwoKmz(const std::string& kmzData, const std::string& basinName) {
    std::vector<HurricaneStorm> disturbances;
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, kmzData.data(), kmzData.size(), 0)) {
        return disturbances;
    }
    
    int file_idx = -1;
    mz_uint num_files = mz_zip_reader_get_num_files(&zip);
    for (mz_uint f = 0; f < num_files; ++f) {
        char filename[256];
        if (mz_zip_reader_get_filename(&zip, f, filename, sizeof(filename))) {
            if (std::string(filename).find(".kml") != std::string::npos ||
                std::string(filename).find(".KML") != std::string::npos) {
                file_idx = f;
                break;
            }
        }
    }

    if (file_idx >= 0) {
        size_t uncomp_size;
        void* p = mz_zip_reader_extract_to_heap(&zip, file_idx, &uncomp_size, 0);
        if (p) {
            tinyxml2::XMLDocument doc;
            if (doc.Parse((const char*)p, uncomp_size) == tinyxml2::XML_SUCCESS) {
                tinyxml2::XMLElement* root = doc.FirstChildElement("kml");
                if (root) {
                    std::function<void(tinyxml2::XMLElement*)> findPlacemarks = [&](tinyxml2::XMLElement* el) {
                        if (!el) return;
                        if (std::string(el->Name()) == "Placemark") {
                            std::string distNum;
                            std::string prob7day;
                            tinyxml2::XMLElement* ext = el->FirstChildElement("ExtendedData");
                            if (ext) {
                                for (tinyxml2::XMLElement* dataEl = ext->FirstChildElement("Data"); dataEl; dataEl = dataEl->NextSiblingElement("Data")) {
                                    const char* name = dataEl->Attribute("name");
                                    if (name) {
                                        tinyxml2::XMLElement* val = dataEl->FirstChildElement("value");
                                        if (val && val->GetText()) {
                                            if (std::string(name) == "Disturbance") distNum = val->GetText();
                                            if (std::string(name) == "7day_percentage") prob7day = val->GetText();
                                        }
                                    }
                                }
                            }

                            if (!distNum.empty()) { 
                                std::string stormId = "invest_" + basinName + "_" + distNum;
                                HurricaneStorm* existing = nullptr;
                                for (auto& s : disturbances) {
                                    if (s.id == stormId) {
                                        existing = &s;
                                        break;
                                    }
                                }

                                HurricaneStorm storm;
                                if (existing) {
                                    storm = std::move(*existing);
                                } else {
                                    storm.id = stormId;
                                    storm.basin = basinName;
                                    storm.category = 0;
                                }

                                storm.name = distNum + " (" + prob7day + ")";

                                if (!prob7day.empty()) {
                                    try {
                                        storm.probability = std::stoi(prob7day);
                                    } catch (...) {}
                                }

                                tinyxml2::XMLElement* point = el->FirstChildElement("Point");
                                if (point) {
                                    tinyxml2::XMLElement* coords = point->FirstChildElement("coordinates");
                                    if (coords && coords->GetText()) {
                                        auto pts = parseCoordinates(coords->GetText());
                                        if (!pts.empty()) {
                                            storm.lat = pts[0].lat;
                                            storm.lon = pts[0].lon;
                                            storm.track.push_back(pts[0]);
                                        }
                                    }
                                }

                                tinyxml2::XMLElement* polygon = el->FirstChildElement("Polygon");
                                if (polygon) {
                                    tinyxml2::XMLElement* outer = polygon->FirstChildElement("outerBoundaryIs");
                                    if (outer) {
                                        tinyxml2::XMLElement* ring = outer->FirstChildElement("LinearRing");
                                        if (ring) {
                                            tinyxml2::XMLElement* coords = ring->FirstChildElement("coordinates");
                                            if (coords && coords->GetText()) {
                                                storm.cone = parseCoordinates(coords->GetText());
                                            }
                                        }
                                    }
                                }
                                
                                if (storm.lat == 0.0 && storm.lon == 0.0 && !storm.cone.empty()) {
                                    // No point provided, use polygon centroid for hit detection and X rendering
                                    double sumLat = 0, sumLon = 0;
                                    for (const auto& pt : storm.cone) {
                                        sumLat += pt.lat;
                                        sumLon += pt.lon;
                                    }
                                    storm.lat = sumLat / storm.cone.size();
                                    storm.lon = sumLon / storm.cone.size();
                                    storm.track.push_back({storm.lat, storm.lon});
                                }
                                
                                if (existing) {
                                    *existing = std::move(storm);
                                } else if (storm.lat != 0.0 || !storm.cone.empty()) {
                                    disturbances.push_back(std::move(storm));
                                }
                            }
                        }
                        for (tinyxml2::XMLElement* child = el->FirstChildElement(); child; child = child->NextSiblingElement()) {
                            findPlacemarks(child);
                        }
                    };
                    findPlacemarks(root);
                }
            }
            mz_free(p);
        }
    }
    mz_zip_reader_end(&zip);
    
    std::vector<HurricaneStorm> merged;
    for (auto& s : disturbances) {
        auto it = std::find_if(merged.begin(), merged.end(), [&](const HurricaneStorm& existing) {
            return existing.id == s.id;
        });
        if (it != merged.end()) {
            if (s.lat != 0.0) { it->lat = s.lat; it->lon = s.lon; it->track = s.track; }
            if (!s.cone.empty()) { it->cone = s.cone; }
            if (it->name.find("%") == std::string::npos && s.name.find("%") != std::string::npos) {
                it->name = s.name;
            }
        } else {
            merged.push_back(s);
        }
    }

    return merged;
}

static void parseActiveStormKmz(const std::string& kmzData, HurricaneStorm& baseStorm) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, kmzData.data(), kmzData.size(), 0)) {
        return;
    }
    
    int file_idx = -1;
    mz_uint num_files = mz_zip_reader_get_num_files(&zip);
    for (mz_uint f = 0; f < num_files; ++f) {
        char filename[256];
        if (mz_zip_reader_get_filename(&zip, f, filename, sizeof(filename))) {
            if (std::string(filename).find(".kml") != std::string::npos ||
                std::string(filename).find(".KML") != std::string::npos) {
                file_idx = f;
                break;
            }
        }
    }

    if (file_idx >= 0) {
        size_t uncomp_size;
        void* p = mz_zip_reader_extract_to_heap(&zip, file_idx, &uncomp_size, 0);
        if (p) {
            tinyxml2::XMLDocument doc;
            if (doc.Parse((const char*)p, uncomp_size) == tinyxml2::XML_SUCCESS) {
                tinyxml2::XMLElement* root = doc.FirstChildElement("kml");
                if (root) {
                    std::function<void(tinyxml2::XMLElement*)> findCoords = [&](tinyxml2::XMLElement* el) {
                        if (!el) return;
                        if (std::string(el->Name()) == "Placemark") {
                            tinyxml2::XMLElement* nameEl = el->FirstChildElement("name");
                            std::string pName = nameEl && nameEl->GetText() ? nameEl->GetText() : "";
                            
                            // If baseStorm name is empty and we find a name that's not just "Forecast" or "RADIUS...", we could use it
                            if (baseStorm.name.empty() && !pName.empty() && pName != "Forecast" && pName.find("RADIUS") == std::string::npos && pName.find("JTWC") == std::string::npos) {
                                baseStorm.name = pName;
                            }

                            tinyxml2::XMLElement* lineString = el->FirstChildElement("LineString");
                            if (lineString) {
                                tinyxml2::XMLElement* coords = lineString->FirstChildElement("coordinates");
                                if (coords && coords->GetText()) {
                                    if (baseStorm.track.empty()) {
                                        baseStorm.track = parseCoordinates(coords->GetText());
                                    }
                                }
                            }

                            // Only append <Point> to track if it's not JTWC.
                            // JTWC's LineString already contains the full track, and appending Points causes loops.
                            tinyxml2::XMLElement* point = el->FirstChildElement("Point");
                            if (point && baseStorm.id.find("jtwc") == std::string::npos) {
                                tinyxml2::XMLElement* coords = point->FirstChildElement("coordinates");
                                if (coords && coords->GetText()) {
                                    auto pts = parseCoordinates(coords->GetText());
                                    if (!pts.empty()) {
                                        baseStorm.track.push_back(pts[0]);
                                    }
                                }
                            }
                            
                            tinyxml2::XMLElement* polygon = el->FirstChildElement("Polygon");
                            if (polygon) {
                                tinyxml2::XMLElement* outer = polygon->FirstChildElement("outerBoundaryIs");
                                if (outer) {
                                    tinyxml2::XMLElement* ring = outer->FirstChildElement("LinearRing");
                                    if (ring) {
                                        tinyxml2::XMLElement* coords = ring->FirstChildElement("coordinates");
                                        if (coords && coords->GetText()) {
                                            auto newPoly = parseCoordinates(coords->GetText());
                                            if (!newPoly.empty()) {
                                                if (baseStorm.cone.empty()) {
                                                    baseStorm.cone = newPoly;
                                                } else if (newPoly.size() > baseStorm.cone.size()) {
                                                    baseStorm.windRadii.push_back(baseStorm.cone);
                                                    baseStorm.cone = newPoly;
                                                } else {
                                                    baseStorm.windRadii.push_back(newPoly);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        for (tinyxml2::XMLElement* child = el->FirstChildElement(); child; child = child->NextSiblingElement()) {
                            findCoords(child);
                        }
                    };
                    findCoords(root);
                }
            }
            mz_free(p);
        }
    }
    mz_zip_reader_end(&zip);
    
    // Set lat/lon from the first point in track
    if (!baseStorm.track.empty() && baseStorm.lat == 0.0) {
        baseStorm.lat = baseStorm.track.back().lat;
        baseStorm.lon = baseStorm.track.back().lon;
    }
}

void HurricaneProvider::fetch(bool force) {
  lastFetchMs_ = SDL_GetTicks();
  static const char *kUrl = "https://www.nhc.noaa.gov/CurrentStorms.json";

  std::weak_ptr<HurricaneProvider> weakSelf = shared_from_this();

  net_.fetchAsync(
      kUrl,
      [weakSelf](std::string body) {
        if (body.empty()) return;

        auto self = weakSelf.lock();
        if (!self) return;

        WorkerService::getInstance().submitTask([self, body]() {
          try {
            auto j = json::parse(body);
            auto update = std::make_shared<HurricaneData>();

            const json *storms = nullptr;
            if (j.contains("activeStorms") && j["activeStorms"].is_array())
              storms = &j["activeStorms"];

            if (storms) {
              for (const auto &s : *storms) {
                HurricaneStorm storm;
                auto str = [&](const char *k) -> std::string { return (s.contains(k) && s[k].is_string()) ? s[k].get<std::string>() : ""; };
                storm.id = str("id");
                storm.name = str("name");
                storm.basin = str("basin");
                storm.advisory = str("headline");
                storm.movement = str("movement");

                if (s.contains("lat") && s["lat"].is_number()) storm.lat = s["lat"].get<double>();
                if (s.contains("lon") && s["lon"].is_number()) storm.lon = s["lon"].get<double>();
                if (s.contains("intensity") && s["intensity"].is_number()) storm.maxWindKt = s["intensity"].get<int>();
                if (s.contains("pressure") && s["pressure"].is_number()) storm.pressureMb = s["pressure"].get<int>();
                if (s.contains("category") && s["category"].is_number()) storm.category = s["category"].get<int>();

                auto ts = str("lastUpdate");
                storm.lastUpdate = ts.size() > 16 ? ts.substr(0, 16) : ts;

                if (!storm.name.empty()) update->storms.push_back(std::move(storm));
              }
            }

            update->valid = true;
            update->lastUpdate = std::chrono::system_clock::now();


            auto updateMutex = std::make_shared<std::mutex>();

            std::vector<std::pair<std::string, std::string>> gtwoUrls = {
                {"https://www.nhc.noaa.gov/xgtwo/gtwo_atl.kmz", "AL"},
                {"https://www.nhc.noaa.gov/xgtwo/gtwo_pac.kmz", "EP"},
                {"https://www.nhc.noaa.gov/xgtwo/gtwo_cpac.kmz", "CP"}
            };

            auto pending = std::make_shared<std::atomic<int>>(update->storms.size() + gtwoUrls.size() + 1);

            auto finalize = [pending, update, self, updateMutex]() {
                if (pending->fetch_sub(1) == 1) {
                    std::lock_guard<std::mutex> lk(*updateMutex);
                    
                    // Clean up names
                    for (auto& s : update->storms) {
                        if (s.name.empty() || s.name == "TCFA AREA" || s.name.find("Storm Track") != std::string::npos) {
                            std::string n = s.id;
                            size_t uscore = n.find('_');
                            if (uscore != std::string::npos) n = n.substr(uscore + 1);
                            for (char& c : n) c = std::toupper(c);
                            s.name = n;
                        }
                    }

                    // Sort storms: Named/Typhoons first (by category descending), then by name
                    std::sort(update->storms.begin(), update->storms.end(), [](const HurricaneStorm& a, const HurricaneStorm& b) {
                        if (a.category != b.category) return a.category > b.category;
                        return a.name < b.name;
                    });

                    // Deduplicate storms (GTWO EP and CP often overlap, causing duplicates)
                    std::vector<HurricaneStorm> uniqueStorms;
                    for (auto& s : update->storms) {
                        bool dup = false;
                        for (auto& u : uniqueStorms) {
                            if (u.name == s.name && u.category == s.category) {
                                if (std::abs(u.lat - s.lat) < 5.0 && std::abs(u.lon - s.lon) < 5.0) {
                                    dup = true;
                                    break;
                                }
                            }
                        }
                        if (!dup) uniqueStorms.push_back(std::move(s));
                    }
                    update->storms = std::move(uniqueStorms);

                    SDL_Event event;
                    SDL_zero(event);
                    event.type = HamClock::AE_BASE_EVENT + HamClock::AE_HURRICANE_DATA_READY;
                    event.user.data1 = new HurricaneData(*update);
                    SDL_PushEvent(&event);
                }
            };

            // Fetch JTWC RSS
            self->net_.fetchAsync("https://www.metoc.navy.mil/jtwc/rss/jtwc.rss", [update, updateMutex, pending, self, finalize](std::string rssBody) {
                if (!rssBody.empty()) {
                    WorkerService::getInstance().submitTask([update, updateMutex, pending, self, rssBody, finalize]() mutable {
                        std::vector<std::string> jtwcKmzUrls;
                        
                        tinyxml2::XMLDocument doc;
                        if (doc.Parse(rssBody.c_str()) == tinyxml2::XML_SUCCESS) {
                            tinyxml2::XMLElement* root = doc.FirstChildElement("rss");
                            if (root) {
                                tinyxml2::XMLElement* channel = root->FirstChildElement("channel");
                                if (channel) {
                                    for (tinyxml2::XMLElement* item = channel->FirstChildElement("item"); item; item = item->NextSiblingElement("item")) {
                                        tinyxml2::XMLElement* desc = item->FirstChildElement("description");
                                        if (desc && desc->GetText()) {
                                            std::string text = desc->GetText();
                                            size_t pos = 0;
                                            while ((pos = text.find("href='", pos)) != std::string::npos) {
                                                pos += 6;
                                                size_t end = text.find("'", pos);
                                                if (end != std::string::npos) {
                                                    std::string url = text.substr(pos, end - pos);
                                                    if (url.find(".kmz") != std::string::npos) {
                                                        jtwcKmzUrls.push_back(url);
                                                    }
                                                    pos = end;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (!jtwcKmzUrls.empty()) {
                            pending->fetch_add(jtwcKmzUrls.size());
                            for (const auto& kmzUrl : jtwcKmzUrls) {
                                std::string stormId = "unknown";
                                size_t slash = kmzUrl.find_last_of('/');
                                if (slash != std::string::npos) {
                                    stormId = kmzUrl.substr(slash + 1);
                                    size_t dot = stormId.find('.');
                                    if (dot != std::string::npos) stormId = stormId.substr(0, dot);
                                }
                                
                                bool isInvest = (stormId.size() >= 4 && stormId[2] == '9');
                                
                                HurricaneStorm baseStorm;
                                baseStorm.id = (isInvest ? "invest_" : "jtwc_") + stormId;
                                baseStorm.basin = "WP";
                                baseStorm.category = isInvest ? 0 : 1;

                                self->net_.fetchAsync(kmzUrl, [update, updateMutex, stormId, baseStorm, finalize](std::string kmzData) {
                                    if (!kmzData.empty()) {
                                        WorkerService::getInstance().submitTask([update, updateMutex, kmzData, stormId, baseStorm, finalize]() mutable {
                                            parseActiveStormKmz(kmzData, baseStorm);
                                            
                                            if (!baseStorm.track.empty()) {
                                                std::lock_guard<std::mutex> lk(*updateMutex);
                                                update->storms.push_back(std::move(baseStorm));
                                            }
                                            finalize();
                                        });
                                    } else {
                                        finalize();
                                    }
                                }, 5000, true);
                            }
                        }
                        finalize();
                    });
                } else {
                    finalize();
                }
            }, 5000, true);

            for (const auto& pair : gtwoUrls) {
                std::string kmzUrl = pair.first;
                std::string basin = pair.second;
                self->net_.fetchAsync(kmzUrl, [update, updateMutex, basin, finalize](std::string kmzData) {
                    if (!kmzData.empty()) {
                        WorkerService::getInstance().submitTask([update, updateMutex, basin, kmzData, finalize]() {
                            auto merged = parseGtwoKmz(kmzData, basin);
                            if (!merged.empty()) {
                                std::lock_guard<std::mutex> lk(*updateMutex);
                                for (auto& s : merged) {
                                    update->storms.push_back(std::move(s));
                                }
                            }
                            finalize();
                        });
                    } else {
                        finalize();
                    }
                }, 5000, true);
            }

            for (size_t i = 0; i < update->storms.size(); ++i) {
                std::string stormIdUpper = update->storms[i].id;
                for (auto &c : stormIdUpper) c = std::toupper(c);
                
                char kmzUrl[256];
                if (stormIdUpper == "AL112017") {
                    std::snprintf(kmzUrl, sizeof(kmzUrl), "https://www.nhc.noaa.gov/gis/examples/al112017_best_track.kmz");
                } else {
                    std::snprintf(kmzUrl, sizeof(kmzUrl), "https://www.nhc.noaa.gov/storm_graphics/api/%s_5day_latest.kmz", stormIdUpper.c_str());
                }
                
                HurricaneStorm baseStorm = update->storms[i];
                self->net_.fetchAsync(kmzUrl, [update, pending, updateMutex, i, stormIdUpper, baseStorm, finalize](std::string kmzData) {
                    if (!kmzData.empty()) {
                        WorkerService::getInstance().submitTask([update, pending, updateMutex, i, kmzData, stormIdUpper, baseStorm, finalize]() mutable {
                            parseActiveStormKmz(kmzData, baseStorm);
                            
                            size_t trackSize = baseStorm.track.size();
                            size_t coneSize = baseStorm.cone.size();

                            if (updateMutex) { 
                                std::lock_guard<std::mutex> lk(*updateMutex); 
                                update->storms[i] = std::move(baseStorm); 
                            }
                            
                            LOG_I("HurricaneProvider", "Storm {} loaded {} track pts, {} cone pts",
                                  stormIdUpper, trackSize, coneSize);

                            finalize();
                        });
                    } else {
                        finalize();
                    }
                }, 5000, true); // short timeout for KMZ
            }
          } catch (...) {
          }
        });
      },
      1800, force);
}
