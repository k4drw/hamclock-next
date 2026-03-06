#include "DRAPProvider.h"
#include "../core/Logger.h"
#include <sstream>

DRAPProvider::DRAPProvider(NetworkManager &net,
                           std::shared_ptr<DRAPDataStore> gridStore)
    : net_(net), gridStore_(std::move(gridStore)) {}

void DRAPProvider::fetch(DataCb cb) {
  const char *url =
      "https://services.swpc.noaa.gov/text/drap_global_frequencies.txt";

  auto gridStore = gridStore_;
  net_.fetchAsync(url, [cb, gridStore](std::string body) {
    if (body.empty()) {
      LOG_W("DRAPProvider", "Empty response from DRAP data source");
      return;
    }

    try {
      float max_freq = 0.0f;
      bool found_any = false;
      DRAPGrid grid;
      int maxCols = 0;

      std::stringstream ss(body);
      std::string line;

      while (std::getline(ss, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '\r' ||
            line[0] == '\n')
          continue;

        // Look for pipe delimiter
        size_t pipe_pos = line.find('|');
        if (pipe_pos == std::string::npos)
          continue;

        // Parse row values after pipe
        std::string freqs = line.substr(pipe_pos + 1);
        std::stringstream ss_vals(freqs);
        float val;
        int rowCols = 0;

        while (ss_vals >> val) {
          grid.cells.push_back(val);
          if (val > max_freq)
            max_freq = val;
          found_any = true;
          rowCols++;
        }

        if (rowCols > 0) {
          grid.rows++;
          if (rowCols > maxCols)
            maxCols = rowCols;
        }
      }

      if (found_any) {
        // Store full grid
        if (gridStore) {
          grid.cols = maxCols;
          grid.valid = true;
          grid.updated = std::chrono::system_clock::now();
          gridStore->set(grid);
        }

        // Backward-compat: invoke text callback with max frequency
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f", max_freq);
        cb(std::string(buf));
        LOG_D("DRAPProvider", "DRAP max frequency: {:.1f} MHz, grid {}x{}",
              max_freq, maxCols, grid.rows);
      } else {
        LOG_W("DRAPProvider", "No DRAP data found in response");
      }
    } catch (const std::exception &e) {
      LOG_E("DRAPProvider", "Error parsing DRAP data: {}", e.what());
    }
  });
}
