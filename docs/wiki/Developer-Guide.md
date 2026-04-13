# Developer Guide
HamClock-Next is built with C++20, SDL2, and a multi-threaded architecture using a clean separation between data retrieval (Providers) and UI (Widgets). It is continuously enhanced using AI-assisted development paradigms.

## AI Assistant Integration (MCP)
HamClock-Next actively uses the **Model Context Protocol (MCP)** to accelerate development. The `hamclock-bridge` MCP server provides your AI assistant (e.g., Claude Code or Gemini) with tools to understand architecture decisions, scaffold boilerplate code, and trace logic against the original C implementation.

### Recommended Workflow
If using Claude Code, it automatically loads the local `.mcp` server. Your assistant can read the original legacy codebase, identify missing paths, and write the new classes natively.

**Commands to tell your AI:**
- *"Give me the project context and source layout overview."*
- *"Show me the status of features in the 'data_panel' category."*
- *"Scaffold the boilerplate for a KpIndex widget."*

For complete setup instructions, see the `MCP_GUIDE.md` file in the root directory.

## Adding a New Widget

To add a new data panel to HamClock-Next, you typically need to build three components:
1.  **Data Structure**: A struct in `src/common/` holding the fetched variables.
2.  **Provider**: A background service class (in `src/providers/`) that fetches the data from the internet via HTTP or serial/socket and updates the struct.
3.  **Widget**: A UI class (in `src/ui/widgets/`) that registers with the layout engine and draws the data over the pre-allocated panel space.

### Building & Running

Refer to the [Building from Source](Building-from-Source.md) page for detailed library requirements.

Use standard CMake commands to build quickly during development:
```bash
cmake -S . -B build
cmake --build build -j10
./build/hamclock-next
```
