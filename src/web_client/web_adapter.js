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

  pf_web_m4_playtest_install__sig: "viiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
  pf_web_m4_playtest_install: function (
    walkAxis,
    dashAxis,
    inputProbePassed,
    airFacingProbePassed,
    instantDoubleJumpProbePassed,
    doubleJumpCancelProbePassed,
    doubleJumpCancelCounterProbePassed,
    batDropProbePassed,
    glideTossProbePassed,
    jumpCancelThrowProbePassed,
    jumpCancelProbePassed,
    edgeHopProbePassed,
    edgeDashProbePassed,
    foxTrotProbePassed,
    moonwalkProbePassed,
    teeterCancelProbePassed,
    stageHumpingProbePassed,
    tauntCancelProbePassed,
    scarJumpProbePassed,
    teamWobbleProbePassed,
    pivotProbePassed,
    dashCancelProbePassed,
    dashingShieldProbePassed,
    shieldPlatformDropProbePassed,
    smallStepForwardSmashProbePassed,
    dropCancelProbePassed,
    vCancelProbePassed,
    approachProbePassed,
    spacingProbePassed,
    sharkingProbePassed,
    crossUpProbePassed,
    mindgameProbePassed,
    jugglingProbePassed,
    ladderProbePassed,
    killConfirmProbePassed,
    zeroToDeathProbePassed,
    ledgeCancelProbePassed,
    plankingProbePassed,
    jumpCancelledGrabProbePassed,
    boostGrabProbePassed,
    jabCancelProbePassed,
    jabResetProbePassed,
    chainGrabProbePassed,
    combatProbePassed,
    reactionProbePassed,
    shieldProbePassed,
    shieldBreakProbePassed,
    tumbleProbePassed,
    floorRecoveryProbePassed,
    techChaseProbePassed,
    surfaceTechProbePassed,
    airDodgeProbePassed,
    groundDodgeProbePassed,
    aerialLCancelProbePassed,
    matchProbePassed,
    shortHopLaserProbePassed,
    campingProbePassed,
    shineSpikeProbePassed,
    chargeStorageProbePassed,
    aerialLandingLagTicks,
    strongAerialLandingLagTicks
  ) {
    function emptyGamepadInput() {
      return {
        horizontal: 0,
        vertical: 0,
        jump: false,
        attack: false,
        strongAttack: false,
        special: false,
        taunt: false,
        shield: false,
      };
    }

    function gamepadButtonPressed(gamepad, index) {
      var button =
        gamepad && gamepad.buttons && index < gamepad.buttons.length
          ? gamepad.buttons[index]
          : null;
      return (
        button !== null &&
        button !== undefined &&
        (button.pressed === true || Number(button.value) >= 0.5)
      );
    }

    function gamepadAxis(gamepad, index) {
      var value =
        gamepad && gamepad.axes && index < gamepad.axes.length
          ? Number(gamepad.axes[index])
          : 0;
      if (!Number.isFinite(value) || Math.abs(value) < 0.2) {
        return 0;
      }
      value = Math.max(-1, Math.min(1, value));
      var magnitude = Math.round(Math.abs(value) * dashAxis);
      return value < 0 ? -magnitude : magnitude;
    }

    function mapStandardGamepad(gamepad) {
      var input = emptyGamepadInput();
      if (
        !gamepad ||
        gamepad.connected === false ||
        gamepad.mapping !== "standard"
      ) {
        return input;
      }

      input.horizontal = gamepadAxis(gamepad, 0);
      input.vertical = gamepadAxis(gamepad, 1);
      var dpadUp = gamepadButtonPressed(gamepad, 12);
      var dpadDown = gamepadButtonPressed(gamepad, 13);
      var dpadLeft = gamepadButtonPressed(gamepad, 14);
      var dpadRight = gamepadButtonPressed(gamepad, 15);
      if (dpadLeft || dpadRight) {
        input.horizontal =
          dpadLeft === dpadRight ? 0 : dpadLeft ? -dashAxis : dashAxis;
      }
      if (dpadUp || dpadDown) {
        input.vertical =
          dpadUp === dpadDown ? 0 : dpadUp ? -dashAxis : dashAxis;
      }
      input.attack = gamepadButtonPressed(gamepad, 0);
      input.strongAttack = gamepadButtonPressed(gamepad, 1);
      input.jump = gamepadButtonPressed(gamepad, 2);
      input.special = gamepadButtonPressed(gamepad, 3);
      input.taunt = gamepadButtonPressed(gamepad, 8);
      input.shield =
        gamepadButtonPressed(gamepad, 4) ||
        gamepadButtonPressed(gamepad, 5) ||
        gamepadButtonPressed(gamepad, 6) ||
        gamepadButtonPressed(gamepad, 7);
      return input;
    }

    function collectStandardGamepads(gamepads) {
      var result = {
        connected: 0,
        inputs: [emptyGamepadInput(), emptyGamepadInput()],
      };
      var index;
      for (
        index = 0;
        gamepads && index < gamepads.length && result.connected < 2;
        ++index
      ) {
        var gamepad = gamepads[index];
        if (
          gamepad &&
          gamepad.connected !== false &&
          gamepad.mapping === "standard"
        ) {
          result.inputs[result.connected] = mapStandardGamepad(gamepad);
          ++result.connected;
        }
      }
      return result;
    }

    function pollStandardGamepads() {
      if (
        typeof navigator === "undefined" ||
        typeof navigator.getGamepads !== "function"
      ) {
        return collectStandardGamepads([]);
      }
      try {
        return collectStandardGamepads(navigator.getGamepads());
      } catch (error) {
        return collectStandardGamepads([]);
      }
    }

    function runGamepadMappingProbe() {
      function buttons() {
        return Array.from({ length: 17 }, function () {
          return { pressed: false, value: 0 };
        });
      }

      var analogButtons = buttons();
      analogButtons[0] = { pressed: true, value: 1 };
      analogButtons[2] = { pressed: true, value: 1 };
      analogButtons[6] = { pressed: false, value: 0.75 };
      analogButtons[8] = { pressed: true, value: 1 };
      var analog = {
        connected: true,
        mapping: "standard",
        axes: [0.5, -0.25],
        buttons: analogButtons,
      };
      var dpadButtons = buttons();
      dpadButtons[1] = { pressed: true, value: 1 };
      dpadButtons[3] = { pressed: true, value: 1 };
      dpadButtons[12] = { pressed: true, value: 1 };
      dpadButtons[15] = { pressed: true, value: 1 };
      var dpad = {
        connected: true,
        mapping: "standard",
        axes: [0.1, 0.1],
        buttons: dpadButtons,
      };
      var ignored = {
        connected: true,
        mapping: "",
        axes: [1, 1],
        buttons: dpadButtons,
      };
      var centered = mapStandardGamepad({
        connected: true,
        mapping: "standard",
        axes: [0.1, -0.1],
        buttons: buttons(),
      });
      var result = collectStandardGamepads([ignored, analog, null, dpad]);
      return (
        centered.horizontal === 0 &&
        centered.vertical === 0 &&
        result.connected === 2 &&
        result.inputs[0].horizontal === Math.round(dashAxis * 0.5) &&
        result.inputs[0].vertical === -Math.round(dashAxis * 0.25) &&
        result.inputs[0].attack &&
        !result.inputs[0].strongAttack &&
        result.inputs[0].jump &&
        !result.inputs[0].special &&
        result.inputs[0].taunt &&
        result.inputs[0].shield &&
        result.inputs[1].horizontal === dashAxis &&
        result.inputs[1].vertical === -dashAxis &&
        !result.inputs[1].attack &&
        result.inputs[1].strongAttack &&
        !result.inputs[1].jump &&
        result.inputs[1].special &&
        !result.inputs[1].taunt &&
        !result.inputs[1].shield
      );
    }

    var gamepadApiAvailable =
      typeof navigator !== "undefined" &&
      typeof navigator.getGamepads === "function";
    var gamepadProbePassed = runGamepadMappingProbe();
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
        ".pf-m4-gamepads{color:#a9b7ca;font:12px/1 ui-monospace,monospace}" +
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
        ".pf-m4-item-state,.pf-m4-projectile-state{grid-column:1/-1}" +
        ".pf-m4-event-panel{grid-column:1/-1}.pf-m4-event-help{" +
        "color:#7287a4;margin:3px 0 8px}.pf-m4-event-feed{" +
        "list-style:none;padding:0;margin:0;display:grid;gap:5px}" +
        ".pf-m4-event-feed li{display:grid;grid-template-columns:112px 1fr;" +
        "gap:10px;padding:6px 8px;border-left:3px solid #3a587d;" +
        "background:#101a29;color:#bed0e8}.pf-m4-event-feed code{" +
        "color:#75dfff}.pf-m4-event-empty{color:#7287a4!important}" +
        ".pf-m4-note{color:#8294ad;font-size:12px;margin:13px 0 0}" +
        "@media(max-width:680px){.pf-m4-heading{flex-direction:column}" +
        ".pf-m4-controls,.pf-m4-state-grid{grid-template-columns:1fr}" +
        ".pf-m4-tick{margin-left:0;width:100%}" +
        "#pf-m4-playtest{padding:16px}}";
      document.head.appendChild(style);
    }

    var section = document.createElement("section");
    section.id = "pf-m4-playtest";
    section.dataset.ready =
      gamepadApiAvailable && gamepadProbePassed ? "true" : "false";
    section.dataset.gamepadProbe = gamepadProbePassed ? "pass" : "fail";
    section.dataset.gamepadApi =
      gamepadApiAvailable ? "available" : "unavailable";
    section.dataset.teamLab = "inactive";
    section.dataset.batDropProbe = batDropProbePassed ? "pass" : "fail";
    section.dataset.glideTossProbe = glideTossProbePassed ? "pass" : "fail";
    section.dataset.jumpCancelThrowProbe =
      jumpCancelThrowProbePassed ? "pass" : "fail";
    section.dataset.jumpCancelProbe =
      jumpCancelProbePassed ? "pass" : "fail";
    section.dataset.shortHopLaserProbe =
      shortHopLaserProbePassed ? "pass" : "fail";
    section.dataset.campingProbe = campingProbePassed ? "pass" : "fail";
    section.dataset.shineSpikeProbe = shineSpikeProbePassed ? "pass" : "fail";
    section.dataset.chargeStorageProbe =
      chargeStorageProbePassed ? "pass" : "fail";
    section.dataset.moonwalkProbe = moonwalkProbePassed ? "pass" : "fail";
    section.dataset.teeterCancelProbe =
      teeterCancelProbePassed ? "pass" : "fail";
    section.dataset.stageHumpingProbe =
      stageHumpingProbePassed ? "pass" : "fail";
    section.dataset.tauntCancelProbe =
      tauntCancelProbePassed ? "pass" : "fail";
    section.dataset.scarJumpProbe = scarJumpProbePassed ? "pass" : "fail";
    section.dataset.teamWobbleProbe =
      teamWobbleProbePassed ? "pass" : "fail";
    section.setAttribute("aria-label", "M4 movement and combat playtest");

    var heading = document.createElement("div");
    heading.className = "pf-m4-heading";
    var headingCopy = document.createElement("div");
    var title = document.createElement("h1");
    title.textContent = "M4 real-simulation browser playtest";
    var subtitle = document.createElement("p");
    subtitle.textContent =
      "Keyboard and up to two Standard Gamepads drive the same deterministic " +
      "Q16.16 simulation used by native, replay, rollback, and headless " +
      "execution. Active attack hitboxes are drawn over the production " +
      "collision state.";
    headingCopy.appendChild(title);
    headingCopy.appendChild(subtitle);
    var live = document.createElement("span");
    live.className = "pf-m4-live";
    live.textContent =
      inputProbePassed &&
      airFacingProbePassed &&
      instantDoubleJumpProbePassed &&
      doubleJumpCancelProbePassed &&
      doubleJumpCancelCounterProbePassed &&
      batDropProbePassed &&
      glideTossProbePassed &&
      jumpCancelThrowProbePassed &&
      jumpCancelProbePassed &&
      edgeHopProbePassed &&
      edgeDashProbePassed &&
      foxTrotProbePassed &&
      moonwalkProbePassed &&
      teeterCancelProbePassed &&
      stageHumpingProbePassed &&
      tauntCancelProbePassed &&
      scarJumpProbePassed &&
      teamWobbleProbePassed &&
      pivotProbePassed &&
      dashCancelProbePassed &&
      dashingShieldProbePassed &&
      shieldPlatformDropProbePassed &&
      smallStepForwardSmashProbePassed &&
      dropCancelProbePassed &&
      vCancelProbePassed &&
      approachProbePassed &&
      spacingProbePassed &&
      sharkingProbePassed &&
      crossUpProbePassed &&
      mindgameProbePassed &&
      jugglingProbePassed &&
      ladderProbePassed &&
      killConfirmProbePassed &&
      zeroToDeathProbePassed &&
      ledgeCancelProbePassed &&
      plankingProbePassed &&
      jumpCancelledGrabProbePassed &&
      boostGrabProbePassed &&
      jabCancelProbePassed &&
      chainGrabProbePassed &&
      combatProbePassed &&
      reactionProbePassed &&
      shieldProbePassed &&
      shieldBreakProbePassed &&
      tumbleProbePassed &&
      floorRecoveryProbePassed &&
      techChaseProbePassed &&
      surfaceTechProbePassed &&
      airDodgeProbePassed &&
      groundDodgeProbePassed &&
      aerialLCancelProbePassed &&
      matchProbePassed &&
      shortHopLaserProbePassed &&
      campingProbePassed &&
      shineSpikeProbePassed &&
      chargeStorageProbePassed &&
      gamepadApiAvailable &&
      gamepadProbePassed
        ? "ALL M4 INPUT + GAMEPAD + COMBAT PROBES PASSED"
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
    var teamLabButton = document.createElement("button");
    teamLabButton.type = "button";
    teamLabButton.textContent = "Team Wobble Lab";
    teamLabButton.setAttribute("aria-pressed", "false");
    var tickLabel = document.createElement("span");
    tickLabel.className = "pf-m4-tick";
    tickLabel.textContent = "tick 0 · fixed 60 Hz";
    var gamepadLabel = document.createElement("span");
    gamepadLabel.className = "pf-m4-gamepads";
    gamepadLabel.textContent = gamepadApiAvailable
      ? "standard gamepads 0/2"
      : "gamepad API unavailable";
    toolbar.appendChild(pauseButton);
    toolbar.appendChild(stepButton);
    toolbar.appendChild(resetButton);
    toolbar.appendChild(teamLabButton);
    toolbar.appendChild(gamepadLabel);
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
        "Keyboard: A / D dash or DI · Shift + A / D walk · Shift + S reduced-down shield drop · W or Space jump · F light / directional forward smash · H direct strong · E Pulse Bolt, Down + E Prism Burst reflector, or Up + E Arc Reservoir charge · T taunt · G shield/trigger · F + G grab, or pick up/drop the nearby Relay Rod. Standard Gamepad 1: left stick or D-pad · bottom face light / directional forward smash · right face direct strong · left face jump · top face special · down + top face reflector · up + top face charge · Back/View taunt · any shoulder/trigger shield · light + shield grab/item"
      )
    );
    controls.appendChild(
      controlCard(
        "Player 2",
        "Keyboard: ← / → dash or DI · Shift + horizontal arrows walk · Shift + ↓ reduced-down shield drop · ↑ jump · / or Numpad 0 light / directional forward smash · ' or Numpad 2 direct strong · ; or Numpad 3 Pulse Bolt, Down + special Prism Burst reflector, or Up + special Arc Reservoir charge · , taunt · . or Numpad 1 shield/trigger · light + shield grab/item. Standard Gamepad 2 uses the same controller layout as Player 1"
      )
    );
    section.appendChild(controls);

    var note = document.createElement("p");
    note.className = "pf-m4-note";
    note.textContent =
      "Standard Gamepads are assigned in browser index order and polled every " +
      "simulation tick, so hot-plugging does not alter canonical state. Left " +
      "stick magnitude preserves analog walk/dash thresholds; the D-pad emits " +
      "full magnitude. Keyboard and gamepad buttons may be mixed per player. " +
      "Tap jump and release during the three-tick jump squat for the fixed " +
      "short hop; hold through takeoff for the fixed full hop. Releasing after " +
      "takeoff never changes either apex. For an instant double jump, release " +
      "the first jump during jump squat, then press the other jump key on the " +
      "first airborne frame; the live air-jumps counter changes from 1 to 0. " +
      "Holding one jump key never repeats the input. Tap opposite full directions during " +
      "initial dash to dash-dance; after the state reaches RUN, the same reversal " +
      "enters RUN TURNAROUND instead. To fox-trot, rhythmically tap and release " +
      "one full direction; each fresh tap restarts INITIAL DASH, while holding " +
      "the direction reaches RUN and a reduced-magnitude re-entry only walks. " +
      "To moonwalk, dash, hold Shift plus the opposite direction for two ticks, " +
      "then release Shift while keeping that direction held; the fighter slides " +
      "backward without changing facing. A faster reversal is a dashback. " +
      "To pivot, dash, tap the opposite full direction for one tick, return to " +
      "neutral on the next tick, and immediately attack; the fighter keeps the " +
      "new facing and reversal momentum. Holding the reversal continues the " +
      "dash, while attempting it after RUN enters RUN TURNAROUND. " +
      "For a small-step forward smash, tap and hold a full direction, delay " +
      "one to three simulation ticks, then press the light-attack button; " +
      "the initial-dash travel extends the strong hitbox's reach. Pressing " +
      "direction plus light simultaneously gives the standing comparison, " +
      "while waiting four ticks produces the ordinary non-smash attack. " +
      "For a drop cancel, put both fighters close together on the moving " +
      "platform, press down with the attacker, then press light attack on the " +
      "first airborne tick; a hit returns the attacker to AERIAL LANDING on " +
      "that platform. Waiting one extra tick or whiffing falls through. " +
      "To dash-cancel a run, press down for a traction slide into CROUCH, then " +
      "attack; jump and shield are the other live cancel routes. Shield remains " +
      "locked out during INITIAL DASH and down cannot cancel RUN TURNAROUND. " +
      "Fall beside a ledge while facing inward " +
      "to grab it; after the catch, press inward to climb, down or away to " +
      "release, or jump to ledge-jump. For an edge hop, tap down from hang, " +
      "release it, then press jump plus inward on the next tick and follow " +
      "with an aerial. F and / perform the light jab; H and ' " +
      "perform a slower strong attack on the ground or in the air that " +
      "immediately launches the default fighter into tumble. Translucent boxes " +
      "show active frames, and tumbling " +
      "fighters visibly rotate after hitlag. During hitlag, " +
      "change stick direction for SDI and hold a launch direction for DI. Press " +
      "a light-attack key while airborne for the original aerial. For SHFFL, " +
      "short hop, press the aerial, hold down after the apex to fast-fall, then " +
      "tap the trigger within the seven-frame pre-landing window. A normal " +
      "aerial landing shows 12 ticks of AERIAL LANDING; success shows six " +
      "ticks of L-CANCEL LANDING. The strong aerial is the easy timing drill: " +
      "missing produces 30 ticks of landing lag and L-cancelling reduces it " +
      "to 15. A large red MISS or green L-CANCEL banner, ring, and live " +
      "remaining-frame count appear over the fighter on every aerial landing. " +
      "The live state card exposes trigger age " +
      "(eligible at ages 0–6) so the boundary is directly observable. " +
      "This is now a four-stock match: crossing a blast boundary consumes one " +
      "stock, waits 60 frames, and grants 120 frames of dashed-ring respawn " +
      "invulnerability. The HUD shows stocks and both timers; final-stock KOs " +
      "show a result banner and turn Reset into Rematch. Simultaneous final-stock " +
      "KOs enter the deterministic 300% sudden-death fixture. " +
      "Hold G or . on the ground for a real draining shield; fresh shields " +
      "powershield during their four-tick window, while releases have 15 ticks " +
      "of lag. Press a fresh full horizontal direction with the trigger for a " +
      "forward or backward roll relative to facing; press fresh down with the " +
      "trigger for a spot dodge. These grounded dodges have fixed movement, " +
      "recovery, and invulnerability windows and never flip facing. Tap the " +
      "same trigger shortly before a tumble landing to tech in " +
      "place; hold left or right to tech-roll. " +
      "While airborne, a fresh trigger performs a directional air dodge; " +
      "hold a direction with it, or leave the stick neutral to stop in place. " +
      "For a wavedash, short hop, then press down-left or down-right plus the " +
      "trigger on the first airborne frame. AIR DODGE becomes FALL SPECIAL if " +
      "it finishes in the air; touching a surface instead enters ten ticks of " +
      "SPECIAL LANDING while horizontal momentum slides under traction. " +
      "After a missed tech reaches " +
      "DOWN WAIT, press up or a fresh shield input for neutral getup, left or " +
      "right to roll, or either attack key for the two-sided floor attack. " +
      "The raised block is solid on every side: tumbling into its wall or " +
      "underside with a fresh tech input performs a wall or ceiling tech; " +
      "hold up for a wall-tech jump. Missing the window produces a visible " +
      "wall or ceiling bounce while hitstun continues. " +
      "Press light plus shield while grounded to grab. From RUN this is an " +
      "ordinary dash grab. For boost grab, press and hold light from RUN to " +
      "start DASH ATTACK, then freshly press shield on its next, second, or " +
      "third stored action tick; the cancel preserves the faster slide. A " +
      "later shield press leaves DASH ATTACK intact. During GRAB HOLD, hold a " +
      "full direction and freshly press either attack: forward/back are relative " +
      "to facing, while up/down select the vertical throws. Low-percent down " +
      "throws can lead to another grab; accumulated percent and outward DI move " +
      "the defender beyond the regrab window. " +
      "Use Team Wobble Lab for the four-fighter team fixture: Player 1 controls " +
      "P1, Player 2 controls allied P3, P2 automatically performs legal mash " +
      "inputs, and P4 stays neutral. After either ally grabs P2, press down + " +
      "attack with the holder while the other ally freshly presses light + " +
      "shield; repeat from the opposite side. Starting the waiting grab early " +
      "spends its active window and lets P2 escape. " +
      "The gold Relay Rod starts left of Player 1. Near it, light plus shield " +
      "picks it up; while holding it, light or strong plus a direction throws, " +
      "and light plus shield drops it, including an aerial bat drop. Start a " +
      "ground roll and press light during action ticks 0–4 for a glide toss. " +
      "Dash, jump, then press light during jump squat for a jump-cancel throw; " +
      "waiting until airborne produces an ordinary aerial item throw instead. " +
      "Without an item, hold full up and freshly press light or strong during " +
       "jump squat to cancel into the standing strong attack; neutral, shallow-up, " +
       "and first-airborne-frame attacks keep their ordinary routes. " +
       "Press E or ; (top face on a Standard Gamepad) to fire the fixed-capacity " +
       "Pulse Bolt. Fire it during a short hop for the short-hop laser route; an " +
      "ordinary shield blocks it, while a shield activated during the two-frame " +
      "projectile window reflects ownership and velocity without taking damage. " +
      "Hold down with special for the Prism Burst reflector: its two active " +
      "frames strike nearby fighters down and away, and reverse an overlapping " +
      "Pulse Bolt without using the powershield result. " +
       "The deterministic event feed below records hits, shield interactions, " +
      "grabs, throws, KOs, respawns, sudden death, and results in canonical " +
      "sequence order. " +
      "R resets, P " +
      "pauses, and N single-steps.";
    section.appendChild(note);

    var stateGrid = document.createElement("div");
    stateGrid.className = "pf-m4-state-grid";
    var playerStates = [];
    [0, 1, 2, 3].forEach(function (player) {
      var card = document.createElement("div");
      card.className = "pf-m4-state";
      card.id = "pf-m4-player-" + player;
      card.textContent = "P" + (player + 1) + " waiting for first state";
      card.hidden = player > 1;
      stateGrid.appendChild(card);
      playerStates.push(card);
    });
    var itemState = document.createElement("div");
    itemState.className = "pf-m4-state pf-m4-item-state";
    itemState.id = "pf-m4-item";
    itemState.textContent = "Relay Rod waiting for first state";
    stateGrid.appendChild(itemState);
    var projectileState = document.createElement("div");
    projectileState.className = "pf-m4-state pf-m4-projectile-state";
    projectileState.id = "pf-m4-projectile";
    projectileState.textContent = "Pulse Bolt waiting for first state";
    stateGrid.appendChild(projectileState);
    var eventPanel = document.createElement("div");
    eventPanel.className = "pf-m4-state pf-m4-event-panel";
    var eventTitle = document.createElement("strong");
    eventTitle.textContent = "Deterministic combat event feed";
    var eventHelp = document.createElement("div");
    eventHelp.className = "pf-m4-event-help";
    eventHelp.textContent =
      "Newest last · fixed-capacity per-tick journal · rollback-reproducible";
    var eventFeed = document.createElement("ol");
    eventFeed.className = "pf-m4-event-feed";
    var eventEmpty = document.createElement("li");
    eventEmpty.className = "pf-m4-event-empty";
    eventEmpty.textContent = "No combat events yet.";
    eventFeed.appendChild(eventEmpty);
    eventPanel.appendChild(eventTitle);
    eventPanel.appendChild(eventHelp);
    eventPanel.appendChild(eventFeed);
    stateGrid.appendChild(eventPanel);
    section.appendChild(stateGrid);

    if (replayInspector) {
      document.body.insertBefore(section, replayInspector);
    } else {
      document.body.appendChild(section);
    }

    var state = {
      accumulator: 0,
      aerialLandingLagTicks: aerialLandingLagTicks,
      canvas: canvas,
      dashAxis: dashAxis,
      eventFeed: eventFeed,
      eventLog: [],
      gamepadLabel: gamepadLabel,
      keys: Object.create(null),
      itemState: itemState,
      projectileState: projectileState,
      lastEventSequence: 0,
      lastTime: 0,
      latest: null,
      pauseButton: pauseButton,
      playerStates: playerStates,
      resetButton: resetButton,
      teamLabButton: teamLabButton,
      attackQueued: [false, false],
      strongAttackQueued: [false, false],
      jumpQueued: [false, false],
      shieldQueued: [false, false],
      specialQueued: [false, false],
      tauntQueued: [false, false],
      strongAerialLandingLagTicks: strongAerialLandingLagTicks,
      teamLabActive: false,
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

    function vertical(up, down) {
      if (held(up) === held(down)) {
        return 0;
      }
      var magnitude =
        held("ShiftLeft") || held("ShiftRight")
          ? state.walkAxis
          : state.dashAxis;
      return held(up) ? -magnitude : magnitude;
    }

    function mergeAxis(keyboardAxis, gamepadAxisValue) {
      return keyboardAxis !== 0 ? keyboardAxis : gamepadAxisValue;
    }

    function step() {
      var gamepads = pollStandardGamepads();
      var player0Gamepad = gamepads.inputs[0];
      var player1Gamepad = gamepads.inputs[1];
      state.gamepadLabel.textContent = gamepadApiAvailable
        ? "standard gamepads " + gamepads.connected + "/2"
        : "gamepad API unavailable";
      var player0Jump =
        held("KeyW") ||
        held("Space") ||
        state.jumpQueued[0] ||
        player0Gamepad.jump;
      var player1Jump =
        held("ArrowUp") || state.jumpQueued[1] || player1Gamepad.jump;
      var player0Attack =
        held("KeyF") || state.attackQueued[0] || player0Gamepad.attack;
      var player1Attack =
        held("Slash") ||
        held("Numpad0") ||
        state.attackQueued[1] ||
        player1Gamepad.attack;
      var player0StrongAttack =
        held("KeyH") ||
        state.strongAttackQueued[0] ||
        player0Gamepad.strongAttack;
      var player1StrongAttack =
        held("Quote") ||
        held("Numpad2") ||
        state.strongAttackQueued[1] ||
        player1Gamepad.strongAttack;
      var player0Special =
        held("KeyE") || state.specialQueued[0] || player0Gamepad.special;
      var player1Special =
        held("Semicolon") ||
        held("Numpad3") ||
        state.specialQueued[1] ||
        player1Gamepad.special;
      var player0Taunt =
        held("KeyT") || state.tauntQueued[0] || player0Gamepad.taunt;
      var player1Taunt =
        held("Comma") || state.tauntQueued[1] || player1Gamepad.taunt;
      var player0Shield =
        held("KeyG") || state.shieldQueued[0] || player0Gamepad.shield;
      var player1Shield =
        held("Period") ||
        held("Numpad1") ||
        state.shieldQueued[1] ||
        player1Gamepad.shield;
      var passed = Module._pf_web_m4_playtest_step_special(
        mergeAxis(
          horizontal("KeyA", "KeyD"),
          player0Gamepad.horizontal
        ),
        mergeAxis(vertical("KeyW", "KeyS"), player0Gamepad.vertical),
        player0Jump ? 1 : 0,
        player0Attack ? 1 : 0,
        player0StrongAttack ? 1 : 0,
        player0Shield ? 1 : 0,
        mergeAxis(
          horizontal("ArrowLeft", "ArrowRight"),
          player1Gamepad.horizontal
        ),
        mergeAxis(
          vertical("ArrowUp", "ArrowDown"),
          player1Gamepad.vertical
        ),
        player1Jump ? 1 : 0,
        player1Attack ? 1 : 0,
        player1StrongAttack ? 1 : 0,
        player1Shield ? 1 : 0,
        player0Special ? 1 : 0,
        player1Special ? 1 : 0,
        player0Taunt ? 1 : 0,
        player1Taunt ? 1 : 0
      );
      state.jumpQueued[0] = false;
      state.jumpQueued[1] = false;
      state.attackQueued[0] = false;
      state.attackQueued[1] = false;
      state.strongAttackQueued[0] = false;
      state.strongAttackQueued[1] = false;
      state.shieldQueued[0] = false;
      state.shieldQueued[1] = false;
      state.specialQueued[0] = false;
      state.specialQueued[1] = false;
      state.tauntQueued[0] = false;
      state.tauntQueued[1] = false;
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
      var completed =
        state.latest &&
        (state.latest[22] !== 0 || state.latest[23] !== 0);
      state.keys = Object.create(null);
      state.jumpQueued = [false, false];
      state.attackQueued = [false, false];
      state.strongAttackQueued = [false, false];
      state.shieldQueued = [false, false];
      state.specialQueued = [false, false];
      state.tauntQueued = [false, false];
      state.accumulator = 0;
      Module._pf_web_m4_playtest_reset();
      if (completed) {
        setRunning(true);
      }
    }

    function toggleTeamLab() {
      var nextActive = !state.teamLabActive;

      state.keys = Object.create(null);
      state.jumpQueued = [false, false];
      state.attackQueued = [false, false];
      state.strongAttackQueued = [false, false];
      state.shieldQueued = [false, false];
      state.specialQueued = [false, false];
      state.tauntQueued = [false, false];
      state.eventLog = [];
      state.lastEventSequence = 0;
      state.teamLabActive = nextActive;
      if (!Module._pf_web_m4_playtest_set_team_lab(nextActive ? 1 : 0)) {
        state.teamLabActive = !nextActive;
        if (status) {
          status.textContent += " team_lab_runtime=fail";
          status.dataset.teamLabRuntime = "fail";
        }
        return;
      }
      state.teamLabButton.textContent = nextActive
        ? "Return to Duel Lab"
        : "Team Wobble Lab";
      state.teamLabButton.setAttribute(
        "aria-pressed",
        nextActive ? "true" : "false"
      );
      section.dataset.teamLab = nextActive ? "active" : "inactive";
      state.gamepadLabel.textContent = nextActive
        ? "team lab: controls P1/P3 · P2 auto-mashes"
        : gamepadApiAvailable
          ? "standard gamepads 0/2"
          : "gamepad API unavailable";
      setRunning(true);
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
    teamLabButton.addEventListener("click", toggleTeamLab);

    window.addEventListener(
      "keydown",
      function (event) {
        var wasHeld = held(event.code);
        if (
          event.code === "Space" ||
          event.code === "Slash" ||
          event.code === "Numpad0" ||
          event.code === "Quote" ||
          event.code === "Numpad2" ||
          event.code === "Period" ||
          event.code === "Numpad1" ||
          event.code === "Semicolon" ||
          event.code === "Numpad3" ||
          event.code === "Comma" ||
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
        if (!wasHeld && event.code === "KeyH") {
          state.strongAttackQueued[0] = true;
        }
        if (
          !wasHeld &&
          (event.code === "Quote" || event.code === "Numpad2")
        ) {
          state.strongAttackQueued[1] = true;
        }
        if (!wasHeld && event.code === "KeyG") {
          state.shieldQueued[0] = true;
        }
        if (
          !wasHeld &&
          (event.code === "Period" || event.code === "Numpad1")
        ) {
          state.shieldQueued[1] = true;
        }
        if (!wasHeld && event.code === "KeyE") {
          state.specialQueued[0] = true;
        }
        if (
          !wasHeld &&
          (event.code === "Semicolon" || event.code === "Numpad3")
        ) {
          state.specialQueued[1] = true;
        }
        if (!wasHeld && event.code === "KeyT") {
          state.tauntQueued[0] = true;
        }
        if (!wasHeld && event.code === "Comma") {
          state.tauntQueued[1] = true;
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
      state.strongAttackQueued = [false, false];
      state.shieldQueued = [false, false];
      state.specialQueued = [false, false];
      state.tauntQueued = [false, false];
    });

    if (status) {
      status.textContent +=
        " playtest=" +
        (gamepadApiAvailable && gamepadProbePassed ? "ready" : "fail") +
        " input_probe=" +
        (inputProbePassed ? "pass" : "fail") +
        " air_facing_probe=" +
        (airFacingProbePassed ? "pass" : "fail") +
        " instant_double_jump_probe=" +
        (instantDoubleJumpProbePassed ? "pass" : "fail") +
        " double_jump_cancel_probe=" +
        (doubleJumpCancelProbePassed ? "pass" : "fail") +
        " double_jump_cancel_counter_probe=" +
        (doubleJumpCancelCounterProbePassed ? "pass" : "fail") +
        " bat_drop_probe=" +
        (batDropProbePassed ? "pass" : "fail") +
        " glide_toss_probe=" +
        (glideTossProbePassed ? "pass" : "fail") +
        " jump_cancel_throw_probe=" +
        (jumpCancelThrowProbePassed ? "pass" : "fail") +
        " jump_cancel_probe=" +
        (jumpCancelProbePassed ? "pass" : "fail") +
        " edge_hop_probe=" +
        (edgeHopProbePassed ? "pass" : "fail") +
        " edge_dash_probe=" +
        (edgeDashProbePassed ? "pass" : "fail") +
        " fox_trot_probe=" +
        (foxTrotProbePassed ? "pass" : "fail") +
        " moonwalk_probe=" +
        (moonwalkProbePassed ? "pass" : "fail") +
        " teeter_cancel_probe=" +
        (teeterCancelProbePassed ? "pass" : "fail") +
        " stage_humping_probe=" +
        (stageHumpingProbePassed ? "pass" : "fail") +
        " taunt_cancel_probe=" +
        (tauntCancelProbePassed ? "pass" : "fail") +
        " scar_jump_probe=" +
        (scarJumpProbePassed ? "pass" : "fail") +
        " team_wobble_probe=" +
        (teamWobbleProbePassed ? "pass" : "fail") +
        " pivot_probe=" +
        (pivotProbePassed ? "pass" : "fail") +
        " dash_cancel_probe=" +
        (dashCancelProbePassed ? "pass" : "fail") +
        " dashing_shield_probe=" +
        (dashingShieldProbePassed ? "pass" : "fail") +
        " shield_platform_drop_probe=" +
        (shieldPlatformDropProbePassed ? "pass" : "fail") +
        " small_step_forward_smash_probe=" +
        (smallStepForwardSmashProbePassed ? "pass" : "fail") +
        " drop_cancel_probe=" +
        (dropCancelProbePassed ? "pass" : "fail") +
        " v_cancel_probe=" +
        (vCancelProbePassed ? "pass" : "fail") +
        " approach_probe=" +
        (approachProbePassed ? "pass" : "fail") +
        " spacing_probe=" +
        (spacingProbePassed ? "pass" : "fail") +
        " sharking_probe=" +
        (sharkingProbePassed ? "pass" : "fail") +
        " cross_up_probe=" +
        (crossUpProbePassed ? "pass" : "fail") +
        " mindgame_probe=" +
        (mindgameProbePassed ? "pass" : "fail") +
        " juggling_probe=" +
        (jugglingProbePassed ? "pass" : "fail") +
        " ladder_probe=" +
        (ladderProbePassed ? "pass" : "fail") +
        " kill_confirm_probe=" +
        (killConfirmProbePassed ? "pass" : "fail") +
        " zero_to_death_probe=" +
        (zeroToDeathProbePassed ? "pass" : "fail") +
        " ledge_cancel_probe=" +
        (ledgeCancelProbePassed ? "pass" : "fail") +
        " planking_probe=" +
        (plankingProbePassed ? "pass" : "fail") +
        " jump_cancelled_grab_probe=" +
        (jumpCancelledGrabProbePassed ? "pass" : "fail") +
        " boost_grab_probe=" +
        (boostGrabProbePassed ? "pass" : "fail") +
        " jab_cancel_probe=" +
        (jabCancelProbePassed ? "pass" : "fail") +
        " jab_reset_probe=" +
        (jabResetProbePassed ? "pass" : "fail") +
        " chain_grab_probe=" +
        (chainGrabProbePassed ? "pass" : "fail") +
        " combat_probe=" +
        (combatProbePassed ? "pass" : "fail") +
        " event_journal_probe=" +
        (combatProbePassed ? "pass" : "fail") +
        " reaction_probe=" +
        (reactionProbePassed ? "pass" : "fail") +
        " shield_probe=" +
        (shieldProbePassed ? "pass" : "fail") +
        " shield_break_probe=" +
        (shieldBreakProbePassed ? "pass" : "fail") +
        " powershield_cancel_probe=" +
        (shieldProbePassed ? "pass" : "fail") +
        " tumble_probe=" +
        (tumbleProbePassed ? "pass" : "fail") +
        " floor_recovery_probe=" +
        (floorRecoveryProbePassed ? "pass" : "fail") +
        " tech_chase_probe=" +
        (techChaseProbePassed ? "pass" : "fail") +
        " surface_tech_probe=" +
        (surfaceTechProbePassed ? "pass" : "fail") +
        " air_dodge_probe=" +
        (airDodgeProbePassed ? "pass" : "fail") +
        " ground_dodge_probe=" +
        (groundDodgeProbePassed ? "pass" : "fail") +
        " aerial_l_cancel_probe=" +
        (aerialLCancelProbePassed ? "pass" : "fail") +
        " match_probe=" +
        (matchProbePassed ? "pass" : "fail") +
        " short_hop_laser_probe=" +
        (shortHopLaserProbePassed ? "pass" : "fail") +
        " camping_probe=" +
        (campingProbePassed ? "pass" : "fail") +
        " shine_spike_probe=" +
        (shineSpikeProbePassed ? "pass" : "fail") +
        " charge_storage_probe=" +
        (chargeStorageProbePassed ? "pass" : "fail") +
        " gamepad_probe=" +
        (gamepadProbePassed ? "pass" : "fail") +
        " gamepad_api=" +
        (gamepadApiAvailable ? "available" : "unavailable") +
        " controls=keyboard-gamepad-two-controller-duel-team-lab";
      status.dataset.playtest =
        gamepadApiAvailable && gamepadProbePassed ? "ready" : "fail";
      status.dataset.inputProbe = inputProbePassed ? "pass" : "fail";
      status.dataset.airFacingProbe =
        airFacingProbePassed ? "pass" : "fail";
      status.dataset.instantDoubleJumpProbe =
        instantDoubleJumpProbePassed ? "pass" : "fail";
      status.dataset.doubleJumpCancelProbe =
        doubleJumpCancelProbePassed ? "pass" : "fail";
      status.dataset.doubleJumpCancelCounterProbe =
        doubleJumpCancelCounterProbePassed ? "pass" : "fail";
      status.dataset.batDropProbe = batDropProbePassed ? "pass" : "fail";
      status.dataset.glideTossProbe = glideTossProbePassed ? "pass" : "fail";
      status.dataset.jumpCancelThrowProbe =
        jumpCancelThrowProbePassed ? "pass" : "fail";
      status.dataset.jumpCancelProbe =
        jumpCancelProbePassed ? "pass" : "fail";
      status.dataset.edgeHopProbe = edgeHopProbePassed ? "pass" : "fail";
      status.dataset.edgeDashProbe = edgeDashProbePassed ? "pass" : "fail";
      status.dataset.foxTrotProbe = foxTrotProbePassed ? "pass" : "fail";
      status.dataset.moonwalkProbe = moonwalkProbePassed ? "pass" : "fail";
      status.dataset.teeterCancelProbe =
        teeterCancelProbePassed ? "pass" : "fail";
      status.dataset.stageHumpingProbe =
        stageHumpingProbePassed ? "pass" : "fail";
      status.dataset.tauntCancelProbe =
        tauntCancelProbePassed ? "pass" : "fail";
      status.dataset.scarJumpProbe =
        scarJumpProbePassed ? "pass" : "fail";
      status.dataset.teamWobbleProbe =
        teamWobbleProbePassed ? "pass" : "fail";
      status.dataset.pivotProbe = pivotProbePassed ? "pass" : "fail";
      status.dataset.dashCancelProbe =
        dashCancelProbePassed ? "pass" : "fail";
      status.dataset.dashingShieldProbe =
        dashingShieldProbePassed ? "pass" : "fail";
      status.dataset.shieldPlatformDropProbe =
        shieldPlatformDropProbePassed ? "pass" : "fail";
      status.dataset.smallStepForwardSmashProbe =
        smallStepForwardSmashProbePassed ? "pass" : "fail";
      status.dataset.dropCancelProbe =
        dropCancelProbePassed ? "pass" : "fail";
      status.dataset.vCancelProbe =
        vCancelProbePassed ? "pass" : "fail";
      status.dataset.approachProbe =
        approachProbePassed ? "pass" : "fail";
      status.dataset.spacingProbe =
        spacingProbePassed ? "pass" : "fail";
      status.dataset.sharkingProbe =
        sharkingProbePassed ? "pass" : "fail";
      status.dataset.crossUpProbe =
        crossUpProbePassed ? "pass" : "fail";
      status.dataset.mindgameProbe =
        mindgameProbePassed ? "pass" : "fail";
      status.dataset.jugglingProbe =
        jugglingProbePassed ? "pass" : "fail";
      status.dataset.ladderProbe = ladderProbePassed ? "pass" : "fail";
      status.dataset.killConfirmProbe =
        killConfirmProbePassed ? "pass" : "fail";
      status.dataset.zeroToDeathProbe =
        zeroToDeathProbePassed ? "pass" : "fail";
      status.dataset.ledgeCancelProbe =
        ledgeCancelProbePassed ? "pass" : "fail";
      status.dataset.plankingProbe =
        plankingProbePassed ? "pass" : "fail";
      status.dataset.jumpCancelledGrabProbe =
        jumpCancelledGrabProbePassed ? "pass" : "fail";
      status.dataset.boostGrabProbe =
        boostGrabProbePassed ? "pass" : "fail";
      status.dataset.jabCancelProbe =
        jabCancelProbePassed ? "pass" : "fail";
      status.dataset.chainGrabProbe =
        chainGrabProbePassed ? "pass" : "fail";
      status.dataset.combatProbe = combatProbePassed ? "pass" : "fail";
      status.dataset.eventJournalProbe =
        combatProbePassed ? "pass" : "fail";
      status.dataset.reactionProbe =
        reactionProbePassed ? "pass" : "fail";
      status.dataset.shieldProbe =
        shieldProbePassed ? "pass" : "fail";
      status.dataset.shieldBreakProbe =
        shieldBreakProbePassed ? "pass" : "fail";
      status.dataset.powershieldCancelProbe =
        shieldProbePassed ? "pass" : "fail";
      status.dataset.tumbleProbe =
        tumbleProbePassed ? "pass" : "fail";
      status.dataset.floorRecoveryProbe =
        floorRecoveryProbePassed ? "pass" : "fail";
      status.dataset.techChaseProbe =
        techChaseProbePassed ? "pass" : "fail";
      status.dataset.surfaceTechProbe =
        surfaceTechProbePassed ? "pass" : "fail";
      status.dataset.airDodgeProbe =
        airDodgeProbePassed ? "pass" : "fail";
      status.dataset.groundDodgeProbe =
        groundDodgeProbePassed ? "pass" : "fail";
      status.dataset.aerialLCancelProbe =
        aerialLCancelProbePassed ? "pass" : "fail";
      status.dataset.matchProbe = matchProbePassed ? "pass" : "fail";
      status.dataset.shortHopLaserProbe =
        shortHopLaserProbePassed ? "pass" : "fail";
      status.dataset.campingProbe = campingProbePassed ? "pass" : "fail";
      status.dataset.shineSpikeProbe = shineSpikeProbePassed ? "pass" : "fail";
      status.dataset.chargeStorageProbe =
        chargeStorageProbePassed ? "pass" : "fail";
      status.dataset.gamepadProbe = gamepadProbePassed ? "pass" : "fail";
      status.dataset.gamepadApi =
        gamepadApiAvailable ? "available" : "unavailable";
      status.dataset.controls = "keyboard-gamepad-two-controller-duel-team-lab";
    }
    requestAnimationFrame(frame);
  },

  pf_web_m4_playtest_render__sig: "vpi",
  pf_web_m4_playtest_render: function (viewPointer, viewCount) {
    var state = Module.pfM4Playtest;
    if (!state || viewCount !== 392) {
      return;
    }
    var previousTick = state.latest ? state.latest[1] : -1;
    state.latest = new Int32Array(
      HEAP32.subarray(viewPointer >> 2, (viewPointer >> 2) + viewCount)
    );

    var view = state.latest;
    if (view[0] !== 32) {
      return;
    }
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
    var colors = ["#55e6d0", "#ff7695", "#8ee28d", "#ffd166"];
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
      "KNOCKDOWN",
      "TECH IN PLACE",
      "TECH ROLL",
      "SHIELD",
      "SHIELD STUN",
      "SHIELD RELEASE",
      "SHIELD BREAK",
      "STRONG ATTACK",
      "DOWN WAIT",
      "NEUTRAL GETUP",
      "GETUP ROLL",
      "FLOOR ATTACK",
      "WALL TECH",
      "WALL TECH JUMP",
      "CEILING TECH",
      "WALL BOUNCE",
      "CEILING BOUNCE",
      "AIR DODGE",
      "FALL SPECIAL",
      "SPECIAL LANDING",
      "AERIAL ATTACK",
      "AERIAL LANDING",
      "L-CANCEL LANDING",
      "FORWARD ROLL",
      "BACKWARD ROLL",
      "SPOT DODGE",
      "STRONG AERIAL",
      "STRONG AERIAL LANDING",
      "STRONG L-CANCEL LANDING",
      "RESPAWN WAIT",
      "ELIMINATED",
      "SHIELD BREAK DOWN",
      "SHIELD BREAK STAND",
      "SHIELD BREAK STUN",
      "GRAB",
      "GRAB HOLD",
      "GRABBED",
      "GRAB RELEASE",
      "FORWARD THROW",
      "BACK THROW",
      "UP THROW",
      "DOWN THROW",
      "DASH ATTACK",
      "JAB FINAL",
      "RESET BOUND",
      "FORCED GETUP",
      "DELAYED AIR JUMP",
      "ITEM THROW",
      "DASH ITEM THROW",
      "PULSE BOLT GROUND",
      "PULSE BOLT AIR",
      "PRISM BURST GROUND",
      "PRISM BURST AIR",
      "ARC RESERVOIR CHARGE",
      "ARC RESERVOIR STORE",
      "ARC RESERVOIR RELEASE",
      "MOONWALK SETUP",
      "MOONWALK",
      "TEETER",
      "CROUCH STEP",
      "TAUNT",
      "WALL JUMP",
    ];

    if (view[1] < previousTick) {
      state.eventLog = [];
      state.lastEventSequence = 0;
    }

    function eventPlayer(slot) {
      return slot === 255 ? "system" : "P" + (slot + 1);
    }

    function eventWinners(mask) {
      var winners = [];
      var slot;
      for (slot = 0; slot < 4; ++slot) {
        if ((mask & (1 << slot)) !== 0) {
          winners.push("P" + (slot + 1));
        }
      }
      return winners.length ? winners.join(" + ") : "draw";
    }

    function eventDescription(event) {
      var source = eventPlayer(event.source);
      var target = eventPlayer(event.target);
      var value = (event.value / q16).toFixed(1);
      var velocity =
        "(" +
        (event.velocityX / q16).toFixed(2) +
        ", " +
        (event.velocityY / q16).toFixed(2) +
        ")";

      switch (event.type) {
        case 1:
          return (
            source +
            " hit " +
            target +
            " for " +
            value +
            "% · launch " +
            velocity +
            ((event.flags & 1) !== 0 ? " · TUMBLE" : "")
          );
        case 2:
          return (
            target +
            " shielded " +
            source +
            " · shield damage " +
            value
          );
        case 3:
          return target + " POWERSHIELDED " + source + " · zero shield damage";
        case 4:
          return target + " SHIELD BROKE against " + source;
        case 5:
          return (
            target +
            " KO · " +
            event.detail +
            " stock" +
            (event.detail === 1 ? "" : "s") +
            " remain" +
            ((event.flags & 2) !== 0 ? " · ELIMINATED" : "")
          );
        case 6:
          return (
            target +
            " respawned · " +
            event.detail +
            "f invulnerability" +
            ((event.flags & 8) !== 0 ? " · 300%" : "")
          );
        case 7:
          return "SUDDEN DEATH · " + value + "% · all players respawn";
        case 8:
          return "MATCH RESULT · " + eventWinners(event.detail) + " win";
        case 9:
          return target + " forfeited";
        case 10:
          return "TIME LIMIT";
        case 11:
          return source + " GRABBED " + target + " at " + value + "%";
        case 12:
          return (
            source + " escaped " + target + "'s grab at " + value + "%"
          );
        case 13:
          return (
            source +
            " " +
            (actionNames[event.detail] || "THREW") +
            " " +
            target +
            " for " +
            value +
            "% · launch " +
            velocity
          );
        case 14:
          return source + " picked up the Relay Rod";
        case 15:
          return source + " dropped the Relay Rod";
        case 16:
          return (
            source +
            " threw the Relay Rod " +
            (["", "forward", "back", "up", "down"][event.detail] ||
              "direction " + event.detail) +
            " · velocity " +
            velocity
          );
        case 17:
          return (
            source +
            " hit " +
            target +
            " with the Relay Rod for " +
            value +
            "% · launch " +
            velocity
          );
        case 18:
          return "Relay Rod reset to its authored spawn";
        case 19:
          return (
            source +
            " fired a Pulse Bolt · " +
            (actionNames[event.detail] || "ACTION " + event.detail) +
            " · velocity " +
            velocity
          );
        case 20:
          return (
            source +
            " hit " +
            target +
            " with a Pulse Bolt for " +
            value +
            "% · launch " +
            velocity
          );
        case 21:
          return (
            source +
            " reflected " +
            target +
            "'s Pulse Bolt · velocity " +
            velocity
          );
        default:
          return "unknown event type " + event.type;
      }
    }

    function renderEventFeed() {
      state.eventFeed.textContent = "";
      if (state.eventLog.length === 0) {
        var empty = document.createElement("li");
        empty.className = "pf-m4-event-empty";
        empty.textContent = "No combat events yet.";
        state.eventFeed.appendChild(empty);
        return;
      }
      state.eventLog.forEach(function (event) {
        var row = document.createElement("li");
        var key = document.createElement("code");
        var description = document.createElement("span");
        key.textContent = "#" + event.sequence + " · tick " + event.tick;
        description.textContent = eventDescription(event);
        row.appendChild(key);
        row.appendChild(description);
        state.eventFeed.appendChild(row);
      });
    }

    var eventCount = Math.max(0, Math.min(16, view[201]));
    var eventIndex;
    for (eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
      var eventBase = 202 + eventIndex * 10;
      var sequence = view[eventBase];
      if (sequence <= state.lastEventSequence) {
        continue;
      }
      state.eventLog.push({
        sequence: sequence,
        tick: view[eventBase + 1],
        type: view[eventBase + 2],
        source: view[eventBase + 3],
        target: view[eventBase + 4],
        value: view[eventBase + 5],
        velocityX: view[eventBase + 6],
        velocityY: view[eventBase + 7],
        flags: view[eventBase + 8],
        detail: view[eventBase + 9],
      });
      state.lastEventSequence = sequence;
    }
    if (state.eventLog.length > 10) {
      state.eventLog = state.eventLog.slice(state.eventLog.length - 10);
    }
    renderEventFeed();

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

    var solidLeft = sx(view[14]);
    var solidRight = sx(view[15]);
    var solidTop = sy(view[16]);
    var solidBottom = sy(view[17]);
    var solidGradient = context.createLinearGradient(
      solidLeft,
      solidTop,
      solidRight,
      solidBottom
    );
    solidGradient.addColorStop(0, "#425a78");
    solidGradient.addColorStop(1, "#1f3048");
    context.fillStyle = solidGradient;
    context.strokeStyle = "#b8c7da";
    context.lineWidth = 3;
    context.fillRect(
      solidLeft,
      solidTop,
      solidRight - solidLeft,
      solidBottom - solidTop
    );
    context.strokeRect(
      solidLeft,
      solidTop,
      solidRight - solidLeft,
      solidBottom - solidTop
    );

    context.save();
    context.fillStyle = "#eaf3ff";
    context.font = "bold 16px ui-monospace, monospace";
    context.textAlign = "center";
    context.fillText(
      "P1 STOCKS " +
        view[25 + 32] +
        "  ·  P2 STOCKS " +
        view[25 + 44 + 32],
      canvas.width / 2,
      25
    );
    context.restore();

    var itemBase = 362;
    var itemStateCode = view[itemBase + 1];
    if (view[itemBase] !== 0 && itemStateCode > 0 && itemStateCode < 4) {
      var itemWorldX = view[itemBase + 6];
      var itemWorldY = view[itemBase + 7];
      var itemX = sx(itemWorldX);
      var itemY = sy(itemWorldY);
      var itemWidth =
        sx(itemWorldX + view[itemBase + 14]) -
        sx(itemWorldX - view[itemBase + 14]);
      var itemHeight =
        sy(itemWorldY + view[itemBase + 15]) -
        sy(itemWorldY - view[itemBase + 15]);

      if (view[itemBase + 5] !== 0) {
        var itemHitboxLeft = sx(itemWorldX - view[itemBase + 16]);
        var itemHitboxRight = sx(itemWorldX + view[itemBase + 16]);
        var itemHitboxTop = sy(itemWorldY - view[itemBase + 17]);
        var itemHitboxBottom = sy(itemWorldY + view[itemBase + 17]);
        context.fillStyle = "#ffbd4a2b";
        context.strokeStyle = "#ffd36a";
        context.lineWidth = 2;
        context.fillRect(
          itemHitboxLeft,
          itemHitboxTop,
          itemHitboxRight - itemHitboxLeft,
          itemHitboxBottom - itemHitboxTop
        );
        context.strokeRect(
          itemHitboxLeft,
          itemHitboxTop,
          itemHitboxRight - itemHitboxLeft,
          itemHitboxBottom - itemHitboxTop
        );
      }

      context.save();
      context.translate(itemX, itemY);
      if (itemStateCode === 3) {
        context.rotate(
          Math.atan2(view[itemBase + 9], view[itemBase + 8]) + Math.PI / 2
        );
      }
      var rodGradient = context.createLinearGradient(
        -itemWidth / 2,
        0,
        itemWidth / 2,
        0
      );
      rodGradient.addColorStop(0, "#f29b24");
      rodGradient.addColorStop(0.5, "#fff2a8");
      rodGradient.addColorStop(1, "#d56b1b");
      context.fillStyle = rodGradient;
      context.strokeStyle = "#fff1b8";
      context.lineWidth = 2;
      context.fillRect(
        -Math.max(4, itemWidth) / 2,
        -Math.max(10, itemHeight) / 2,
        Math.max(4, itemWidth),
        Math.max(10, itemHeight)
      );
      context.strokeRect(
        -Math.max(4, itemWidth) / 2,
        -Math.max(10, itemHeight) / 2,
        Math.max(4, itemWidth),
        Math.max(10, itemHeight)
      );
      context.restore();

      context.fillStyle = "#ffe7a0";
      context.font = "bold 10px ui-monospace, monospace";
      context.textAlign = "center";
      context.fillText("RELAY ROD", itemX, itemY - Math.max(10, itemHeight) / 2 - 8);
    }

    var projectileBase = 380;
    var projectileStateCode = view[projectileBase + 1];
    if (
      view[projectileBase] !== 0 &&
      projectileStateCode > 0 &&
      projectileStateCode < 3
    ) {
      var projectileWorldX = view[projectileBase + 4];
      var projectileWorldY = view[projectileBase + 5];
      var projectileX = sx(projectileWorldX);
      var projectileY = sy(projectileWorldY);
      var projectileWidth = Math.max(
        7,
        sx(projectileWorldX + view[projectileBase + 9]) -
          sx(projectileWorldX - view[projectileBase + 9])
      );
      var projectileHeight = Math.max(
        7,
        sy(projectileWorldY + view[projectileBase + 10]) -
          sy(projectileWorldY - view[projectileBase + 10])
      );
      var projectileOwner = view[projectileBase + 2];

      context.save();
      context.shadowColor =
        projectileOwner >= 0 && projectileOwner < colors.length
          ? colors[projectileOwner]
          : "#7deeff";
      context.shadowBlur = 18;
      context.fillStyle = "#d8fbff";
      context.strokeStyle = "#61e8ff";
      context.lineWidth = 2;
      context.beginPath();
      context.ellipse(
        projectileX,
        projectileY,
        projectileWidth / 2,
        projectileHeight / 2,
        0,
        0,
        Math.PI * 2
      );
      context.fill();
      context.stroke();
      context.shadowBlur = 0;
      context.fillStyle = "#bff8ff";
      context.font = "bold 9px ui-monospace, monospace";
      context.textAlign = "center";
      context.fillText(
        "PULSE BOLT",
        projectileX,
        projectileY - projectileHeight / 2 - 7
      );
      context.restore();
    }

    var livePlayerCount = state.teamLabActive ? 4 : 2;
    state.playerStates.forEach(function (card, playerIndex) {
      card.hidden = playerIndex >= livePlayerCount;
    });
    [0, 1, 2, 3].forEach(function (playerIndex) {
      if (playerIndex >= livePlayerCount) {
        return;
      }
      var base = 25 + playerIndex * 44;
      var x = sx(view[base]);
      var y = sy(view[base + 1]);
      var halfWidth =
        (view[12] / q16 / (blastRight - blastLeft)) * usableWidth;
      var halfHeight =
        (view[13] / q16 / (blastBottom - blastTop)) * usableHeight;
      var width = Math.max(14, halfWidth * 2);
      var height = Math.max(28, halfHeight * 2);
      var facing = view[base + 5];
      var actionState = view[base + 4];
      var respawning = actionState === 44;
      var eliminated = actionState === 45;
      var tumbling =
        view[base + 22] !== 0 && actionState !== 13;
      var prone =
        actionState === 15 ||
        actionState === 23 ||
        actionState === 26 ||
        actionState === 46 ||
        (actionState === 59 && view[base + 6] !== 0);
      var invulnerable = view[base + 28] !== 0;
      var shielding =
        view[base + 4] === 18 ||
        view[base + 4] === 19 ||
        (view[base + 4] === 13 && view[base + 26] > 0);

      context.globalAlpha = eliminated ? 0.12 : respawning ? 0.32 : 1;
      if (view[base + 14]) {
        var hitboxLeft = sx(view[base + 15]);
        var hitboxRight = sx(view[base + 16]);
        var hitboxTop = sy(view[base + 17]);
        var hitboxBottom = sy(view[base + 18]);

        context.fillStyle =
          actionState === 22 || actionState === 41
            ? "#ff5f874d"
            : actionState === 26
              ? "#b977ff55"
              : "#ffb34744";
        context.strokeStyle =
          actionState === 22 || actionState === 41
            ? "#ff8cab"
            : actionState === 26
              ? "#d7adff"
              : "#ffd089";
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

      if (view[base + 35]) {
        var grabboxLeft = sx(view[base + 36]);
        var grabboxRight = sx(view[base + 37]);
        var grabboxTop = sy(view[base + 38]);
        var grabboxBottom = sy(view[base + 39]);

        context.fillStyle = "#62e7ff3d";
        context.strokeStyle = "#8cf3ff";
        context.lineWidth = 2;
        context.fillRect(
          grabboxLeft,
          grabboxTop,
          grabboxRight - grabboxLeft,
          grabboxBottom - grabboxTop
        );
        context.strokeRect(
          grabboxLeft,
          grabboxTop,
          grabboxRight - grabboxLeft,
          grabboxBottom - grabboxTop
        );
      }

      if (shielding) {
        var shieldFraction = Math.max(
          0,
          Math.min(1, view[base + 25] / (60 * q16))
        );
        var shieldRadius =
          Math.max(width, height) * (0.72 + shieldFraction * 0.2);
        context.fillStyle =
          view[base + 27] !== 0 ? "#f7fbff55" : colors[playerIndex] + "33";
        context.strokeStyle =
          view[base + 27] !== 0 ? "#ffffff" : colors[playerIndex];
        context.lineWidth = view[base + 27] !== 0 ? 4 : 2;
        context.beginPath();
        context.arc(x, y, shieldRadius, 0, Math.PI * 2);
        context.fill();
        context.stroke();
      }

      context.save();
      if (prone) {
        var standingHeight = height;
        width = Math.max(width * 1.35, height * 1.15);
        height = Math.max(12, width * 0.28);
        y += (standingHeight - height) / 2;
      }
      context.translate(x, y);
      if (tumbling) {
        context.rotate(
          (view[1] % 24) *
            (Math.PI / 12) *
            (playerIndex === 0 ? 1 : -1)
        );
      }
      context.shadowColor = colors[playerIndex] + "88";
      context.shadowBlur = tumbling ? 24 : 16;
      context.fillStyle =
        actionState === 13 ? "#ffffff" : colors[playerIndex];
      context.fillRect(-width / 2, -height / 2, width, height);
      context.shadowBlur = 0;
      context.fillStyle = "#07111c";
      context.beginPath();
      context.moveTo(facing * width * 0.45, -5);
      context.lineTo(facing * width * 0.75, 0);
      context.lineTo(facing * width * 0.45, 5);
      context.closePath();
      context.fill();
      context.restore();
      if (invulnerable) {
        context.strokeStyle = "#fff6a8";
        context.lineWidth = 3;
        context.setLineDash([5, 4]);
        context.beginPath();
        context.arc(
          x,
          y,
          Math.max(width, height) * 0.72,
          0,
          Math.PI * 2
        );
        context.stroke();
        context.setLineDash([]);
      }
      if (actionState === 48) {
        var orbit = (view[1] % 60) * (Math.PI / 30);
        var starRadius = Math.max(width, height) * 0.92;
        var starIndex;

        context.save();
        context.fillStyle = "#fff19a";
        context.font = "bold 20px system-ui, sans-serif";
        context.textAlign = "center";
        context.textBaseline = "middle";
        for (starIndex = 0; starIndex < 3; starIndex += 1) {
          var starAngle = orbit + starIndex * ((Math.PI * 2) / 3);
          context.fillText(
            "✦",
            x + Math.cos(starAngle) * starRadius,
            y - height * 0.7 + Math.sin(starAngle) * starRadius * 0.28
          );
        }
        context.fillStyle = "#351f09dd";
        context.strokeStyle = "#fff19a";
        context.lineWidth = 2;
        context.font = "bold 14px ui-monospace, monospace";
        var mashLabel = "MASH · " + view[base + 29] + "f";
        var mashWidth = context.measureText(mashLabel).width + 16;
        var mashY = Math.max(24, y - height / 2 - 36);
        context.fillRect(x - mashWidth / 2, mashY - 12, mashWidth, 24);
        context.strokeRect(x - mashWidth / 2, mashY - 12, mashWidth, 24);
        context.fillStyle = "#fffbd2";
        context.fillText(mashLabel, x, mashY);
        context.restore();
      }
      if (actionState === 51) {
        var escapeLabel = "MASH OUT · " + view[base + 40] + "f";
        var escapeLabelY = Math.max(24, y - height / 2 - 36);
        var escapeLabelWidth;

        context.save();
        context.fillStyle = "#15364ddd";
        context.strokeStyle = "#8cf3ff";
        context.lineWidth = 2;
        context.font = "bold 14px ui-monospace, monospace";
        context.textAlign = "center";
        context.textBaseline = "middle";
        escapeLabelWidth = context.measureText(escapeLabel).width + 16;
        context.fillRect(
          x - escapeLabelWidth / 2,
          escapeLabelY - 12,
          escapeLabelWidth,
          24
        );
        context.strokeRect(
          x - escapeLabelWidth / 2,
          escapeLabelY - 12,
          escapeLabelWidth,
          24
        );
        context.fillStyle = "#dffaff";
        context.fillText(escapeLabel, x, escapeLabelY);
        context.restore();
      }
      context.globalAlpha = 1;

      if (respawning) {
        context.save();
        context.fillStyle = "#fff6a8";
        context.font = "bold 15px ui-monospace, monospace";
        context.textAlign = "center";
        context.fillText(
          "RESPAWN " + view[base + 33] + "f",
          x,
          Math.max(22, y - height / 2 - 18)
        );
        context.restore();
      }

      var landingFeedback = null;
      var landingLagTotal = 0;
      var landingSucceeded = false;
      if (actionState === 36) {
        landingFeedback = "MISSED L-CANCEL";
        landingLagTotal = state.aerialLandingLagTicks;
      } else if (actionState === 37) {
        landingFeedback = "L-CANCEL!";
        landingLagTotal = Math.max(
          1,
          Math.floor(state.aerialLandingLagTicks / 2)
        );
        landingSucceeded = true;
      } else if (actionState === 42) {
        landingFeedback = "MISSED STRONG L-CANCEL";
        landingLagTotal = state.strongAerialLandingLagTicks;
      } else if (actionState === 43) {
        landingFeedback = "STRONG L-CANCEL!";
        landingLagTotal = Math.max(
          1,
          Math.floor(state.strongAerialLandingLagTicks / 2)
        );
        landingSucceeded = true;
      }
      if (landingFeedback !== null) {
        var landingLagRemaining = Math.max(
          0,
          landingLagTotal - view[base + 29]
        );
        var landingLabel =
          landingFeedback + " · " + landingLagRemaining + "f";
        var landingLabelY = Math.max(24, y - height / 2 - 34);
        var landingLabelWidth;

        context.save();
        context.font = "bold 16px ui-monospace, monospace";
        context.textAlign = "center";
        context.textBaseline = "middle";
        landingLabelWidth = context.measureText(landingLabel).width + 18;
        context.fillStyle = landingSucceeded ? "#0b4f3fee" : "#64263aee";
        context.strokeStyle = landingSucceeded ? "#79ffd3" : "#ff9ab0";
        context.lineWidth = 2;
        context.fillRect(
          x - landingLabelWidth / 2,
          landingLabelY - 13,
          landingLabelWidth,
          26
        );
        context.strokeRect(
          x - landingLabelWidth / 2,
          landingLabelY - 13,
          landingLabelWidth,
          26
        );
        context.fillStyle = landingSucceeded ? "#baffea" : "#ffe1e8";
        context.fillText(landingLabel, x, landingLabelY);
        context.strokeStyle = landingSucceeded ? "#79ffd3" : "#ff6f91";
        context.lineWidth = 4;
        context.beginPath();
        context.arc(x, y, Math.max(width, height) * 0.88, 0, Math.PI * 2);
        context.stroke();
        context.restore();
      }

      var action =
        actionNames[actionState] || "STATE " + actionState;
      if (view[base + 22] !== 0) {
        action = "TUMBLE · " + action;
      }
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
        " · stocks " +
        view[base + 32] +
        "<br>respawn wait " +
        view[base + 33] +
        "f · respawn invulnerability " +
        view[base + 34] +
        "f" +
        "<br>damage " +
        (view[base + 11] / q16).toFixed(1) +
        "% · hitlag " +
        view[base + 12] +
        " · hitstun " +
        view[base + 13] +
        " · hit event " +
        view[base + 19] +
        "<br>tumble " +
        view[base + 22] +
        " · SDI pulses " +
        view[base + 23] +
        " · tech window " +
        view[base + 20] +
        " · lockout " +
        view[base + 21] +
        " · tech direction " +
        view[base + 24] +
        "<br>shield " +
        (view[base + 25] / q16).toFixed(2) +
        " / 60 · shield stun " +
        view[base + 26] +
        " · powershield " +
        view[base + 27] +
        " · invulnerable " +
        view[base + 28] +
        " · action tick " +
        view[base + 29] +
        "<br>L-cancel trigger age " +
        (view[base + 30] === 255 ? "not armed" : view[base + 30]) +
        " · eligible " +
        view[base + 31] +
        "<br>grab target " +
        (view[base + 41] === 255 ? "none" : "P" + (view[base + 41] + 1)) +
        " · grab owner " +
        (view[base + 42] === 255 ? "none" : "P" + (view[base + 42] + 1)) +
        " · escape " +
        view[base + 40] +
        "f · Arc Reservoir " +
        view[base + 43] +
        " / 120f";
    });

    var itemStateNames = [
      "INACTIVE",
      "GROUND",
      "HELD",
      "AIRBORNE",
      "RESPAWN WAIT",
    ];
    var itemDirectionNames = ["none", "forward", "back", "up", "down"];
    state.itemState.innerHTML =
      "<strong>Relay Rod · " +
      (itemStateNames[itemStateCode] || "STATE " + itemStateCode) +
      "</strong><br>x " +
      (view[itemBase + 6] / q16).toFixed(3) +
      " · y " +
      (view[itemBase + 7] / q16).toFixed(3) +
      " · vx " +
      (view[itemBase + 8] / q16).toFixed(3) +
      " · vy " +
      (view[itemBase + 9] / q16).toFixed(3) +
      "<br>holder " +
      (view[itemBase + 2] === 255
        ? "none"
        : "P" + (view[itemBase + 2] + 1)) +
      " · source " +
      (view[itemBase + 3] === 255
        ? "none"
        : "P" + (view[itemBase + 3] + 1)) +
      " · throw " +
      (itemDirectionNames[view[itemBase + 4]] || view[itemBase + 4]) +
      " · hitbox " +
      view[itemBase + 5] +
      "<br>lifetime " +
      view[itemBase + 10] +
      "f · respawn " +
      view[itemBase + 11] +
      "f · pickup lockout " +
      view[itemBase + 12] +
      "f · hit mask " +
      view[itemBase + 13];

    var projectileStateNames = ["INACTIVE", "SPAWNING", "ACTIVE"];
    state.projectileState.innerHTML =
      "<strong>Pulse Bolt · " +
      (projectileStateNames[projectileStateCode] ||
        "STATE " + projectileStateCode) +
      "</strong><br>x " +
      (view[projectileBase + 4] / q16).toFixed(3) +
      " · y " +
      (view[projectileBase + 5] / q16).toFixed(3) +
      " · vx " +
      (view[projectileBase + 6] / q16).toFixed(3) +
      " · vy " +
      (view[projectileBase + 7] / q16).toFixed(3) +
      "<br>owner " +
      (view[projectileBase + 2] === 255
        ? "none"
        : "P" + (view[projectileBase + 2] + 1)) +
      " · hitbox " +
      view[projectileBase + 3] +
      " · lifetime " +
      view[projectileBase + 8] +
      "f · powershield reflect window " +
      view[projectileBase + 11] +
      "f";

    if (view[21] !== 0 || view[22] !== 0 || view[23] !== 0) {
      var resultLabel;
      var resultColor;

      if (view[22] !== 0) {
        if (
          state.teamLabActive &&
          (view[24] & 5) !== 0 &&
          (view[24] & 10) === 0
        ) {
          resultLabel = "TEAM P1/P3 WINS";
          resultColor = colors[0];
        } else if (
          state.teamLabActive &&
          (view[24] & 10) !== 0 &&
          (view[24] & 5) === 0
        ) {
          resultLabel = "TEAM P2/P4 WINS";
          resultColor = colors[1];
        } else if ((view[24] & 1) !== 0 && (view[24] & 2) === 0) {
          resultLabel = "PLAYER 1 WINS";
          resultColor = colors[0];
        } else if ((view[24] & 2) !== 0 && (view[24] & 1) === 0) {
          resultLabel = "PLAYER 2 WINS";
          resultColor = colors[1];
        } else {
          resultLabel = "MATCH DRAW";
          resultColor = "#f3f7ff";
        }
      } else if (view[23] !== 0) {
        resultLabel = "TIME LIMIT";
        resultColor = "#f3f7ff";
      } else {
        resultLabel = "SUDDEN DEATH · 300%";
        resultColor = "#ffd166";
      }

      context.save();
      context.fillStyle = "#07101add";
      context.fillRect(canvas.width / 2 - 190, 48, 380, 48);
      context.strokeStyle = resultColor;
      context.lineWidth = 3;
      context.strokeRect(canvas.width / 2 - 190, 48, 380, 48);
      context.fillStyle = resultColor;
      context.font = "bold 22px ui-monospace, monospace";
      context.textAlign = "center";
      context.textBaseline = "middle";
      context.fillText(resultLabel, canvas.width / 2, 72);
      context.restore();
    }

    context.fillStyle = "#8da2bb";
    context.font = "12px ui-monospace, monospace";
    context.textAlign = "left";
    context.fillText("blast zone", padding + 8, padding + 18);
    context.textAlign = "right";
    context.fillText("real sim · Q16.16 · 60 Hz", canvas.width - padding, 22);

    state.tickLabel.textContent =
      "tick " +
      view[1] +
      " · fixed 60 Hz · " +
      view[18] +
      "-stock " +
      (state.teamLabActive ? "Team Wobble lab" : "duel");
    state.resetButton.textContent = view[22] !== 0 ? "Rematch" : "Reset";
    if (view[22] !== 0 || view[23] !== 0) {
      state.running = false;
      state.pauseButton.textContent = "Resume";
      state.pauseButton.setAttribute("aria-pressed", "true");
    }
  },
});
