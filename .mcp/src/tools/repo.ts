import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { ensureIndexed, reindex } from "../state.js";
import { findFiles, findSymbols } from "../indexer.js";
import { formatRepoMap } from "../helpers.js";

export function registerRepoTools(server: McpServer) {
  server.tool(
    "repo_map",
    "Generate a high-level map/summary of the hamclock-next repository structure.",
    {},
    { readOnlyHint: true },
    async () => {
      const index = await ensureIndexed();
      return { content: [{ type: "text", text: formatRepoMap(index) }] };
    }
  );

  server.tool(
    "find_files",
    "Find files in the hamclock-next repository matching a glob pattern.",
    {
      pattern: z.string().describe("Glob pattern (e.g. '*.cpp', 'src/ui/*')"),
    },
    { readOnlyHint: true },
    async ({ pattern }) => {
      const index = await ensureIndexed();
      const results = findFiles(index, pattern);
      return {
        content: [
          {
            type: "text",
            text: results.length
              ? results.map((f) => `- ${f.path} (${f.line_count} lines)`).join("\n")
              : "No files found matching pattern.",
          },
        ],
      };
    }
  );

  server.tool(
    "find_symbols",
    "Find symbols (functions, classes, structs) matching a regex pattern.",
    {
      pattern: z.string().describe("Regex pattern for symbol name"),
    },
    { readOnlyHint: true },
    async ({ pattern }) => {
      const index = await ensureIndexed();
      const results = findSymbols(index, pattern);
      return {
        content: [
          {
            type: "text",
            text: results.length
              ? results
                  .map(
                    (r) =>
                      `- \`${r.symbol.name}\` (${r.symbol.kind}) in \`${r.file}:${r.symbol.line}\`${
                        r.symbol.signature ? `\n  \`${r.symbol.signature}\`` : ""
                      }`
                  )
                  .join("\n")
              : "No symbols found matching pattern.",
          },
        ],
      };
    }
  );

  server.tool(
    "reindex_repo",
    "Force re-indexing of the hamclock-next repository.",
    {},
    { readOnlyHint: false, idempotentHint: true },
    async () => {
      await reindex();
      return { content: [{ type: "text", text: "Successfully re-indexed hamclock-next repository." }] };
    }
  );
}
