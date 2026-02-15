# HamClock Bridge: MCP Integration Guide

The HamClock project utilizes the **Model Context Protocol (MCP)** to provide AI assistants (like Claude and Gemini) with deep, real-time knowledge of the codebase, feature parity status, and implementation paths.

This guide explains how to set up and use the HamClock MCP server to accelerate development and contributions.

## 🚀 Overview

The **HamClock Bridge** MCP server connects the two codebases:
1.  **`hamclock-original`**: The reference implementation (legacy C/C++).
2.  **`hamclock-next`**: The modern, SDL2/C++20 reimplementation.

It allows an AI assistant to:
-   **Analyze Gaps**: Identify missing or partial features compared to the original.
-   **Trace Logic**: Provide direct pointers to the original's logic and the corresponding `next` implementation.
-   **Generate Tickets**: Create ready-to-use implementation plans with technical context.
-   **Scaffold Widgets**: Generate C++ boilerplate for new panels and providers automatically.

---

## 🛠️ Setup (For Contributors)

To use the MCP server in your local development environment with an MCP-capable AI assistant:

### 1. Build the MCP Server
Ensure you have Node.js installed, then:
```bash
cd .mcp
npm install
npm run build
```

### 2. Configure Your Assistant
Add the server to your assistant's configuration (e.g., `claude_desktop_config.json` or equivalent):

**Example Config:**
```json
{
  "mcpServers": {
    "hamclock-bridge": {
      "command": "node",
      "args": ["/absolute/path/to/hamclock-next/.mcp/dist/index.js"]
    }
  }
}
```

---

## 🏗️ Key Tools & Features

Once active, the following tools become available to your AI assistant:

### 📊 Parity & Progress
- **`parity_summary`**: View the overall progress of the `next` codebase vs `original`.
- **`parity_list`**: Search for features by status (`MISSING`, `PARTIAL`, `IMPLEMENTED`).
- **`parity_feature`**: Get JSON metadata for a specific feature, including technical debt notes.
- **`parity_verify_feature`**: Ping a running hamclock-next instance to see if a feature's API is live.

### 🔍 Codebase Intelligence
- **`project_context`**: Get project context and architectural knowledge. Available sections:
  - `project_context` - Origin story, tech stack, philosophy
  - `source_layout` - Directory structure, naming conventions
  - `original_vs_next` - Key architectural differences
  - `widget_scaffolding` - How to add new widgets
  - `api_reference` - WebServer endpoint summary
  - `contribution_guide` - Code style, AI assistant tips
  - `feature_status_summary` - Progress overview
  - **`decisions`** ⭐ - Architectural decision records with rationale (9 entries covering SDL2, libpredict, libcurl, layout system, etc.)
  - **`gotchas`** ⭐ - Debugging knowledge: non-obvious behaviors, footguns, initialization order issues (10 entries with severity ratings)
  - **`api_examples`** ⭐ - Working curl commands for all 22 WebServer endpoints organized by category
- **`repo_map <original|next>`**: Generates a high-level summary of the entire repository structure.
- **`find_symbols`**: Regex search for functions/classes. Essential for finding *where* a feature was implemented in the original.
- **`find_files`**: Glob-based file discovery.

### 📚 Knowledge Resources
These are also available as standalone MCP resources (use `ReadMcpResourceTool`):
- **`hamclock://project/decisions`**: Why key architectural choices were made (alternatives considered, tradeoffs, files affected)
- **`hamclock://project/gotchas`**: Hard-won debugging knowledge (symptom, cause, fix for each gotcha)
- **`hamclock://project/api-examples`**: Copy-paste curl examples for every WebServer endpoint (core, input injection, config, debug API)

### 📝 Task Management
- **`parity_create_ticket`**: Generates a detailed implementation plan for a single feature.
- **`parity_create_batch_tickets`**: Generates task lists for an entire category or priority block.

### ⚡ Automation & Scaffolding
- **`get_scaffolding_template`**: **The most powerful tool for contributors.** Give it a PascalCase name (e.g., `SolarPanel`), and it returns complete, ready-to-save C++ boilerplate for:
    - [Name]Data.h (Model)
    - [Name]Provider.cpp/h (Service/Network)
    - [Name]Panel.cpp/h (UI/Widget)

---

## 💡 Example Questions to Ask

With the new `decisions`, `gotchas`, and `api_examples` sections, you can ask:

**Architectural Understanding:**
- *"Why did we use libpredict instead of porting P13.cpp?"*
- *"What were the tradeoffs of choosing SDL2 over custom framebuffer code?"*
- *"Show me all decisions related to networking"*

**Debugging Help:**
- *"What are the high-severity gotchas I should know about?"*
- *"Why might SDL rendering crash with 'OpenGL context not current' errors?"*
- *"Are there any initialization order issues with SDL_ttf?"*

**API Testing:**
- *"Give me a curl command to test the DX location API"*
- *"How do I inject a mouse click via the WebServer?"*
- *"Show me all debug endpoints and how to enable them"*

---

## 🔄 Recommended Workflow for Contributors

1.  **Orient**: Ask: *"Give me the project context and source layout overview."*
2.  **Understand Decisions**: Ask: *"Show me architectural decisions for the layout system"* or *"Why did we choose SDL2 over Qt?"*
3.  **Check for Pitfalls**: Ask: *"Are there any gotchas related to SDL rendering I should know about?"*
4.  **Identify a Gap**: Ask: *"Show me the status of features in the 'data_panel' category."*
5.  **Plan**: Ask: *"Generate an implementation ticket for `kp_index`."*
6.  **Boilerplate**: Ask: *"Scaffold the boilerplate for a KpIndex widget."*
7.  **Refine**: Use the code pointers from the ticket to copy-paste or port logic from `hamclock-original`.
8.  **Test**: Ask: *"Show me curl examples for testing the WebServer API"* - all endpoints have working examples in `api_examples`.

---

## 📈 Current Status
The server maintains a dynamic mapping in `.mcp/feature_map.json`. This file is updated as features are ported, ensuring the AI always has the most accurate view of what remains to be done.
