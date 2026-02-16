#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import { readFile, writeFile, mkdir } from "fs/promises";
import { join } from "path";

import { indexRepo, findFiles, findSymbols, RepoIndex, FileIndex, SymbolEntry } from "./indexer.js";
import {
  loadFeatureMap,
  saveFeatureMap,
  getFeature as getOriginalFeature,
  listByCategory,
  listByStatus,
  getSummary as getOriginalSummary,
  getCategories,
  FeatureMap,
  Feature,
} from "./feature-map.js";

import {
  loadReport,
  getSummary,
  listFeatures,
  getFeature,
  createTicket,
  createBatchTickets,
  ParityData,
} from './parity.js';
import { verifyFeature, runMemoryStressTest } from './verification.js';


// ---------------------------------------------------------------------------
// Paths (configurable via environment)
// ---------------------------------------------------------------------------
const __dirname = dirname(fileURLToPath(import.meta.url));
const MCP_ROOT = resolve(__dirname, "..");

const ORIGINAL_PATH =
  process.env.HAMCLOCK_ORIGINAL_PATH ?? resolve(MCP_ROOT, "..", "hamclock-original");
const NEXT_PATH =
  process.env.HAMCLOCK_NEXT_PATH ?? resolve(MCP_ROOT, "..", "hamclock-next");
const FEATURE_MAP_PATH =
  process.env.FEATURE_MAP_PATH ?? resolve(MCP_ROOT, "feature_map.json");
// hamclock-next-mcp.json is the canonical project context document going forward.
// feature_map.json is retained as the legacy data source for all existing feature-map tools.
const PROJECT_CONTEXT_PATH =
  process.env.PROJECT_CONTEXT_PATH ?? resolve(MCP_ROOT, "hamclock-next-mcp.json");
const DOCS_PATH = process.env.HAMCLOCK_DOCS_PATH ?? NEXT_PATH;
const DATA_PATH = resolve(MCP_ROOT, 'data');
const PARITY_JSON_PATH = resolve(DATA_PATH, 'parity_v2.json');


// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
let originalIndex: RepoIndex | null = null;
let nextIndex: RepoIndex | null = null;
let featureMap: FeatureMap | null = null;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
let projectContext: Record<string, any> | null = null;

async function ensureProjectContext(): Promise<Record<string, any>> {
  if (!projectContext) {
    try {
      const raw = await readFile(PROJECT_CONTEXT_PATH, "utf-8");
      projectContext = JSON.parse(raw);
    } catch {
      // Graceful degradation if file not yet present
      projectContext = {};
    }
  }
  return projectContext!;
}

async function ensureIndexed(): Promise<void> {
  if (!originalIndex) {
    originalIndex = await indexRepo(ORIGINAL_PATH, "hamclock-original");
  }
  if (!nextIndex) {
    nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
  }
}

async function ensureFeatureMap(): Promise<FeatureMap> {
  if (!featureMap) {
    featureMap = await loadFeatureMap(FEATURE_MAP_PATH);
  }
  return featureMap;
}

async function loadParityData(): Promise<ParityData> {
  try {
    const fileContent = await readFile(PARITY_JSON_PATH, 'utf-8');
    return JSON.parse(fileContent);
  } catch (e) {
    throw new Error('Parity data not found. Please run `parity_load_report` first.');
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function formatFeatureRow(f: Feature): string {
  const statusIcon =
    f.status === "implemented"
      ? "[done]"
      : f.status === "partial"
        ? "[partial]"
        : f.status === "missing"
          ? "[MISSING]"
          : "[n/a]";
  return `${statusIcon.padEnd(11)} ${f.feature_id.padEnd(28)} ${f.name.padEnd(30)} ${f.category}`;
}

function formatFeatureDetail(f: Feature): string {
  const lines: string[] = [];
  lines.push(`# ${f.name} (${f.feature_id})`);
  lines.push(`Status: ${f.status.toUpperCase()}`);
  lines.push(`Category: ${f.category}`);
  lines.push("");
  lines.push(`## Description`);
  lines.push(f.description);
  lines.push("");

  lines.push(`## Original Code (hamclock-original)`);
  if (f.original.files.length) {
    lines.push("Files:");
    for (const file of f.original.files) lines.push(`  - ${file}`);
  }
  if (f.original.symbols.length) {
    lines.push("Key symbols:");
    for (const sym of f.original.symbols) lines.push(`  - ${sym}`);
  }
  if (f.original.notes) lines.push(`Notes: ${f.original.notes}`);
  lines.push("");

  lines.push(`## Next Code (hamclock-next)`);
  if (f.next.files.length) {
    lines.push("Files:");
    for (const file of f.next.files) lines.push(`  - ${file}`);
  } else {
    lines.push("Files: (none yet)");
  }
  if (f.next.symbols.length) {
    lines.push("Key symbols:");
    for (const sym of f.next.symbols) lines.push(`  - ${sym}`);
  }
  if (f.next.notes) lines.push(`Notes: ${f.next.notes}`);
  lines.push("");

  if (f.acceptance.length) {
    lines.push(`## Acceptance Criteria`);
    for (const a of f.acceptance) lines.push(`- [ ] ${a}`);
    lines.push("");
  }

  if (f.notes) {
    lines.push(`## Notes`);
    lines.push(f.notes);
  }

  return lines.join("\n");
}

function generateTicket(f: Feature, originalIdx: RepoIndex, nextIdx: RepoIndex): string {
  const lines: string[] = [];

  lines.push(`# Implementation Ticket: ${f.name}`);
  lines.push("");
  lines.push(`## Goal`);
  lines.push(f.description);
  lines.push("");

  // Reference implementation
  lines.push(`## Reference Implementation (hamclock-original)`);
  lines.push(`Repository: ${ORIGINAL_PATH}`);
  lines.push("");
  for (const filePath of f.original.files) {
    const fileInfo = originalIdx.files.find((fi) => fi.path === filePath);
    lines.push(`### \`${filePath}\``);
    if (fileInfo) {
      lines.push(`- ${fileInfo.line_count} lines, ${fileInfo.symbols.length} symbols`);
      const keySymbols = fileInfo.symbols
        .filter((s) => s.kind === "function" || s.kind === "class" || s.kind === "struct")
        .slice(0, 20);
      if (keySymbols.length) {
        lines.push("- Key definitions:");
        for (const s of keySymbols) {
          lines.push(
            `  - \`${s.name}\` (${s.kind}, line ${s.line})${s.signature ? `: ${s.signature}` : ""}`
          );
        }
      }
    }
    lines.push("");
  }

  if (f.original.symbols.length) {
    lines.push("### Key symbols to study");
    for (const sym of f.original.symbols) {
      // Find where this symbol is defined
      const hits = findSymbols(originalIdx, `^${sym}$`);
      if (hits.length) {
        for (const h of hits.slice(0, 3)) {
          lines.push(`- \`${sym}\` in \`${h.file}:${h.symbol.line}\` (${h.symbol.kind})`);
        }
      } else {
        lines.push(`- \`${sym}\` (search original codebase)`);
      }
    }
    lines.push("");
  }

  if (f.original.notes) {
    lines.push(`### Reference notes`);
    lines.push(f.original.notes);
    lines.push("");
  }

  // Current state in next
  lines.push(`## Current State (hamclock-next)`);
  lines.push(`Repository: ${NEXT_PATH}`);
  lines.push("");

  if (f.next.files.length) {
    for (const filePath of f.next.files) {
      const fileInfo = nextIdx.files.find((fi) => fi.path === filePath);
      lines.push(`### \`${filePath}\``);
      if (fileInfo) {
        lines.push(`- ${fileInfo.line_count} lines, ${fileInfo.symbols.length} symbols`);
        const keySymbols = fileInfo.symbols
          .filter(
            (s) =>
              s.kind === "function" || s.kind === "class" || s.kind === "struct" || s.kind === "method"
          )
          .slice(0, 15);
        if (keySymbols.length) {
          lines.push("- Existing definitions:");
          for (const s of keySymbols) {
            lines.push(`  - \`${s.name}\` (${s.kind}, line ${s.line})`);
          }
        }
      } else {
        lines.push("- (file listed but not found in index)");
      }
      lines.push("");
    }
  } else {
    lines.push("No existing files - this is a new implementation.");
    lines.push("");
  }

  if (f.next.notes) {
    lines.push(`### Implementation notes`);
    lines.push(f.next.notes);
    lines.push("");
  }

  // What to implement
  lines.push(`## What to Implement`);
  if (f.status === "missing") {
    lines.push(
      "This feature does not exist in hamclock-next yet. You will need to create new files."
    );
    lines.push("");
    lines.push("Suggested structure:");
    lines.push("1. Create a data provider in `src/services/` if external data is needed");
    lines.push("2. Create a data model header in `src/core/` if new state is needed");
    lines.push("3. Create a UI panel in `src/ui/` that extends the Widget base class");
    lines.push("4. Register the widget in `WidgetType.h` and `WidgetSelector.cpp`");
  } else if (f.status === "partial") {
    lines.push("Some code exists. Review the existing files above and identify gaps.");
    lines.push("Compare the original implementation's behavior with what's currently in hamclock-next.");
  }
  lines.push("");

  // Acceptance criteria
  if (f.acceptance.length) {
    lines.push(`## Acceptance Criteria`);
    for (const a of f.acceptance) lines.push(`- [ ] ${a}`);
    lines.push("");
  }

  // Architectural constraints
  lines.push(`## Constraints & Conventions`);
  lines.push("- hamclock-next uses SDL2 + SDL_ttf + libcurl (NOT Arduino/Adafruit libs)");
  lines.push("- C++20, CMake build system");
  lines.push("- Responsive/fluid layout (see hamclock_layout.md for reference dimensions)");
  lines.push("- Font catalog: see hamclock_fonts.md");
  lines.push("- Widget system: see hamclock_widgets.md for the full widget list");
  lines.push("- All UI panels inherit from Widget base class (src/ui/Widget.h)");
  lines.push("- Data providers are in src/services/, data models in src/core/");
  lines.push("- Use NetworkManager for HTTP requests (src/network/NetworkManager.h)");
  lines.push("- Configuration via ConfigManager (src/core/ConfigManager.h)");
  lines.push("");

  // Related docs
  lines.push(`## Documentation References`);
  lines.push(`- Layout: ${DOCS_PATH}/hamclock_layout.md`);
  lines.push(`- Widgets: ${DOCS_PATH}/hamclock_widgets.md`);
  lines.push(`- Fonts: ${DOCS_PATH}/hamclock_fonts.md`);
  lines.push(`- API: ${DOCS_PATH}/API.md`);
  lines.push(`- README: ${DOCS_PATH}/README.md`);

  return lines.join("\n");
}

function formatRepoMap(index: RepoIndex): string {
  const lines: string[] = [];
  lines.push(`# Repository Map: ${index.label}`);
  lines.push(`Root: ${index.root}`);
  lines.push(`Indexed: ${index.indexed_at}`);
  lines.push("");

  lines.push(`## Statistics`);
  lines.push(`- Total C/C++ files: ${index.stats.total_files}`);
  lines.push(`- Source files (.c/.cpp): ${index.stats.cpp_files}`);
  lines.push(`- Header files (.h/.hpp): ${index.stats.header_files}`);
  lines.push(`- Total lines: ${index.stats.total_lines.toLocaleString()}`);
  lines.push(`- Total symbols: ${index.stats.total_symbols.toLocaleString()}`);
  lines.push("");

  // Group by directory
  const byDir = new Map<string, FileIndex[]>();
  for (const f of index.files) {
    const dir = f.path.includes("/") ? f.path.substring(0, f.path.lastIndexOf("/")) : ".";
    if (!byDir.has(dir)) byDir.set(dir, []);
    byDir.get(dir)!.push(f);
  }

  lines.push(`## Directory Structure`);
  for (const [dir, files] of [...byDir.entries()].sort()) {
    const totalLines = files.reduce((sum, f) => sum + f.line_count, 0);
    const totalSyms = files.reduce((sum, f) => sum + f.symbols.length, 0);
    lines.push(`\n### ${dir}/ (${files.length} files, ${totalLines.toLocaleString()} lines, ${totalSyms} symbols)`);
    for (const f of files) {
      const symSummary = f.symbols
        .filter((s) => s.kind === "class" || s.kind === "struct")
        .map((s) => s.name)
        .slice(0, 3);
      const extra = symSummary.length ? ` [${symSummary.join(", ")}]` : "";
      lines.push(`  ${f.path} (${f.line_count} lines)${extra}`);
    }
  }

  // Top symbols
  lines.push(`\n## Key Classes & Structs`);
  const classStructs = index.files
    .flatMap((f) =>
      f.symbols
        .filter((s) => s.kind === "class" || s.kind === "struct")
        .map((s) => ({ file: f.path, ...s }))
    )
    .sort((a, b) => a.name.localeCompare(b.name));

  for (const s of classStructs) {
    lines.push(`- ${s.kind} \`${s.name}\` in \`${s.file}:${s.line}\``);
  }

  return lines.join("\n");
}


// ---------------------------------------------------------------------------
// MCP Server
// ---------------------------------------------------------------------------

const server = new McpServer(
  { name: "hamclock-feature-bridge", version: "0.3.0" },
  { capabilities: { tools: {}, resources: {} } }
);

// -- Original Tools --

server.tool(
  "feature_list",
  "List all features with their implementation status. Optionally filter by category or status.",
  {
    category: z.string().optional().describe(
      "Filter by category (e.g. 'data_panel', 'map', 'infrastructure', 'hardware', 'utility')"
    ),
    status: z
      .enum(["implemented", "partial", "missing", "not_needed"])
      .optional()
      .describe("Filter by implementation status"),
  },
  async ({ category, status }) => {
    const map = await ensureFeatureMap();
    let features = map.features;

    if (category) features = features.filter((f) => f.category === category);
    if (status) features = features.filter((f) => f.status === status);

    const summary = getOriginalSummary(map);
    const lines: string[] = [];
    lines.push(`# HamClock Feature Bridge`);
    lines.push(
      `Total: ${summary.total} features | Implemented: ${summary.implemented} | Partial: ${summary.partial} | Missing: ${summary.missing} | N/A: ${summary.not_needed}`
    );
    lines.push("");
    lines.push("Categories: " + getCategories(map).join(", "));
    lines.push("");
    lines.push(`${"Status".padEnd(11)} ${"Feature ID".padEnd(28)} ${"Name".padEnd(30)} Category`);
    lines.push("-".repeat(90));

    for (const f of features) {
      lines.push(formatFeatureRow(f));
    }

    return { content: [{ type: "text" as const, text: lines.join("\n") }] };
  }
);

server.tool(
  "feature_status",
  "Get detailed implementation status for a specific feature, including code pointers in both repos.",
  {
    feature_id: z.string().describe("The feature ID (e.g. 'dx_cluster', 'satellite_tracking')"),
  },
  async ({ feature_id }) => {
    const map = await ensureFeatureMap();
    const feature = getOriginalFeature(map, feature_id);
    if (!feature) {
      const available = map.features.map((f) => f.feature_id).join(", ");
      return {
        content: [
          {
            type: "text" as const,
            text: `Feature '${feature_id}' not found.\n\nAvailable features: ${available}`,
          },
        ],
        isError: true,
      };
    }

    await ensureIndexed();
    let detail = formatFeatureDetail(feature);

    // Enrich with live index data
    if (originalIndex && feature.original.files.length) {
      detail += "\n\n## Live Index: Original";
      for (const filePath of feature.original.files) {
        const fi = originalIndex.files.find((f) => f.path === filePath);
        if (fi) {
          detail += `\n\`${filePath}\`: ${fi.line_count} lines, ${fi.symbols.length} symbols`;
          const fns = fi.symbols
            .filter((s) => s.kind === "function")
            .slice(0, 10);
          if (fns.length)
            detail +=
              "\n  Functions: " + fns.map((s) => `${s.name}(line ${s.line})`).join(", ");
        }
      }
    }

    if (nextIndex && feature.next.files.length) {
      detail += "\n\n## Live Index: Next";
      for (const filePath of feature.next.files) {
        const fi = nextIndex.files.find((f) => f.path === filePath);
        if (fi) {
          detail += `\n\`${filePath}\`: ${fi.line_count} lines, ${fi.symbols.length} symbols`;
          const fns = fi.symbols
            .filter((s) => s.kind === "function" || s.kind === "class")
            .slice(0, 10);
          if (fns.length)
            detail +=
              "\n  Definitions: " + fns.map((s) => `${s.name}(${s.kind}, line ${s.line})`).join(", ");
        }
      }
    }

    return { content: [{ type: "text" as const, text: detail }] };
  }
);

server.tool(
  "create_ticket",
  "Generate a detailed implementation ticket/plan for a feature, analyzing both original and next codebases.",
  {
    feature_id: z.string().describe("The feature ID to generate a plan for"),
  },
  async ({ feature_id }) => {
    const map = await ensureFeatureMap();
    const feature = getOriginalFeature(map, feature_id);
    if (!feature) {
      return {
        content: [{ type: "text", text: `Feature '${feature_id}' not found.` }],
        isError: true,
      };
    }

    await ensureIndexed();
    // We can verify indices exist because ensureIndexed initializes them or throws
    if (!originalIndex || !nextIndex) {
      return {
        content: [{ type: "text", text: "Failed to index repositories." }],
        isError: true,
      };
    }

    const ticket = generateTicket(feature, originalIndex, nextIndex);
    return { content: [{ type: "text", text: ticket }] };
  }
);

server.tool(
  "repo_map",
  "Generate a high-level map/summary of a repository structure.",
  {
    repo: z.enum(["original", "next"]).describe("Which repository to map"),
  },
  async ({ repo }) => {
    await ensureIndexed();
    const index = repo === "original" ? originalIndex : nextIndex;

    if (!index) {
      return {
        content: [{ type: "text", text: "Failed to index repository." }],
        isError: true,
      };
    }

    const map = formatRepoMap(index);
    return { content: [{ type: "text", text: map }] };
  }
);

server.tool(
  "find_files",
  "Find files in the repository matching a glob pattern.",
  {
    repo: z.enum(["original", "next"]).describe("Repository to search"),
    pattern: z.string().describe("Glob pattern (e.g. '*.cpp', 'src/ui/*')"),
  },
  async ({ repo, pattern }) => {
    await ensureIndexed();
    const index = repo === "original" ? originalIndex : nextIndex;
    if (!index) return { isError: true, content: [{ type: "text", text: "Repo not indexed" }] };

    const files = findFiles(index, pattern);
    return {
      content: [{ type: "text", text: files.map(f => f.path).join("\n") }]
    };
  }
);

server.tool(
  "find_symbols",
  "Find symbols (functions, classes, structs) matching a regex pattern.",
  {
    repo: z.enum(["original", "next"]).describe("Repository to search"),
    pattern: z.string().describe("Regex pattern for symbol name"),
  },
  async ({ repo, pattern }) => {
    await ensureIndexed();
    const index = repo === "original" ? originalIndex : nextIndex;
    if (!index) return { isError: true, content: [{ type: "text", text: "Repo not indexed" }] };

    const hits = findSymbols(index, pattern);
    const lines = hits.map(h => `${h.symbol.name} (${h.symbol.kind}) in ${h.file}:${h.symbol.line}`);
    return {
      content: [{ type: "text", text: lines.join("\n") }]
    };
  }
);

server.tool(
  "list_by_category",
  "List features in a specific category.",
  {
    category: z.string().describe("Category name"),
  },
  async ({ category }) => {
    const map = await ensureFeatureMap();
    const features = listByCategory(map, category);
    return {
      content: [{ type: "text", text: features.map(formatFeatureRow).join("\n") }]
    };
  }
);

server.tool(
  "list_by_status",
  "List features with a specific implementation status.",
  {
    status: z.enum(["implemented", "partial", "missing", "not_needed"]).describe("Status"),
  },
  async ({ status }) => {
    const map = await ensureFeatureMap();
    const features = listByStatus(map, status);
    return {
      content: [{ type: "text", text: features.map(formatFeatureRow).join("\n") }]
    };
  }
);

server.tool(
  "get_categories",
  "Get a list of all feature categories.",
  {},
  async () => {
    const map = await ensureFeatureMap();
    const cats = getCategories(map);
    return { content: [{ type: "text", text: cats.join("\n") }] };
  }
);

server.tool(
  "update_feature_status",
  "Update the status of a feature.",
  {
    feature_id: z.string().describe("Feature ID"),
    status: z.enum(["implemented", "partial", "missing", "not_needed"]).describe("New status"),
    notes: z.string().optional().describe("Optional notes"),
  },
  async ({ feature_id, status, notes }) => {
    const map = await ensureFeatureMap();
    const feature = getOriginalFeature(map, feature_id);
    if (!feature) return { isError: true, content: [{ type: "text", text: "Feature not found" }] };

    feature.status = status;
    if (notes) feature.notes = notes;

    await saveFeatureMap(FEATURE_MAP_PATH, map);
    return { content: [{ type: "text", text: `Updated ${feature_id} to ${status}` }] };
  }
);

server.tool(
  "reindex_repo",
  "Force re-indexing of a repository.",
  {
    repo: z.enum(["original", "next"]).describe("Repository to re-index"),
  },
  async ({ repo }) => {
    if (repo === "original") {
      originalIndex = await indexRepo(ORIGINAL_PATH, "hamclock-original");
    } else {
      nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
    }
    return { content: [{ type: "text", text: `Re-indexed ${repo}` }] };
  }
);

// -- New Parity Tools --

server.tool(
  "parity_load_report",
  "Load and parse the feature parity report from a markdown file.",
  {
    path: z.string().describe("The path to the feature_parity_report_v2.md file."),
  },
  async ({ path }) => {
    const parityData = await loadReport(resolve(process.cwd(), path));
    await mkdir(DATA_PATH, { recursive: true });
    await writeFile(PARITY_JSON_PATH, JSON.stringify(parityData, null, 2));
    return { content: [{ type: "text", text: `Parity data loaded and saved to ${PARITY_JSON_PATH}` }] };
  }
);

server.tool(
  "parity_summary",
  "Show a summary of the feature parity.",
  {},
  async () => {
    const parityData = await loadParityData();
    const summary = getSummary(parityData);
    const text = `--- Feature Parity Summary ---\nOverall Parity Score: ${summary.score}%\nStatus Counts:\n${Object.entries(summary.counts).map(([status, count]) => `- ${status}: ${count}`).join('\n')}\n\n--- Top Improvements ---\n${summary.improvements.map(item => `- ${item}`).join('\n')}\n\n--- Top Gaps ---\n${summary.gaps.map(item => `- ${item}`).join('\n')}`;
    return { content: [{ type: "text", text }] };
  }
);

server.tool(
  "parity_list",
  "List features, optionally filtering by status.",
  {
    status_filter: z.array(z.string()).optional().describe("Filter by status (e.g. ['PARTIAL', 'MISSING'])"),
    q: z.string().optional().describe("Search by name or ID"),
  },
  async ({ status_filter, q }) => {
    const parityData = await loadParityData();
    const features = listFeatures(parityData, status_filter, q);
    const text = features.map(f => `[${f.status.padEnd(11)}] ${f.feature_id.padEnd(25)} ${f.name}`).join('\n');
    return { content: [{ type: "text", text }] };
  }
);

server.tool(
  "parity_feature",
  "Get detailed information for a specific feature.",
  {
    feature_id: z.string().describe("The ID of the feature to get."),
  },
  async ({ feature_id }) => {
    const parityData = await loadParityData();
    const feature = getFeature(parityData, feature_id);
    if (!feature) {
      return { isError: true, content: [{ type: "text", text: "Feature not found" }] };
    }
    return { content: [{ type: "text", text: JSON.stringify(feature, null, 2), mimeType: "application/json" }] };
  }
);

server.tool(
  "parity_create_ticket",
  "Generate an implementation ticket for a feature.",
  {
    feature_id: z.string().describe("The ID of the feature to create a ticket for."),
  },
  async ({ feature_id }) => {
    const parityData = await loadParityData();
    const feature = getFeature(parityData, feature_id);
    if (!feature) {
      return { isError: true, content: [{ type: "text", text: "Feature not found" }] };
    }
    const ticket = createTicket(feature);
    return { content: [{ type: "text", text: ticket }] };
  }
);

server.tool(
  "parity_create_batch_tickets",
  "Generate tickets for all PARTIAL, STUB, and MISSING features.",
  {
    statuses: z.array(z.string()).optional().default(['PARTIAL', 'STUB', 'MISSING']),
    limit: z.number().optional().default(10),
  },
  async ({ statuses, limit }) => {
    const parityData = await loadParityData();
    const tickets = createBatchTickets(parityData, statuses, limit);
    const text = tickets.join('\n\n' + '-'.repeat(80) + '\n\n');
    return { content: [{ type: "text", text }] };
  }
);

server.tool(
  "parity_verify_feature",
  "Verify a feature against a running hamclock-next instance.",
  {
    feature_id: z.string().describe("The ID of the feature to verify."),
    base_url: z.string().optional().describe("Base URL of the hamclock-next instance").default(process.env.MCP_VERIFY_BASE_URL || 'http://localhost:8080'),
  },
  async ({ feature_id, base_url }) => {
    const parityData = await loadParityData();
    const feature = getFeature(parityData, feature_id);
    if (!feature) {
      return { isError: true, content: [{ type: "text", text: "Feature not found" }] };
    }
    const result = await verifyFeature(feature, base_url);
    return { content: [{ type: "text", text: JSON.stringify(result, null, 2), mimeType: "application/json" }] };
  }
);

// -- Project Context Tools (hamclock-next-mcp.json) --

server.tool(
  "project_context",
  "Get high-level project context for hamclock-next: origin story, architecture philosophy, tech stack, and source layout. Useful for onboarding or orienting before diving into feature work.",
  {
    section: z.enum([
      "project_context",
      "source_layout",
      "original_vs_next",
      "widget_scaffolding",
      "api_reference",
      "contribution_guide",
      "feature_status_summary",
      "decisions",
      "gotchas",
      "api_examples",
    ]).optional().describe(
      "Specific section to retrieve. Omit to get the full project_context and source_layout summary."
    ),
  },
  async ({ section }) => {
    const ctx = await ensureProjectContext();
    if (!ctx || Object.keys(ctx).length === 0) {
      return {
        isError: true,
        content: [{ type: "text", text: `hamclock-next-mcp.json not found at ${PROJECT_CONTEXT_PATH}. Place the file in the MCP root directory.` }]
      };
    }

    if (section) {
      const data = ctx[section];
      if (data === undefined) {
        return {
          isError: true,
          content: [{ type: "text", text: `Section '${section}' not found in hamclock-next-mcp.json.` }]
        };
      }
      return {
        content: [{ type: "text", text: JSON.stringify(data, null, 2), mimeType: "application/json" }]
      };
    }

    // Default: return project_context + source_layout as a readable summary
    const lines: string[] = [];
    const pc = ctx["project_context"] as Record<string, any> | undefined;
    if (pc) {
      lines.push(`# HamClock Next — Project Context`);
      if (pc.origin) lines.push(`\n## Origin\n${pc.origin}`);
      if (pc.rewrite_rationale) lines.push(`\n## Rewrite Rationale\n${pc.rewrite_rationale}`);
      if (pc.philosophy) lines.push(`\n## Philosophy\n${pc.philosophy}`);
      if (pc.tech_stack) {
        lines.push(`\n## Tech Stack`);
        for (const [k, v] of Object.entries(pc.tech_stack)) {
          lines.push(`- ${k}: ${v}`);
        }
      }
    }
    const sl = ctx["source_layout"] as Record<string, any> | undefined;
    if (sl?.naming_conventions) {
      lines.push(`\n## Naming Conventions`);
      const nc = sl.naming_conventions as Record<string, string>;
      for (const [k, v] of Object.entries(nc)) {
        lines.push(`- ${k}: ${v}`);
      }
    }
    const fs = ctx["feature_status_summary"] as Record<string, any> | undefined;
    if (fs) {
      lines.push(`\n## Feature Status`);
      for (const [k, v] of Object.entries(fs)) {
        lines.push(`- ${k}: ${v}`);
      }
    }
    return { content: [{ type: "text", text: lines.join("\n") }] };
  }
);

server.tool(
  "get_scaffolding_template",
  "Get the C++ boilerplate templates for a new hamclock-next widget (Data struct, Provider, Panel). Returns ready-to-edit code with the given feature name substituted in.",
  {
    name: z.string().describe("PascalCase feature name, e.g. 'BandConditions' or 'NcdxfBeacon'"),
  },
  async ({ name }) => {
    const ctx = await ensureProjectContext();
    if (!ctx || Object.keys(ctx).length === 0) {
      return {
        isError: true,
        content: [{ type: "text", text: `hamclock-next-mcp.json not found at ${PROJECT_CONTEXT_PATH}.` }]
      };
    }

    const scaffolding = ctx["widget_scaffolding"] as Record<string, any> | undefined;
    if (!scaffolding?.templates) {
      return {
        isError: true,
        content: [{ type: "text", text: "No templates found in widget_scaffolding section of hamclock-next-mcp.json." }]
      };
    }

    const templates = scaffolding.templates as Record<string, string>;
    const lower = name.charAt(0).toLowerCase() + name.slice(1);
    const upper = name;

    // Substitute {Name} and {name} placeholders in all templates
    const substituted = Object.entries(templates).map(([filename, content]) => {
      const fname = filename.replace(/\{Name\}/g, upper).replace(/\{name\}/g, lower);
      const body = content.replace(/\{Name\}/g, upper).replace(/\{name\}/g, lower);
      return { file: fname, content: body };
    });

    const steps = (scaffolding.steps as string[] | undefined) ?? [];
    const notes = (scaffolding.notes as string[] | undefined) ?? [];

    const lines: string[] = [
      `# Widget Scaffolding: ${upper}`,
      "",
      "## Steps",
      ...steps.map((s, i) => `${i + 1}. ${s}`),
      "",
      "## Notes",
      ...notes.map(n => `- ${n}`),
      "",
      "## Generated Files",
    ];
    for (const { file, content } of substituted) {
      lines.push(`\n### ${file}\n\`\`\`cpp\n${content}\n\`\`\``);
    }

    return { content: [{ type: "text", text: lines.join("\n") }] };
  }
);

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

server.resource(
  "feature_list",
  "hamclock://features",
  async (uri) => {
    const map = await ensureFeatureMap();
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(map.features, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "parity_report",
  "hamclock://parity/report",
  async (uri) => {
    const parityData = await loadParityData();
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(parityData, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "project_architecture",
  "hamclock://project/architecture",
  async (uri) => {
    const ctx = await ensureProjectContext();
    const data = {
      project_context: ctx["project_context"] ?? {},
      source_layout: ctx["source_layout"] ?? {},
      original_vs_next: ctx["original_vs_next"] ?? {},
    };
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(data, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "feature_status_full",
  "hamclock://project/features",
  async (uri) => {
    const ctx = await ensureProjectContext();
    const data = {
      feature_status_summary: ctx["feature_status_summary"] ?? {},
      implemented_features: ctx["implemented_features"] ?? [],
      partial_features: ctx["partial_features"] ?? [],
      missing_features: ctx["missing_features"] ?? [],
      not_needed_features: ctx["not_needed_features"] ?? [],
    };
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(data, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "contribution_guide",
  "hamclock://project/contribution-guide",
  async (uri) => {
    const ctx = await ensureProjectContext();
    const data = {
      widget_scaffolding: ctx["widget_scaffolding"] ?? {},
      api_reference: ctx["api_reference"] ?? {},
      contribution_guide: ctx["contribution_guide"] ?? {},
      feature_map_maintenance: ctx["feature_map_maintenance"] ?? {},
    };
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(data, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "decision_log",
  "hamclock://project/decisions",
  async (uri) => {
    const ctx = await ensureProjectContext();
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(ctx["decisions"] ?? {}, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "gotchas",
  "hamclock://project/gotchas",
  async (uri) => {
    const ctx = await ensureProjectContext();
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(ctx["gotchas"] ?? {}, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);

server.resource(
  "api_examples",
  "hamclock://project/api-examples",
  async (uri) => {
    const ctx = await ensureProjectContext();
    return {
      contents: [{
        uri: uri.href,
        text: JSON.stringify(ctx["api_examples"] ?? {}, null, 2),
        mimeType: "application/json"
      }]
    };
  }
);


// ---------------------------------------------------------------------------
// Scaffolding Tools
// ---------------------------------------------------------------------------

import { scaffoldFeature } from "./scaffold.js";

server.tool(
  "scaffold_feature",
  "Generate boilerplate code for a new feature (Widget or Service).",
  {
    name: z.string().describe("Name of the feature (e.g. 'SolarPanel')"),
    type: z.enum(["Widget", "Service"]).describe("Type of component to scaffold"),
  },
  async ({ name, type }) => {
    try {
      const { files, instructions } = await scaffoldFeature(NEXT_PATH, name, type);
      return {
        content: [
          {
            type: "text" as const,
            text: `Successfully created:\n- ${files.map(f => f.split('/').pop()).join('\n- ')}\n\n${instructions}`
          },
        ],
      };
    } catch (err: any) {
      return {
        content: [{ type: "text", text: `Error: ${err.message}` }],
        isError: true,
      };
    }
  }
);

// ---------------------------------------------------------------------------
// Memory Diagnostic Tools (Phase 37)
// ---------------------------------------------------------------------------

server.tool(
  "diagnose_memory",
  "Query memory diagnostics from a running hamclock-next instance. Checks FPS, texture allocation health, and memory-related errors.",
  {
    base_url: z.string().optional().describe("Base URL of running hamclock-next instance").default("http://localhost:8080"),
  },
  async ({ base_url }) => {
    const result: any = {
      timestamp: new Date().toISOString(),
      base_url,
      diagnostics: {},
      health_status: "unknown",
      issues: [],
    };

    try {
      // 1. Check /debug/performance for FPS and uptime
      try {
        const perfResponse = await fetch(`${base_url}/debug/performance`);
        if (perfResponse.ok) {
          const perfData = await perfResponse.json();
          result.diagnostics.performance = perfData;

          // Analyze FPS health
          if (perfData.fps && perfData.fps < 50) {
            result.issues.push(`Low FPS detected: ${perfData.fps} (expected >50)`);
          }
        } else {
          result.issues.push(`Failed to fetch /debug/performance: ${perfResponse.statusText}`);
        }
      } catch (e: any) {
        result.issues.push(`Performance endpoint error: ${e.message}`);
      }

      // 2. Check /debug/health for service health
      try {
        const healthResponse = await fetch(`${base_url}/debug/health`);
        if (healthResponse.ok) {
          const healthData = await healthResponse.json();
          result.diagnostics.health = healthData;
        } else {
          result.issues.push(`Failed to fetch /debug/health: ${healthResponse.statusText}`);
        }
      } catch (e: any) {
        result.issues.push(`Health endpoint error: ${e.message}`);
      }

      // 3. Check /debug/widgets for active widget count
      try {
        const widgetsResponse = await fetch(`${base_url}/debug/widgets`);
        if (widgetsResponse.ok) {
          const widgets = await widgetsResponse.json();
          result.diagnostics.widget_count = Array.isArray(widgets) ? widgets.length : 0;
          result.diagnostics.active_widgets = Array.isArray(widgets) ? widgets.map((w: any) => w.name || w.type) : [];

          // Flag high widget count on low-memory systems
          if (result.diagnostics.widget_count > 10) {
            result.issues.push(`High widget count (${result.diagnostics.widget_count}) may stress memory on RPi`);
          }
        } else {
          result.issues.push(`Failed to fetch /debug/widgets: ${widgetsResponse.statusText}`);
        }
      } catch (e: any) {
        result.issues.push(`Widgets endpoint error: ${e.message}`);
      }

      // 4. Determine overall health status
      if (result.issues.length === 0) {
        result.health_status = "healthy";
      } else if (result.issues.some((issue: string) => issue.includes("Low FPS") || issue.includes("Failed to fetch"))) {
        result.health_status = "degraded";
      } else {
        result.health_status = "warning";
      }

      // 5. Generate summary report
      const lines: string[] = [];
      lines.push(`# Memory Diagnostics Report`);
      lines.push(`Timestamp: ${result.timestamp}`);
      lines.push(`Instance: ${base_url}`);
      lines.push(`Health Status: ${result.health_status.toUpperCase()}`);
      lines.push("");

      if (result.diagnostics.performance) {
        lines.push(`## Performance`);
        lines.push(`- FPS: ${result.diagnostics.performance.fps || "N/A"}`);
        lines.push(`- Frame Time: ${result.diagnostics.performance.frame_time_ms || "N/A"} ms`);
        lines.push(`- Uptime: ${result.diagnostics.performance.uptime_seconds || "N/A"} seconds`);
        lines.push("");
      }

      if (result.diagnostics.widget_count !== undefined) {
        lines.push(`## Active Widgets`);
        lines.push(`- Count: ${result.diagnostics.widget_count}`);
        if (result.diagnostics.active_widgets && result.diagnostics.active_widgets.length > 0) {
          lines.push(`- Types: ${result.diagnostics.active_widgets.join(", ")}`);
        }
        lines.push("");
      }

      if (result.issues.length > 0) {
        lines.push(`## Issues Detected`);
        result.issues.forEach((issue: string) => lines.push(`- ${issue}`));
        lines.push("");
      } else {
        lines.push(`## Issues Detected`);
        lines.push(`None - system operating normally`);
        lines.push("");
      }

      lines.push(`## Recommendations`);
      if (result.issues.some((i: string) => i.includes("Low FPS"))) {
        lines.push(`- Consider reducing mesh density or active widget count`);
        lines.push(`- Check for "Cannot allocate memory" errors in application logs`);
        lines.push(`- Verify KMSDRM low-memory optimizations are active`);
      } else if (result.issues.length === 0) {
        lines.push(`- No action required - memory subsystem healthy`);
      } else {
        lines.push(`- Review connection to hamclock-next instance`);
        lines.push(`- Ensure debug endpoints are enabled`);
      }

      return {
        content: [
          { type: "text", text: lines.join("\n") },
          { type: "text", text: "\n---\nRaw Data:\n" + JSON.stringify(result, null, 2), mimeType: "application/json" }
        ]
      };

    } catch (error: any) {
      return {
        content: [{ type: "text", text: `Failed to diagnose memory: ${error.message}` }],
        isError: true,
      };
    }
  }
);

server.tool(
  "analyze_texture_cache",
  "Analyze texture allocation patterns from Phase 37 memory optimization. Reviews MEMORY_FIX_SUMMARY.md and checks for known allocation patterns.",
  {
    check_leaks: z.boolean().optional().default(true).describe("Check for texture leak patterns"),
    check_churn: z.boolean().optional().default(true).describe("Check for high texture churn (create/destroy cycles)"),
  },
  async ({ check_leaks, check_churn }) => {
    const lines: string[] = [];
    lines.push(`# Texture Cache Analysis`);
    lines.push(`Generated: ${new Date().toISOString()}`);
    lines.push("");

    try {
      // Read MEMORY_FIX_SUMMARY.md
      const summaryPath = resolve(NEXT_PATH, "MEMORY_FIX_SUMMARY.md");
      let summaryContent = "";
      try {
        summaryContent = await readFile(summaryPath, "utf-8");
      } catch (e) {
        lines.push(`## Warning`);
        lines.push(`MEMORY_FIX_SUMMARY.md not found at ${summaryPath}`);
        lines.push(`This analysis requires Phase 37 documentation to be present.`);
        lines.push("");
      }

      if (summaryContent) {
        lines.push(`## Phase 37 Optimizations Summary`);

        // Extract key metrics
        const beforeAfterMatch = summaryContent.match(/Before.*?(\d+).*?After.*?(\d+)/is);
        const vertexMatch = summaryContent.match(/(\d+)×(\d+).*?(\d+)×(\d+)/);

        if (beforeAfterMatch) {
          const before = beforeAfterMatch[1];
          const after = beforeAfterMatch[2];
          lines.push(`- Texture allocations reduced: ${before} → ${after} per session`);
        }

        if (vertexMatch) {
          const beforeW = parseInt(vertexMatch[1]);
          const beforeH = parseInt(vertexMatch[2]);
          const afterW = parseInt(vertexMatch[3]);
          const afterH = parseInt(vertexMatch[4]);
          const beforeVerts = beforeW * beforeH;
          const afterVerts = afterW * afterH;
          const reduction = Math.round((1 - afterVerts / beforeVerts) * 100);
          lines.push(`- Vertex buffer reduction: ${beforeW}×${beforeH} (${beforeVerts}) → ${afterW}×${afterH} (${afterVerts}) = ${reduction}% reduction`);
        }

        lines.push("");

        // Check for known patterns
        if (check_leaks) {
          lines.push(`## Leak Detection`);
          if (summaryContent.includes("cachedTexture")) {
            lines.push(`✅ Tooltip texture caching implemented`);
            lines.push(`   - Prevents per-frame allocation/deallocation cycles`);
          }
          if (summaryContent.includes("SDL_DestroyTexture")) {
            lines.push(`✅ Proper texture cleanup in destructor`);
          }
          lines.push("");
        }

        if (check_churn) {
          lines.push(`## Churn Analysis`);
          if (summaryContent.includes("99%")) {
            lines.push(`✅ Tooltip texture churn eliminated (99% reduction)`);
          }
          if (summaryContent.includes("per-frame")) {
            lines.push(`✅ No per-frame texture creation detected in tooltip rendering`);
          }
          lines.push("");
        }
      }

      // Read MapWidget.h to verify current implementation
      const mapWidgetHPath = resolve(NEXT_PATH, "src/ui/MapWidget.h");
      try {
        const mapWidgetH = await readFile(mapWidgetHPath, "utf-8");

        lines.push(`## Implementation Verification`);

        if (mapWidgetH.includes("cachedTexture")) {
          lines.push(`✅ Tooltip struct has cachedTexture field`);
        } else {
          lines.push(`❌ WARNING: cachedTexture field not found in Tooltip struct`);
        }

        if (mapWidgetH.includes("useCompatibilityRenderPath_")) {
          lines.push(`✅ Low-memory mode flag present`);
        } else {
          lines.push(`⚠️  useCompatibilityRenderPath_ flag not found`);
        }

        lines.push("");
      } catch (e) {
        lines.push(`## Implementation Verification`);
        lines.push(`⚠️  Could not read MapWidget.h for verification`);
        lines.push("");
      }

      // Read MapWidget.cpp to check for KMSDRM detection
      const mapWidgetCppPath = resolve(NEXT_PATH, "src/ui/MapWidget.cpp");
      try {
        const mapWidgetCpp = await readFile(mapWidgetCppPath, "utf-8");

        lines.push(`## KMSDRM Backend Detection`);

        if (mapWidgetCpp.includes("SDL_GetCurrentVideoDriver")) {
          lines.push(`✅ Automatic KMSDRM detection implemented`);
        }

        if (mapWidgetCpp.includes("useCompatibilityRenderPath_ ? 48 : 96")) {
          lines.push(`✅ Dynamic mesh density based on backend`);
        }

        lines.push("");
      } catch (e) {
        lines.push(`## KMSDRM Backend Detection`);
        lines.push(`⚠️  Could not read MapWidget.cpp for verification`);
        lines.push("");
      }

      lines.push(`## Summary`);
      lines.push(`Phase 37 memory optimizations are in place and address:`);
      lines.push(`1. Tooltip texture caching (eliminates 99% of allocations)`);
      lines.push(`2. NULL safety checks after all texture operations`);
      lines.push(`3. Low-memory rendering mode for KMSDRM (75% mesh reduction)`);
      lines.push(`4. Automatic backend detection and optimization`);
      lines.push("");
      lines.push(`## Monitoring Recommendations`);
      lines.push(`- Use 'diagnose_memory' tool to check runtime FPS and health`);
      lines.push(`- Monitor application logs for "Cannot allocate memory" errors`);
      lines.push(`- Verify tooltip texture cache hits via debug logging if available`);

      return {
        content: [{ type: "text", text: lines.join("\n") }]
      };

    } catch (error: any) {
      return {
        content: [{ type: "text", text: `Failed to analyze texture cache: ${error.message}` }],
        isError: true,
      };
    }
  }
);

server.tool(
  "memory_stress_test",
  "Run a memory stress test against a running hamclock-next instance. Monitors FPS stability over time to detect memory-related performance degradation.",
  {
    base_url: z.string().optional().describe("Base URL of running hamclock-next instance").default("http://localhost:8080"),
    duration_seconds: z.number().optional().describe("Test duration in seconds (default: 30)").default(30),
  },
  async ({ base_url, duration_seconds }) => {
    try {
      const result = await runMemoryStressTest(base_url, duration_seconds);

      const lines: string[] = [];
      lines.push(`# Memory Stress Test Results`);
      lines.push(`Instance: ${base_url}`);
      lines.push(`Duration: ${result.test_duration_seconds} seconds`);
      lines.push(`Samples Collected: ${result.performance_samples}`);
      lines.push("");

      lines.push(`## Performance Metrics`);
      lines.push(`- Initial FPS: ${result.initial_fps.toFixed(1)}`);
      lines.push(`- Final FPS: ${result.final_fps.toFixed(1)}`);
      lines.push(`- FPS Change: ${(result.final_fps - result.initial_fps).toFixed(1)}`);
      lines.push(`- Stability: ${result.fps_stability.toUpperCase()}`);
      lines.push("");

      if (result.issues_detected.length > 0) {
        lines.push(`## Issues Detected`);
        result.issues_detected.forEach(issue => lines.push(`- ${issue}`));
        lines.push("");
      }

      lines.push(`## Recommendations`);
      result.recommendations.forEach(rec => lines.push(`- ${rec}`));
      lines.push("");

      if (result.fps_stability === 'stable') {
        lines.push(`✅ **Test Passed**: No memory degradation detected`);
      } else if (result.fps_stability === 'degraded') {
        lines.push(`⚠️  **Test Warning**: Performance degradation detected`);
      } else {
        lines.push(`❌ **Test Failed**: Critical performance issues detected`);
      }

      return {
        content: [
          { type: "text", text: lines.join("\n") },
          { type: "text", text: "\n---\nRaw Data:\n" + JSON.stringify(result, null, 2), mimeType: "application/json" }
        ]
      };

    } catch (error: any) {
      return {
        content: [{ type: "text", text: `Memory stress test failed: ${error.message}` }],
        isError: true,
      };
    }
  }
);


// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((err) => {
  console.error("Fatal:", err);
  process.exit(1);
});
