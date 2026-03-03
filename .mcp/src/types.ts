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

export interface ParityFeature {
  feature_id: string;
  name: string;
  status: 'EQUIVALENT' | 'EQUIVALENT+' | 'PARTIAL' | 'STUB' | 'MISSING';
  source_strategy: 'proxy-dependent -> direct-source' | 'unknown -> direct-source' | 'mixed -> unknown' | 'mixed -> direct-source' | 'mixed -> mixed' | 'N/A';
  original_pointers: string[];
  next_pointers: string[];
  notes: string;
  suggested_gaps: string[];
}

export interface ParityData {
  features: ParityFeature[];
  top_improvements: string[];
  top_gaps: string[];
  parity_score: number;
}

export interface VerificationResult {
  runtime_verification: 'disabled' | 'enabled';
  reachable?: boolean;
  widgets_found?: string[];
  screenshots?: string[];
  violations_report?: string[];
  memory_stress_test?: MemoryStressTestResult;
}

export interface MemoryStressTestResult {
  test_duration_seconds: number;
  initial_fps: number;
  final_fps: number;
  fps_stability: 'stable' | 'degraded' | 'critical';
  performance_samples: number;
  issues_detected: string[];
  recommendations: string[];
}
