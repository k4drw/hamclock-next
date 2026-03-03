import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import { readFile } from "fs/promises";
import { RepoIndex, ParityData } from "./types.js";
import { indexRepo } from "./indexer.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
export const MCP_ROOT = resolve(__dirname, "..");

export const ORIGINAL_PATH =
  process.env.HAMCLOCK_ORIGINAL_PATH ?? resolve(MCP_ROOT, "..", "hamclock-original");
export const NEXT_PATH =
  process.env.HAMCLOCK_NEXT_PATH ?? resolve(MCP_ROOT, "..", "hamclock-next");
export const PROJECT_CONTEXT_PATH =
  process.env.PROJECT_CONTEXT_PATH ?? resolve(MCP_ROOT, "hamclock-next-mcp.json");
export const DOCS_PATH = process.env.HAMCLOCK_DOCS_PATH ?? NEXT_PATH;
export const DATA_PATH = resolve(MCP_ROOT, 'data');
export const PARITY_JSON_PATH = resolve(DATA_PATH, 'parity_v2.json');

let originalIndex: RepoIndex | null = null;
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

export async function ensureIndexed(): Promise<{ original: RepoIndex; next: RepoIndex }> {
  if (!originalIndex) {
    originalIndex = await indexRepo(ORIGINAL_PATH, "hamclock-original");
  }
  if (!nextIndex) {
    nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
  }
  return { original: originalIndex, next: nextIndex };
}

export async function reindex(): Promise<{ original: RepoIndex; next: RepoIndex }> {
  originalIndex = await indexRepo(ORIGINAL_PATH, "hamclock-original");
  nextIndex = await indexRepo(NEXT_PATH, "hamclock-next");
  return { original: originalIndex, next: nextIndex };
}

export async function loadParityData(): Promise<ParityData> {
  try {
    const fileContent = await readFile(PARITY_JSON_PATH, 'utf-8');
    return JSON.parse(fileContent);
  } catch (e) {
    throw new Error('Parity data not found. Please run `parity_load_report` first.');
  }
}
