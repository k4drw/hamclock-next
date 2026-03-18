import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { loadParityData } from "../state.js";

const VOACAP_SCHEMA = {
  schema_version: "1.1",
  description: "HamClock-Next VOACAP map overlay request/response schema. Used by MCP tools and the /api/propagation/voacap WebServer endpoint.",
  request: {
    band: {
      type: "string",
      enum: ["80m","40m","30m","20m","17m","15m","12m","10m","6m"],
      description: "Amateur band (converted to MHz internally)"
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
    schema_version: "string (1.1)",
    overlay_type: "string (muf|reliability|toa)",
    projection: "string (equirectangular)",
    bounds: { west: -180, east: 180, south: -90, north: 90 },
    width: "integer",
    height: "integer",
    image_png_b64: "string (base64-encoded PNG)",
    colormap: [
      { value: "number", color: "#RRGGBB", label: "string" }
    ],
    cache_key: "string",
    timestamp: "string (ISO-8601)",
    ttl_seconds: "integer",
    compute_location: "string (internal|wasm)",
    solar_indices: { sfi: "number", kp: "number", ssn: "number" },
  },
  colormaps: {
    muf: "0→purple, 35+→red",
    reliability: "0%→gray, 100%→green",
    toa: "0-5ms→green, >40ms→black",
  },
  alignment_notes: "Overlay output is 660x330 equirectangular. Native desktop builds use internal C++ propagation engine. WASM builds use a simplified internal model.",
  backend_free_options: {
    kc2g_muf_rt: "Fetch https://prop.kc2g.com/renders/current/mufd-normal-now.png for near-real-time MUF map.",
    band_conditions: "BandConditionsPanel computes DE-to-DX per-band reliability in-app.",
  }
};

const BAND_TO_MHZ: Record<string, number> = {
  "80m": 3.573, "40m": 7.074, "30m": 10.136, "20m": 14.074,
  "17m": 18.1, "15m": 21.074, "12m": 24.9, "10m": 28.074, "6m": 50.313,
};

export function registerPropagationTools(server: McpServer) {
  server.tool(
    "voacap_overlay_schema",
    "Get the canonical request/response schema for VOACAP propagation map overlays. Use this before implementing overlay requests to understand parameters, response format, and colormap.",
    {
      section: z.enum(["full", "request", "response", "colormaps", "alignment", "backend_free"]).optional()
        .describe("Schema section to retrieve. Omit for full schema."),
    },
    { readOnlyHint: true },
    async ({ section }) => {
      if (!section || section === "full") {
        return { content: [{ type: "text" as const, text: JSON.stringify(VOACAP_SCHEMA, null, 2) }] };
      }
      const sectionMap: Record<string, object> = {
        request: VOACAP_SCHEMA.request,
        response: VOACAP_SCHEMA.response,
        colormaps: VOACAP_SCHEMA.colormaps,
        alignment: { alignment_notes: VOACAP_SCHEMA.alignment_notes },
        backend_free: VOACAP_SCHEMA.backend_free_options,
      };
      const data = sectionMap[section];
      if (!data) return { isError: true, content: [{ type: "text" as const, text: "Section not found" }] };
      return { content: [{ type: "text" as const, text: JSON.stringify(data, null, 2) }] };
    }
  );

  server.tool(
    "get_voacap_overlay",
    "Build VOACAP propagation map overlay parameters and return the API endpoint URL for fetching the overlay from a running hamclock-next instance.",
    {
      tx_lat: z.number().describe("Transmitter latitude (-90 to 90)."),
      tx_lon: z.number().describe("Transmitter longitude (-180 to 180)."),
      band: z.enum(["80m","40m","30m","20m","17m","15m","12m","10m","6m"]).optional().describe("Amateur band"),
      freq_mhz: z.number().optional().describe("Explicit MHz (overrides band). 0 = MUF auto-select."),
      hour_utc: z.number().min(0).max(23).optional().describe("UTC hour"),
      year: z.number().optional().describe("Year"),
      month: z.number().min(1).max(12).optional().describe("Month 1-12"),
      path: z.number().min(0).max(1).optional().describe("0=short-path, 1=long-path"),
      mode: z.enum(["SSB","CW","FT8","WSPR","AM","RTTY"]).optional().describe("Modulation mode"),
      watts: z.number().optional().describe("TX power in watts"),
      overlay_type: z.enum(["muf","reliability","toa"]).optional().describe("Overlay type"),
    },
    { readOnlyHint: true },
    async ({ tx_lat, tx_lon, band, freq_mhz, hour_utc, year, month, path, mode, watts, overlay_type }) => {
      const now = new Date();
      const utcHour = hour_utc ?? now.getUTCHours();
      const utcYear = year ?? now.getUTCFullYear();
      const utcMonth = month ?? (now.getUTCMonth() + 1);
      const resolvedFreq = freq_mhz ?? (band ? BAND_TO_MHZ[band] : 14.074);
      const resolvedMode = mode ?? "SSB";
      const resolvedWatts = watts ?? 100;
      const resolvedPath = path ?? 0;
      const resolvedOverlay = overlay_type ?? "reliability";

      const lines: string[] = [];
      lines.push(`# VOACAP Overlay (Internal Engine)`);
      lines.push(`Overlay Type: **${resolvedOverlay}**`);
      lines.push(`TX: ${tx_lat.toFixed(4)}°, ${tx_lon.toFixed(4)}°`);
      lines.push(`Frequency: ${resolvedFreq} MHz (${band ?? "custom"})`);
      lines.push(`UTC ${String(utcHour).padStart(2, "0")}:00 on ${utcYear}-${String(utcMonth).padStart(2, "0")}`);
      lines.push(`Mode: ${resolvedMode} @ ${resolvedWatts}W, ${resolvedPath === 0 ? "short" : "long"}-path`);
      lines.push(``);
      lines.push(`To fetch this overlay programmatically from a running instance, use:`);
      lines.push(`\`GET /api/propagation/voacap?lat=${tx_lat}&lon=${tx_lon}&mhz=${resolvedFreq}&type=${resolvedOverlay}\``);

      return { content: [{ type: "text", text: lines.join("\n") }] };
    }
  );

  server.tool(
    "propagation_parity_gaps",
    "List all propagation-related parity gaps between hamclock-original and hamclock-next, sourced from parity_v2.json with a static fallback.",
    {},
    { readOnlyHint: true },
    async () => {
      const PROPAGATION_KEYWORDS = ["propagat", "voacap", "grayline", "muf", "band_condition", "ionospher"];
      let text = "# Propagation Parity Gaps\n\n";

      try {
        const parityData = await loadParityData();
        const propFeatures = parityData.features.filter(f =>
          PROPAGATION_KEYWORDS.some(kw =>
            f.feature_id.toLowerCase().includes(kw) || f.name.toLowerCase().includes(kw)
          )
        );
        if (propFeatures.length > 0) {
          for (const f of propFeatures) {
            text += `## ${f.name}\n- **Status:** ${f.status}\n- **Notes:** ${f.notes}\n`;
            if (f.suggested_gaps.length > 0) {
              text += `- **Gaps:** ${f.suggested_gaps.join("; ")}\n`;
            }
            text += "\n";
          }
          return { content: [{ type: "text", text }] };
        }
      } catch {
        // parity data unavailable — fall through to static list
      }

      // Static fallback when parity data is not loaded
      const staticGaps = [
        {
          feature: "VOACAP MUF Map",
          status: "Implemented (Internal)",
          notes: "The internal engine handles MUF map generation. Improvements should be made directly to PropEngine.cpp."
        },
        {
          feature: "VOACAP TOA Map",
          status: "Implemented (Internal)",
          notes: "TOA map is currently calculated using path-length based on ionospheric reflection height."
        },
        {
          feature: "Grayline Propagation Enhancement",
          status: "Partial",
          notes: "The original HamClock includes a 1dB signal boost near the terminator. This logic should be calibrated in PropEngine.cpp."
        }
      ];
      text += "*Note: parity_v2.json not loaded — showing static fallback. Run `parity_sync` to load live data.*\n\n";
      for (const gap of staticGaps) {
        text += `## ${gap.feature}\n- **Status:** ${gap.status}\n- **Notes:** ${gap.notes}\n\n`;
      }
      return { content: [{ type: "text", text }] };
    }
  );
}
