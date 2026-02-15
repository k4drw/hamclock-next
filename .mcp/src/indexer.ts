/**
 * Repository indexer - walks file trees and extracts C/C++ symbols.
 */
import { readdir, readFile, stat } from "fs/promises";
import { join, relative, extname } from "path";

export interface SymbolEntry {
  name: string;
  kind: "function" | "class" | "struct" | "enum" | "define" | "method";
  line: number;
  signature?: string;
}

export interface FileIndex {
  path: string; // relative to repo root
  size: number;
  line_count: number;
  symbols: SymbolEntry[];
}

export interface RepoIndex {
  root: string;
  label: string;
  indexed_at: string;
  files: FileIndex[];
  stats: {
    total_files: number;
    cpp_files: number;
    header_files: number;
    total_lines: number;
    total_symbols: number;
  };
}

const CPP_EXTENSIONS = new Set([
  ".cpp",
  ".c",
  ".h",
  ".hpp",
  ".cxx",
  ".cc",
  ".hxx",
]);
const SKIP_DIRS = new Set([
  ".git",
  ".github",
  ".agent",
  "node_modules",
  "build",
  "cmake-build-debug",
  "cmake-build-release",
  ".cache",
]);

// Skip giant generated files (font data, image data, zones, etc.)
const SKIP_FILES = new Set([
  "Germano-Bold-16.cpp",
  "Germano-Bold-30.cpp",
  "Germano-Regular-16.cpp",
  "moon_imgs.cpp",
  "zones.cpp",
  "favicon.cpp",
  "prefixes.cpp",
  "liveweb-html.cpp",
  "stb_image_write.h",
]);

async function walkDir(dir: string, root: string): Promise<string[]> {
  const results: string[] = [];
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch {
    return results;
  }
  for (const entry of entries) {
    if (entry.name.startsWith(".") && SKIP_DIRS.has(entry.name)) continue;
    if (SKIP_DIRS.has(entry.name)) continue;
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      results.push(...(await walkDir(fullPath, root)));
    } else {
      results.push(relative(root, fullPath));
    }
  }
  return results;
}

export function extractSymbols(content: string, filePath: string): SymbolEntry[] {
  const symbols: SymbolEntry[] = [];
  const lines = content.split("\n");
  const isHeader = filePath.endsWith(".h") || filePath.endsWith(".hpp");

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const trimmed = line.trim();

    // Skip comments and empty lines
    if (
      trimmed.startsWith("//") ||
      trimmed.startsWith("/*") ||
      trimmed.startsWith("*") ||
      trimmed === ""
    )
      continue;

    // #define (skip include guards and trivial ones)
    let match = trimmed.match(/^#define\s+(\w{3,})(?:\s+(.+))?$/);
    if (match && !match[1].endsWith("_H") && !match[1].endsWith("_HPP")) {
      symbols.push({ name: match[1], kind: "define", line: i + 1 });
      continue;
    }

    // enum (class)? Name
    match = trimmed.match(/^enum\s+(?:class\s+)?(\w+)/);
    if (match) {
      symbols.push({ name: match[1], kind: "enum", line: i + 1 });
      continue;
    }

    // class Name (but not forward declarations)
    match = trimmed.match(/^(?:template\s*<[^>]*>\s*)?class\s+(\w+)(?:\s*:\s*|[^;]*\{)/);
    if (match) {
      symbols.push({ name: match[1], kind: "class", line: i + 1 });
      continue;
    }

    // struct Name (but not forward declarations)
    match = trimmed.match(/^(?:typedef\s+)?struct\s+(\w+)(?:\s*\{|\s*:)/);
    if (match) {
      symbols.push({ name: match[1], kind: "struct", line: i + 1 });
      continue;
    }

    // Function/method definitions: look for lines with ( that end with { or have { on next line
    // Skip declarations (lines ending with ;)
    if (trimmed.endsWith(";") || trimmed.startsWith("#") || trimmed.startsWith("typedef"))
      continue;

    // Match: [qualifiers] return_type name(params) [const] [override] {
    match = trimmed.match(
      /^(?:static\s+|virtual\s+|inline\s+|explicit\s+|constexpr\s+|extern\s+)*(?:[\w:*&<>,\s]+?\s+)?(\w+)\s*\(([^)]*)\)\s*(?:const\s*)?(?:override\s*)?(?:noexcept\s*)?(?:\{|$)/
    );
    if (match) {
      const name = match[1];
      // Filter out control flow keywords and common false positives
      if (
        [
          "if",
          "for",
          "while",
          "switch",
          "catch",
          "return",
          "sizeof",
          "delete",
          "throw",
        ].includes(name)
      )
        continue;

      // Check if this is a definition (has { on this line or next)
      const hasOpenBrace =
        trimmed.includes("{") ||
        (i + 1 < lines.length && lines[i + 1].trim().startsWith("{"));

      // In headers, also capture declarations (for API mapping)
      if (hasOpenBrace || isHeader) {
        const sig = trimmed.replace(/\s*\{.*$/, "").trim();
        symbols.push({
          name,
          kind: hasOpenBrace ? "function" : "method",
          line: i + 1,
          signature: sig.length < 200 ? sig : undefined,
        });
      }
    }
  }

  return symbols;
}

export async function indexRepo(
  repoPath: string,
  label: string
): Promise<RepoIndex> {
  const allFiles = await walkDir(repoPath, repoPath);
  const files: FileIndex[] = [];
  let totalLines = 0;
  let totalSymbols = 0;
  let cppFiles = 0;
  let headerFiles = 0;

  for (const relPath of allFiles.sort()) {
    const ext = extname(relPath).toLowerCase();
    const basename = relPath.split("/").pop() || "";

    if (!CPP_EXTENSIONS.has(ext)) continue;
    if (SKIP_FILES.has(basename)) continue;

    const fullPath = join(repoPath, relPath);
    let content: string;
    let fileSize: number;

    try {
      const st = await stat(fullPath);
      fileSize = st.size;
      // Skip files > 500KB (generated data files)
      if (fileSize > 512_000) continue;
      content = await readFile(fullPath, "utf-8");
    } catch {
      continue;
    }

    const lineCount = content.split("\n").length;
    const symbols = extractSymbols(content, relPath);

    if (ext === ".h" || ext === ".hpp" || ext === ".hxx") headerFiles++;
    else cppFiles++;

    totalLines += lineCount;
    totalSymbols += symbols.length;

    files.push({
      path: relPath,
      size: fileSize,
      line_count: lineCount,
      symbols,
    });
  }

  return {
    root: repoPath,
    label,
    indexed_at: new Date().toISOString(),
    files,
    stats: {
      total_files: files.length,
      cpp_files: cppFiles,
      header_files: headerFiles,
      total_lines: totalLines,
      total_symbols: totalSymbols,
    },
  };
}

/** Find files in the index matching a pattern (glob-like, case-insensitive). */
export function findFiles(index: RepoIndex, pattern: string): FileIndex[] {
  const re = new RegExp(pattern.replace(/\*/g, ".*"), "i");
  return index.files.filter((f) => re.test(f.path));
}

/** Find symbols across all files matching a name pattern. */
export function findSymbols(
  index: RepoIndex,
  pattern: string
): Array<{ file: string; symbol: SymbolEntry }> {
  const re = new RegExp(pattern, "i");
  const results: Array<{ file: string; symbol: SymbolEntry }> = [];
  for (const file of index.files) {
    for (const sym of file.symbols) {
      if (re.test(sym.name)) {
        results.push({ file: file.path, symbol: sym });
      }
    }
  }
  return results;
}
