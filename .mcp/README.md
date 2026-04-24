# MCP Server - HamClock-Next

This document describes the tools provided by the HamClock-Next MCP server.

## Overview

The MCP server provides tools for contributors and AI agents working on `hamclock-next`. Feature parity with the original HamClock is 100% complete as of 2026-03-30, and the project is now in active feature development beyond the original.

### Available Tool Categories

- **Repo tools** (`repo_map`, `find_files`, `find_symbols`, `reindex_repo`) — Navigate and search the hamclock-next source tree.
- **Contributor tools** (`project_context`, `contributor_start`, `open_issues`, `get_scaffolding_template`, `new_feature_checklist`) — Onboarding, C++ boilerplate generation, and contributor checklists.
- **Propagation tools** (`voacap_overlay_schema`, `get_voacap_overlay`) — VOACAP request/response schema and overlay parameter builder.
- **Diagnostic tools** (`diagnose_memory`, `analyze_texture_cache`, `memory_stress_test`) — Runtime memory and FPS diagnostics for Raspberry Pi (KMSDRM) deployments.
- **Scaffold tools** (`scaffold_feature`) — Generate Widget or Service C++ boilerplate and write to disk.

### Resources

- `hamclock://project/architecture` — Project context, source layout, and original-vs-next comparison.
- `hamclock://project/features` — Full feature status list (all 88+ features).
- `hamclock://project/contribution-guide` — Widget scaffolding templates and API reference.
- `hamclock://project/decisions` — Decision log.
- `hamclock://project/gotchas` — Known gotchas.
- `hamclock://project/api-examples` — REST API usage examples.

## Memory Diagnostics (Phase 37)

The MCP server includes specialized tools for diagnosing memory-related issues, particularly for Raspberry Pi (KMSDRM backend) deployments. These tools were developed during Phase 37 to address "Cannot allocate memory" errors and texture allocation issues.

### `diagnose_memory`

Query memory diagnostics from a running hamclock-next instance. Checks FPS, texture allocation health, and memory-related errors by polling debug endpoints.

**What it checks:**
- `/debug/performance` - FPS and frame time stability
- `/debug/health` - Service health status
- `/debug/widgets` - Active widget count and complexity

**Usage:**
```bash
# Diagnose memory on local instance
./mcp/src/index.ts diagnose_memory

# Diagnose memory on remote instance
./mcp/src/index.ts diagnose_memory --base-url http://192.168.1.100:8080
```

**Output includes:**
- Health status (healthy/degraded/warning)
- Current FPS and performance metrics
- Active widget count
- Detected issues and recommendations

**When to use:**
- After deploying to Raspberry Pi
- When experiencing performance degradation
- To verify Phase 37 optimizations are effective

### `analyze_texture_cache`

Analyze texture allocation patterns from Phase 37 memory optimizations. Reviews `MEMORY_FIX_SUMMARY.md` and verifies implementation of caching mechanisms.

**What it checks:**
- Tooltip texture caching implementation
- NULL safety checks in texture operations
- KMSDRM low-memory mode activation
- Dynamic mesh density configuration

**Usage:**
```bash
# Full analysis
./mcp/src/index.ts analyze_texture_cache

# Check for leaks only
./mcp/src/index.ts analyze_texture_cache --check-leaks --no-check-churn

# Check for churn only
./mcp/src/index.ts analyze_texture_cache --no-check-leaks --check-churn
```

**Output includes:**
- Phase 37 optimization summary (before/after metrics)
- Leak detection (texture caching verification)
- Churn analysis (per-frame allocation elimination)
- Implementation verification (code checks)
- KMSDRM backend detection status

**When to use:**
- To verify Phase 37 optimizations are properly implemented
- Before/after performance comparisons
- When investigating texture-related memory issues

### `memory_stress_test`

Run a memory stress test against a running hamclock-next instance. Monitors FPS stability over time (default 30 seconds) to detect memory-related performance degradation.

**Test methodology:**
1. Samples FPS at 2-second intervals
2. Compares initial vs final FPS
3. Detects degradation patterns
4. Classifies stability: stable/degraded/critical

**Usage:**
```bash
# Default 30-second test
./mcp/src/index.ts memory_stress_test

# Custom duration (60 seconds)
./mcp/src/index.ts memory_stress_test --duration-seconds 60

# Test remote instance
./mcp/src/index.ts memory_stress_test --base-url http://192.168.1.100:8080 --duration-seconds 120
```

**Stability classifications:**
- **Stable**: FPS remains above 50, minimal variance
- **Degraded**: FPS drops >20% or average <50
- **Critical**: FPS drops below 30

**Output includes:**
- Initial and final FPS measurements
- FPS change over test duration
- Stability classification
- Issues detected (e.g., high FPS variance)
- Recommendations for remediation

**When to use:**
- After implementing Phase 37 optimizations
- To validate tooltip texture caching effectiveness
- When investigating intermittent performance issues
- During continuous operation testing on RPi

**Recommended test scenarios:**
1. **Tooltip stress**: Move cursor over map continuously during test
2. **Widget churn**: Cycle through multiple active widgets
3. **Long-duration**: Run 5+ minute test to detect memory leaks

### Phase 37 Optimization Context

The memory diagnostic tools support the following Phase 37 optimizations:

1. **Tooltip Texture Caching**
   - Eliminated 99% of texture allocations (180 → 1-2 per session)
   - Cached texture in `MapWidget::Tooltip` struct
   - Only regenerates when text changes

2. **NULL Safety**
   - Comprehensive NULL checks after all `SDL_CreateTexture` calls
   - Enhanced error logging with `SDL_GetError()` diagnostics
   - Graceful degradation on allocation failure

3. **Low-Memory Mode (KMSDRM)**
   - Automatic detection of KMSDRM backend
   - 75% mesh density reduction (96×48 → 48×24 grids)
   - Reduced vertex buffer sizes without visual quality loss

4. **Enhanced Diagnostics**
   - SDL error context in all texture operations
   - Surface dimension logging in FontManager
   - Memory pressure indicators in logs

**Related documentation:**
- `MEMORY_FIX_SUMMARY.md` - Detailed before/after analysis
- `working_docs/PROJECT_STATUS.md` - Phase 37 summary
- `MCP_GUIDE.md` - MCP tools and features guide

