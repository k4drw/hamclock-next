import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { loadParityData } from "../state.js";
import { getSummary, listFeatures, getFeature, createTicket, createBatchTickets, loadReport, saveReport } from "../parity.js";
import { verifyFeature } from "../verification.js";
import { PARITY_JSON_PATH } from "../state.js";

export function registerParityTools(server: McpServer) {
  server.tool(
    "parity_sync",
    "Synchronize parity_v2.json from a Markdown report file (e.g. feature_overview.md).",
    {
      source_path: z.string().describe("Path to the Markdown report file."),
    },
    async ({ source_path }) => {
      const data = await loadReport(source_path);
      await saveReport(PARITY_JSON_PATH, data);
      return { content: [{ type: "text", text: `Successfully synchronized parity data from ${source_path} to ${PARITY_JSON_PATH}` }] };
    }
  );

  server.tool(
    "parity_summary",
    "Show a summary of the feature parity.",
    {},
    async () => {
      const parityData = await loadParityData();
      const summary = getSummary(parityData);
      const text = `--- Feature Parity Summary ---
Overall Parity Score: ${summary.score}%
Status Counts:
${Object.entries(summary.counts).map(([status, count]) => `- ${status}: ${count}`).join('\n')}

--- Top Improvements ---
${summary.improvements.map(item => `- ${item}`).join('\n')}

--- Top Gaps ---
${summary.gaps.map(item => `- ${item}`).join('\n')}`;
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
      const text = features.map(f => `- [${f.status}] **${f.name}** (${f.feature_id})`).join('\n');
      return { content: [{ type: "text", text: text || "No features found." }] };
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
        return { content: [{ type: "text", text: `Feature '${feature_id}' not found.` }] };
      }
      return { content: [{ type: "text", text: JSON.stringify(feature, null, 2) }] };
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
        return { content: [{ type: "text", text: `Feature '${feature_id}' not found.` }] };
      }
      return { content: [{ type: "text", text: createTicket(feature) }] };
    }
  );

  server.tool(
    "parity_create_batch_tickets",
    "Generate tickets for all PARTIAL, STUB, and MISSING features.",
    {
      statuses: z.array(z.string()).default(['PARTIAL', 'STUB', 'MISSING']),
      limit: z.number().default(10),
    },
    async ({ statuses, limit }) => {
      const parityData = await loadParityData();
      const tickets = createBatchTickets(parityData, statuses, limit);
      return { content: [{ type: "text", text: tickets.join('\n\n---\n\n') || "No features match criteria." }] };
    }
  );

  server.tool(
    "parity_verify_feature",
    "Verify a feature against a running hamclock-next instance.",
    {
      feature_id: z.string().describe("The ID of the feature to verify."),
      base_url: z.string().default("http://localhost:8080").describe("Base URL of the hamclock-next instance"),
    },
    async ({ feature_id, base_url }) => {
      const parityData = await loadParityData();
      const feature = getFeature(parityData, feature_id);
      if (!feature) {
        return { content: [{ type: "text", text: `Feature '${feature_id}' not found.` }] };
      }
      const result = await verifyFeature(feature, base_url);
      return { content: [{ type: "text", text: JSON.stringify(result, null, 2) }] };
    }
  );
}
