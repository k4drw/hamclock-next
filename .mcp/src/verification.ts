import { ParityFeature } from './parity.js';

export interface VerificationResult {
  runtime_verification: 'disabled' | 'enabled';
  reachable?: boolean;
  widgets_found?: string[];
  screenshots?: string[];
  violations_report?: string[];
}

export async function verifyFeature(feature: ParityFeature, baseUrl: string): Promise<VerificationResult> {
  if (!baseUrl) {
    return { runtime_verification: 'disabled' };
  }

  const result: VerificationResult = {
    runtime_verification: 'enabled',
    reachable: false,
    widgets_found: [],
    screenshots: [],
    violations_report: [],
  };

  try {
    // 1. Check reachability and get widget list
    const widgetsResponse = await fetch(`${baseUrl}/debug/widgets`);
    if (!widgetsResponse.ok) {
      result.violations_report?.push(`Failed to fetch /debug/widgets: ${widgetsResponse.statusText}`);
      return result;
    }
    result.reachable = true;
    const widgets = await widgetsResponse.json();
    result.widgets_found = widgets.map((w: any) => w.name);

    // 2. Check if the feature's widget is present
    const featureName = feature.name.replace(/\s+/g, '');
    const isWidgetPresent = result.widgets_found?.some(w => w.toLowerCase().includes(featureName.toLowerCase()));

    if (!isWidgetPresent) {
      result.violations_report?.push(`Widget for feature '${feature.name}' not found in /debug/widgets`);
    }

    // 3. (Optional) Take a screenshot
    const screenshotResponse = await fetch(`${baseUrl}/live.jpg`);
    if (screenshotResponse.ok) {
      const buffer = await screenshotResponse.arrayBuffer();
      // For now, we'll just confirm we can get the image.
      // In a real scenario, we might save this to a file.
      result.screenshots?.push(`live.jpg (size: ${buffer.byteLength} bytes)`);
    } else {
        result.violations_report?.push(`Failed to fetch /live.jpg: ${screenshotResponse.statusText}`);
    }

  } catch (error: any) {
    result.violations_report?.push(`Error during verification: ${error.message}`);
  }

  return result;
}
