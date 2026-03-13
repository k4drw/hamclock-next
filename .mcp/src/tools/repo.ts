import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { ensureIndexed, reindex } from "../state.js";
import { findFiles, findSymbols } from "../indexer.js";
import { formatRepoMap } from "../helpers.js";

export function registerRepoTools(server: McpServer) {
  server.tool(
    "repo_map",
    "Generate a high-level map/summary of a repository structure.",
    {
      repo: z.enum(["original", "next"]).describe("Which repository to map"),
    },
    async ({ repo }) => {
      const { original, next } = await ensureIndexed();
      const index = repo === "original" ? original : next;
      return { content: [{ type: "text", text: formatRepoMap(index) }] };
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
      const { original, next } = await ensureIndexed();
      const index = repo === "original" ? original : next;
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
      repo: z.enum(["original", "next"]).describe("Repository to search"),
      pattern: z.string().describe("Regex pattern for symbol name"),
    },
    async ({ repo, pattern }) => {
      const { original, next } = await ensureIndexed();
      const index = repo === "original" ? original : next;
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
    "Force re-indexing of a repository.",
    {
      repo: z.enum(["original", "next"]).describe("Repository to re-index"),
    },
    async ({ repo }) => {
      await reindex();
      return { content: [{ type: "text", text: `Successfully re-indexed ${repo} repository.` }] };
    }
  );
}
