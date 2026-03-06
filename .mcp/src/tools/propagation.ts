import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";

const VOACAP_SCHEMA = {
  schema_version: "1.0",
  description: "HamClock-Next VOACAP map overlay request/response schema. Used by MCP tools and the /api/propagation/voacap WebServer endpoint.",
  request: {
    band: {
      type: "string",
      enum: ["80m","40m","30m","20m","17m","15m","12m","10m","6m"],
      description: "Amateur band (converted to MHz internally: 80m→3.573, 40m→7.074, 30m→10.136, 20m→14.074, 17m→18.1, 15m→21.074, 12m→24.9, 10m→28.074, 6m→50.313)"
    },
    freq_mhz: {
      type: "number",
      description: "Explicit frequency in MHz. Overrides 'band' if provided. 0 = use MUF-based auto-select.",
    },
    hour_utc: { type: "integer", minimum: 0, maximum: 23, description: "UTC hour for prediction" },
    year: { type: "integer", description: "Year (e.g. 2026)" },
    month: { type: "integer", minimum: 1, maximum: 12, description: "Month (1-12)" },
    tx_lat: { type: "number", minimum: -90, maximum: 90, description: "Transmitter latitude (decimal degrees)" },
    tx_lon: { type: "number", minimum: -180, maximum: 180, description: "Transmitter longitude (decimal degrees)" },
    path: { type: "integer", enum: [0, 1], description: "0=short-path (default), 1=long-path" },
    mode: { type: "string", enum: ["SSB","CW","FT8","WSPR","AM","RTTY"], description: "Modulation mode affecting signal margin" },
    watts: { type: "number", minimum: 1, maximum: 1500, description: "TX power in watts (default 100)" },
    overlay_type: { type: "string", enum: ["muf","reliability","toa"], description: "muf=Maximum Usable Frequency, reliability=propagation reliability 0-100%, toa=time-of-arrival 0-40ms" },
    width: { type: "integer", default: 660, description: "Output image width in pixels (equirectangular)" },
    height: { type: "integer", default: 330, description: "Output image height in pixels (equirectangular)" },
  },
  response: {
    schema_version: "string (1.0)",
    overlay_type: "string (muf|reliability|toa)",
    projection: "string (equirectangular)",
    bounds: { west: -180, east: 180, south: -90, north: 90 },
    width: "integer",
    height: "integer",
    image_png_b64: "string (base64-encoded PNG, preferred for web frontend)",
    image_bmp_z_b64: "string (base64 zlib-compressed RGB565 BMP — original HamClock wire format)",
    colormap: [
      { value: "number (scale units: MHz for MUF, % for rel, ms for toa)", color: "#RRGGBB", label: "string" }
    ],
    cache_key: "string (sha256 of canonical request params — use for client-side cache)",
    timestamp: "string (ISO-8601 generation time)",
    ttl_seconds: "integer (suggested cache lifetime: 1800 for MUF/REL, 900 for real-time variants)",
    compute_location: "string (backend|mcp|wasm)",
    backend_url: "string|null (URL of open-hamclock-backend that served this)",
    solar_indices: { sfi: "number", kp: "number", ssn: "number" },
    ionosonde_count: "integer (KC2G ionosonde stations used for interpolation, 0 if solar-model fallback)",
  },
  colormaps: {
    muf: "0→purple(#4000C0), 4→darkblue(#0040FF), 9→cyan(#00CCFF), 15→lightblue(#80FFFF), 20→green(#00FF80), 27→yellow(#FFFF00), 30→orange(#FF8000), 35+→red(#FF0000)",
    reliability: "0%→gray(#606060), 21%→pinkish(#CC4080), 40-60%→yellow(#FFFF00), 83-100%→green(#00FF80)",
    toa: "0-5ms→green(#00FF80), 5-15ms→yellow(#FFFF00), 15-25ms→pink(#FF80C0), 25-40ms→gray(#808080), >40ms→black(#000000)",
  },
  alignment_notes: "Overlay output is 660x330 equirectangular (WGS84, -180W to +180E, -90S to +90N). MapWidget uses azimuthal equidistant or Robinson projection. For full integration, overlay pixels must be re-projected. MVP approach: render overlay in a separate flat-projection panel or use a dedicated overlay view. Full parity: reproject 660x330 grid in MapWidget render loop (GPU-accelerated).",
  compute_location_guide: {
    wasm_browser: "Use open-hamclock-backend via CORS proxy (serve.py /proxy/<url>). NumPy vectorized calc not feasible in WASM without dedicated port.",
    native_web_only: "Use /api/propagation/voacap WebServer endpoint which proxies to OHB when OHB_URL env var is configured.",
    native_desktop_offline: "Future: port simplified MUF/REL model to C++ in VoacapProvider (see voacap_service.py as reference).",
    rpi_with_ohb: "open-hamclock-backend on localhost. Set OHB_URL=http://localhost:8081 in hamclock-next environment.",
  },
  backend_free_options: {
    kc2g_muf_rt: "Fetch https://prop.kc2g.com/renders/current/mufd-normal-now.png — near-real-time MUF map, no backend needed, via CORS proxy in web mode.",
    band_conditions: "BandConditionsPanel already computes DE-to-DX per-band reliability in-app. No backend needed.",
    voacap_on_demand: "Requires open-hamclock-backend or future C++ port. Cannot avoid backend for on-demand full-grid overlay.",
  }
};

const BAND_TO_MHZ: Record<string, number> = {
  "80m": 3.573, "40m": 7.074, "30m": 10.136, "20m": 14.074,
  "17m": 18.1, "15m": 21.074, "12m": 24.9, "10m": 28.074, "6m": 50.313,
};

export function registerPropagationTools(server: McpServer) {
  server.tool(
    "voacap_overlay_schema",
    "Get the canonical request/response schema for VOACAP propagation map overlays. Use this before implementing overlay requests to understand parameters, response format, colormap, projection alignment notes, and compute location tradeoffs.",
    {
      section: z.enum(["full", "request", "response", "colormaps", "alignment", "compute", "backend_free"]).optional()
        .describe("Schema section to retrieve. Omit for full schema."),
    },
    async ({ section }) => {
      if (!section || section === "full") {
        return { content: [{ type: "text" as const, text: JSON.stringify(VOACAP_SCHEMA, null, 2), mimeType: "application/json" }] };
      }
      const sectionMap: Record<string, object> = {
        request: VOACAP_SCHEMA.request,
        response: VOACAP_SCHEMA.response,
        colormaps: VOACAP_SCHEMA.colormaps,
        alignment: { alignment_notes: VOACAP_SCHEMA.alignment_notes },
        compute: VOACAP_SCHEMA.compute_location_guide,
        backend_free: VOACAP_SCHEMA.backend_free_options,
      };
      const data = sectionMap[section];
      if (!data) return { isError: true, content: [{ type: "text" as const, text: "Section not found" }] };
      return { content: [{ type: "text" as const, text: JSON.stringify(data, null, 2), mimeType: "application/json" }] };
    }
  );

  server.tool(
    "get_voacap_overlay",
    "Request a VOACAP propagation map overlay. If open-hamclock-backend is reachable at the given URL, proxies the request and returns overlay metadata. Otherwise returns the schema and instructions for self-hosting the backend.",
    {
      tx_lat: z.number().describe("Transmitter latitude (-90 to 90). Use DE latitude as default."),
      tx_lon: z.number().describe("Transmitter longitude (-180 to 180). Use DE longitude as default."),
      band: z.enum(["80m","40m","30m","20m","17m","15m","12m","10m","6m"]).optional().describe("Amateur band"),
      freq_mhz: z.number().optional().describe("Explicit MHz (overrides band). 0 = MUF auto-select."),
      hour_utc: z.number().min(0).max(23).optional().describe("UTC hour (default: current hour)"),
      year: z.number().optional().describe("Year (default: current year)"),
      month: z.number().min(1).max(12).optional().describe("Month 1-12 (default: current month)"),
      path: z.number().min(0).max(1).optional().describe("0=short-path (default), 1=long-path"),
      mode: z.enum(["SSB","CW","FT8","WSPR","AM","RTTY"]).optional().describe("Modulation mode (default: SSB)"),
      watts: z.number().optional().describe("TX power in watts (default: 100)"),
      overlay_type: z.enum(["muf","reliability","toa"]).optional().describe("Overlay type (default: reliability)"),
      backend_url: z.string().optional().describe("open-hamclock-backend base URL (default: http://localhost:8081)"),
    },
    async ({ tx_lat, tx_lon, band, freq_mhz, hour_utc, year, month, path, mode, watts, overlay_type, backend_url }) => {
      const now = new Date();
      const utcHour = hour_utc ?? now.getUTCHours();
      const utcYear = year ?? now.getUTCFullYear();
      const utcMonth = month ?? (now.getUTCMonth() + 1);
      const resolvedFreq = freq_mhz ?? (band ? BAND_TO_MHZ[band] : 14.074);
      const resolvedMode = mode ?? "SSB";
      const resolvedWatts = watts ?? 100;
      const resolvedPath = path ?? 0;
      const resolvedOverlay = overlay_type ?? "reliability";
      const ohbUrl = backend_url ?? process.env.OHB_URL ?? "http://localhost:8081";

      const requestParams = {
        TXLAT: tx_lat, TXLNG: tx_lon,
        MHZ: resolvedFreq,
        UTC: utcHour, YEAR: utcYear, MONTH: utcMonth,
        PATH: resolvedPath,
        MODE: resolvedMode,
        WATTS: resolvedWatts,
        WIDTH: 660, HEIGHT: 330,
      };

      const lines: string[] = [];
      lines.push(`# VOACAP Overlay Request`);
      lines.push(`Overlay Type: **${resolvedOverlay}**`);
      lines.push(`TX: ${tx_lat.toFixed(4)}°, ${tx_lon.toFixed(4)}°`);
      lines.push(`Frequency: ${resolvedFreq} MHz (${band ?? "custom"})`);
      lines.push(`UTC ${String(utcHour).padStart(2, "0")}:00 on ${utcYear}-${String(utcMonth).padStart(2, "0")}`);
      lines.push(`Mode: ${resolvedMode} @ ${resolvedWatts}W, ${resolvedPath === 0 ? "short" : "long"}-path`);
      lines.push(``);

      const endpoint = resolvedOverlay === "muf"
        ? `/ham/HamClock/fetchVOACAP-MUF.pl`
        : resolvedOverlay === "toa"
          ? `/ham/HamClock/fetchVOACAP-TOA.pl`
          : `/ham/HamClock/fetchBandConditions.pl`;

      const queryString = new URLSearchParams(
        Object.entries(requestParams).reduce((acc, [k, v]) => { acc[k] = String(v); return acc; }, {} as Record<string, string>)
      ).toString();

      let backendStatus = "not_checked";
      let backendResponse: string | null = null;

      try {
        const testUrl = `${ohbUrl}/ham/HamClock/version.pl`;
        const resp = await fetch(testUrl);
        if (resp.ok) {
          backendStatus = "reachable";
          lines.push(`✅ open-hamclock-backend reachable at ${ohbUrl}`);

          const overlayEndpoint = `${ohbUrl}${endpoint}?${queryString}`;
          lines.push(`
## Overlay URL`);
          lines.push(````
${overlayEndpoint}
````);

          if (resolvedOverlay === "muf" || resolvedOverlay === "toa") {
            lines.push(`
⚠️  Note: fetchVOACAP-MUF.pl and fetchVOACAP-TOA.pl are not yet implemented in open-hamclock-backend. Use fetchBandConditions.pl for per-band reliability data instead.`);
          } else {
            try {
              const dataResp = await fetch(`${ohbUrl}/ham/HamClock/fetchBandConditions.pl?${queryString}`);
              if (dataResp.ok) {
                backendResponse = await dataResp.text();
                lines.push(`
## Band Conditions Response (${resolvedOverlay})`);
                lines.push(````
${backendResponse.substring(0, 500)}
````);
              }
            } catch {
              lines.push(`
⚠️  Could not fetch band conditions data.`);
            }
          }
        } else {
          backendStatus = "unreachable";
        }
      } catch {
        backendStatus = "unreachable";
      }

      if (backendStatus === "unreachable") {
        lines.push(`⚠️  open-hamclock-backend not reachable at ${ohbUrl}`);
        lines.push(``);
        lines.push(`## How to get VOACAP overlays without a backend`);
        lines.push(``);
        lines.push(`**Option 1 (easiest): KC2G real-time MUF map** (no backend needed)`);
        lines.push(`Fetch directly: `https://prop.kc2g.com/renders/current/mufd-normal-now.png``);
        lines.push(``);
        lines.push(`**Option 2: Run open-hamclock-backend locally**`);
        lines.push(``cd open-hamclock-backend && docker-compose up``);
        lines.push(`Then set OHB_URL=http://localhost:8081 in your environment.`);
        lines.push(``);
        lines.push(`**Option 3: Direct endpoint call (when OHB is running)**`);
        lines.push(````
GET ${ohbUrl}${endpoint}?${queryString}
````);
      }

      return { content: [{ type: "text", text: lines.join("
") }] };
    }
  );

  server.tool(
    "check_propagation_backend",
    "Check if open-hamclock-backend is reachable and what propagation endpoints it provides. Use this before attempting VOACAP overlay requests.",
    {
      backend_url: z.string().optional().describe("open-hamclock-backend base URL (default: http://localhost:8081)"),
    },
    async ({ backend_url }) => {
      const ohbUrl = backend_url ?? process.env.OHB_URL ?? "http://localhost:8081";
      const endpoints = [
        "/ham/HamClock/version.pl",
        "/ham/HamClock/fetchBandConditions.pl",
        "/ham/HamClock/fetchVOACAP-MUF.pl",
        "/ham/HamClock/fetchVOACAP-TOA.pl",
      ];

      const lines: string[] = [`# Propagation Backend Status: ${ohbUrl}`];

      for (const ep of endpoints) {
        try {
          const resp = await fetch(`${ohbUrl}${ep}`);
          if (resp.ok) {
            lines.push(`- ✅ `${ep}` is reachable`);
          } else {
            lines.push(`- ❌ `${ep}` returned ${resp.status}`);
          }
        } catch {
          lines.push(`- ❌ `${ep}` is unreachable`);
        }
      }

      return { content: [{ type: "text", text: lines.join("
") }] };
    }
  );

  server.tool(
    "propagation_parity_gaps",
    "List all propagation-related parity gaps between hamclock-original and hamclock-next, with implementation priority and suggested approach for each.",
    {},
    async () => {
      const gaps = [
        {
          feature: "VOACAP MUF Map",
          priority: "High",
          status: "Missing in OHB",
          suggested_approach: "Implement fetchVOACAP-MUF.pl in open-hamclock-backend using the existing solar model and MUF interpolation logic from KC2G or original HamClock."
        },
        {
          feature: "VOACAP TOA Map",
          priority: "Medium",
          status: "Missing in OHB",
          suggested_approach: "Implement fetchVOACAP-TOA.pl in open-hamclock-backend. Requires path-length calculation based on ionospheric reflection height."
        },
        {
          feature: "Grayline Propagation Enhancement",
          priority: "Medium",
          status: "Partial",
          suggested_approach: "The original HamClock includes a 1dB signal boost near the terminator. This logic should be added to PropEngine.cpp in hamclock-next."
        }
      ];

      let text = "# Propagation Parity Gaps

";
      for (const gap of gaps) {
        text += `## ${gap.feature}
- **Priority:** ${gap.priority}
- **Status:** ${gap.status}
- **Approach:** ${gap.suggested_approach}

`;
      }

      return { content: [{ type: "text", text }] };
    }
  );
}
