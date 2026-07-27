import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const bytes = await readFile(path.join(root, "public", "movement_core.wasm"));
const { instance } = await WebAssembly.instantiate(bytes, {});
const api = instance.exports;

for (const name of [
  "m0_version",
  "m0_reset",
  "m0_step",
  "m0_get",
  "m0_stage_get",
  "m0_model",
  "m0_seed",
]) {
  assert.equal(typeof api[name], "function", `missing export ${name}`);
}
assert.equal(api.m0_version(), 1);

const traceInput = (tick) => {
  const phase = tick % 480;
  const jumpPhase = tick % 173;
  let moveX = 0;
  if (phase < 80) moveX = 32767;
  else if (phase < 145) moveX = -32767;
  else if (phase < 220) moveX = 13500;
  else if (phase < 300) moveX = -22500;
  else if (phase < 350) moveX = 32767;
  const jumpPressed = jumpPhase === 12 || jumpPhase === 94;
  const jumpHeld =
    (jumpPhase >= 12 && jumpPhase < 19) ||
    (jumpPhase >= 94 && jumpPhase < 112);
  const downHeld =
    (phase >= 226 && phase < 245) || (phase >= 405 && phase < 420);
  return [moveX, jumpPressed, jumpHeld, downHeld];
};

const runTrace = () => {
  api.m0_reset(20260727);
  assert.notEqual(api.m0_model(0), api.m0_model(1));
  let maxDelta = 0;
  for (let tick = 0; tick < 7200; tick += 1) {
    api.m0_step(...traceInput(tick));
    const deltaX = Math.abs(api.m0_get(0, 0) - api.m0_get(1, 0));
    const deltaY = Math.abs(api.m0_get(0, 1) - api.m0_get(1, 1));
    maxDelta = Math.max(maxDelta, deltaX, deltaY);
  }
  return {
    maxDelta,
    a: [0, 1, 2, 3, 4, 5].map((field) => api.m0_get(0, field)),
    b: [0, 1, 2, 3, 4, 5].map((field) => api.m0_get(1, field)),
  };
};

const first = runTrace();
const second = runTrace();
assert.deepEqual(second, first, "WebAssembly replay diverged");
assert.equal(first.a[4], 7200);
assert.equal(first.b[4], 7200);
assert.ok(
  Math.abs(first.maxDelta - 0.00147247314453125) < 1e-12,
  `unexpected trace delta ${first.maxDelta}`,
);

console.log(
  `wasm-self-test=pass trace_ticks=7200 max_position_delta=${first.maxDelta.toFixed(9)}`,
);
