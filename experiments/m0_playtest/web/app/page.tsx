"use client";

import { useCallback, useEffect, useRef, useState } from "react";

type Candidate = 0 | 1;
type Point = { x: number; y: number };
type Controls = {
  left: boolean;
  right: boolean;
  walk: boolean;
  down: boolean;
  jump: boolean;
  jumpPressed: boolean;
};
type GamepadInput = {
  moveX: number;
  down: boolean;
  jump: boolean;
  jumpPressed: boolean;
  lastJump: boolean;
  connected: boolean;
};
type MovementApi = {
  m0_version(): number;
  m0_reset(seed: number): void;
  m0_step(moveX: number, jumpPressed: number, jumpHeld: number, down: number): void;
  m0_get(candidate: Candidate, field: number): number;
  m0_stage_get(field: number): number;
  m0_model(candidate: Candidate): number;
};

const STEP = 1 / 60;
const DEFAULT_SEED = 20260727;
const SCORE_DIMENSIONS = [
  "Input immediacy",
  "Ground control",
  "Air control",
  "Collision/platform stability",
  "Movement expression",
  "Visual stability",
  "Overall fun",
] as const;
type ScoreDimension = (typeof SCORE_DIMENSIONS)[number];
type Scores = Record<ScoreDimension, [number, number]>;
const MANEUVERS = [
  "Ten rapid left-right dash-dance sequences.",
  "Slow walks into full-speed runs and immediate reversals.",
  "Ten short hops and ten full hops.",
  "Aerial drift reversals before and after the apex.",
  "Double jumps with early and late direction changes.",
  "Fast falls at several heights.",
  "Platform landings and intentional platform drops.",
  "Run off both edges and attempt an aerial return.",
];

function freshControls(): Controls {
  return {
    left: false,
    right: false,
    walk: false,
    down: false,
    jump: false,
    jumpPressed: false,
  };
}

function freshScores(): Scores {
  return Object.fromEntries(
    SCORE_DIMENSIONS.map((dimension) => [dimension, [0, 0]]),
  ) as Scores;
}

function nextSeed() {
  const value = crypto.getRandomValues(new Uint32Array(1))[0];
  return value || DEFAULT_SEED;
}

async function loadCore(): Promise<MovementApi> {
  const response = await fetch("/movement_core.wasm?v=2");
  if (!response.ok) throw new Error(`Movement core returned ${response.status}`);
  const bytes = await response.arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const api = instance.exports as unknown as MovementApi;
  if (api.m0_version() !== 2) throw new Error("Unsupported movement core ABI");
  return api;
}

function stateLabel(api: MovementApi, candidate: Candidate) {
  if (api.m0_get(candidate, 6) > 0) return "JUMP SQUAT";
  if (api.m0_get(candidate, 7) > 0) {
    if (api.m0_get(candidate, 10) > 0) return "DASH";
    return api.m0_get(candidate, 8) > 0 ? "PLATFORM" : "GROUNDED";
  }
  return "AIRBORNE";
}

function draw(
  canvas: HTMLCanvasElement,
  api: MovementApi,
  focus: Candidate,
  trails: [Point[], Point[]],
  showTrails: boolean,
) {
  const context = canvas.getContext("2d");
  if (!context) return;
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  const ratio = Math.min(window.devicePixelRatio || 1, 2);
  const pixelWidth = Math.round(width * ratio);
  const pixelHeight = Math.round(height * ratio);
  if (canvas.width !== pixelWidth || canvas.height !== pixelHeight) {
    canvas.width = pixelWidth;
    canvas.height = pixelHeight;
  }
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);
  const sky = context.createLinearGradient(0, 0, 0, height);
  sky.addColorStop(0, "#101c2b");
  sky.addColorStop(1, "#07101b");
  context.fillStyle = sky;
  context.fillRect(0, 0, width, height);

  const gap = 16;
  const panelWidth = (width - gap) / 2;
  const floorLeft = api.m0_stage_get(0);
  const floorRight = api.m0_stage_get(1);
  const floorY = api.m0_stage_get(2);
  const platformLeft = api.m0_stage_get(3);
  const platformRight = api.m0_stage_get(4);
  const platformY = api.m0_stage_get(5);
  const halfWidth = api.m0_stage_get(6);
  const halfHeight = api.m0_stage_get(7);

  for (const candidate of [0, 1] as Candidate[]) {
    const left = candidate * (panelWidth + gap);
    const scale = Math.min((panelWidth - 30) / 21, (height - 66) / 10.9);
    const originX = left + panelWidth / 2;
    const originY = 40 + 1.2 * scale;
    const sx = (x: number) => originX + x * scale;
    const sy = (y: number) => originY + y * scale;

    context.fillStyle =
      focus === candidate ? "rgba(72,231,207,.055)" : "rgba(255,255,255,.008)";
    context.fillRect(left, 0, panelWidth, height);
    context.strokeStyle =
      focus === candidate ? "rgba(72,231,207,.72)" : "rgba(145,167,191,.17)";
    context.lineWidth = focus === candidate ? 2 : 1;
    context.strokeRect(left + 0.5, 0.5, panelWidth - 1, height - 1);

    context.font = "700 12px monospace";
    context.fillStyle = focus === candidate ? "#48e7cf" : "#8799ad";
    context.fillText(`CANDIDATE ${candidate === 0 ? "A" : "B"}`, left + 14, 22);

    context.lineCap = "round";
    context.lineWidth = 5;
    context.strokeStyle = "#63768c";
    context.beginPath();
    context.moveTo(sx(floorLeft), sy(floorY));
    context.lineTo(sx(floorRight), sy(floorY));
    context.stroke();
    context.lineWidth = 3;
    context.strokeStyle = "#9aabba";
    context.beginPath();
    context.moveTo(sx(platformLeft), sy(platformY));
    context.lineTo(sx(platformRight), sy(platformY));
    context.stroke();

    if (showTrails) {
      trails[candidate].forEach((point, index, points) => {
        if (!index) return;
        context.strokeStyle = `rgba(72,231,207,${(index / points.length) * 0.35})`;
        context.lineWidth = 1.4;
        context.beginPath();
        context.moveTo(sx(points[index - 1].x), sy(points[index - 1].y));
        context.lineTo(sx(point.x), sy(point.y));
        context.stroke();
      });
    }

    const x = api.m0_get(candidate, 0);
    const y = api.m0_get(candidate, 1);
    const fighterWidth = Math.max(12, halfWidth * 2 * scale);
    const fighterHeight = Math.max(21, halfHeight * 2 * scale);
    context.shadowColor = "rgba(72,231,207,.46)";
    context.shadowBlur = 14;
    context.fillStyle = "#ddfffa";
    context.fillRect(
      sx(x) - fighterWidth / 2,
      sy(y) - fighterHeight / 2,
      fighterWidth,
      fighterHeight,
    );
    context.shadowBlur = 0;
    context.fillStyle = "#17343a";
    context.fillRect(sx(x) - fighterWidth * 0.18, sy(y) - 2, fighterWidth * 0.36, 3);

    context.font = "600 10px monospace";
    context.fillStyle = "#71869d";
    context.fillText(stateLabel(api, candidate), left + 14, height - 15);
    context.textAlign = "right";
    context.fillText(`x ${x.toFixed(3)}  y ${y.toFixed(3)}`, left + panelWidth - 14, height - 15);
    context.textAlign = "left";
  }
}

export default function Home() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const apiRef = useRef<MovementApi | null>(null);
  const controlsRef = useRef(freshControls());
  const gamepadRef = useRef<GamepadInput>({
    moveX: 0,
    down: false,
    jump: false,
    jumpPressed: false,
    lastJump: false,
    connected: false,
  });
  const trailsRef = useRef<[Point[], Point[]]>([[], []]);
  const runningRef = useRef(true);
  const focusRef = useRef<Candidate>(0);
  const trailsOnRef = useRef(true);
  const accumulatorRef = useRef(0);
  const lastTimeRef = useRef(0);
  const maxDeltaRef = useRef(0);
  const frameRef = useRef(0);
  const seedRef = useRef(DEFAULT_SEED);
  const [status, setStatus] = useState<"loading" | "ready" | "error">("loading");
  const [error, setError] = useState("");
  const [running, setRunning] = useState(true);
  const [focus, setFocus] = useState<Candidate>(0);
  const [seed, setSeed] = useState(DEFAULT_SEED);
  const [tick, setTick] = useState(0);
  const [maxDelta, setMaxDelta] = useState(0);
  const [trailsOn, setTrailsOn] = useState(true);
  const [mapping, setMapping] = useState<[string, string] | null>(null);
  const [gamepadConnected, setGamepadConnected] = useState(false);
  const [scores, setScores] = useState<Scores>(freshScores);
  const [notes, setNotes] = useState("");
  const [perceptible, setPerceptible] = useState("");
  const [repeatable, setRepeatable] = useState("");
  const [snags, setSnags] = useState("");
  const [preference, setPreference] = useState("");
  const [decision, setDecision] = useState("");
  const [copyStatus, setCopyStatus] = useState("");
  const scoredCount = SCORE_DIMENSIONS.reduce(
    (count, dimension) =>
      count + Number(scores[dimension][0] > 0) + Number(scores[dimension][1] > 0),
    0,
  );
  const scoresComplete = scoredCount === SCORE_DIMENSIONS.length * 2;

  const reset = useCallback((newSeed?: number) => {
    const api = apiRef.current;
    if (!api) return;
    const selectedSeed = newSeed ?? seedRef.current;
    api.m0_reset(selectedSeed);
    seedRef.current = selectedSeed;
    trailsRef.current = [[], []];
    maxDeltaRef.current = 0;
    accumulatorRef.current = 0;
    setSeed(selectedSeed);
    setTick(0);
    setMaxDelta(0);
    setMapping(null);
    if (newSeed !== undefined) {
      setScores(freshScores());
      setNotes("");
      setPerceptible("");
      setRepeatable("");
      setSnags("");
      setPreference("");
      setDecision("");
      setCopyStatus("");
    }
  }, []);

  const simulate = useCallback(() => {
    const api = apiRef.current;
    if (!api) return;
    const controls = controlsRef.current;
    const gamepad = gamepadRef.current;
    const digitalMove =
      controls.left === controls.right
        ? 0
        : (controls.left ? -1 : 1) * (controls.walk ? 13500 : 32767);
    const moveX = digitalMove || gamepad.moveX;
    api.m0_step(
      moveX,
      controls.jumpPressed || gamepad.jumpPressed ? 1 : 0,
      controls.jump || gamepad.jump ? 1 : 0,
      controls.down || gamepad.down ? 1 : 0,
    );
    controls.jumpPressed = false;
    gamepad.jumpPressed = false;
    const a = { x: api.m0_get(0, 0), y: api.m0_get(0, 1) };
    const b = { x: api.m0_get(1, 0), y: api.m0_get(1, 1) };
    trailsRef.current[0].push(a);
    trailsRef.current[1].push(b);
    if (trailsRef.current[0].length > 150) trailsRef.current[0].shift();
    if (trailsRef.current[1].length > 150) trailsRef.current[1].shift();
    maxDeltaRef.current = Math.max(
      maxDeltaRef.current,
      Math.abs(a.x - b.x),
      Math.abs(a.y - b.y),
    );
  }, []);

  const reveal = useCallback(() => {
    const api = apiRef.current;
    if (!api || !scoresComplete) return;
    const label = (candidate: Candidate) =>
      api.m0_model(candidate) === 0 ? "float32" : "float32";
    setMapping([label(0), label(1)]);
  }, [scoresComplete]);

  const pollGamepad = useCallback(() => {
    if (!("getGamepads" in navigator)) return;
    const pad = Array.from(navigator.getGamepads()).find(
      (candidate): candidate is Gamepad => candidate !== null,
    );
    const state = gamepadRef.current;
    const connected = Boolean(pad);
    if (connected !== state.connected) {
      state.connected = connected;
      setGamepadConnected(connected);
    }
    if (!pad) {
      state.moveX = 0;
      state.down = false;
      state.jump = false;
      state.lastJump = false;
      return;
    }
    const axis = Math.abs(pad.axes[0] ?? 0) >= 0.18 ? pad.axes[0] : 0;
    state.moveX = Math.round(Math.max(-1, Math.min(1, axis)) * 32767);
    state.down = Boolean(pad.buttons[13]?.pressed || (pad.axes[1] ?? 0) > 0.55);
    const jump = Boolean(pad.buttons[0]?.pressed);
    if (jump && !state.lastJump) state.jumpPressed = true;
    state.jump = jump;
    state.lastJump = jump;
  }, []);

  useEffect(() => {
    let cancelled = false;
    loadCore()
      .then((api) => {
        if (cancelled) return;
        apiRef.current = api;
        api.m0_reset(DEFAULT_SEED);
        setStatus("ready");
      })
      .catch((reason: unknown) => {
        if (cancelled) return;
        setStatus("error");
        setError(reason instanceof Error ? reason.message : "Unknown load error");
      });
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    runningRef.current = running;
    focusRef.current = focus;
    trailsOnRef.current = trailsOn;
  }, [running, focus, trailsOn]);

  useEffect(() => {
    const down = (event: KeyboardEvent) => {
      if (["Space", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].includes(event.code)) {
        event.preventDefault();
      }
      const controls = controlsRef.current;
      if (event.code === "ShiftLeft" || event.code === "ShiftRight") controls.walk = true;
      if (event.code === "KeyA" || event.code === "ArrowLeft") controls.left = true;
      if (event.code === "KeyD" || event.code === "ArrowRight") controls.right = true;
      if (event.code === "KeyS" || event.code === "ArrowDown") controls.down = true;
      if (event.code === "Space" || event.code === "KeyW" || event.code === "ArrowUp") {
        if (!controls.jump) controls.jumpPressed = true;
        controls.jump = true;
      }
      if (event.repeat) return;
      if (event.code === "Digit1") setFocus(0);
      if (event.code === "Digit2") setFocus(1);
      if (event.code === "KeyP") setRunning((value) => !value);
      if (event.code === "KeyN" && !runningRef.current) simulate();
      if (event.code === "KeyR") reset();
      if (event.code === "KeyT") setTrailsOn((value) => !value);
      if (event.code === "KeyV") reveal();
    };
    const up = (event: KeyboardEvent) => {
      const controls = controlsRef.current;
      if (event.code === "ShiftLeft" || event.code === "ShiftRight") {
        controls.walk = event.shiftKey;
      }
      if (event.code === "KeyA" || event.code === "ArrowLeft") controls.left = false;
      if (event.code === "KeyD" || event.code === "ArrowRight") controls.right = false;
      if (event.code === "KeyS" || event.code === "ArrowDown") controls.down = false;
      if (event.code === "Space" || event.code === "KeyW" || event.code === "ArrowUp") {
        controls.jump = false;
      }
    };
    const blur = () => {
      controlsRef.current = freshControls();
    };
    window.addEventListener("keydown", down, { passive: false });
    window.addEventListener("keyup", up);
    window.addEventListener("blur", blur);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
      window.removeEventListener("blur", blur);
    };
  }, [reset, reveal, simulate]);

  useEffect(() => {
    const frame = (time: number) => {
      const api = apiRef.current;
      const canvas = canvasRef.current;
      if (api && canvas) {
        pollGamepad();
        if (!lastTimeRef.current) lastTimeRef.current = time;
        const elapsed = Math.min((time - lastTimeRef.current) / 1000, 0.1);
        lastTimeRef.current = time;
        if (runningRef.current) {
          accumulatorRef.current += elapsed;
          while (accumulatorRef.current >= STEP) {
            simulate();
            accumulatorRef.current -= STEP;
          }
        }
        draw(canvas, api, focusRef.current, trailsRef.current, trailsOnRef.current);
        const currentTick = api.m0_get(0, 4);
        if (currentTick % 6 === 0) {
          setTick(currentTick);
          setMaxDelta(maxDeltaRef.current);
        }
      }
      frameRef.current = requestAnimationFrame(frame);
    };
    frameRef.current = requestAnimationFrame(frame);
    return () => cancelAnimationFrame(frameRef.current);
  }, [pollGamepad, simulate]);

  const setTouch = (
    key: "left" | "right" | "down" | "jump",
    pressed: boolean,
  ) => {
    const controls = controlsRef.current;
    if (key === "jump" && pressed && !controls.jump) controls.jumpPressed = true;
    controls[key] = pressed;
  };

  const setScore = (
    dimension: ScoreDimension,
    candidate: Candidate,
    value: number,
  ) => {
    setScores((current) => ({
      ...current,
      [dimension]: current[dimension].map((score, index) =>
        index === candidate ? value : score,
      ) as [number, number],
    }));
    setCopyStatus("");
  };

  const copyResults = async () => {
    if (!mapping) return;
    const scoreLines = SCORE_DIMENSIONS.map(
      (dimension) =>
        `- ${dimension}: A ${scores[dimension][0]}/5, B ${scores[dimension][1]}/5`,
    );
    const result = [
      "M0 movement representation playtest",
      `Seed: ${seed}`,
      `Input: ${gamepadConnected ? "gamepad/browser" : "keyboard or touch/browser"}`,
      `Maximum observed position delta: ${maxDelta.toFixed(9)}`,
      "",
      "Blind scores:",
      ...scoreLines,
      "",
      `Candidate A: ${mapping[0]}`,
      `Candidate B: ${mapping[1]}`,
      `Difference perceptible: ${perceptible || "not answered"}`,
      `Difference repeatable: ${repeatable || "not answered"}`,
      `Critical snag/jitter/contact issue: ${snags || "not answered"}`,
      `Preferred candidate: ${preference || "not answered"}`,
      `Owner decision: ${decision || "not answered"}`,
      `Notes: ${notes.trim() || "none"}`,
    ].join("\n");
    try {
      await navigator.clipboard.writeText(result);
      setCopyStatus("Results copied");
    } catch {
      setCopyStatus("Clipboard unavailable");
    }
  };

  return (
    <main className="site-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Ultra Performance Platform Fighter · M0</p>
          <h1>Movement Lab</h1>
          <p className="hero-copy">
            A blind feel test between two numeric representations, running the
            same pure-C movement model at a fixed 60 Hz in WebAssembly.
          </p>
        </div>
        <div className={`core-status ${status}`} data-testid="core-status">
          <span aria-hidden="true" />
          {status === "loading" ? "Loading C core" : status === "ready" ? "C core online" : "Core failed"}
        </div>
      </header>

      {status === "error" ? (
        <section className="error-card">
          <strong>The movement core did not load.</strong>
          <p>{error}</p>
          <button onClick={() => window.location.reload()}>Reload test</button>
        </section>
      ) : (
        <section className="lab-card" aria-busy={status === "loading"}>
          <div className="lab-toolbar">
            <div className="metric"><span>SEED</span><strong>{seed}</strong></div>
            <div className="metric"><span>TICK</span><strong data-testid="tick">{tick.toLocaleString()}</strong></div>
            <div className="metric wide"><span>MAX POSITION Δ</span><strong>{maxDelta.toFixed(6)}</strong></div>
            <div className="toolbar-actions">
              <button className={focus === 0 ? "active" : ""} onClick={() => setFocus(0)}>Focus A</button>
              <button className={focus === 1 ? "active" : ""} onClick={() => setFocus(1)}>Focus B</button>
            </div>
          </div>

          <canvas
            ref={canvasRef}
            className="playfield"
            aria-label="Side-by-side movement simulation for Candidate A and Candidate B"
          />

          <div className="control-row">
            <div className="simulation-actions">
              <button onClick={() => setRunning((value) => !value)}>{running ? "Pause" : "Resume"}</button>
              <button disabled={running} onClick={simulate}>Step</button>
              <button onClick={() => reset()}>Reset</button>
              <button onClick={() => reset(nextSeed())}>New blind run</button>
              <button onClick={() => setTrailsOn((value) => !value)}>Trails {trailsOn ? "on" : "off"}</button>
              <button
                className="reveal-button"
                disabled={!scoresComplete}
                title={
                  scoresComplete
                    ? "Reveal the randomized model assignment"
                    : "Complete the blind scorecard below first"
                }
                onClick={reveal}
              >
                {mapping
                  ? "Revealed"
                  : scoresComplete
                    ? "Reveal models"
                    : `Score first · ${scoredCount}/14`}
              </button>
            </div>
            <div className="touch-controls" aria-label="On-screen movement controls">
              {(["left", "right", "down"] as const).map((key) => (
                <button
                  key={key}
                  aria-label={key === "left" ? "Move left" : key === "right" ? "Move right" : "Fast fall or platform drop"}
                  onPointerDown={(event) => {
                    event.currentTarget.setPointerCapture(event.pointerId);
                    setTouch(key, true);
                  }}
                  onPointerUp={() => setTouch(key, false)}
                  onPointerCancel={() => setTouch(key, false)}
                >
                  {key === "left" ? "←" : key === "right" ? "→" : "↓"}
                </button>
              ))}
              <button
                className="jump-control"
                aria-label="Jump"
                onPointerDown={(event) => {
                  event.currentTarget.setPointerCapture(event.pointerId);
                  setTouch("jump", true);
                }}
                onPointerUp={() => setTouch("jump", false)}
                onPointerCancel={() => setTouch("jump", false)}
              >
                JUMP
              </button>
            </div>
          </div>

          {mapping && (
            <div className="reveal-strip" role="status" data-testid="mapping">
              <span>Candidate A <strong>{mapping[0]}</strong></span>
              <span>Candidate B <strong>{mapping[1]}</strong></span>
              <small>Start a new blind run to hide the assignment again.</small>
            </div>
          )}
        </section>
      )}

      <section className="quick-guide">
        <div>
          <p className="section-kicker">Blind protocol</p>
          <h2>Push both candidates through the same maneuvers</h2>
        </div>
        <ol>
          {MANEUVERS.map((maneuver, index) => (
            <li key={maneuver}>
              <span>{String(index + 1).padStart(2, "0")}</span>
              {maneuver}
            </li>
          ))}
        </ol>
        <p className="key-hint">
          Keyboard: tap <kbd>A</kbd>/<kbd>D</kbd> to dash dance · hold{" "}
          <kbd>Shift</kbd> + <kbd>A</kbd>/<kbd>D</kbd> to walk ·{" "}
          <kbd>Space</kbd> jump ·{" "}
          <kbd>S</kbd> fast fall · <kbd>1</kbd>/<kbd>2</kbd> focus ·{" "}
          <kbd>R</kbd> reset · <kbd>P</kbd> pause · Controller:{" "}
          <span className={gamepadConnected ? "device-live" : ""}>
            {gamepadConnected ? "connected" : "move a stick or press a button to connect"}
          </span>
        </p>
      </section>

      <section className="score-card">
        <div className="score-heading">
          <div>
            <p className="section-kicker">Score before reveal</p>
            <h2>Blind movement scorecard</h2>
          </div>
          <p>
            Rate every dimension from 1 (poor) to 5 (excellent). The assignment
            stays locked until all fourteen scores are recorded.
          </p>
        </div>

        <div className="score-table">
          <div className="score-row score-header" aria-hidden="true">
            <span>Dimension</span>
            <span>Candidate A</span>
            <span>Candidate B</span>
          </div>
          {SCORE_DIMENSIONS.map((dimension) => (
            <div className="score-row" key={dimension}>
              <strong>{dimension}</strong>
              {([0, 1] as Candidate[]).map((candidate) => (
                <div
                  className="rating-group"
                  role="group"
                  aria-label={`${dimension}, Candidate ${candidate === 0 ? "A" : "B"}`}
                  key={candidate}
                >
                  {[1, 2, 3, 4, 5].map((value) => (
                    <button
                      className={scores[dimension][candidate] === value ? "selected" : ""}
                      aria-label={`${value} out of 5`}
                      aria-pressed={scores[dimension][candidate] === value}
                      key={value}
                      onClick={() => setScore(dimension, candidate, value)}
                    >
                      {value}
                    </button>
                  ))}
                </div>
              ))}
            </div>
          ))}
        </div>

        <label className="notes-field">
          <span>Notes while still blind</span>
          <textarea
            value={notes}
            onChange={(event) => setNotes(event.target.value)}
            placeholder="Snags, jitter, platform contacts, or a movement moment worth repeating…"
            rows={3}
          />
        </label>

        <div className="reveal-gate">
          <div>
            <strong>{scoresComplete ? "Scorecard complete" : `${scoredCount} of 14 scores recorded`}</strong>
            <span>
              {mapping
                ? "Assignment revealed. Finish the decision record below."
                : "Do not use the numeric delta as a substitute for feel."}
            </span>
          </div>
          <button disabled={!scoresComplete || Boolean(mapping)} onClick={reveal}>
            {mapping ? "Assignment revealed" : "Reveal Candidate A / B"}
          </button>
        </div>

        {mapping && (
          <div className="decision-panel">
            <div className="assignment">
              <span>Candidate A <strong>{mapping[0]}</strong></span>
              <span>Candidate B <strong>{mapping[1]}</strong></span>
            </div>
            <div className="decision-grid">
              <label>
                <span>Difference perceptible?</span>
                <select value={perceptible} onChange={(event) => setPerceptible(event.target.value)}>
                  <option value="">Choose…</option>
                  <option>No</option>
                  <option>Yes</option>
                  <option>Unsure</option>
                </select>
              </label>
              <label>
                <span>Repeatable after reset?</span>
                <select value={repeatable} onChange={(event) => setRepeatable(event.target.value)}>
                  <option value="">Choose…</option>
                  <option>No</option>
                  <option>Yes</option>
                  <option>Unsure</option>
                </select>
              </label>
              <label>
                <span>Critical snag or jitter?</span>
                <select value={snags} onChange={(event) => setSnags(event.target.value)}>
                  <option value="">Choose…</option>
                  <option>No</option>
                  <option>Candidate A</option>
                  <option>Candidate B</option>
                  <option>Both</option>
                </select>
              </label>
              <label>
                <span>Preferred candidate</span>
                <select value={preference} onChange={(event) => setPreference(event.target.value)}>
                  <option value="">Choose…</option>
                  <option>Candidate A</option>
                  <option>Candidate B</option>
                  <option>No preference</option>
                  <option>Retest required</option>
                </select>
              </label>
              <label className="decision-choice">
                <span>Owner decision</span>
                <select value={decision} onChange={(event) => setDecision(event.target.value)}>
                  <option value="">Choose…</option>
                  <option>Approve float32</option>
                  <option>Approve float32</option>
                  <option>Request changes and retest</option>
                </select>
              </label>
            </div>
            <div className="copy-row">
              <p>Copy this record, then paste it back into our chat to close M0.</p>
              <button onClick={copyResults}>Copy results</button>
              <span role="status">{copyStatus}</span>
            </div>
          </div>
        )}
      </section>
    </main>
  );
}
