#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { loadParityData, ensureProjectContext } from "./state.js";

// Import tool registration functions
import { registerRepoTools } from "./tools/repo.js";
import { registerParityTools } from "./tools/parity.js";
import { registerContributorTools } from "./tools/contributor.js";
import { registerPropagationTools } from "./tools/propagation.js";
import { registerScaffoldTools } from "./tools/scaffold.js";
import { registerDiagnosticTools } from "./tools/diagnostics.js";

// Initialize MCP Server
const server = new McpServer(
  { name: "hamclock-next", version: "1.0.0" },
  { capabilities: { tools: {}, resources: {} } }
);

// Register all tools
registerRepoTools(server);
registerParityTools(server);
registerContributorTools(server);
registerPropagationTools(server);
registerScaffoldTools(server);
registerDiagnosticTools(server);

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

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
// Server Startup
// ---------------------------------------------------------------------------

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error("HamClock-Next MCP Server running on stdio");
}

main().catch((error) => {
  console.error("Fatal error in main():", error);
  process.exit(1);
});
