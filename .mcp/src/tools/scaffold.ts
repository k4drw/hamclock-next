import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { scaffoldFeature } from "../scaffold.js";
import { NEXT_PATH } from "../state.js";

export function registerScaffoldTools(server: McpServer) {
  server.tool(
    "scaffold_feature",
    "Generate and write boilerplate C++ files for a new feature (Widget or Service) into the hamclock-next source tree.",
    {
      name: z.string().describe("Name of the feature (e.g. 'SolarPanel')"),
      type: z.enum(["Widget", "Service"]).describe("Type of component to scaffold"),
    },
    { readOnlyHint: false, destructiveHint: false },
    async ({ name, type }) => {
      try {
        const result = await scaffoldFeature(NEXT_PATH, name, type);
        const text = result.instructions + "\n\nFiles created:\n" + result.files.join("\n");
        return { content: [{ type: "text", text }] };
      } catch (e: any) {
        return { isError: true, content: [{ type: "text", text: `Failed to scaffold ${type} '${name}': ${e.message}` }] };
      }
    }
  );
}
