# HamClock-Next WASM Packaging

## Building (via Docker) - Recommended

If you don't have the Emscripten SDK installed locally, you can build using Docker:

```bash
./scripts/build-wasm-docker.sh
```

This will automatically pull the `emscripten/emsdk` image and compile the project into `build-wasm/`.

## Deployment Requirements

1. **COOP/COEP Headers**:
   - The server must send the following headers:
     ```
     Cross-Origin-Opener-Policy: same-origin
     Cross-Origin-Embedder-Policy: require-corp
     ```
   - This is required for `SharedArrayBuffer` support (used by pthreads).

2. **CORS Proxy**:
   - External APIs (NOAA, PSK Reporter, etc.) do not allow cross-origin requests from browser apps.
   - The bundled `serve.py` includes an integrated proxy at `/proxy/<url>` — no separate setup needed for local use.
   - The CORS proxy URL defaults to `/proxy/` and is configurable at runtime via the web config SPA (Network tab) without recompiling.
   - For nginx deployments, see `cors-proxy.conf` for a ready-to-use location block.

3. **Persistence**:
   - Configuration is stored in IndexedDB (IDBFS).
   - It survives page reloads on the same origin.

## Development

Use `serve.py` to test locally. It serves files **and** proxies external API calls:

```bash
python3 serve.py ../../build-wasm
```

Open `http://localhost:8090/hamclock-wasm.html`.
The app will proxy all external requests through `http://localhost:8090/proxy/<url>` automatically.

## Changing the Proxy URL at Runtime

If you deploy behind nginx or a Cloudflare Worker instead of using `serve.py`:

1. Open `http://<your-host>/` (the web config SPA).
2. Click the **Network** tab.
3. Set **CORS Proxy URL** to your proxy prefix (e.g. `https://your-domain.com/proxy/`).
4. Click **Save**, then reload the WASM app.

Set to empty string only if your deployment already adds `Access-Control-Allow-Origin: *` to all upstream responses.

## nginx Production Deployment

See `cors-proxy.conf` for a complete nginx location block that forwards `/proxy/<url>` to the target host.
