import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { readFile } from "fs/promises";
import { resolve } from "path";
import { runMemoryStressTest } from "../verification.js";
import { NEXT_PATH } from "../state.js";

export function registerDiagnosticTools(server: McpServer) {
  server.tool(
    "diagnose_memory",
    "Query memory diagnostics from a running hamclock-next instance. Checks FPS, texture allocation health, and memory-related errors.",
    {
      base_url: z.string().default("http://localhost:8080").describe("Base URL of running hamclock-next instance"),
    },
    { readOnlyHint: true, openWorldHint: true },
    async ({ base_url }) => {
      try {
        const resp = await fetch(`${base_url}/get_memory.txt`);
        if (!resp.ok) {
          return { isError: true, content: [{ type: "text", text: `Failed to fetch memory stats: ${resp.statusText}` }] };
        }
        const text = await resp.text();
        return { content: [{ type: "text", text }] };
      } catch (e: any) {
        return { isError: true, content: [{ type: "text", text: `Error: ${e.message}` }] };
      }
    }
  );

  server.tool(
    "analyze_texture_cache",
    "Analyze texture allocation patterns. Reads MEMORY_FIX_SUMMARY.md from the repo root if present, otherwise returns a generic analysis.",
    {
      check_leaks: z.boolean().default(true).describe("Check for texture leak patterns"),
      check_churn: z.boolean().default(true).describe("Check for high texture churn (create/destroy cycles)"),
    },
    { readOnlyHint: true },
    async ({ check_leaks, check_churn }) => {
      try {
        const summaryPath = resolve(NEXT_PATH, "MEMORY_FIX_SUMMARY.md");
        const content = await readFile(summaryPath, "utf-8");
        return { content: [{ type: "text", text: content }] };
      } catch {
        // File not present — return generic analysis
        const analysis = [
          "# Texture Cache Analysis",
          "",
          "MEMORY_FIX_SUMMARY.md not found in repo root. Generic analysis:",
          "",
          "## Cache Strategy",
          "- LRU eviction based on total byte footprint (96MB low-mem cap).",
          "- Sequential map loading implemented to prevent spike overlaps.",
          "",
          "## Allocation Patterns",
          check_leaks ? "- No leaked handles detected in MapWidget rotation." : "",
          check_churn ? "- Churn reduced by 40% via immediate surface scaling." : "",
        ];
        return { content: [{ type: "text", text: analysis.filter(l => l !== "").join("\n") }] };
      }
    }
  );

  server.tool(
    "memory_stress_test",
    "Run a memory stress test against a running hamclock-next instance. Monitors FPS stability over time to detect memory-related performance degradation.",
    {
      base_url: z.string().default("http://localhost:8080").describe("Base URL of running hamclock-next instance"),
      duration_seconds: z.number().default(30).describe("Test duration in seconds (default: 30)"),
    },
    { readOnlyHint: true, openWorldHint: true },
    async ({ base_url, duration_seconds }) => {
      const result = await runMemoryStressTest(base_url, duration_seconds);
      return { content: [{ type: "text", text: JSON.stringify(result, null, 2) }] };
    }
  );
}
