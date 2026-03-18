import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { existsSync } from "fs";
import { resolve } from "path";
import { ensureProjectContext, PROJECT_CONTEXT_PATH, loadParityData, NEXT_PATH } from "../state.js";

export function registerContributorTools(server: McpServer) {
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
        "pr_workflow",
        "open_issues",
      ]).optional().describe(
        "Specific section to retrieve. Omit to get the full project_context and source_layout summary."
      ),
    },
    { readOnlyHint: true },
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
          content: [{ type: "text", text: JSON.stringify(data, null, 2) }]
        };
      }

      // Default: return project_context + source_layout as a readable summary
      const lines: string[] = [];
      const pc = ctx["project_context"] as Record<string, any> | undefined;
      if (pc) {
        lines.push(`# HamClock Next — Project Context`);
        if (pc.origin) lines.push(`
## Origin
${pc.origin}`);
        if (pc.rewrite_rationale) lines.push(`
## Rewrite Rationale
${pc.rewrite_rationale}`);
        if (pc.philosophy) lines.push(`
## Philosophy
${pc.philosophy}`);
        if (pc.tech_stack) {
          lines.push(`
## Tech Stack`);
          for (const [k, v] of Object.entries(pc.tech_stack)) {
            lines.push(`- ${k}: ${v}`);
          }
        }
      }
      const sl = ctx["source_layout"] as Record<string, any> | undefined;
      if (sl?.naming_conventions) {
        lines.push(`
## Naming Conventions`);
        const nc = sl.naming_conventions as Record<string, string>;
        for (const [k, v] of Object.entries(nc)) {
          lines.push(`- ${k}: ${v}`);
        }
      }
      const fs = ctx["feature_status_summary"] as Record<string, any> | undefined;
      if (fs) {
        lines.push(`
## Feature Status`);
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
    { readOnlyHint: true },
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

  server.tool(
    "contributor_start",
    "Single-call onboarding for a ham + AI assistant. Returns everything needed to begin contributing: project origin, tech stack, build instructions, PR workflow, where to start, and the AI execution rules.",
    {},
    { readOnlyHint: true },
    async () => {
      const ctx = await ensureProjectContext();
      const pc = ctx["project_context"] as Record<string, any> | undefined;
      const prw = ctx["pr_workflow"] as Record<string, any> | undefined;

      const lines: string[] = [
        "# Contributing to HamClock-Next",
        "",
        "## What is HamClock-Next?",
        pc?.origin ?? "HamClock-Next is a full SDL2/C++20 rewrite of Elwood Downey (WB0OEW)'s HamClock — a feature-rich amateur radio dashboard for Linux, macOS, and Windows.",
        "",
        "## Tech Stack (what the AI will be writing)",
        "- C++20, SDL2, CMake",
        "- No external UI framework — all rendering is SDL2 primitives + SDL_ttf",
        "- Widgets inherit from Widget base class (src/ui/Widget.h)",
        "- Data providers in src/services/, display panels in src/ui/",
        "- Embedded REST API in src/network/WebServer.cpp",
        "- Config stored as JSON via ConfigManager",
        "",
        "## How to Build",
        "```bash",
        "cmake -S . -B build && cmake --build build --target hamclock-next -j$(nproc)",
        "./build/hamclock-next",
        "```",
        "",
        "## How to Submit a PR",
      ];

      if (prw) {
        lines.push(`- **Branch naming:** ${prw.fork_and_branch ?? "feature/<name> or fix/<name>"}`);
        lines.push(`- **Commit format:** ${prw.commit_format ?? "feat: one-line summary\n\n- bullet list"}`);
        if (Array.isArray(prw.pr_checklist)) {
          lines.push("- **PR Checklist:**");
          for (const item of prw.pr_checklist) {
            lines.push(`  - [ ] ${item}`);
          }
        }
      }

      lines.push(
        "",
        "## Where to Start",
        "Run: `open_issues`                      → see what needs doing",
        "Run: `parity_list` with status_filter=[\"PARTIAL\",\"MISSING\"]  → see parity gaps",
        "Run: `get_scaffolding_template SolarFlux` → get boilerplate for a new widget",
        "Run: `new_feature_checklist SolarFlux`    → registration checklist for a widget",
        "",
        "## Rules the AI Must Follow (from CLAUDE.md)",
      );

      if (prw?.execution_gate) {
        lines.push(`- **Execution Gate:** ${prw.execution_gate}`);
      }
      lines.push(
        "- No more than 5 files modified per task (Scope Lock)",
        "- State parity impact before changing any code",
        "- No refactoring unrelated systems",
        "- No autonomous initiative — implement only what was asked",
        "- After task completion: stop and wait for next instruction",
      );

      return { content: [{ type: "text", text: lines.join("\n") }] };
    }
  );

  server.tool(
    "open_issues",
    "Returns a curated, difficulty-ranked list of open work items for contributors. Use difficulty to filter.",
    {
      difficulty: z.enum(["easy", "medium", "hard", "all"]).optional().default("all")
        .describe("Filter by difficulty level. Default: all"),
    },
    { readOnlyHint: true },
    async ({ difficulty }) => {
      const ctx = await ensureProjectContext();
      const issues = ctx["open_issues"] as Record<string, any> | undefined;

      if (!issues) {
        return { isError: true, content: [{ type: "text", text: "open_issues section not found in hamclock-next-mcp.json" }] };
      }

      const lines: string[] = ["# Open Issues for HamClock-Next Contributors", ""];

      const renderGroup = (label: string, key: string) => {
        const items = issues[key] as Array<Record<string, any>> | undefined;
        if (!items?.length) return;
        lines.push(`## ${label}`);
        for (const item of items) {
          lines.push(`- [ ] **${item.title}**`);
          if (item.description) lines.push(`  ${item.description}`);
          if (item.files?.length) lines.push(`  Files: ${item.files.join(", ")}`);
          if (item.hint) lines.push(`  Hint: ${item.hint}`);
        }
        lines.push("");
      };

      if (difficulty === "all" || difficulty === "easy") {
        renderGroup("Easy (1–2 files, well-defined scope)", "easy");
      }
      if (difficulty === "all" || difficulty === "medium") {
        renderGroup("Medium (2–4 files, new functionality)", "medium");
      }
      if (difficulty === "all" || difficulty === "hard") {
        renderGroup("Hard (architectural or multi-system)", "hard");
      }

      // Also surface PARTIAL/MISSING from parity_v2
      if (difficulty === "all") {
        try {
          const parityData = await loadParityData();
          const gaps = parityData.features.filter((f: any) => f.status === "PARTIAL" || f.status === "MISSING");
          if (gaps.length) {
            lines.push("## Parity Gaps (from parity_v2.json)");
            for (const f of gaps) {
              lines.push(`- [ ] **${f.name}** [${f.status}] — ${f.notes}`);
            }
          }
        } catch {
          // parity data optional
        }
      }

      return { content: [{ type: "text", text: lines.join("\n") }] };
    }
  );

  server.tool(
    "new_feature_checklist",
    "Given a feature name and type, returns the exact registration checklist for adding it to hamclock-next. Prevents the AI from missing a registration step.",
    {
      name: z.string().describe("PascalCase feature name, e.g. 'SolarFlux' or 'AsteroidTracker'"),
      type: z.enum(["widget", "endpoint", "config_field"]).optional().default("widget")
        .describe("Type of feature to add. Default: widget"),
    },
    { readOnlyHint: true },
    async ({ name, type }) => {
      const lines: string[] = [];

      if (type === "widget") {
        lines.push(`# New Widget Checklist: ${name}`, "");
        lines.push("## Files to Create");
        lines.push(`1. \`src/ui/${name}Panel.h\`       — widget class declaration`);
        lines.push(`2. \`src/ui/${name}Panel.cpp\`     — widget render + update logic`);
        lines.push(`3. \`src/services/${name}Provider.h/.cpp\` — data fetch (if new data source needed)`, "");
        lines.push("## Files to Modify");
        lines.push(`4. \`src/core/WidgetType.h\`         — add ${name.toUpperCase()} to enum`);
        lines.push(`5. \`src/ui/WidgetSelector.cpp\`     — add entry to allTypes[] array`);
        lines.push(`6. \`CMakeLists.txt\`                — add ${name}Panel.cpp to SOURCES`, "");
        lines.push("## Registration Pattern");
        const minimalExample = existsSync(resolve(NEXT_PATH, "src/ui/ENVPanel.cpp"))
          ? "`src/ui/ENVPanel.cpp`"
          : "an existing minimal Panel in `src/ui/`";
        const providerExample = existsSync(resolve(NEXT_PATH, "src/ui/AsteroidPanel.cpp"))
          ? "`src/ui/AsteroidPanel.cpp`"
          : "an existing Panel + Provider pair in `src/ui/` and `src/services/`";
        lines.push(`- See: ${minimalExample} for a minimal single-file example`);
        lines.push(`- See: ${providerExample} for the data-provider pattern`, "");
        lines.push("## Testing");
        lines.push(`- Build: \`cmake --build build --target hamclock-next\``);
        lines.push(`- Open widget selector (gear icon), verify ${name} appears`);
        lines.push(`- Select it in a pane, verify it renders without crash`);
        lines.push(`- Run 5 minutes, verify data updates`);
        lines.push(`- Verify rendering uses ThemeColors tokens and strictly avoids hardcoded RGB literals.`, "");
      } else if (type === "endpoint") {
        lines.push(`# New REST Endpoint Checklist: ${name}`, "");
        lines.push("## File to Modify");
        lines.push("1. `src/network/WebServer.cpp` — add handler registration", "");
        lines.push("## Pattern");
        lines.push("```cpp");
        lines.push(`svr_.Get("/${name}", [this](const httplib::Request& req, httplib::Response& res) {`);
        lines.push(`    // read params: req.get_param_value("key")`);
        lines.push(`    // respond: res.set_content(json, "application/json");`);
        lines.push("});");
        lines.push("```", "");
        lines.push("## Documentation");
        lines.push("- Add endpoint to `API.md` (method, path, params, return)");
        lines.push("- Add to `hamclock-next-mcp.json` api_reference.endpoints array", "");
      } else if (type === "config_field") {
        lines.push(`# New Config Field Checklist: ${name}`, "");
        lines.push("## Files to Modify");
        lines.push("1. `src/core/ConfigManager.h` — add field to AppConfig struct");
        lines.push("2. `src/core/ConfigManager.cpp` — add to fromJson() and toJson()");
        lines.push("3. `src/ui/SetupScreen.cpp` — add UI control if user-facing", "");
        lines.push("## Pattern");
        lines.push("```cpp");
        lines.push(`// In AppConfig struct:`);
        lines.push(`std::string ${name.charAt(0).toLowerCase() + name.slice(1)} = "default_value";`);
        lines.push(`// In fromJson: cfg.${name.charAt(0).toLowerCase() + name.slice(1)} = j.value("${name}", cfg.${name.charAt(0).toLowerCase() + name.slice(1)});`);
        lines.push(`// In toJson:   j["${name}"] = cfg.${name.charAt(0).toLowerCase() + name.slice(1)};`);
        lines.push("```", "");
      }

      lines.push("## CLAUDE.md Execution Gate");
      lines.push("State before making any changes:");
      lines.push("1. Task: [one sentence]");
      lines.push("2. Exact files: [list]");
      lines.push("3. No other files touched: [confirm]");
      lines.push("4. Parity impact: [yes/no, explain]");
      lines.push("5. Risk: Low / Medium / High");

      return { content: [{ type: "text", text: lines.join("\n") }] };
    }
  );
}
