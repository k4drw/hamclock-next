import { RepoIndex, FileIndex } from "./types.js";

export function formatRepoMap(index: RepoIndex): string {
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
    lines.push(`
### ${dir}/ (${files.length} files, ${totalLines.toLocaleString()} lines, ${totalSyms} symbols)`);
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
  lines.push(`
## Key Classes & Structs`);
  const classStructs = index.files
    .flatMap((f) =>
      f.symbols
        .filter((s) => s.kind === "class" || s.kind === "struct")
        .map((s) => ({ file: f.path, ...s }))
    )
    .sort((a, b) => a.name.localeCompare(b.name));

  for (const s of classStructs) {
    lines.push(`- ${s.kind} `${s.name}` in `${s.file}:${s.line}``);
  }

  return lines.join("
");
}
