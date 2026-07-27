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
});
