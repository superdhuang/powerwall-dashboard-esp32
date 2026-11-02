import json
import os
import struct
import subprocess
import sys
from collections import deque
from pathlib import Path
from playwright.sync_api import sync_playwright
from PIL import Image
import io
import numpy as np

ASSETS_DIR = str(Path(__file__).resolve().parent / "assets")
os.makedirs(ASSETS_DIR, exist_ok=True)

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page(viewport={"width": 1000, "height": 800}, device_scale_factor=1)
    page.on("console", lambda msg: print(f"Browser console {msg.type}: {msg.text}") if msg.type == "error" else None)
    page.on("pageerror", lambda error: print(f"Browser page error: {error}"))

    def handle_route(route):
        resp = route.fetch()
        text = resp.text()
        inject = """
        window.__PW3D = {
          THREE, scene, camera, controls, renderer,
          get wireMeshes() { return wireMeshes; },
          get wireRanges() { return wireRanges; },
          get wirePathLengths() { return wirePathLengths; },
          get offGridXNode() { return offGridXNode; },
          get sceneMaterials() { return sceneMaterials; },
          get weather() { return weather; },
          weatherReady, apply3DPowerflow, selectWeatherMode, switchState, CALLOUT_ANCHORS_3D
        };
        """
        text = text.replace("pollLive();", inject).replace("setInterval(pollLive, 10000);", "")
        route.fulfill(response=resp, body=text.encode("utf-8"))

    page.route("http://localhost:8787/", handle_route)
    page.goto("http://localhost:8787/", wait_until="domcontentloaded")
    page.wait_for_function("window.__PW3D && window.__PW3D.wireMeshes && window.__PW3D.wireMeshes.solar && window.__PW3D.weather && window.__PW3D.offGridXNode", timeout=60000)
    page.wait_for_timeout(1200)

    # Lock stage to 704x440, hide floating header and HTML callouts, and zero out all wire flows & SOE
    anchors_2d = page.evaluate("""() => {
        const { THREE, camera, controls, renderer, apply3DPowerflow, sceneMaterials, wireMeshes, CALLOUT_ANCHORS_3D } = window.__PW3D;
        document.querySelectorAll('#stage-container > div').forEach(el => el.style.display = 'none');
        const sc = document.getElementById('stage-container');
        sc.style.width = '704px';
        sc.style.height = '440px';
        sc.style.flex = 'none';
        camera.aspect = 704 / 440;
        camera.position.set(4.614, 1.92, 3.516);
        controls.target.set(-2, 0.302, -1);
        camera.lookAt(controls.target);
        camera.updateProjectionMatrix();
        camera.updateMatrixWorld(true);
        renderer.setSize(704, 440, false);

        apply3DPowerflow({ solarW: 0, pwW: 0, gridW: 0, loadW: 0, soePct: 0, isOffGrid: false }, true);
        for (const m of Object.values(wireMeshes)) {
            m.material.uniforms.routeEnabled.value = false;
            m.material.uniforms.direction.value = 0;
        }
        if (sceneMaterials && sceneMaterials.soe) {
            sceneMaterials.soe.uniforms.soe.value = 0.0;
            sceneMaterials.soe.uniforms.pulseStrength.value = 0.0;
        }
        const out = {};
        for (const [k, vec] of Object.entries(CALLOUT_ANCHORS_3D)) {
            const p = vec.clone().project(camera);
            out[k] = {
                x: Math.round((p.x * 0.5 + 0.5) * 704),
                y: Math.round((-p.y * 0.5 + 0.5) * 440)
            };
        }
        return out;
    }""")
    print("2D Callout Anchors on 704x440:", anchors_2d)
    page.wait_for_timeout(200)

    stage_loc = page.locator("#stage")

    def capture_stage_rgb():
        png_bytes = stage_loc.screenshot(type="png")
        return np.array(Image.open(io.BytesIO(png_bytes)).convert("RGB"), dtype=np.int16)

    if "--phase-effects-only" in sys.argv:
        project_root = os.path.dirname(os.path.abspath(__file__))
        dashboard_root = os.path.dirname(project_root)
        dashboard_revision = subprocess.check_output(
            ["git", "-C", dashboard_root, "rev-parse", "HEAD"], text=True
        ).strip()
        if not dashboard_revision.startswith("0bb851c"):
            raise RuntimeError(
                "Phase-effect extraction is calibrated against dashboard commit 0bb851c; "
                f"current HEAD is {dashboard_revision}"
            )

        # The phase atlas and reflection LUT are captured from the committed dashboard's
        # fixed 704x440 camera. Keep all other scene inputs identical between captures.
        page.evaluate("window.__PW3D.selectWeatherMode('night')")
        page.wait_for_timeout(180)
        page.evaluate("""() => {
            const weather = window.__PW3D.weather;
            if (weather && typeof weather.update === 'function') {
                weather.update(0);
                weather.update = () => {};
            }
            for (const mesh of Object.values(window.__PW3D.wireMeshes)) {
                mesh.material.uniforms.routeEnabled.value = false;
                mesh.material.uniforms.direction.value = 0;
            }
            window.__PW3D.apply3DPowerflow(
                {solarW: 0, pwW: 0, gridW: 0, loadW: 0, soePct: 0, isOffGrid: false}, true
            );
        }""")
        moon_node = page.evaluate("window.__PW3D.scene.getObjectByName('Moon') !== null")
        solar_panel = page.evaluate("window.__PW3D.scene.getObjectByName('SolarPanel2') !== null")
        if not moon_node or not solar_panel:
            raise RuntimeError("Committed scene is missing Moon or SolarPanel2 mesh")

        page.evaluate("""() => {
            const moon = window.__PW3D.scene.getObjectByName('Moon');
            const panel = window.__PW3D.scene.getObjectByName('SolarPanel2');
            moon.visible = false;
            panel.material.uniforms.shine_angle.value = 74.5;
            panel.material.uniforms.shine_factor.value = 0.0;
            panel.material.uniforms.shine_color.value.set(0.407843, 0.458824, 0.596078, 1.0);
        }""")
        page.wait_for_timeout(100)
        moon_base_rgb = capture_stage_rgb()

        moon_samples = []
        for astro_phase in (0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875):
            page.evaluate("""phase => {
                const moon = window.__PW3D.scene.getObjectByName('Moon');
                const p = ((Number(phase) % 1) + 1) % 1;
                let remapped = 0.0, crescent = true;
                if (p <= 0.25) remapped = p / 0.25 * 0.5;
                else if (p > 0.75) remapped = 0.5 + (p - 0.75) / 0.25 * 0.5;
                else if (p < 0.5) { remapped = 0.5 + (p - 0.25) / 0.25 * 0.5; crescent = false; }
                else { remapped = (p - 0.5) / 0.25 * 0.5; crescent = false; }
                moon.material.uniforms.phase.value = 1.0 - remapped;
                moon.material.uniforms.is_crescent.value = crescent;
                moon.visible = true;
            }""", astro_phase)
            page.wait_for_timeout(100)
            moon_samples.append(capture_stage_rgb())

        moon_validation_samples = []
        for astro_phase in (0.12, 0.38, 0.62, 0.88):
            page.evaluate("""phase => {
                const moon = window.__PW3D.scene.getObjectByName('Moon');
                const p = ((Number(phase) % 1) + 1) % 1;
                let remapped = 0.0, crescent = true;
                if (p <= 0.25) remapped = p / 0.25 * 0.5;
                else if (p > 0.75) remapped = 0.5 + (p - 0.75) / 0.25 * 0.5;
                else if (p < 0.5) { remapped = 0.5 + (p - 0.25) / 0.25 * 0.5; crescent = false; }
                else { remapped = (p - 0.5) / 0.25 * 0.5; crescent = false; }
                moon.material.uniforms.phase.value = 1.0 - remapped;
                moon.material.uniforms.is_crescent.value = crescent;
                moon.visible = true;
            }""", astro_phase)
            page.wait_for_timeout(100)
            moon_validation_samples.append(capture_stage_rgb())

        moon_union = np.zeros(moon_base_rgb.shape[:2], dtype=bool)
        for sample in moon_samples:
            moon_union |= np.max(np.abs(sample - moon_base_rgb), axis=2) > 2

        # The original firmware night image still has the old, baked crescent.
        # Locate its connected silhouette against the moonless dashboard capture
        # and add those coordinates as constant moonless pixels to the atlas.
        legacy_path = os.path.join(ASSETS_DIR, "stage_night.png")
        if not os.path.exists(legacy_path):
            raise RuntimeError(f"Missing legacy firmware night asset: {legacy_path}")
        legacy_rgb = np.asarray(Image.open(legacy_path).convert("RGB"), dtype=np.int16)
        if legacy_rgb.shape != moon_base_rgb.shape:
            raise RuntimeError(f"Legacy night image size {legacy_rgb.shape} does not match dashboard {moon_base_rgb.shape}")
        def luminance(image):
            return (image[:, :, 0] * 299 + image[:, :, 1] * 587 + image[:, :, 2] * 114) // 1000
        legacy_diff = np.abs(luminance(legacy_rgb) - luminance(moon_base_rgb))
        roi_x0, roi_y0, roi_x1, roi_y1 = 475, 52, 531, 101
        legacy_candidate = legacy_diff[roi_y0:roi_y1, roi_x0:roi_x1] > 3
        seen = np.zeros_like(legacy_candidate, dtype=bool)
        largest_component = []
        for seed_y, seed_x in zip(*np.where(legacy_candidate)):
            if seen[seed_y, seed_x]:
                continue
            component = []
            pending = [(int(seed_y), int(seed_x))]
            seen[seed_y, seed_x] = True
            while pending:
                cy, cx = pending.pop()
                component.append((cy, cx))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if (0 <= ny < legacy_candidate.shape[0] and
                            0 <= nx < legacy_candidate.shape[1] and
                            legacy_candidate[ny, nx] and not seen[ny, nx]):
                            seen[ny, nx] = True
                            pending.append((ny, nx))
            if len(component) > len(largest_component):
                largest_component = component
        if len(largest_component) < 100:
            raise RuntimeError(f"Could not identify the legacy crescent in {legacy_path}; largest mask={len(largest_component)}")
        legacy_moon_mask = np.zeros_like(moon_union)
        for y, x in largest_component:
            legacy_moon_mask[roi_y0 + y, roi_x0 + x] = True
        component_x = [x for _, x in largest_component]
        component_y = [y for y, _ in largest_component]
        center_x = roi_x0 + (min(component_x) + max(component_x)) * 0.5
        center_y = roi_y0 + (min(component_y) + max(component_y)) * 0.5
        cleanup_radius = 20.0
        for y in range(max(0, int(center_y - cleanup_radius)), min(legacy_moon_mask.shape[0], int(center_y + cleanup_radius + 1))):
            for x in range(max(0, int(center_x - cleanup_radius)), min(legacy_moon_mask.shape[1], int(center_x + cleanup_radius + 1))):
                if (x - center_x) ** 2 + (y - center_y) ** 2 <= cleanup_radius ** 2:
                    legacy_moon_mask[y, x] = True
        moon_union |= legacy_moon_mask
        moon_ys, moon_xs = np.where(moon_union)
        if len(moon_xs) == 0:
            raise RuntimeError("Moon phase captures produced no changed pixels")

        def rgb565(rgb):
            r, g, b = (int(v) for v in rgb)
            return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

        def rgb565_components(rgb):
            r, g, b = (int(v) for v in rgb)
            return (r >> 3, g >> 2, b >> 3)

        def decode565_components(rgb):
            r, g, b = rgb
            return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))

        validation_errors = []
        for astro_phase, actual in zip((0.12, 0.38, 0.62, 0.88), moon_validation_samples):
            position = astro_phase * 8.0
            phase_index = min(int(position), 7)
            t = position - phase_index
            next_index = (phase_index + 1) & 7
            errors = []
            for y, x in zip(moon_ys, moon_xs):
                a = rgb565_components(moon_samples[phase_index][y, x])
                b = rgb565_components(moon_samples[next_index][y, x])
                predicted_components = tuple(int(round(a[c] + (b[c] - a[c]) * t)) for c in range(3))
                predicted_rgb = decode565_components(predicted_components)
                actual_rgb = decode565_components(rgb565_components(actual[y, x]))
                errors.extend(abs(predicted_rgb[c] - actual_rgb[c]) for c in range(3))
            validation_errors.append((astro_phase, float(np.mean(errors)), int(np.percentile(errors, 95)), int(max(errors))))
        print(f"Eight-keyframe moon interpolation RGB error (phase, MAE, P95, max): {validation_errors}")

        moon_records = []
        for y, x in zip(moon_ys, moon_xs):
            if legacy_moon_mask[y, x]:
                clean_pixel = rgb565(moon_base_rgb[y, x])
                samples = [clean_pixel] * len(moon_samples)
            else:
                samples = [rgb565(sample[y, x]) for sample in moon_samples]
            moon_records.append((int(x), int(y), samples))

        # Capture a common unlit solar-panel base and full-shine deltas. The shader
        # effect is reconstructed from uv1 at runtime, so flash stores short spans
        # instead of eight full RGB delta images for every panel pixel.
        page.evaluate("window.__PW3D.selectWeatherMode('clear')")
        page.wait_for_timeout(120)
        page.evaluate("""() => {
            const moon = window.__PW3D.scene.getObjectByName('Moon');
            const panel = window.__PW3D.scene.getObjectByName('SolarPanel2');
            moon.visible = false;
            panel.material.uniforms.shine_angle.value = 74.5;
            panel.material.uniforms.shine_factor.value = 0.0;
            panel.material.uniforms.shine_color.value.set(0.407843, 0.458824, 0.596078, 1.0);
        }""")
        page.wait_for_timeout(100)
        solar_base_rgb = capture_stage_rgb()

        def capture_full_shine(factor, color):
            page.evaluate("""cfg => {
                const panel = window.__PW3D.scene.getObjectByName('SolarPanel2');
                const u = panel.material.uniforms;
                u.shine_factor.value = cfg.factor;
                u.shine_angle.value = 74.5;
                u.shine_offset.value = -10000.0;
                u.shine_color.value.set(cfg.color[0], cfg.color[1], cfg.color[2], 1.0);
            }""", {"factor": factor, "color": color})
            page.wait_for_timeout(100)
            return capture_stage_rgb() - solar_base_rgb

        solar_day_delta = capture_full_shine(1.0, (0.407843, 0.458824, 0.596078))
        solar_night_delta = capture_full_shine(0.4, (0.38, 0.46, 0.64))
        solar_geometry = page.evaluate("""() => {
            const mesh = window.__PW3D.scene.getObjectByName('SolarPanel2');
            const uv = mesh.geometry.attributes.uv1;
            let minX=Infinity, minY=Infinity, maxX=-Infinity, maxY=-Infinity;
            for (let i=0; i<uv.count; i++) {
                minX=Math.min(minX,uv.getX(i)); minY=Math.min(minY,uv.getY(i));
                maxX=Math.max(maxX,uv.getX(i)); maxY=Math.max(maxY,uv.getY(i));
            }
            return {minX,minY,rangeX:maxX-minX,rangeY:maxY-minY};
        }""")
        if solar_geometry["rangeX"] <= 0 or solar_geometry["rangeY"] <= 0:
            raise RuntimeError(f"SolarPanel2 has invalid uv1 bounds: {solar_geometry}")

        page.evaluate("""bounds => {
            const THREE = window.__PW3D.THREE;
            const panel = window.__PW3D.scene.getObjectByName('SolarPanel2');
            panel.material = new THREE.ShaderMaterial({
                side: THREE.DoubleSide,
                uniforms: {
                    uvMin: {value: new THREE.Vector2(bounds.minX, bounds.minY)},
                    uvRange: {value: new THREE.Vector2(bounds.rangeX, bounds.rangeY)}
                },
                vertexShader: `
                    varying vec2 vUv1;
                    void main() {
                        vUv1 = uv1;
                        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
                    }
                `,
                fragmentShader: `
                    uniform vec2 uvMin;
                    uniform vec2 uvRange;
                    varying vec2 vUv1;
                    void main() {
                        vec2 uv = clamp((vUv1 - uvMin) / uvRange, 0.0, 1.0);
                        gl_FragColor = vec4(uv.x, uv.y, 1.0, 1.0);
                    }
                `
            });
        }""", solar_geometry)
        page.wait_for_timeout(120)
        solar_uv_rgb = capture_stage_rgb()
        solar_uv_info = page.evaluate("""() => {
            const panel = window.__PW3D.scene.getObjectByName('SolarPanel2');
            return {
                materialType: panel.material.type,
                isShaderMaterial: panel.material.isShaderMaterial,
                uniforms: Object.keys(panel.material.uniforms || {}),
                uv1Count: panel.geometry.attributes.uv1 ? panel.geometry.attributes.uv1.count : 0,
                visible: panel.visible,
                rendererErrors: window.__PW3D.renderer.info.programs.map(p => {
                    const d = p.diagnostics;
                    return d ? {runnable:d.runnable, programLog:d.programLog, vertexShader:d.vertexShader, fragmentShader:d.fragmentShader} : null;
                }).filter(d => d && !d.runnable)
            };
        }""")
        if solar_uv_info["rendererErrors"]:
            raise RuntimeError(f"SolarPanel2 UV capture shader failed: {solar_uv_info['rendererErrors']}")
        solar_mask = (solar_uv_rgb[:, :, 2] > 245) & (np.max(np.abs(solar_day_delta), axis=2) > 2)
        max_abs_delta = max(int(np.max(np.abs(solar_day_delta))), int(np.max(np.abs(solar_night_delta))))
        solar_spans = []
        for y in range(solar_mask.shape[0]):
            xs = np.where(solar_mask[y])[0]
            if len(xs) == 0:
                continue
            groups = np.split(xs, np.where(np.diff(xs) != 1)[0] + 1)
            for group in groups:
                for offset in range(0, len(group), 16):
                    run = group[offset:offset + 16]
                    x0, x1 = int(run[0]), int(run[-1])
                    solar_spans.append((
                        x0, int(y), len(run),
                        int(solar_uv_rgb[y, x0, 0]), int(solar_uv_rgb[y, x1, 0]),
                        int(solar_uv_rgb[y, x0, 1]), int(solar_uv_rgb[y, x1, 1]),
                        tuple(int(v) for v in np.clip(solar_day_delta[y, x0], 0, 255)),
                        tuple(int(v) for v in np.clip(solar_day_delta[y, x1], 0, 255)),
                        tuple(int(v) for v in np.clip(solar_night_delta[y, x0], 0, 255)),
                        tuple(int(v) for v in np.clip(solar_night_delta[y, x1], 0, 255)),
                    ))
        if len(solar_spans) == 0:
            raise RuntimeError("SolarPanel2 reflection captures produced no panel spans")

        include_path = os.path.join(project_root, "firmware", "src", "stage_phase_effects.inc")
        with open(include_path, "w", encoding="utf-8") as out:
            out.write(f"// Generated from powerwall dashboard commit {dashboard_revision}.\n")
            out.write("// Source: extract_3d_assets.py --phase-effects-only\n")
            out.write("const StageMoonPixel kStageMoonPixels[] = {\n")
            for x, y, samples in moon_records:
                sample_text = ", ".join(f"0x{pixel:04x}" for pixel in samples)
                out.write(f"  {{{x}, {y}, {{{sample_text}}}}},\n")
            out.write("};\n")
            out.write("const size_t kStageMoonPixelCount = sizeof(kStageMoonPixels) / sizeof(kStageMoonPixels[0]);\n\n")
            out.write("const float kStageSolarUvMinX = " + f"{solar_geometry['minX']:.9f}f;\n")
            out.write("const float kStageSolarUvRangeX = " + f"{solar_geometry['rangeX']:.9f}f;\n")
            out.write("const float kStageSolarUvMinY = " + f"{solar_geometry['minY']:.9f}f;\n")
            out.write("const float kStageSolarUvRangeY = " + f"{solar_geometry['rangeY']:.9f}f;\n")
            out.write("const StageSolarSpan kStageSolarSpans[] = {\n")
            for x, y, count, ux0, ux1, uy0, uy1, day0, day1, night0, night1 in solar_spans:
                c_day0 = ", ".join(str(v) for v in day0)
                c_day1 = ", ".join(str(v) for v in day1)
                c_night0 = ", ".join(str(v) for v in night0)
                c_night1 = ", ".join(str(v) for v in night1)
                out.write(f"  {{{x}, {y}, {count}, {ux0}, {ux1}, {uy0}, {uy1}, {{{c_day0}}}, {{{c_day1}}}, {{{c_night0}}}, {{{c_night1}}}}},\n")
            out.write("};\n")
            out.write("const size_t kStageSolarSpanCount = sizeof(kStageSolarSpans) / sizeof(kStageSolarSpans[0]);\n")

        print(f"Captured moon base and eight phase keyframes: {len(moon_records)} pixels")
        print(f"Replaced legacy baked crescent with moonless sky: {int(legacy_moon_mask.sum())} pixels")
        print(f"Captured day/night shader and uv1 panel spans: {int(solar_mask.sum())} pixels, {len(solar_spans)} spans, max |delta|={max_abs_delta}")
        print(f"Generated {include_path} ({os.path.getsize(include_path)} bytes) from {dashboard_revision}")
        browser.close()
        raise SystemExit(0)

    presets = ["clear", "partly", "overcast", "rain", "storm", "snow", "wind", "sunrise", "sunset", "night"]
    for pid in presets:
        page.evaluate(f"""() => {{
            window.__PW3D.selectWeatherMode('{pid}');
            window.__PW3D.sceneMaterials.soe.uniforms.soe.value = 0.0;
            for (const m of Object.values(window.__PW3D.wireMeshes)) {{
                m.material.uniforms.routeEnabled.value = false;
                m.material.uniforms.direction.value = 0;
            }}
        }}""")
        page.wait_for_timeout(160)
        png_bytes = stage_loc.screenshot(type="png")
        with open(os.path.join(ASSETS_DIR, f"stage_{pid}.png"), "wb") as f:
            f.write(png_bytes)
        print(f"Captured clean stage_{pid}.png")
        if pid in ("rain", "storm", "snow", "wind"):
            for fidx in (1, 2):
                page.wait_for_timeout(200)
                p2 = stage_loc.screenshot(type="png")
                with open(os.path.join(ASSETS_DIR, f"stage_{pid}_{fidx}.png"), "wb") as f:
                    f.write(p2)

    # Freeze weather animation completely before extracting exact 3D mesh pixel tables
    page.evaluate("""() => {
        window.__PW3D.selectWeatherMode('night');
        const w = window.__PW3D.scene.getObjectByName('OriginalWeather');
        const r = window.__PW3D.scene.getObjectByName('OriginalClassicRain');
        if (w) w.visible = false;
        if (r) r.visible = false;
        window.__PW3D.sceneMaterials.soe.uniforms.soe.value = 0.0;
        for (const m of Object.values(window.__PW3D.wireMeshes)) {
            m.material.uniforms.routeEnabled.value = false;
            m.material.uniforms.direction.value = 0;
        }
    }""")
    page.wait_for_timeout(120)

    base_rgb = capture_stage_rgb()

    # 2. Extract OffGridX 3D mesh pixels
    page.evaluate("window.__PW3D.offGridXNode.visible = true;")
    page.wait_for_timeout(80)
    offx_rgb = capture_stage_rgb()
    page.evaluate("window.__PW3D.offGridXNode.visible = false;")
    page.wait_for_timeout(80)

    diff_x = np.max(np.abs(offx_rgb - base_rgb), axis=2)
    ys, xs = np.where(diff_x > 4)
    with open(os.path.join(ASSETS_DIR, "offgridx_3d.bin"), "wb") as f:
        f.write(struct.pack("<I", len(xs)))
        for y, x in zip(ys, xs):
            r, g, b = offx_rgb[y, x]
            f.write(struct.pack("<HHBBB", int(x), int(y), int(r), int(g), int(b)))
    print(f"Extracted OffGridX 3D mesh: {len(xs)} pixels (x={np.min(xs)}..{np.max(xs)}, y={np.min(ys)}..{np.max(ys)})")

    # 3. Extract Powerwall 2 SOE LED bar pixels
    page.evaluate("""() => {
        const { sceneMaterials } = window.__PW3D;
        sceneMaterials.soe.uniforms.soe.value = 1.0;
        sceneMaterials.soe.uniforms.pulseStrength.value = 0.0;
    }""")
    page.wait_for_timeout(80)
    soe_rgb = capture_stage_rgb()
    page.evaluate("window.__PW3D.sceneMaterials.soe.uniforms.soe.value = 0.0;")
    page.wait_for_timeout(80)

    diff_soe = np.max(np.abs(soe_rgb - base_rgb), axis=2)
    sys_y, sys_x = np.where(diff_soe > 3)
    min_y = int(np.min(sys_y))
    max_y = int(np.max(sys_y))
    with open(os.path.join(ASSETS_DIR, "soe_3d.bin"), "wb") as f:
        f.write(struct.pack("<I", len(sys_x)))
        for y, x in zip(sys_y, sys_x):
            dr = max(0, int(soe_rgb[y, x, 0] - base_rgb[y, x, 0]))
            dg = max(0, int(soe_rgb[y, x, 1] - base_rgb[y, x, 1]))
            db = max(0, int(soe_rgb[y, x, 2] - base_rgb[y, x, 2]))
            frac = (max_y - y) / max(1.0, float(max_y - min_y))
            f.write(struct.pack("<HHBBBf", int(x), int(y), min(255, dr), min(255, dg), min(255, db), float(frac)))
    print(f"Extracted Powerwall2 SOE 3D LED bar: {len(sys_x)} pixels (x={np.min(sys_x)}..{np.max(sys_x)}, y={min_y}..{max_y})")

    # 4. Extract 3D Wire pixels & exact UV.x for each wire: grid(0), load(1), battery(2), solar(3)
    wire_keys = ["grid", "load", "battery", "solar"]
    wire_records = []
    for wid, wkey in enumerate(wire_keys):
        page.evaluate(f"""() => {{
            const mesh = window.__PW3D.wireMeshes['{wkey}'];
            const u = mesh.material.uniforms;
            u._savedBase = u.baseColor.value.clone();
            u.baseColor.value.copy(u.flowColor.value);
            u.routeEnabled.value = false;
            u.direction.value = 0;
        }}""")
        page.wait_for_timeout(80)
        lit_rgb = capture_stage_rgb()

        max_uv = page.evaluate(f"window.__PW3D.wireRanges['{wkey}'][1]")
        page.evaluate(f"""() => {{
            const {{ THREE, wireMeshes, wireRanges }} = window.__PW3D;
            const mesh = wireMeshes['{wkey}'];
            mesh._origMat = mesh.material;
            const maxUv = wireRanges['{wkey}'][1];
            const term = '{wkey}' === 'solar' ? 2.2 : 1000.0;
            mesh.material = new THREE.ShaderMaterial({{
                side: THREE.DoubleSide,
                uniforms: {{ maxUv: {{ value: maxUv }}, term: {{ value: term }} }},
                vertexShader: `
                    varying vec2 vUv;
                    void main() {{
                        vUv = uv;
                        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
                    }}
                `,
                fragmentShader: `
                    uniform float maxUv;
                    uniform float term;
                    varying vec2 vUv;
                    void main() {{
                        if (vUv.x > term) discard;
                        float norm = clamp(vUv.x / maxUv, 0.0, 1.0);
                        float v = floor(norm * 65535.0 + 0.5);
                        float hi = floor(v / 256.0) / 255.0;
                        float lo = mod(v, 256.0) / 255.0;
                        gl_FragColor = vec4(hi, lo, 1.0, 1.0);
                    }}
                `
            }});
        }}""")
        page.wait_for_timeout(80)
        uv_rgb = capture_stage_rgb()

        page.evaluate(f"""() => {{
            const mesh = window.__PW3D.wireMeshes['{wkey}'];
            mesh.material = mesh._origMat;
            mesh.material.uniforms.baseColor.value.copy(mesh.material.uniforms._savedBase);
        }}""")
        page.wait_for_timeout(60)

        diff_w = np.max(np.abs(lit_rgb - base_rgb), axis=2)
        wys, wxs = np.where(diff_w > 2)
        count_w = 0
        for y, x in zip(wys, wxs):
            dr = int(lit_rgb[y, x, 0] - base_rgb[y, x, 0])
            dg = int(lit_rgb[y, x, 1] - base_rgb[y, x, 1])
            db = int(lit_rgb[y, x, 2] - base_rgb[y, x, 2])
            hi = int(uv_rgb[y, x, 0])
            lo = int(uv_rgb[y, x, 1])
            norm = (hi * 256.0 + lo) / 65535.0
            u_val = float(norm * max_uv)
            wire_records.append((int(x), int(y), int(wid), u_val, dr, dg, db))
            count_w += 1
        print(f"Extracted 3D wire '{wkey}' (id={wid}): {count_w} pixels (x={np.min(wxs)}..{np.max(wxs)}, y={np.min(wys)}..{np.max(wys)})")

    with open(os.path.join(ASSETS_DIR, "wires_3d.bin"), "wb") as f:
        f.write(struct.pack("<I", len(wire_records)))
        for x, y, wid, u_val, dr, dg, db in wire_records:
            f.write(struct.pack("<HHBfhhh", x, y, wid, u_val, dr, dg, db))
    print(f"Saved clean wires_3d.bin with {len(wire_records)} total wire pixels")

    browser.close()
