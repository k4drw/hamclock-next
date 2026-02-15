# Implementation Tickets - Partial Features

**Generated:** 2026-02-15
**Status:** 11 partial features identified, ranked by implementation effort.
**Note on Effort:** Estimates below are **AI-Accelerated**. Using the provided MCP (Model Context Protocol) tools for scaffolding and logic bridging can reduce development time by 50-70%.

## Overview

This document tracks implementation tickets for all partial features in HamClock Next. Features are ranked by estimated effort (Small → Large) to help prioritize development work.

**Summary (AI-Accelerated):**
- 🟢 **1 Small** (~1-2 hours): Quick win
- 🟡 **1 Small-Medium** (~2-4 hours): Low-hanging fruit
- 🟠 **1 Medium** (~4-8 hours): Moderate effort
- 🔴 **1 Medium-Large** (~6-12 hours): Significant work
- 🔴 **2 Large** (~10-20 hours each): Major projects

---

## 🟢 SMALL EFFORT

### #1: Countdown Timer ⭐ RECOMMENDED STARTING POINT

**Feature ID:** `countdown_timer`
**Category:** Data Panel
**Estimated Effort:** 1-2 hours (AI-Accelerated)

#### Current State
- ✅ Display remaining time (days/hours/minutes/seconds)
- ✅ Visual alarm ("EVENT ACTIVE!" when countdown reaches zero)
- ✅ Event name display
- ✅ CountdownPanel.cpp/h exist and work

#### What's Missing
- ❌ Configuration UI (no way to set target date/time/event name)
- ❌ ConfigManager integration for persistence
- ❌ Audio alarm capability
- ❌ Setup screen integration

#### Implementation Tasks
1. Add countdown configuration to `src/core/ConfigManager`
   - Target date/time (Unix timestamp)
   - Event name (string)
   - Audio alarm enabled (bool)
2. Add "Countdown" tab to SetupScreen.cpp
   - Date/time picker UI
   - Event name text input
   - Audio alarm toggle
3. Update CountdownPanel to load config from ConfigManager
4. Add audio alarm using SDL_mixer (when countdown reaches zero)
5. Save/load settings on apply/startup

#### Acceptance Criteria
- [ ] Set target date/time for countdown
- [ ] Display remaining time (days, hours, minutes, seconds)
- [ ] Visual or audio alarm when countdown reaches zero
- [ ] Configurable event name

#### Files to Modify
- `src/ui/CountdownPanel.h` - Add config loading
- `src/ui/CountdownPanel.cpp` - Load from ConfigManager, add audio
- `src/ui/SetupScreen.cpp` - Add countdown config tab
- `src/core/ConfigManager.cpp` - Add countdown settings

---

## 🟡 SMALL-MEDIUM EFFORT

### #2: NOAA Space Weather Scales

**Feature ID:** `noaa_space_weather`
**Category:** Data Panel
**Estimated Effort:** 2-4 hours (AI-Accelerated)

#### Current State
- ✅ SpaceWeatherPanel displays current space weather data
- ✅ K-index data fetched (can derive G-scale)
- ✅ Color coding for various metrics
- ✅ NOAAProvider fetches multiple data sources

#### What's Missing
- ❌ X-ray flux data (needed for R-scale: Radio Blackout)
- ❌ Solar Energetic Particle flux data (needed for S-scale: Solar Radiation)
- ❌ Formal NOAA R/S/G scale display (R1-R5, S1-S5, G1-G5)
- ❌ Current alert status

#### Implementation Tasks
1. Add X-ray flux fetching to NOAAProvider
   - URL: `https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json`
   - Parse flux levels, determine R-scale (R1-R5)
2. Add SEP flux fetching to NOAAProvider
   - URL: `https://services.swpc.noaa.gov/json/goes/primary/integral-protons-6-hour.json`
   - Parse particle flux, determine S-scale (S1-S5)
3. Add xray_flux and sep_flux fields to SolarData struct
4. Create dedicated NOAAScalesPanel (or extend SpaceWeatherPanel)
   - Display R/S/G scales with color coding:
     - None: Green
     - Minor (R1/S1/G1): Yellow
     - Moderate (R2/S2/G2): Orange
     - Strong+ (R3+/S3+/G3+): Red
5. Derive G-scale from K-index:
   - G1: Kp=5, G2: Kp=6, G3: Kp=7, G4: Kp=8, G5: Kp=9

#### Acceptance Criteria
- [ ] Fetch NOAA space weather scales
- [ ] Display R, S, G levels with color coding
- [ ] Show current alert status

#### Files to Modify
- `src/services/NOAAProvider.cpp` - Add X-ray and SEP fetching
- `src/services/NOAAProvider.h` - Add new URLs
- `src/core/SolarData.h` - Add xray_flux, sep_flux fields
- `src/ui/SpaceWeatherPanel.cpp` - Add R/S/G scale display

---

## 🟠 MEDIUM EFFORT

### #3: ADIF Log Viewer

**Feature ID:** `adif`
**Category:** Data Panel
**Estimated Effort:** 4-8 hours (AI-Accelerated)

#### Current State
- ✅ ADIFProvider parses ADIF files (basic)
- ✅ ADIFPanel displays statistics:
  - Total QSO count
  - Top 3 bands
  - Latest 5 callsigns
- ✅ Basic ADIF tag parsing (CALL, MODE, BAND)

#### What's Missing
- ❌ Scrollable QSO list viewer
- ❌ Map plotting of QSO locations
- ❌ Filtering by band/mode/date
- ❌ File selection UI
- ❌ Full ADIF tag support (date/time, grid, etc.)

#### Implementation Tasks
1. Extend ADIFProvider to store full QSO list (not just stats)
   - Parse additional tags: QSO_DATE, TIME_ON, GRIDSQUARE, LAT/LON
   - Store in vector of QSO structs
2. Add scrollable list view to ADIFPanel
   - Display format: "DATE TIME CALL BAND MODE GRID"
   - Scroll controls (mouse wheel, arrows)
   - Pagination or virtual scrolling for large logs
3. Add map overlay for QSOs
   - Integrate with MapWidget (similar to DX Cluster spots)
   - Plot QSO locations as markers
   - Color code by band or mode
4. Add filtering UI
   - Band filter dropdown
   - Mode filter dropdown
   - Date range picker
5. Add file selection
   - Add ADIF file path to SetupScreen
   - File browser or text input
   - Auto-reload on file change

#### Acceptance Criteria
- [ ] Parse ADIF log files
- [ ] Display QSO list with callsign, date, band, mode
- [ ] Plot QSO locations on map
- [ ] Filter by band, mode, or date range
- [ ] ADIF file selection/loading

#### Files to Modify
- `src/services/ADIFProvider.cpp` - Full parser, QSO storage
- `src/services/ADIFProvider.h` - Add QSO struct
- `src/ui/ADIFPanel.cpp` - Scrollable list view, filters
- `src/ui/MapWidget.cpp` - Add ADIF spot rendering
- `src/ui/SetupScreen.cpp` - Add ADIF file config

#### Reference
Original implementation:
- `adif.cpp` (488 lines) - Display and map overlay
- `adif_parser.cpp` (591 lines) - Full ADIF parser

---

## 🔴 MEDIUM-LARGE EFFORT

### #4: Band Conditions (VOACAP)

**Feature ID:** `band_conditions`
**Category:** Data Panel
**Estimated Effort:** 6-12 hours (AI-Accelerated)

#### Current State
- ✅ BandConditionsProvider uses SFI/K heuristics
- ✅ BandConditionsPanel displays per-band reliability
- ✅ Color-coded bars (green/yellow/red)
- ✅ Functionally works with rule-of-thumb propagation estimates

#### What's Missing
- ❌ VOACAP API integration for real propagation predictions
- ❌ DE-DX path-specific calculations
- ❌ Updates when DX location changes

#### Implementation Notes
**Current heuristic implementation may be "good enough"** for most users. VOACAP integration requires:
- External API dependency (availability concerns)
- Complex propagation modeling
- Fallback logic if API unavailable

**Consider de-prioritizing** unless users specifically request VOACAP accuracy.

#### Implementation Tasks (if pursuing VOACAP)
1. Research VOACAP API endpoint
   - Identify public VOACAP service or run local instance
   - Understand API parameters (lat/lon, frequency, time, etc.)
2. Add VOACAP fetching to BandConditionsProvider
   - Pass DE and DX coordinates
   - Request predictions for all ham bands (160m-6m)
   - Parse reliability percentages
3. Add fallback to heuristic method if VOACAP unavailable
4. Update when ConfigManager DX location changes
5. Cache VOACAP results (expensive computation)

#### Acceptance Criteria
- [ ] Fetch VOACAP predictions for DE-DX path
- [ ] Display per-band reliability (160m through 6m)
- [ ] Color-coded bars (green/yellow/red)
- [ ] Update when DX location changes

#### Files to Modify
- `src/services/BandConditionsProvider.cpp` - Add VOACAP API calls
- `src/core/BandConditionsData.h` - Add VOACAP-specific fields

---

## 🔴 LARGE EFFORT

### #5: Rotator/Gimbal Control

**Feature ID:** `rotator_gimbal`
**Category:** Hardware
**Estimated Effort:** 10-20 hours (AI-Accelerated)

#### Current State
- ✅ GimbalPanel displays satellite azimuth/elevation
- ✅ Visual compass display with azimuth indicator
- ✅ Elevation bar gauge
- ✅ Real-time updates from satellite predictor
- ✅ **Display works perfectly as passive tracker**

#### What's Missing
- ❌ Hardware rotator communication (Hamlib integration)
- ❌ Rotate-to-target functionality
- ❌ Rotator protocol support (Yaesu GS-232, etc.)
- ❌ Serial/TCP communication with rotator
- ❌ Error handling for rotator failures

#### Implementation Tasks
1. Add Hamlib dependency
   - Update CMakeLists.txt to link libhamlib
   - Add Hamlib headers
2. Create RotatorController class (src/hardware/)
   - Connect to rotator via TCP or serial
   - Implement Hamlib rotator commands:
     - `get_position` - Query current az/el
     - `set_position` - Command rotation to target
     - `stop` - Emergency stop
3. Add rotator configuration to SetupScreen
   - Rotator model selection (GS-232, etc.)
   - Connection type (TCP/serial)
   - Port/baudrate settings
4. Update GimbalPanel for control mode
   - Add "Track Satellite" button
   - Add "Rotate to Target" controls
   - Display current vs. target position
   - Show rotator status (connected, moving, stopped)
5. Add safety checks
   - Azimuth/elevation limits
   - Collision avoidance
   - Timeout handling
6. **Testing requires physical rotator hardware**

#### Acceptance Criteria
- [ ] Display current rotator position (azimuth)
- [ ] Rotate to target bearing
- [ ] Visual compass display
- [ ] Support common rotator protocols (Yaesu GS-232, etc.)

#### Files to Create/Modify
- `src/hardware/RotatorController.h` - NEW
- `src/hardware/RotatorController.cpp` - NEW
- `src/ui/GimbalPanel.cpp` - Add control mode
- `src/ui/SetupScreen.cpp` - Add rotator config
- `CMakeLists.txt` - Add libhamlib dependency

#### Reference
Original implementation: `gimbal.cpp` (1108 lines) - Full rotator control with Hamlib

---

### #6: Space Weather Time-Series Plots (6 Features)

**Feature IDs:** `solar_flux`, `kp_index`, `sunspot_number`, `xray_flux`, `solar_wind`, `bz_bt`
**Category:** Data Panel
**Estimated Effort:** 15-25 hours (AI-Accelerated)

#### Current State
- ✅ NOAAProvider fetches current values for all metrics
- ✅ SpaceWeatherPanel displays current values with color coding
- ✅ Data sources: SFI, K-index, A-index, SSN, solar wind, Bz/Bt, etc.

#### What's Missing
- ❌ **Historical data storage** (only current values stored)
- ❌ **Time-series graph widget** (no reusable chart component)
- ❌ **Data persistence** across restarts
- ❌ **NOAA historical data endpoints** (30-day history)
- ❌ Individual plot panels for each metric

#### Implementation Strategy
This is a **batch project** - build shared infrastructure once, then create 6 plots quickly.

**Phase 1: Infrastructure (10-15 hours AI-Accelerated)**
1. Create TimeSeriesStore class (src/core/)
   - Ring buffer for historical data (configurable size)
   - Thread-safe storage
   - Persistence to JSON file on disk
2. Create TimeSeriesWidget base class (src/ui/)
   - Reusable line chart component
   - Auto-scaling Y-axis
   - Time labels on X-axis
   - Grid lines, legend
   - Configurable colors, line thickness
3. Update NOAAProvider to store history
   - On each fetch, append to TimeSeriesStore
   - Keep 30 days of data (configurable)
   - Load persisted data on startup
4. Add NOAA historical data endpoints
   - Fetch 30-day history on first run
   - Incremental updates after that

**Phase 2: Individual Plots (1-2 hours each × 6)**
1. SolarFluxPanel - 10.7cm solar flux (green/yellow/red zones)
2. KpIndexPanel - Kp bar chart (G-scale overlays)
3. SunspotPanel - SSN line chart
4. XRayPanel - X-ray flux with C/M/X flare indicators
5. SolarWindPanel - Dual-axis (speed + density)
6. BzBtPanel - Bz/Bt with southward Bz highlighting

#### Acceptance Criteria

**Infrastructure:**
- [ ] Historical data storage (30-day ring buffer)
- [ ] Data persistence across restarts
- [ ] Reusable time-series graph widget

**Per Plot:**
- [ ] Fetch historical data from NOAA/SWPC
- [ ] Display as time-series plot
- [ ] Show current value prominently
- [ ] Color coding for conditions

#### Files to Create
- `src/core/TimeSeriesStore.h` - NEW
- `src/core/TimeSeriesStore.cpp` - NEW
- `src/ui/TimeSeriesWidget.h` - NEW
- `src/ui/TimeSeriesWidget.cpp` - NEW
- `src/ui/SolarFluxPanel.cpp` - NEW (or extend existing SolarPanel)
- `src/ui/KpIndexPanel.cpp` - NEW
- `src/ui/SunspotPanel.cpp` - NEW
- `src/ui/XRayPanel.cpp` - NEW
- `src/ui/SolarWindPanel.cpp` - NEW
- `src/ui/BzBtPanel.cpp` - NEW

#### Files to Modify
- `src/services/NOAAProvider.cpp` - Add historical fetching + storage

#### Reference
Original implementation: `spacewx.cpp` (1843 lines) - All space weather data fetching and plotting

---

## 📊 Recommended Implementation Order

### Phase 1: Quick Wins (1-2 weeks)
1. **Countdown Timer** (1-2 hrs) ⭐ Start here
2. **NOAA Space Weather Scales** (2-4 hrs)

### Phase 2: Medium Features (2-4 weeks)
3. **ADIF Log Viewer** (4-8 hrs)

### Phase 3: Major Projects (4-8 weeks)
4. **Space Weather Time-Series Plots** (15-25 hrs)
   - Addresses 6 features at once
   - Reusable infrastructure benefits future features

### Phase 4: Hardware Integration (Optional, 6-8 weeks)
5. **Rotator/Gimbal Control** (10-20 hrs)
   - Requires physical hardware for testing
   - Lower priority for most users

### Phase 5: Optional Enhancement
6. **Band Conditions VOACAP** (6-12 hrs)
   - Consider skipping - current heuristic impl works
   - External API dependency
   - Low ROI vs. effort

---

## Notes

### Testing Requirements
- **Countdown Timer**: Easy to test (set timer for 1 minute)
- **NOAA Scales**: Verify against live NOAA data
- **ADIF Viewer**: Need sample ADIF log files
- **Space Weather Plots**: Wait 24-48 hours to see data accumulate
- **Rotator Control**: **Requires physical rotator hardware**
- **Band Conditions**: Compare VOACAP vs. heuristic accuracy

### Dependencies
- Countdown Timer: SDL_mixer (audio)
- Rotator Control: libhamlib
- All others: Existing dependencies (SDL2, libcurl, nlohmann/json)

### Documentation
See also:
- `original-analysis/hamclock_widgets.md` - Widget system reference
- `original-analysis/hamclock_layout.md` - Layout guidelines
- `original-analysis/hamclock_fonts.md` - Font catalog
- `API.md` - Remote API reference
- `original-analysis/DATA_SOURCES.md` - Live data providers
- MCP tools: `feature_status`, `create_ticket`, `list_by_status`, `get_scaffolding_template`
