import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";

export function registerScaffoldTools(server: McpServer) {
  server.tool(
    "scaffold_feature",
    "Generate boilerplate code for a new feature (Widget or Service).",
    {
      name: z.string().describe("Name of the feature (e.g. 'SolarPanel')"),
      type: z.enum(["Widget", "Service"]).describe("Type of component to scaffold"),
    },
    async ({ name, type }) => {
      const lines: string[] = [`# Scaffolding ${type}: ${name}`, ""];

      if (type === "Widget") {
        lines.push(`## src/ui/${name}Panel.h`);
        lines.push("```cpp");
        lines.push("#pragma once");
        lines.push('#include "Widget.h"');
        lines.push(`class ${name}Panel : public Widget { /* ... */ };`);
        lines.push("```");
      } else {
        lines.push(`## src/services/${name}Provider.h`);
        lines.push("```cpp");
        lines.push("#pragma once");
        lines.push(`class ${name}Provider { /* ... */ };`);
        lines.push("```");
      }

      return { content: [{ type: "text", text: lines.join("\n") }] };
    }
  );
}
