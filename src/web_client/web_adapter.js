mergeInto(LibraryManager.library, {
  pf_web_set_status__deps: ["$UTF8ToString"],
  pf_web_set_status__sig: "vp",
  pf_web_set_status: function (messagePointer) {
    var status = document.getElementById("pf-status");

    if (!status) {
      status = document.createElement("pre");
      status.id = "pf-status";
      document.body.appendChild(status);
    }

    status.textContent = UTF8ToString(messagePointer);
  },

  pf_web_render_probe__sig: "ipppppi",
  pf_web_render_probe: function (
    clearPointer,
    positionsPointer,
    textureCoordinatesPointer,
    colorsPointer,
    texturePointer,
    vertexCount
  ) {
    var status = document.getElementById("pf-status");
    var canvas = document.getElementById("canvas");

    function fail(reason) {
      if (!status) {
        status = document.createElement("pre");
        status.id = "pf-status";
        document.body.appendChild(status);
      }
      status.textContent += " webgl2=fail reason=" + reason;
      status.dataset.webgl2 = "fail";
      return 0;
    }

    function compile(gl, type, source) {
      var shader = gl.createShader(type);
      gl.shaderSource(shader, source);
      gl.compileShader(shader);
      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        gl.deleteShader(shader);
        return null;
      }
      return shader;
    }

    if (!canvas) {
      canvas = document.createElement("canvas");
      canvas.id = "canvas";
      document.body.appendChild(canvas);
    }
    canvas.width = 128;
    canvas.height = 128;

    var gl = canvas.getContext("webgl2", {
      alpha: false,
      antialias: false,
      preserveDrawingBuffer: true,
    });
    if (!gl) {
      return fail("context");
    }

    var vertexShader = compile(
      gl,
      gl.VERTEX_SHADER,
      "#version 300 es\n" +
        "layout(location=0) in vec2 a_position;\n" +
        "layout(location=1) in vec2 a_uv;\n" +
        "layout(location=2) in vec4 a_color;\n" +
        "out vec2 v_uv;\n" +
        "out vec4 v_color;\n" +
        "void main(){v_uv=a_uv;v_color=a_color;" +
        "gl_Position=vec4(a_position,0.0,1.0);}\n"
    );
    var fragmentShader = compile(
      gl,
      gl.FRAGMENT_SHADER,
      "#version 300 es\n" +
        "precision mediump float;\n" +
        "in vec2 v_uv;\n" +
        "in vec4 v_color;\n" +
        "uniform sampler2D u_texture;\n" +
        "out vec4 output_color;\n" +
        "void main(){output_color=texture(u_texture,v_uv)*v_color;}\n"
    );
    if (!vertexShader || !fragmentShader) {
      return fail("shader");
    }

    var program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      gl.deleteProgram(program);
      return fail("link");
    }

    var positions = HEAPF32.subarray(
      positionsPointer >> 2,
      (positionsPointer >> 2) + vertexCount * 2
    );
    var textureCoordinates = HEAPF32.subarray(
      textureCoordinatesPointer >> 2,
      (textureCoordinatesPointer >> 2) + vertexCount * 2
    );
    var colors = HEAPF32.subarray(
      colorsPointer >> 2,
      (colorsPointer >> 2) + vertexCount * 4
    );
    var interleaved = new Float32Array(vertexCount * 8);
    var vertex;
    for (vertex = 0; vertex < vertexCount; ++vertex) {
      var output = vertex * 8;
      interleaved[output] = positions[vertex * 2];
      interleaved[output + 1] = positions[vertex * 2 + 1];
      interleaved[output + 2] = textureCoordinates[vertex * 2];
      // The packet defines SDL-style top-left UVs; WebGL texture V is bottom-up.
      interleaved[output + 3] = 1.0 - textureCoordinates[vertex * 2 + 1];
      interleaved[output + 4] = colors[vertex * 4];
      interleaved[output + 5] = colors[vertex * 4 + 1];
      interleaved[output + 6] = colors[vertex * 4 + 2];
      interleaved[output + 7] = colors[vertex * 4 + 3];
    }

    var vertexArray = gl.createVertexArray();
    var vertexBuffer = gl.createBuffer();
    gl.bindVertexArray(vertexArray);
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, interleaved, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 32, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 32, 8);
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(2, 4, gl.FLOAT, false, 32, 16);

    var texture = gl.createTexture();
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      2,
      2,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      HEAPU8.subarray(texturePointer, texturePointer + 16)
    );

    var clear = HEAPF32.subarray(clearPointer >> 2, (clearPointer >> 2) + 4);
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clearColor(clear[0], clear[1], clear[2], clear[3]);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.useProgram(program);
    gl.uniform1i(gl.getUniformLocation(program, "u_texture"), 0);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    gl.drawArrays(gl.TRIANGLES, 0, vertexCount);
    gl.finish();

    var pixel = new Uint8Array(4);
    gl.readPixels(
      canvas.width >> 1,
      canvas.height >> 1,
      1,
      1,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      pixel
    );

    gl.deleteTexture(texture);
    gl.deleteBuffer(vertexBuffer);
    gl.deleteVertexArray(vertexArray);
    gl.deleteProgram(program);

    if (pixel[0] <= 20 && pixel[1] <= 30 && pixel[2] <= 50) {
      return fail("pixel");
    }

    var webgpu =
      typeof navigator !== "undefined" && navigator.gpu
        ? "available"
        : "unavailable";
    status.textContent +=
      " webgl2=pass batch_draws=1 pixel=" +
      pixel[0] +
      "," +
      pixel[1] +
      "," +
      pixel[2] +
      "," +
      pixel[3] +
      " webgpu=" +
      webgpu;
    status.dataset.webgl2 = "pass";
    return 1;
  },

  pf_web_replay_inspector__deps: ["$UTF8ToString"],
  pf_web_replay_inspector__sig: "vppiiip",
  pf_web_replay_inspector: function (
    positionsPointer,
    hashesPointer,
    tickCount,
    playerCount,
    winnerMask,
    finalHashPointer
  ) {
    var checkpointCount = tickCount + 1;
    var positionCount = checkpointCount * playerCount * 2;
    var hashCount = checkpointCount * 32;
    var positions = new Int32Array(
      HEAP32.subarray(
        positionsPointer >> 2,
        (positionsPointer >> 2) + positionCount
      )
    );
    var hashes = new Uint8Array(
      HEAPU8.subarray(hashesPointer, hashesPointer + hashCount)
    );
    var finalHash = UTF8ToString(finalHashPointer);
    var status = document.getElementById("pf-status");
    var oldInspector = document.getElementById("pf-replay-inspector");

    if (oldInspector) {
      oldInspector.remove();
    }

    var style = document.getElementById("pf-replay-style");
    if (!style) {
      style = document.createElement("style");
      style.id = "pf-replay-style";
      style.textContent =
        "body{margin:0;padding:24px;background:#0a0d14;color:#e9eef8;" +
        "font:15px/1.5 ui-monospace,SFMono-Regular,Consolas,monospace}" +
        "#pf-runtime-summary{max-width:900px;margin:0 auto}" +
        "#pf-status{white-space:pre-wrap;color:#9fb0cb}" +
        "#canvas{width:96px;height:96px;image-rendering:pixelated;" +
        "border:1px solid #263249;border-radius:8px}" +
        "#pf-replay-inspector{max-width:900px;margin:24px auto 0;padding:24px;" +
        "background:#111827;border:1px solid #263249;border-radius:16px;" +
        "box-shadow:0 18px 50px #0008}" +
        "#pf-replay-inspector h1{font:700 24px/1.2 system-ui;margin:0 0 6px}" +
        "#pf-replay-inspector p{font-family:system-ui;color:#a9b6ca;" +
        "margin:0 0 18px}" +
        ".pf-badges{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:18px}" +
        ".pf-badge{padding:5px 9px;background:#18243a;border-radius:999px;" +
        "color:#b9cbeb;font-size:12px}" +
        "#pf-replay-canvas{display:block;width:100%;height:auto;" +
        "background:#090d16;border:1px solid #263249;border-radius:12px}" +
        "#pf-replay-slider{width:100%;margin:18px 0 8px;accent-color:#62d0ff}" +
        ".pf-row{display:flex;justify-content:space-between;gap:16px;" +
        "align-items:baseline}.pf-hash{overflow-wrap:anywhere;color:#88d9ff;" +
        "font-size:12px}.pf-help{font-size:13px!important;margin-top:14px!important}" +
        "@media(max-width:600px){body{padding:12px}" +
        "#pf-replay-inspector{padding:16px}.pf-row{align-items:flex-start;" +
        "flex-direction:column;gap:4px}}";
      document.head.appendChild(style);
    }

    var inspector = document.createElement("section");
    inspector.id = "pf-replay-inspector";
    var title = document.createElement("h1");
    title.textContent = "M2 deterministic replay inspector";
    inspector.appendChild(title);
    var summary = document.createElement("p");
    summary.textContent =
      "The same authored-C simulation generated, encoded, and verified this " +
      "four-player trace inside WebAssembly.";
    inspector.appendChild(summary);

    var badges = document.createElement("div");
    badges.className = "pf-badges";
    [
      "verified replay",
      tickCount + " ticks",
      playerCount + " players",
      "winner mask " + winnerMask,
      "60 Hz",
    ].forEach(function (label) {
      var badge = document.createElement("span");
      badge.className = "pf-badge";
      badge.textContent = label;
      badges.appendChild(badge);
    });
    inspector.appendChild(badges);

    var canvas = document.createElement("canvas");
    canvas.id = "pf-replay-canvas";
    canvas.width = 840;
    canvas.height = 400;
    inspector.appendChild(canvas);

    var slider = document.createElement("input");
    slider.id = "pf-replay-slider";
    slider.type = "range";
    slider.min = "0";
    slider.max = String(tickCount);
    slider.value = "0";
    slider.step = "1";
    slider.setAttribute("aria-label", "Replay tick");
    inspector.appendChild(slider);

    var row = document.createElement("div");
    row.className = "pf-row";
    var tickLabel = document.createElement("strong");
    var positionLabel = document.createElement("span");
    row.appendChild(tickLabel);
    row.appendChild(positionLabel);
    inspector.appendChild(row);

    var hashLabel = document.createElement("div");
    hashLabel.className = "pf-hash";
    inspector.appendChild(hashLabel);
    var help = document.createElement("p");
    help.className = "pf-help";
    help.textContent =
      "Drag the timeline. Trails and the per-tick SHA-256 state hash are " +
      "derived from the verified replay; player 3 forfeits on the final tick.";
    inspector.appendChild(help);
    document.body.appendChild(inspector);

    function hexHash(tick) {
      var output = "";
      var offset = tick * 32;
      var index;
      for (index = 0; index < 32; ++index) {
        output += hashes[offset + index].toString(16).padStart(2, "0");
      }
      return output;
    }

    function draw(tick) {
      var context = canvas.getContext("2d");
      var colors = ["#62d0ff", "#ff6b86", "#8ee28d", "#ffd166"];
      var arenaLeft = 52;
      var arenaRight = canvas.width - 52;
      var arenaTop = 40;
      var arenaBottom = canvas.height - 54;
      var player;

      context.clearRect(0, 0, canvas.width, canvas.height);
      context.fillStyle = "#090d16";
      context.fillRect(0, 0, canvas.width, canvas.height);
      context.strokeStyle = "#27344a";
      context.lineWidth = 2;
      context.strokeRect(
        arenaLeft,
        arenaTop,
        arenaRight - arenaLeft,
        arenaBottom - arenaTop
      );
      context.strokeStyle = "#41516c";
      context.beginPath();
      context.moveTo(arenaLeft, arenaBottom);
      context.lineTo(arenaRight, arenaBottom);
      context.stroke();

      function screenPosition(checkpoint, slot) {
        var offset = (checkpoint * playerCount + slot) * 2;
        var x = positions[offset] / 65536;
        var y = positions[offset + 1] / 65536;
        return {
          x: arenaLeft + ((x + 64) / 128) * (arenaRight - arenaLeft),
          y: arenaBottom - (y / 64) * (arenaBottom - arenaTop),
          worldX: x,
          worldY: y,
        };
      }

      for (player = 0; player < playerCount; ++player) {
        var trailStart = Math.max(0, tick - 45);
        var checkpoint;
        context.strokeStyle = colors[player] + "88";
        context.lineWidth = 2;
        context.beginPath();
        for (
          checkpoint = trailStart;
          checkpoint <= tick;
          ++checkpoint
        ) {
          var trailPosition = screenPosition(checkpoint, player);
          if (checkpoint === trailStart) {
            context.moveTo(trailPosition.x, trailPosition.y);
          } else {
            context.lineTo(trailPosition.x, trailPosition.y);
          }
        }
        context.stroke();

        var current = screenPosition(tick, player);
        context.fillStyle = colors[player];
        context.beginPath();
        context.arc(current.x, current.y, 10, 0, Math.PI * 2);
        context.fill();
        context.fillStyle = "#07101a";
        context.font = "bold 12px system-ui";
        context.textAlign = "center";
        context.textBaseline = "middle";
        context.fillText(String(player), current.x, current.y);
      }

      var selected = screenPosition(tick, 0);
      tickLabel.textContent = "Tick " + tick + " / " + tickCount;
      positionLabel.textContent =
        "P0 x=" +
        selected.worldX.toFixed(3) +
        " y=" +
        selected.worldY.toFixed(3);
      hashLabel.textContent = "state sha256: " + hexHash(tick);
    }

    slider.addEventListener("input", function () {
      draw(Number(slider.value));
    });
    draw(0);

    if (status) {
      status.textContent +=
        " replay=pass ticks=" +
        tickCount +
        " winner_mask=" +
        winnerMask +
        " final_sha256=" +
        finalHash;
      status.dataset.replay = "pass";
    }
  },

  pf_web_m4_playtest_install__sig: "viiii",
  pf_web_m4_playtest_install: function (
    walkAxis,
    dashAxis,
    inputProbePassed,
    combatProbePassed
  ) {
    var status = document.getElementById("pf-status");
    var replayInspector = document.getElementById("pf-replay-inspector");
    var previous = document.getElementById("pf-m4-playtest");

    if (previous) {
      previous.remove();
    }

    var style = document.getElementById("pf-m4-playtest-style");
    if (!style) {
      style = document.createElement("style");
      style.id = "pf-m4-playtest-style";
      style.textContent =
        "#pf-m4-playtest{max-width:1100px;margin:24px auto 0;padding:24px;" +
        "background:linear-gradient(145deg,#111b2b,#0d1421);" +
        "border:1px solid #2d405e;border-radius:18px;" +
        "box-shadow:0 22px 60px #0009;font-family:system-ui,sans-serif}" +
        ".pf-m4-heading{display:flex;justify-content:space-between;gap:20px;" +
        "align-items:flex-start;margin-bottom:18px}" +
        ".pf-m4-heading h1{font-size:25px;line-height:1.15;margin:0 0 7px}" +
        ".pf-m4-heading p{color:#a9b7ca;margin:0;max-width:720px}" +
        ".pf-m4-live{background:#123b32;color:#8ff3cf;border:1px solid #276b59;" +
        "border-radius:999px;font:700 11px/1 ui-monospace,monospace;" +
        "letter-spacing:.08em;padding:8px 11px;white-space:nowrap}" +
        "#pf-m4-canvas{display:block;width:100%;height:auto;aspect-ratio:16/8;" +
        "background:#07101a;border:1px solid #2b3d58;border-radius:14px}" +
        ".pf-m4-toolbar{display:flex;align-items:center;gap:10px;" +
        "flex-wrap:wrap;margin:14px 0}" +
        ".pf-m4-toolbar button{background:#182841;color:#e8f1ff;" +
        "border:1px solid #385274;border-radius:9px;padding:8px 13px;" +
        "font:700 12px/1 system-ui;cursor:pointer}" +
        ".pf-m4-toolbar button:hover{background:#203654}" +
        ".pf-m4-tick{color:#8edcff;font:12px/1 ui-monospace,monospace;" +
        "margin-left:auto}" +
        ".pf-m4-controls{display:grid;grid-template-columns:1fr 1fr;" +
        "gap:12px;margin:12px 0}" +
        ".pf-m4-control-card{background:#0b1320;border:1px solid #263b58;" +
        "border-radius:12px;padding:13px;color:#adbbce;font-size:13px}" +
        ".pf-m4-control-card strong{color:#f3f7ff;display:block;margin-bottom:6px}" +
        ".pf-m4-control-card kbd{background:#1d2b40;border:1px solid #425875;" +
        "border-bottom-width:2px;border-radius:5px;color:#dcecff;" +
        "font:11px/1 ui-monospace,monospace;padding:2px 5px}" +
        ".pf-m4-state-grid{display:grid;grid-template-columns:1fr 1fr;" +
        "gap:12px;margin-top:12px}" +
        ".pf-m4-state{background:#0a111d;border:1px solid #263851;" +
        "border-radius:10px;padding:10px 12px;font:12px/1.5 ui-monospace,monospace;" +
        "color:#9fb0c7}.pf-m4-state strong{color:#edf5ff}" +
        ".pf-m4-note{color:#8294ad;font-size:12px;margin:13px 0 0}" +
        "@media(max-width:680px){.pf-m4-heading{flex-direction:column}" +
        ".pf-m4-controls,.pf-m4-state-grid{grid-template-columns:1fr}" +
        ".pf-m4-tick{margin-left:0;width:100%}" +
        "#pf-m4-playtest{padding:16px}}";
      document.head.appendChild(style);
    }

    var section = document.createElement("section");
    section.id = "pf-m4-playtest";
    section.dataset.ready = "true";
    section.setAttribute("aria-label", "M4 movement and combat playtest");

    var heading = document.createElement("div");
    heading.className = "pf-m4-heading";
    var headingCopy = document.createElement("div");
    var title = document.createElement("h1");
    title.textContent = "M4 real-simulation browser playtest";
    var subtitle = document.createElement("p");
    subtitle.textContent =
      "Two keyboard players drive the same deterministic Q16.16 simulation " +
      "used by native, replay, rollback, and headless execution. Active attack " +
      "hitboxes are drawn over the production collision state.";
    headingCopy.appendChild(title);
    headingCopy.appendChild(subtitle);
    var live = document.createElement("span");
    live.className = "pf-m4-live";
    live.textContent =
      inputProbePassed && combatProbePassed
        ? "INPUT + COMBAT PROBES PASSED"
        : "RUNTIME PROBE FAILED";
    heading.appendChild(headingCopy);
    heading.appendChild(live);
    section.appendChild(heading);

    var canvas = document.createElement("canvas");
    canvas.id = "pf-m4-canvas";
    canvas.width = 960;
    canvas.height = 480;
    canvas.setAttribute("aria-label", "Live deterministic combat stage");
    section.appendChild(canvas);

    var toolbar = document.createElement("div");
    toolbar.className = "pf-m4-toolbar";
    var pauseButton = document.createElement("button");
    pauseButton.type = "button";
    pauseButton.textContent = "Pause";
    pauseButton.setAttribute("aria-pressed", "false");
    var stepButton = document.createElement("button");
    stepButton.type = "button";
    stepButton.textContent = "Step";
    var resetButton = document.createElement("button");
    resetButton.type = "button";
    resetButton.textContent = "Reset";
    var tickLabel = document.createElement("span");
    tickLabel.className = "pf-m4-tick";
    tickLabel.textContent = "tick 0 · fixed 60 Hz";
    toolbar.appendChild(pauseButton);
    toolbar.appendChild(stepButton);
    toolbar.appendChild(resetButton);
    toolbar.appendChild(tickLabel);
    section.appendChild(toolbar);

    var controls = document.createElement("div");
    controls.className = "pf-m4-controls";
    function controlCard(player, bindings) {
      var card = document.createElement("div");
      card.className = "pf-m4-control-card";
      var label = document.createElement("strong");
      label.textContent = player;
      card.appendChild(label);
      card.appendChild(document.createTextNode(bindings));
      return card;
    }
    controls.appendChild(
      controlCard(
        "Player 1",
        "A / D dash · Shift + A / D walk · W or Space jump · F attack · S down / fast fall"
      )
    );
    controls.appendChild(
      controlCard(
        "Player 2",
        "← / → dash · Shift + ← / → walk · ↑ jump · / or Numpad 0 attack · ↓ down / fast fall"
      )
    );
    section.appendChild(controls);

    var note = document.createElement("p");
    note.className = "pf-m4-note";
    note.textContent =
      "Tap jump and release during the three-tick jump squat for the fixed " +
      "short hop; hold through takeoff for the fixed full hop. Releasing after " +
      "takeoff never changes either apex. Tap opposite full directions during " +
      "initial dash to dash-dance; after the state reaches RUN, the same reversal " +
      "enters RUN TURNAROUND instead. Fall beside a ledge while facing inward " +
      "to grab it; after the catch, press inward to climb, down or away to " +
      "release, or jump to ledge-jump. F and / perform the first data-driven " +
      "ground attack; translucent boxes show its active frames. R resets, P " +
      "pauses, and N single-steps.";
    section.appendChild(note);

    var stateGrid = document.createElement("div");
    stateGrid.className = "pf-m4-state-grid";
    var playerStates = [];
    [0, 1].forEach(function (player) {
      var card = document.createElement("div");
      card.className = "pf-m4-state";
      card.id = "pf-m4-player-" + player;
      card.textContent = "P" + (player + 1) + " waiting for first state";
      stateGrid.appendChild(card);
      playerStates.push(card);
    });
    section.appendChild(stateGrid);

    if (replayInspector) {
      document.body.insertBefore(section, replayInspector);
    } else {
      document.body.appendChild(section);
    }

    var state = {
      accumulator: 0,
      canvas: canvas,
      dashAxis: dashAxis,
      keys: Object.create(null),
      lastTime: 0,
      latest: null,
      pauseButton: pauseButton,
      playerStates: playerStates,
      attackQueued: [false, false],
      jumpQueued: [false, false],
      running: true,
      tickLabel: tickLabel,
      walkAxis: walkAxis,
    };
    Module.pfM4Playtest = state;

    function held(code) {
      return state.keys[code] === true;
    }

    function horizontal(negative, positive) {
      if (held(negative) === held(positive)) {
        return 0;
      }
      var magnitude =
        held("ShiftLeft") || held("ShiftRight")
          ? state.walkAxis
          : state.dashAxis;
      return held(negative) ? -magnitude : magnitude;
    }

    function step() {
      var player0Jump =
        held("KeyW") || held("Space") || state.jumpQueued[0];
      var player1Jump =
        held("ArrowUp") || state.jumpQueued[1];
      var player0Attack =
        held("KeyF") || state.attackQueued[0];
      var player1Attack =
        held("Slash") || held("Numpad0") || state.attackQueued[1];
      var passed = Module._pf_web_m4_playtest_step(
        horizontal("KeyA", "KeyD"),
        held("KeyS") ? state.dashAxis : 0,
        player0Jump ? 1 : 0,
        player0Attack ? 1 : 0,
        horizontal("ArrowLeft", "ArrowRight"),
        held("ArrowDown") ? state.dashAxis : 0,
        player1Jump ? 1 : 0,
        player1Attack ? 1 : 0
      );
      state.jumpQueued[0] = false;
      state.jumpQueued[1] = false;
      state.attackQueued[0] = false;
      state.attackQueued[1] = false;
      if (!passed) {
        state.running = false;
        state.pauseButton.textContent = "Resume";
        state.pauseButton.setAttribute("aria-pressed", "true");
        if (status) {
          status.textContent += " playtest_runtime=fail";
          status.dataset.playtestRuntime = "fail";
        }
      }
    }

    function setRunning(running) {
      state.running = running;
      state.pauseButton.textContent = running ? "Pause" : "Resume";
      state.pauseButton.setAttribute("aria-pressed", running ? "false" : "true");
      state.accumulator = 0;
      state.lastTime = 0;
    }

    function reset() {
      state.keys = Object.create(null);
      state.jumpQueued = [false, false];
      state.attackQueued = [false, false];
      state.accumulator = 0;
      Module._pf_web_m4_playtest_reset();
    }

    function frame(time) {
      if (state.running) {
        if (!state.lastTime) {
          state.lastTime = time;
        }
        var elapsed = Math.min(time - state.lastTime, 100);
        state.lastTime = time;
        state.accumulator += elapsed;
        while (state.accumulator >= 1000 / 60) {
          step();
          state.accumulator -= 1000 / 60;
        }
      } else {
        state.lastTime = time;
      }
      requestAnimationFrame(frame);
    }

    pauseButton.addEventListener("click", function () {
      setRunning(!state.running);
    });
    stepButton.addEventListener("click", function () {
      if (!state.running) {
        step();
      }
    });
    resetButton.addEventListener("click", reset);

    window.addEventListener(
      "keydown",
      function (event) {
        var wasHeld = held(event.code);
        if (
          event.code === "Space" ||
          event.code === "Slash" ||
          event.code === "Numpad0" ||
          event.code.indexOf("Arrow") === 0
        ) {
          event.preventDefault();
        }
        state.keys[event.code] = true;
        if (!wasHeld && (event.code === "KeyW" || event.code === "Space")) {
          state.jumpQueued[0] = true;
        }
        if (!wasHeld && event.code === "ArrowUp") {
          state.jumpQueued[1] = true;
        }
        if (!wasHeld && event.code === "KeyF") {
          state.attackQueued[0] = true;
        }
        if (
          !wasHeld &&
          (event.code === "Slash" || event.code === "Numpad0")
        ) {
          state.attackQueued[1] = true;
        }
        if (event.repeat) {
          return;
        }
        if (event.code === "KeyR") {
          reset();
        } else if (event.code === "KeyP") {
          setRunning(!state.running);
        } else if (event.code === "KeyN" && !state.running) {
          step();
        }
      },
      { passive: false }
    );
    window.addEventListener("keyup", function (event) {
      state.keys[event.code] = false;
    });
    window.addEventListener("blur", function () {
      state.keys = Object.create(null);
      state.jumpQueued = [false, false];
      state.attackQueued = [false, false];
    });

    if (status) {
      status.textContent +=
        " playtest=ready input_probe=" +
        (inputProbePassed ? "pass" : "fail") +
        " combat_probe=" +
        (combatProbePassed ? "pass" : "fail") +
        " controls=keyboard-two-player";
      status.dataset.playtest = "ready";
      status.dataset.inputProbe = inputProbePassed ? "pass" : "fail";
      status.dataset.combatProbe = combatProbePassed ? "pass" : "fail";
    }
    requestAnimationFrame(frame);
  },

  pf_web_m4_playtest_render__sig: "vpi",
  pf_web_m4_playtest_render: function (viewPointer, viewCount) {
    var state = Module.pfM4Playtest;
    if (!state || viewCount !== 54) {
      return;
    }
    state.latest = new Int32Array(
      HEAP32.subarray(viewPointer >> 2, (viewPointer >> 2) + viewCount)
    );

    var view = state.latest;
    var canvas = state.canvas;
    var context = canvas.getContext("2d");
    var q16 = 65536;
    var blastLeft = view[8] / q16;
    var blastRight = view[9] / q16;
    var blastTop = view[10] / q16;
    var blastBottom = view[11] / q16;
    var padding = 34;
    var usableWidth = canvas.width - padding * 2;
    var usableHeight = canvas.height - padding * 2;
    var colors = ["#55e6d0", "#ff7695"];
    var actionNames = [
      "IDLE",
      "WALK",
      "INITIAL DASH",
      "RUN",
      "CROUCH",
      "JUMP SQUAT",
      "AIRBORNE",
      "LANDING",
      "LEDGE HANG",
      "LEDGE CLIMB",
      "RUN TURNAROUND",
      "RUN BRAKE",
      "GROUND ATTACK",
      "HITLAG",
      "HITSTUN",
    ];

    function sx(q16Value) {
      return (
        padding +
        ((q16Value / q16 - blastLeft) / (blastRight - blastLeft)) *
          usableWidth
      );
    }
    function sy(q16Value) {
      return (
        padding +
        ((q16Value / q16 - blastTop) / (blastBottom - blastTop)) *
          usableHeight
      );
    }

    var gradient = context.createLinearGradient(0, 0, 0, canvas.height);
    gradient.addColorStop(0, "#111f34");
    gradient.addColorStop(1, "#07101a");
    context.fillStyle = gradient;
    context.fillRect(0, 0, canvas.width, canvas.height);

    context.setLineDash([7, 8]);
    context.strokeStyle = "#2b4665";
    context.lineWidth = 1;
    context.strokeRect(
      sx(view[8]),
      sy(view[10]),
      sx(view[9]) - sx(view[8]),
      sy(view[11]) - sy(view[10])
    );
    context.setLineDash([]);

    context.strokeStyle = "#8ea2b7";
    context.lineCap = "round";
    context.lineWidth = 8;
    context.beginPath();
    context.moveTo(sx(view[2]), sy(view[4]));
    context.lineTo(sx(view[3]), sy(view[4]));
    context.stroke();

    context.strokeStyle = "#d1dae4";
    context.lineWidth = 5;
    context.beginPath();
    context.moveTo(sx(view[5]), sy(view[7]));
    context.lineTo(sx(view[6]), sy(view[7]));
    context.stroke();

    [0, 1].forEach(function (playerIndex) {
      var base = 14 + playerIndex * 20;
      var x = sx(view[base]);
      var y = sy(view[base + 1]);
      var halfWidth =
        (view[12] / q16 / (blastRight - blastLeft)) * usableWidth;
      var halfHeight =
        (view[13] / q16 / (blastBottom - blastTop)) * usableHeight;
      var width = Math.max(14, halfWidth * 2);
      var height = Math.max(28, halfHeight * 2);
      var facing = view[base + 5];

      if (view[base + 14]) {
        var hitboxLeft = sx(view[base + 15]);
        var hitboxRight = sx(view[base + 16]);
        var hitboxTop = sy(view[base + 17]);
        var hitboxBottom = sy(view[base + 18]);

        context.fillStyle = "#ffb34744";
        context.strokeStyle = "#ffd089";
        context.lineWidth = 2;
        context.fillRect(
          hitboxLeft,
          hitboxTop,
          hitboxRight - hitboxLeft,
          hitboxBottom - hitboxTop
        );
        context.strokeRect(
          hitboxLeft,
          hitboxTop,
          hitboxRight - hitboxLeft,
          hitboxBottom - hitboxTop
        );
      }

      context.shadowColor = colors[playerIndex] + "88";
      context.shadowBlur = 16;
      context.fillStyle =
        view[base + 4] === 13 ? "#ffffff" : colors[playerIndex];
      context.fillRect(x - width / 2, y - height / 2, width, height);
      context.shadowBlur = 0;
      context.fillStyle = "#07111c";
      context.beginPath();
      context.moveTo(x + facing * width * 0.45, y - 5);
      context.lineTo(x + facing * width * 0.75, y);
      context.lineTo(x + facing * width * 0.45, y + 5);
      context.closePath();
      context.fill();

      var action =
        actionNames[view[base + 4]] || "STATE " + view[base + 4];
      state.playerStates[playerIndex].innerHTML =
        "<strong>P" +
        (playerIndex + 1) +
        " · " +
        action +
        "</strong><br>x " +
        (view[base] / q16).toFixed(3) +
        " · y " +
        (view[base + 1] / q16).toFixed(3) +
        " · vx " +
        (view[base + 2] / q16).toFixed(3) +
        " · vy " +
        (view[base + 3] / q16).toFixed(3) +
        "<br>grounded " +
        view[base + 6] +
        " · support " +
        view[base + 7] +
        " · air jumps " +
        view[base + 8] +
        " · fast fall " +
        view[base + 9] +
        " · respawns " +
        view[base + 10] +
        "<br>damage " +
        (view[base + 11] / q16).toFixed(1) +
        "% · hitlag " +
        view[base + 12] +
        " · hitstun " +
        view[base + 13] +
        " · hit event " +
        view[base + 19];
    });

    context.fillStyle = "#8da2bb";
    context.font = "12px ui-monospace, monospace";
    context.textAlign = "left";
    context.fillText("blast zone", padding + 8, padding + 18);
    context.textAlign = "right";
    context.fillText("real sim · Q16.16 · 60 Hz", canvas.width - padding, 22);

    state.tickLabel.textContent = "tick " + view[1] + " · fixed 60 Hz";
  },
});
