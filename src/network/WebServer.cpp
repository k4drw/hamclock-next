#include "WebServer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ui/stb_image_write.h"

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/WatchlistStore.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../core/Logger.h"

#ifdef ENABLE_DEBUG_API
#include "../core/Astronomy.h"
#include "../core/UIRegistry.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#endif

WebServer::WebServer(SDL_Renderer *renderer, AppConfig &cfg,
                     HamClockState &state, ConfigManager &cfgMgr,
                     std::shared_ptr<WatchlistStore> watchlist,
                     std::shared_ptr<SolarDataStore> solar, int port)
    : renderer_(renderer), cfg_(&cfg), state_(&state), cfgMgr_(&cfgMgr),
      watchlist_(watchlist), solar_(solar), port_(port) {}

WebServer::~WebServer() { stop(); }

void WebServer::start() {
  if (running_)
    return;
  running_ = true;
  thread_ = std::thread(&WebServer::run, this);
}

void WebServer::stop() {
  running_ = false;
  if (svrPtr_) {
    static_cast<httplib::Server *>(svrPtr_)->stop();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  svrPtr_ = nullptr;
}

static void stbi_write_to_vector(void *context, void *data, int size) {
  auto *vec = static_cast<std::vector<unsigned char> *>(context);
  auto *bytes = static_cast<unsigned char *>(data);
  vec->insert(vec->end(), bytes, bytes + size);
}

void WebServer::updateFrame() {
  if (!needsCapture_)
    return;

  uint32_t now = SDL_GetTicks();
  // Throttle to 500ms (2 FPS) instead of 100ms (10 FPS) for lower CPU usage
  if (now - lastCaptureTicks_ < 250)
    return;
  lastCaptureTicks_ = now;

  int w, h;
  SDL_GetRendererOutputSize(renderer_, &w, &h);

  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGB24);
  if (!surface)
    return;

  if (SDL_RenderReadPixels(renderer_, NULL, SDL_PIXELFORMAT_RGB24,
                           surface->pixels, surface->pitch) == 0) {
    std::vector<unsigned char> jpeg;
    stbi_write_jpg_to_func(stbi_write_to_vector, &jpeg, w, h, 3,
                           surface->pixels, 70);

    std::lock_guard<std::mutex> lock(jpegMutex_);
    latestJpeg_ = std::move(jpeg);
    needsCapture_ = false;
  }

  SDL_FreeSurface(surface);
}

void WebServer::run() {
  httplib::Server svr;
  svrPtr_ = &svr;

  svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
    std::string html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>HamClock-Next Live (v)HTML";
    html += HAMCLOCK_VERSION;
    html += R"HTML()</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { background: #000; color: #00ff00; text-align: center; font-family: monospace; margin: 0; padding: 10px; overflow: hidden; outline: none; }
        #screen-container { display: inline-block; position: relative; border: 2px solid #333; line-height: 0; background: #111; }
        #screen { max-width: 100vw; max-height: 85vh; cursor: crosshair; }
        .status { margin-top: 15px; font-size: 0.8em; color: #666; }
        /* Hidden input: truly invisible but still focusable */
        #kbd-hidden { 
            position: absolute; 
            left: -500px; 
            width: 1px; 
            height: 1px; 
            opacity: 0; 
            border: none; 
            background: transparent; 
        }
    </style>
</head>
<body>
    <input type="text" id="kbd-hidden" autocomplete="off" autoconnect="off" autofocus>
    <div id="screen-container">
        <img id="screen" src="/live.jpg" draggable="false">
    </div>

    <div class="status">v)HTML";
    html += HAMCLOCK_VERSION;
    html += R"HTML( | Type anywhere to send keys | Click screen for touch</div>

    <script>
        const img = document.getElementById('screen');
        const kbd = document.getElementById('kbd-hidden');
        
        // Refresh rate in milliseconds (500ms = 2 FPS for lower CPU usage)
        // Change to 1000 for 1 FPS (lower CPU usage)
        // Change to 250 for 4 FPS (higher CPU usage)
        const REFRESH_RATE = 500;
        
        function refresh() {
            const nextImg = new Image();
            nextImg.onload = () => { img.src = nextImg.src; };
            nextImg.src = '/live.jpg?t=' + Date.now();
        }
        setInterval(refresh, REFRESH_RATE);

        // Click anywhere to ensure input focus
        document.addEventListener('mousedown', function() {
            kbd.focus();
        });

        img.addEventListener('mousedown', function(e) {
            const rect = img.getBoundingClientRect();
            const rx = (e.clientX - rect.left) / rect.width;
            const ry = (e.clientY - rect.top) / rect.height;
            fetch('/set_touch?rx=' + rx + '&ry=' + ry);
        });

        function sendKey(k) {
            fetch('/set_char?k=' + encodeURIComponent(k));
        }

        // Global Desktop Key Handling
        window.addEventListener('keydown', function(e) {
            const named = ['Backspace', 'Tab', 'Enter', 'Escape', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Delete', 'Home', 'End'];
            if (named.includes(e.key)) {
                e.preventDefault();
                sendKey(e.key);
            }
        });

        window.addEventListener('keypress', function(e) {
            e.preventDefault();
            sendKey(e.key);
        });

        img.addEventListener('contextmenu', e => e.preventDefault());
        
        // Final focus push
        setInterval(() => { if (document.activeElement !== kbd) kbd.focus(); }, 1000);
    </script>
</body>
</html>
)HTML";
    res.set_content(html, "text/html");
  });

  svr.Get("/live.jpg", [this](const httplib::Request &,
                              httplib::Response &res) {
    needsCapture_ = true;
    for (int i = 0; i < 10; ++i) {
      {
        std::lock_guard<std::mutex> lock(jpegMutex_);
        if (!latestJpeg_.empty()) {
          res.set_content(reinterpret_cast<const char *>(latestJpeg_.data()),
                          latestJpeg_.size(), "image/jpeg");
          return;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    res.status = 503;
  });

  svr.Get("/set_touch",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("rx") && req.has_param("ry")) {
              float rx = std::stof(req.get_param_value("rx"));
              float ry = std::stof(req.get_param_value("ry"));
              int w = 800, h = 480;
              SDL_GetRendererOutputSize(renderer_, &w, &h);
              int px = static_cast<int>(rx * w);
              int py = static_cast<int>(ry * h);

              SDL_Event event;
              SDL_zero(event);
              event.type = SDL_MOUSEBUTTONDOWN;
              event.button.button = SDL_BUTTON_LEFT;
              event.button.state = SDL_PRESSED;
              event.button.x = px;
              event.button.y = py;
              SDL_PushEvent(&event);

              SDL_zero(event);
              event.type = SDL_MOUSEBUTTONUP;
              event.button.button = SDL_BUTTON_LEFT;
              event.button.state = SDL_RELEASED;
              event.button.x = px;
              event.button.y = py;
              SDL_PushEvent(&event);
            }
            res.set_content("ok", "text/plain");
          });

  svr.Get("/set_char", [](const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("k")) {
      std::string k = req.get_param_value("k");
      SDL_Event event;
      SDL_zero(event);

      SDL_Keycode code = SDLK_UNKNOWN;
      if (k == "Enter")
        code = SDLK_RETURN;
      else if (k == "Backspace")
        code = SDLK_BACKSPACE;
      else if (k == "Tab")
        code = SDLK_TAB;
      else if (k == "Escape")
        code = SDLK_ESCAPE;
      else if (k == "ArrowLeft")
        code = SDLK_LEFT;
      else if (k == "ArrowRight")
        code = SDLK_RIGHT;
      else if (k == "ArrowUp")
        code = SDLK_UP;
      else if (k == "ArrowDown")
        code = SDLK_DOWN;
      else if (k == "Delete")
        code = SDLK_DELETE;
      else if (k == "Home")
        code = SDLK_HOME;
      else if (k == "End")
        code = SDLK_END;
      else if (k.length() == 1)
        code = k[0];

      if (code != SDLK_UNKNOWN) {
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = code;
        event.key.state = SDL_PRESSED;
        SDL_PushEvent(&event);

        event.type = SDL_KEYUP;
        event.key.state = SDL_RELEASED;
        SDL_PushEvent(&event);

        if (k.length() == 1 && std::isprint(static_cast<unsigned char>(k[0]))) {
          SDL_Event tevent;
          SDL_zero(tevent);
          tevent.type = SDL_TEXTINPUT;
          std::snprintf(tevent.text.text, sizeof(tevent.text.text), "%s",
                        k.c_str());
          SDL_PushEvent(&tevent);
        }
      }
    }
    res.set_content("ok", "text/plain");
  });

#ifdef ENABLE_DEBUG_API
  svr.Get("/debug/widgets",
          [](const httplib::Request &, httplib::Response &res) {
            auto snapshot = UIRegistry::getInstance().getSnapshot();
            nlohmann::json j = nlohmann::json::object();

            for (const auto &[id, info] : snapshot) {
              nlohmann::json w = nlohmann::json::object();
              w["rect"] = {info.rect.x, info.rect.y, info.rect.w, info.rect.h};
              nlohmann::json actions = nlohmann::json::array();
              for (const auto &action : info.actions) {
                nlohmann::json a = nlohmann::json::object();
                a["name"] = action.name;
                a["rect"] = {action.rect.x, action.rect.y, action.rect.w,
                             action.rect.h};
                actions.push_back(a);
              }
              w["actions"] = actions;
              w["data"] = info.data;
              j[id] = w;
            }

            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/debug/click",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("widget") && req.has_param("action")) {
              std::string wname = req.get_param_value("widget");
              std::string aname = req.get_param_value("action");

              auto snapshot = UIRegistry::getInstance().getSnapshot();
              if (snapshot.count(wname)) {
                const auto &info = snapshot[wname];
                for (const auto &action : info.actions) {
                  if (action.name == aname) {
                    // Found it! Calculate center in logical coords
                    int lx = action.rect.x + action.rect.w / 2;
                    int ly = action.rect.y + action.rect.h / 2;

                    // Convert logical to "raw" coordinates that set_touch uses.
                    float rx = static_cast<float>(lx) / 800.0f;
                    float ry = static_cast<float>(ly) / 480.0f;

                    // Now simulate the click as if it came from /set_touch
                    int w = 800, h = 480;
                    SDL_GetRendererOutputSize(renderer_, &w, &h);
                    int px = static_cast<int>(rx * w);
                    int py = static_cast<int>(ry * h);

                    SDL_Event event;
                    SDL_zero(event);
                    event.type = SDL_MOUSEBUTTONDOWN;
                    event.button.button = SDL_BUTTON_LEFT;
                    event.button.state = SDL_PRESSED;
                    event.button.x = px;
                    event.button.y = py;
                    SDL_PushEvent(&event);

                    SDL_zero(event);
                    event.type = SDL_MOUSEBUTTONUP;
                    event.button.button = SDL_BUTTON_LEFT;
                    event.button.state = SDL_RELEASED;
                    event.button.x = px;
                    event.button.y = py;
                    SDL_PushEvent(&event);

                    res.set_content("ok", "text/plain");
                    return;
                  }
                }
                res.status = 404;
                res.set_content("action not found", "text/plain");
                return;
              }
              res.status = 404;
              res.set_content("widget not found", "text/plain");
              return;
            }
            res.status = 400;
            res.set_content("missing parameters", "text/plain");
          });

  svr.Get("/get_config.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            std::string out;
            out += "Callsign    " + cfg_->callsign + "\n";
            out += "Grid        " + cfg_->grid + "\n";
            out += "Theme       " + cfg_->theme + "\n";
            out += "Lat         " + std::to_string(cfg_->lat) + "\n";
            out += "Lon         " + std::to_string(cfg_->lon) + "\n";
            res.set_content(out, "text/plain");
          });

  svr.Get("/get_time.txt",
          [](const httplib::Request &, httplib::Response &res) {
            auto now = std::chrono::system_clock::now();
            std::tm utc{};
            Astronomy::portable_gmtime(&t, &utc);
            char buf[64];
            std::strftime(buf, sizeof(buf),
                          "Clock_UTC %Y-%02d-%02dT%02d:%02d:%02d Z\n", &utc);
            res.set_content(buf, "text/plain");
          });

  svr.Get(
      "/get_de.txt", [this](const httplib::Request &, httplib::Response &res) {
        if (!state_) {
          res.status = 503;
          return;
        }
        std::string out;
        out += "DE_Callsign " + state_->deCallsign + "\n";
        out += "DE_Grid     " + state_->deGrid + "\n";
        out += "DE_Lat      " + std::to_string(state_->deLocation.lat) + "\n";
        out += "DE_Lon      " + std::to_string(state_->deLocation.lon) + "\n";
        res.set_content(out, "text/plain");
      });

  svr.Get("/get_dx.txt", [this](const httplib::Request &,
                                httplib::Response &res) {
    if (!state_) {
      res.status = 503;
      return;
    }
    if (!state_->dxActive) {
      res.set_content("DX not set\n", "text/plain");
      return;
    }
    std::string out;
    out += "DX_Grid     " + state_->dxGrid + "\n";
    out += "DX_Lat      " + std::to_string(state_->dxLocation.lat) + "\n";
    out += "DX_Lon      " + std::to_string(state_->dxLocation.lon) + "\n";
    double dist =
        Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
    double brg =
        Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
    out += "DX_Dist_km  " + std::to_string(static_cast<int>(dist)) + "\n";
    out += "DX_Bearing  " + std::to_string(static_cast<int>(brg)) + "\n";
    res.set_content(out, "text/plain");
  });

  // Programmatic set DE/DX via lat/lon
  svr.Get("/set_mappos",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!state_) {
              res.status = 503;
              return;
            }
            if (!req.has_param("lat") || !req.has_param("lon")) {
              res.status = 400;
              res.set_content("missing lat/lon", "text/plain");
              return;
            }
            double lat = std::stod(req.get_param_value("lat"));
            double lon = std::stod(req.get_param_value("lon"));
            std::string target = "dx"; // default
            if (req.has_param("target"))
              target = req.get_param_value("target");

            if (target == "de") {
              state_->deLocation = {lat, lon};
              state_->deGrid = Astronomy::latLonToGrid(lat, lon);
            } else {
              state_->dxLocation = {lat, lon};
              state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
              state_->dxActive = true;
            }
            nlohmann::json j;
            j["target"] = target;
            j["lat"] = lat;
            j["lon"] = lon;
            j["grid"] = Astronomy::latLonToGrid(lat, lon);
            res.set_content(j.dump(), "application/json");
          });

  svr.Get(
      "/debug/type", [](const httplib::Request &req, httplib::Response &res) {
        if (req.has_param("text")) {
          std::string text = req.get_param_value("text");
          for (char c : text) {
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_TEXTINPUT;
            std::snprintf(event.text.text, sizeof(event.text.text), "%c", c);
            SDL_PushEvent(&event);
          }
          res.set_content("ok", "text/plain");
        } else {
          res.status = 400;
          res.set_content("missing 'text' parameter", "text/plain");
        }
      });

  svr.Get("/debug/keypress",
          [](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("key")) {
              std::string k = req.get_param_value("key");
              SDL_Keycode code = SDLK_UNKNOWN;

              if (k == "enter" || k == "return")
                code = SDLK_RETURN;
              else if (k == "tab")
                code = SDLK_TAB;
              else if (k == "escape" || k == "esc")
                code = SDLK_ESCAPE;
              else if (k == "backspace")
                code = SDLK_BACKSPACE;
              else if (k == "delete" || k == "del")
                code = SDLK_DELETE;
              else if (k == "left")
                code = SDLK_LEFT;
              else if (k == "right")
                code = SDLK_RIGHT;
              else if (k == "up")
                code = SDLK_UP;
              else if (k == "down")
                code = SDLK_DOWN;
              else if (k == "home")
                code = SDLK_HOME;
              else if (k == "end")
                code = SDLK_END;
              else if (k == "space")
                code = SDLK_SPACE;
              else if (k == "f11")
                code = SDLK_F11;

              if (code != SDLK_UNKNOWN) {
                SDL_Event event;
                SDL_zero(event);
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = code;
                event.key.state = SDL_PRESSED;
                SDL_PushEvent(&event);

                event.type = SDL_KEYUP;
                event.key.keysym.sym = code;
                event.key.state = SDL_RELEASED;
                SDL_PushEvent(&event);
                res.set_content("ok", "text/plain");
              } else {
                res.status = 404;
                res.set_content("unknown key", "text/plain");
              }
            } else {
              res.status = 400;
              res.set_content("missing 'key' parameter", "text/plain");
            }
          });

  svr.Get("/set_config",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("call"))
              cfg_->callsign = req.get_param_value("call");
            if (req.has_param("grid"))
              cfg_->grid = req.get_param_value("grid");
            if (req.has_param("theme"))
              cfg_->theme = req.get_param_value("theme");
            if (req.has_param("lat"))
              cfg_->lat = std::stod(req.get_param_value("lat"));
            if (req.has_param("lon"))
              cfg_->lon = std::stod(req.get_param_value("lon"));

            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            res.set_content("ok", "text/plain");
          });

  svr.Get("/debug/watchlist/add",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("call") && watchlist_) {
              watchlist_->add(req.get_param_value("call"));
              res.set_content("ok", "text/plain");
            } else {
              res.status = 400;
              res.set_content("missing call or watchlist store", "text/plain");
            }
          });

  svr.Get("/debug/store/set_solar",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (solar_) {
              SolarData data = solar_->get();
              if (req.has_param("sfi"))
                data.sfi = std::stoi(req.get_param_value("sfi"));
              if (req.has_param("k"))
                data.k_index = std::stoi(req.get_param_value("k"));
              if (req.has_param("sn"))
                data.sunspot_number = std::stoi(req.get_param_value("sn"));
              data.valid = true;
              solar_->set(data);
              res.set_content("ok", "text/plain");
            } else {
              res.status = 503;
              res.set_content("solar store not available", "text/plain");
            }
          });

  svr.Get("/debug/performance",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j;
            j["fps"] = state_->fps;
            j["port"] = port_;
            j["running_since"] = SDL_GetTicks() / 1000;
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/debug/logs", [](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j;
    j["status"] = "OK";
    j["info"] = "Logs are written to rotating file (~/.hamclock/hamclock.log) "
                "and stderr (journalctl).";
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/debug/health", [this](const httplib::Request &,
                                  httplib::Response &res) {
    nlohmann::json j;
    for (const auto &[name, status] : state_->services) {
      nlohmann::json s;
      s["ok"] = status.ok;
      s["lastError"] = status.lastError;
      if (status.lastSuccess.time_since_epoch().count() > 0) {
        auto t = std::chrono::system_clock::to_time_t(status.lastSuccess);
        std::tm tm_utc{};
        Astronomy::portable_gmtime(&t, &tm_utc);
        std::stringstream ss;
        ss << std::put_time(&tm_utc, "%Y-%m-%d %H:%M:%S");
        s["lastSuccess"] = ss.str();
      }
      j[name] = s;
    }
    res.set_content(j.dump(2), "application/json");
  });
#endif

  LOG_I("WebServer", "Listening on port {}...", port_);
  svr.listen("0.0.0.0", port_);
  svrPtr_ = nullptr;
}
