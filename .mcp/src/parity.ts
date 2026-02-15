import { marked } from 'marked';
import { readFile, writeFile, mkdir } from 'fs/promises';
import { dirname, join } from 'path';

// Define the JSON schema for the parity data
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

function toSnakeCase(str: string): string {
  return str.toLowerCase().replace(/\s+/g, '_').replace(/[^\w_]/g, '');
}

export async function loadReport(path: string): Promise<ParityData> {
  const markdown = await readFile(path, 'utf-8');
  const tokens = marked.lexer(markdown);

  const features: ParityFeature[] = [];
  const top_improvements: string[] = [];
  const top_gaps: string[] = [];
  let parity_score = 0;

  const inventoryTable = tokens.find(token => token.type === 'table');
  if (inventoryTable && 'rows' in inventoryTable) {
    inventoryTable.rows.forEach((row: any) => {
      const feature: ParityFeature = {
        feature_id: toSnakeCase(row[0].text),
        name: row[0].text.replace(/\*\*/g, ''),
        original_pointers: row[1].text.split(',').map((s: string) => s.trim()),
        next_pointers: row[2].text.split(',').map((s: string) => s.trim()),
        status: row[3].text.replace(/\*\*/g, '') as ParityFeature['status'],
        source_strategy: row[4].text as ParityFeature['source_strategy'],
        notes: row[5].text,
        suggested_gaps: [], // This will be populated from the 'Actionable Gaps' section
      };
      features.push(feature);
    });
  }

  const improvementsHeading = tokens.findIndex(token => token.type === 'heading' && token.text.includes('Top 10 Improvements'));
  if (improvementsHeading !== -1) {
    const list = tokens[improvementsHeading + 1];
    if (list && list.type === 'list') {
      list.items.forEach((item: any) => {
        top_improvements.push(item.text);
      });
    }
  }

  const gapsHeading = tokens.findIndex(token => token.type === 'heading' && token.text.includes('Top 10 Real Parity Gaps'));
  if (gapsHeading !== -1) {
    const list = tokens[gapsHeading + 1];
    if (list && list.type === 'list') {
      list.items.forEach((item: any) => {
        top_gaps.push(item.text);
      });
    }
  }
  
  const gapsSection = tokens.findIndex(token => token.type === 'heading' && token.text.includes('Actionable Gaps'));
  if (gapsSection !== -1) {
      for(let i = gapsSection + 1; i < tokens.length; i++) {
          const token = tokens[i];
          if (token.type === 'heading' && token.depth === 2) {
              break;
          }
          if (token.type === 'list') {
              token.items.forEach((item: any) => {
                  const featureName = item.text.split(':')[0].replace(/\*\*/g, '');
                  const feature = features.find(f => f.name === featureName);
                  if (feature) {
                      feature.suggested_gaps.push(item.text);
                  }
              });
          }
      }
  }

  const scoreHeading = tokens.findIndex(token => token.type === 'heading' && token.text.includes('Updated Parity Score'));
  if (scoreHeading !== -1) {
    const text = tokens[scoreHeading + 1];
    if (text && text.type === 'paragraph') {
        const match = text.text.match(/(\d+)%/);
        if (match) {
            parity_score = parseInt(match[1], 10);
        }
    }
  }

  return { features, top_improvements, top_gaps, parity_score };
}

export function getSummary(data: ParityData): { score: number; counts: Record<string, number>; improvements: string[]; gaps: string[] } {
  const counts: Record<string, number> = {};
  for (const feature of data.features) {
    counts[feature.status] = (counts[feature.status] || 0) + 1;
  }
  return {
    score: data.parity_score,
    counts,
    improvements: data.top_improvements,
    gaps: data.top_gaps,
  };
}

export function listFeatures(data: ParityData, status_filter?: string[], q?: string): ParityFeature[] {
  let features = data.features;
  if (status_filter && status_filter.length > 0) {
    features = features.filter(f => status_filter.includes(f.status));
  }
  if (q) {
    features = features.filter(f => f.name.toLowerCase().includes(q.toLowerCase()) || f.feature_id.toLowerCase().includes(q.toLowerCase()));
  }
  return features;
}

export function getFeature(data: ParityData, feature_id: string): ParityFeature | undefined {
  return data.features.find(f => f.feature_id === feature_id);
}

export function createTicket(feature: ParityFeature, style: 'claude' | 'gemini' = 'claude'): string {
  const lines: string[] = [];

  lines.push(`# Implementation Ticket: ${feature.name}`);
  lines.push('');
  lines.push(`**Feature ID:** \`${feature.feature_id}\``);
  lines.push(`**Current Status:** ${feature.status}`);
  lines.push('');

  lines.push('## 1. Goal');
  lines.push(`Implement the **${feature.name}** feature in \`hamclock-next\` to achieve functional parity with the original \`hamclock-original\`.`);
  lines.push('');
  lines.push(`**Why:** ${feature.notes}`);
  if (feature.suggested_gaps.length > 0) {
    lines.push('');
    lines.push('**Specific Gaps to Address:**');
    feature.suggested_gaps.forEach(gap => lines.push(`- ${gap}`));
  }
  lines.push('');

  lines.push('## 2. Acceptance Criteria (User-Visible Behavior)');
  lines.push('- The feature should be accessible through the HamClock-Next UI.');
  lines.push('- It must provide the same core information and allow the same user interactions as the original.');
  lines.push('- The visual output should be clean, modern, and consistent with the `hamclock-next` design system. It does not need to be a pixel-perfect copy of the original.');
  lines.push('');

  lines.push('## 3. Implementation Guidance');
  lines.push('');
  lines.push('### Reference Implementation (`hamclock-original`)');
  lines.push('Study these files to understand the original behavior:');
  feature.original_pointers.forEach(p => lines.push(`- \`${p}\``));
  lines.push('');

  lines.push('### Likely Files to Modify (`hamclock-next`)');
  if (feature.next_pointers.length > 0 && feature.next_pointers[0] !== '') {
    lines.push('This feature may already have a partial implementation. Start by examining:');
    feature.next_pointers.forEach(p => lines.push(`- \`${p}\``));
  } else {
    lines.push('This is likely a new implementation. You may need to create new files:');
    lines.push(`- **UI Panel:** \`src/ui/\` (e.g., \`src/ui/${feature.name.replace(/\s+/g, "")}Panel.cpp\`)`);
    lines.push('- **Data Provider:** `src/services/` (if external data is required)');
    lines.push('- **Data Model:** `src/core/` (for new state management)');
  }
  lines.push('');

  lines.push('## 4. Minimal Test Checklist');
  lines.push('- [ ] **Build:** The code compiles successfully without warnings.');
  lines.push('- [ ] **Run:** `hamclock-next` starts without crashing.');
  lines.push('- [ ] **Visual Check:** The widget/panel appears in the UI and displays data correctly.');
  lines.push('- [ ] **Interaction Check:** User can interact with the feature as expected (e.g., clicks, hovers).');
  lines.push('');

  lines.push('## 5. Regression Guards & Constraints');
  lines.push('- **DO NOT** introduce breaking changes to the 800x480 logical resolution.');
  lines.push('- **DO NOT** add new fonts. Use the existing font catalog.');
  lines.push('- **ADHERE** to the existing provider architecture for data fetching.');
  lines.push('- **MAINTAIN** consistency with the existing UI/UX patterns.');

  return lines.join('\n');
}

export function createBatchTickets(data: ParityData, statuses: string[] = ['PARTIAL', 'STUB', 'MISSING'], limit: number = 10): string[] {
    const featuresToTicket = data.features.filter(f => statuses.includes(f.status));
    const tickets: string[] = [];
    for (let i = 0; i < Math.min(featuresToTicket.length, limit); i++) {
        tickets.push(createTicket(featuresToTicket[i]));
    }
    return tickets;
}
