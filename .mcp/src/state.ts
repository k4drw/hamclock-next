import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import { readFile } from "fs/promises";
import { RepoIndex } from "./types.js";
import { indexRepo } from "./indexer.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
export const MCP_ROOT = resolve(__dirname, "..");

export const NEXT_PATH =
  process.env.HAMCLOCK_NEXT_PATH ?? resolve(MCP_ROOT, "..", "hamclock-next");
export const PROJECT_CONTEXT_PATH =
  process.env.PROJECT_CONTEXT_PATH ?? resolve(MCP_ROOT, "hamclock-next-mcp.json");
export const DOCS_PATH = process.env.HAMCLOCK_DOCS_PATH ?? NEXT_PATH;
let nextIndex: RepoIndex | null = null;
let projectContext: Record<string, any> | null = null;

export async function ensureProjectContext(): Promise<Record<string, any>> {
  if (!projectContext) {
    try {
      const raw = await readFile(PROJECT_CONTEXT_PATH, "utf-8");
      projectContext = JSON.parse(raw);
    } catch {
      projectContext = {};
    }
  }
  return projectContext!;
}

export async function ensureIndexed(): Promise<RepoIndex> {
  if (!nextIndex) {
    nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
  }
  return nextIndex;
}

export async function reindex(): Promise<RepoIndex> {
  nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
  return nextIndex;
}
