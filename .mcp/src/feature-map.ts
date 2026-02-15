// Already implemented in previous steps.
import { readFile, writeFile } from "fs/promises";

export interface CodePointer {
  files: string[];
  symbols: string[];
  notes?: string;
}

export interface Feature {
  feature_id: string;
  name: string;
  category: string;
  description: string;
  original: CodePointer;
  next: CodePointer;
  status: "implemented" | "partial" | "missing" | "not_needed";
  acceptance: string[];
  notes?: string;
}

export interface FeatureMap {
  version: string;
  updated_at: string;
  features: Feature[];
}

export async function loadFeatureMap(path: string): Promise<FeatureMap> {
  try {
    const raw = await readFile(path, "utf-8");
    return JSON.parse(raw) as FeatureMap;
  } catch {
    return { version: "1.0", updated_at: new Date().toISOString(), features: [] };
  }
}

export async function saveFeatureMap(
  path: string,
  map: FeatureMap
): Promise<void> {
  map.updated_at = new Date().toISOString();
  await writeFile(path, JSON.stringify(map, null, 2) + "\n", "utf-8");
}

export function getFeature(map: FeatureMap, id: string): Feature | undefined {
  return map.features.find((f) => f.feature_id === id);
}

export function listByCategory(map: FeatureMap, category?: string): Feature[] {
  if (!category) return map.features;
  return map.features.filter((f) => f.category === category);
}

export function listByStatus(
  map: FeatureMap,
  status: Feature["status"]
): Feature[] {
  return map.features.filter((f) => f.status === status);
}

export function getCategories(map: FeatureMap): string[] {
  return [...new Set(map.features.map((f) => f.category))].sort();
}

export function getSummary(map: FeatureMap): {
  total: number;
  implemented: number;
  partial: number;
  missing: number;
  not_needed: number;
  by_category: Record<string, { total: number; implemented: number; partial: number; missing: number }>;
} {
  const summary = {
    total: map.features.length,
    implemented: 0,
    partial: 0,
    missing: 0,
    not_needed: 0,
    by_category: {} as Record<
      string,
      { total: number; implemented: number; partial: number; missing: number }
    >,
  };

  for (const f of map.features) {
    summary[f.status]++;
    if (!summary.by_category[f.category]) {
      summary.by_category[f.category] = {
        total: 0,
        implemented: 0,
        partial: 0,
        missing: 0,
      };
    }
    const cat = summary.by_category[f.category];
    cat.total++;
    if (f.status !== "not_needed") {
      (cat as Record<string, number>)[f.status]++;
    }
  }

  return summary;
}
