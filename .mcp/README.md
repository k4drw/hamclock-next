# MCP Server - HamClock Feature Parity Tools

This document describes the CLI tools for managing and tracking the feature parity between `hamclock-original` and `hamclock-next`.

## Overview

The MCP server provides a suite of tools to help contributors close the feature parity gap. It allows you to:

- Load a feature parity report in Markdown format.
- Query the parity status of features.
- Generate implementation tickets for missing or partial features.
- Optionally verify features against a running `hamclock-next` instance.

## CLI Usage

### `parity:load <path>`

Load and parse the feature parity report. This command reads a Markdown file (like `feature_parity_report_v2.md`), parses the feature table, and saves the data to `./mcp/data/parity_v2.json`.

**Usage:**
```bash
./mcp/src/index.ts parity:load ../feature_parity_report_v2.md
```

### `parity:summary`

Show a summary of the feature parity, including the overall parity score, a breakdown of features by status, and lists of top improvements and gaps.

**Usage:**
```bash
./mcp/src/index.ts parity:summary
```

### `parity:list`

List all features, with options to filter by status or search by name.

**Usage:**
```bash
# List all features
./mcp/src/index.ts parity:list

# List all PARTIAL features
./mcp/src/index.ts parity:list --status PARTIAL

# Search for features related to "satellite"
./mcp/src/index.ts parity:list -q satellite
```

### `parity:ticket <feature_id>`

Generate a detailed implementation ticket for a specific feature, formatted for an AI assistant like Claude or Gemini.

**Usage:**
```bash
./mcp/src/index.ts parity:ticket satellite_tracker
```

### `parity:batch-tickets`

Generate implementation tickets for all features that are currently `PARTIAL`, `STUB`, or `MISSING`.

**Usage:**
```bash
./mcp/src/index.ts parity:batch-tickets
```

### `parity:verify <feature_id>`

(Optional) Verify a feature against a running `hamclock-next` instance.

**Usage:**
```bash
# Verify the 'dx_cluster' feature
MCP_VERIFY_BASE_URL=http://localhost:8080 ./mcp/src/index.ts parity:verify dx_cluster

# Or using the --base-url flag
./mcp/src/index.ts parity:verify dx_cluster --base-url http://localhost:8080
```

## Runtime Verification

To enable runtime verification, you need a `hamclock-next` instance running with the debug endpoints enabled. You can then set the `MCP_VERIFY_BASE_URL` environment variable or use the `--base-url` flag with the `parity:verify` command.

## JSON Schema (`parity_v2.json`)

The `parity_v2.json` file is stored in `./mcp/data/` and has the following structure:

```json
{
  "features": [
    {
      "feature_id": "string",
      "name": "string",
      "status": "EQUIVALENT" | "EQUIVALENT+" | "PARTIAL" | "STUB" | "MISSING",
      "source_strategy": "string",
      "original_pointers": ["string"],
      "next_pointers": ["string"],
      "notes": "string",
      "suggested_gaps": ["string"]
    }
  ],
  "top_improvements": ["string"],
  "top_gaps": ["string"],
  "parity_score": "number"
}
```

## Example Ticket

Here is an example of a ticket generated for the `satellite_tracker` feature:

```markdown
# Implementation Ticket: Satellite Tracker

**Feature ID:** `satellite_tracker`
**Current Status:** PARTIAL

## 1. Goal
Implement the **Satellite Tracker** feature in `hamclock-next` to achieve functional parity with the original `hamclock-original`.

**Why:** `next` has the core components, but it's unclear if all the functionality of the original's `sattool` is present. User can't perform all the same actions yet.

**Specific Gaps to Address:**
- **Satellite Tracker:**
    - **Missing Behavior:** The original `sattool` provided a rich interface for selecting satellites, viewing passes, and managing tracking. The current `SatPanel` in `next` is more basic and may not offer the same level of control or information.
    - **Gap:** Need to implement the full satellite selection and pass prediction UI.

## 2. Acceptance Criteria (User-Visible Behavior)
- The feature should be accessible through the HamClock-Next UI.
- It must provide the same core information and allow the same user interactions as the original.
- The visual output should be clean, modern, and consistent with the `hamclock-next` design system. It does not need to be a pixel-perfect copy of the original.

## 3. Implementation Guidance

### Reference Implementation (`hamclock-original`)
Study these files to understand the original behavior:
- `earthsat.cpp`
- `sattool.cpp`

### Likely Files to Modify (`hamclock-next`)
This feature may already have a partial implementation. Start by examining:
- `core/SatelliteManager.cpp`
- `ui/SatPanel.cpp`

## 4. Minimal Test Checklist
- [ ] **Build:** The code compiles successfully without warnings.
- [ ] **Run:** `hamclock-next` starts without crashing.
- [ ] **Visual Check:** The widget/panel appears in the UI and displays data correctly.
- [ ] **Interaction Check:** User can interact with the feature as expected (e.g., clicks, hovers).

## 5. Regression Guards & Constraints
- **DO NOT** introduce breaking changes to the 800x480 logical resolution.
- **DO NOT** add new fonts. Use the existing font catalog.
- **ADHERE** to the existing provider architecture for data fetching.
- **MAINTAIN** consistency with the existing UI/UX patterns.
```
