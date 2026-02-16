import { ParityFeature } from './parity.js';

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

/**
 * Run a memory stress test by monitoring performance metrics over time.
 * This simulates continuous operation and detects memory-related degradation.
 */
export async function runMemoryStressTest(baseUrl: string, durationSeconds: number = 30): Promise<MemoryStressTestResult> {
  const result: MemoryStressTestResult = {
    test_duration_seconds: durationSeconds,
    initial_fps: 0,
    final_fps: 0,
    fps_stability: 'stable',
    performance_samples: 0,
    issues_detected: [],
    recommendations: [],
  };

  const fpsSamples: number[] = [];
  const startTime = Date.now();
  const endTime = startTime + durationSeconds * 1000;
  const sampleInterval = 2000; // Sample every 2 seconds

  try {
    // Initial sample
    const initialPerf = await fetch(`${baseUrl}/debug/performance`);
    if (initialPerf.ok) {
      const data = await initialPerf.json();
      result.initial_fps = data.fps || 0;
      fpsSamples.push(result.initial_fps);
    } else {
      result.issues_detected.push(`Failed to fetch initial performance data: ${initialPerf.statusText}`);
      return result;
    }

    // Continuous sampling
    while (Date.now() < endTime) {
      await new Promise(resolve => setTimeout(resolve, sampleInterval));

      const perfResponse = await fetch(`${baseUrl}/debug/performance`);
      if (perfResponse.ok) {
        const data = await perfResponse.json();
        if (data.fps) {
          fpsSamples.push(data.fps);
        }
      }
    }

    // Final sample
    const finalPerf = await fetch(`${baseUrl}/debug/performance`);
    if (finalPerf.ok) {
      const data = await finalPerf.json();
      result.final_fps = data.fps || 0;
      fpsSamples.push(result.final_fps);
    }

    result.performance_samples = fpsSamples.length;

    // Analyze FPS stability
    const avgFps = fpsSamples.reduce((sum, fps) => sum + fps, 0) / fpsSamples.length;
    const fpsDropPercent = ((result.initial_fps - result.final_fps) / result.initial_fps) * 100;

    if (result.final_fps < 30) {
      result.fps_stability = 'critical';
      result.issues_detected.push(`Critical FPS degradation: dropped to ${result.final_fps.toFixed(1)} FPS`);
      result.recommendations.push(`Severe memory issue detected - check for allocation failures`);
    } else if (fpsDropPercent > 20) {
      result.fps_stability = 'degraded';
      result.issues_detected.push(`FPS dropped by ${fpsDropPercent.toFixed(1)}%: ${result.initial_fps.toFixed(1)} → ${result.final_fps.toFixed(1)}`);
      result.recommendations.push(`Memory pressure detected - monitor texture allocation patterns`);
    } else if (avgFps < 50) {
      result.fps_stability = 'degraded';
      result.issues_detected.push(`Average FPS below target: ${avgFps.toFixed(1)} (expected >50)`);
      result.recommendations.push(`Consider enabling KMSDRM low-memory optimizations`);
    } else {
      result.fps_stability = 'stable';
      result.recommendations.push(`System stable - no memory issues detected during ${durationSeconds}s test`);
    }

    // Check for anomalies in FPS samples
    const minFps = Math.min(...fpsSamples);
    const maxFps = Math.max(...fpsSamples);
    const fpsRange = maxFps - minFps;

    if (fpsRange > 20) {
      result.issues_detected.push(`High FPS variance detected: ${minFps.toFixed(1)} - ${maxFps.toFixed(1)} FPS`);
      result.recommendations.push(`Investigate texture allocation spikes or garbage collection pauses`);
    }

  } catch (error: any) {
    result.issues_detected.push(`Memory stress test failed: ${error.message}`);
  }

  return result;
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

    // 4. Run memory stress test for map-related features
    // (Phase 37: memory optimization verification)
    const mapRelatedFeatures = ['world_map', 'map_widget', 'night_overlay', 'tooltip'];
    if (mapRelatedFeatures.some(mf => feature.feature_id.toLowerCase().includes(mf))) {
      result.memory_stress_test = await runMemoryStressTest(baseUrl, 30);
    }

  } catch (error: any) {
    result.violations_report?.push(`Error during verification: ${error.message}`);
  }

  return result;
}
