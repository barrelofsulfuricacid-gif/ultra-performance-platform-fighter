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
  pf_web_replay_inspector__sig: "vpppppiiiipi",
  pf_web_replay_inspector: function (
    positionsPointer,
    hashesPointer,
    eventCountsPointer,
    eventValuesPointer,
    replayBytesPointer,
    replaySize,
    tickCount,
    playerCount,
    winnerMask,
    finalHashPointer,
    imported
  ) {
    var checkpointCount = tickCount + 1;
    var positionCount = checkpointCount * playerCount * 2;
    var hashCount = checkpointCount * 32;
    var eventValueCount = checkpointCount * 16 * 10;
    var positions = new Int32Array(
      HEAP32.subarray(
        positionsPointer >> 2,
        (positionsPointer >> 2) + positionCount
      )
    );
    var hashes = new Uint8Array(
      HEAPU8.subarray(hashesPointer, hashesPointer + hashCount)
    );
    var eventCounts = new Int32Array(
      HEAP32.subarray(
        eventCountsPointer >> 2,
        (eventCountsPointer >> 2) + checkpointCount
      )
    );
    var eventValues = new Int32Array(
      HEAP32.subarray(
        eventValuesPointer >> 2,
        (eventValuesPointer >> 2) + eventValueCount
      )
    );
    var replayBytes = new Uint8Array(
      HEAPU8.subarray(replayBytesPointer, replayBytesPointer + replaySize)
    );
    var finalHash = UTF8ToString(finalHashPointer);
    var importedReplay = imported !== 0;
    var importName = importedReplay
      ? Module.pfReplayImportName || "imported replay"
      : "generated canonical replay";
    var totalEventCount = 0;
    var eventTick;
    for (eventTick = 0; eventTick < checkpointCount; ++eventTick) {
      totalEventCount += Math.max(0, Math.min(16, eventCounts[eventTick]));
    }
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
         "font-size:12px}.pf-replay-file-controls{display:flex;gap:10px;" +
         "align-items:center;flex-wrap:wrap;margin:0 0 16px}.pf-replay-file-controls button," +
         ".pf-replay-file-label{border:1px solid #3a5778;border-radius:8px;" +
         "padding:7px 10px;background:#16243a;color:#dcecff;cursor:pointer;" +
         "font:600 12px/1.2 system-ui}.pf-replay-file-label input{position:absolute;" +
         "width:1px;height:1px;overflow:hidden;clip:rect(0,0,0,0)}" +
         ".pf-replay-file-status{color:#91a5bf;font:12px/1.4 system-ui}" +
         ".pf-replay-event-nav{display:flex;align-items:center;gap:8px;" +
         "margin-top:14px}.pf-replay-event-nav button{border:1px solid #344966;" +
         "border-radius:7px;background:#142033;color:#d8e8ff;padding:5px 8px}" +
         ".pf-replay-event-nav button:disabled{opacity:.4}" +
         "#pf-replay-events{display:grid;gap:7px;list-style:none;padding:0;" +
         "margin:10px 0 0}#pf-replay-events li{display:grid;grid-template-columns:auto 1fr;" +
         "gap:10px;padding:8px 10px;border:1px solid #263249;border-radius:8px;" +
         "background:#0c1320}#pf-replay-events code{color:#78d7ff}" +
         "#pf-replay-events span{font:13px/1.35 system-ui;color:#c3d0e3}" +
         ".pf-help{font-size:13px!important;margin-top:14px!important}" +
         "@media(max-width:600px){body{padding:12px}" +
        "#pf-replay-inspector{padding:16px}.pf-row{align-items:flex-start;" +
        "flex-direction:column;gap:4px}}";
      document.head.appendChild(style);
    }

    var inspector = document.createElement("section");
    inspector.id = "pf-replay-inspector";
    inspector.dataset.replaySource = importedReplay ? "file" : "generated";
    inspector.dataset.replayEventVisualization = "verified-per-tick-events";
    var title = document.createElement("h1");
    title.textContent = "Deterministic replay file inspector";
    inspector.appendChild(title);
    var summary = document.createElement("p");
    summary.textContent =
      "The authored C simulation verifies and re-simulates this compatible " +
      "replay inside WebAssembly, including its typed per-tick event journal.";
    inspector.appendChild(summary);

    var badges = document.createElement("div");
    badges.className = "pf-badges";
    [
      "verified replay",
      tickCount + " ticks",
      playerCount + " players",
      "winner mask " + winnerMask,
      totalEventCount + " typed events",
      importedReplay ? "opened from file" : "generated in browser",
      "60 Hz",
    ].forEach(function (label) {
      var badge = document.createElement("span");
      badge.className = "pf-badge";
      badge.textContent = label;
      badges.appendChild(badge);
    });
    inspector.appendChild(badges);

    var fileControls = document.createElement("div");
    fileControls.className = "pf-replay-file-controls";
    var downloadButton = document.createElement("button");
    downloadButton.type = "button";
    downloadButton.textContent = "Download verified replay";
    var fileLabel = document.createElement("label");
    fileLabel.className = "pf-replay-file-label";
    fileLabel.textContent = "Open replay file";
    var fileInput = document.createElement("input");
    fileInput.id = "pf-replay-file";
    fileInput.type = "file";
    fileInput.accept = ".pfreplay,application/octet-stream";
    fileInput.setAttribute("aria-label", "Open compatible replay file");
    fileLabel.appendChild(fileInput);
    var fileStatus = document.createElement("span");
    fileStatus.id = "pf-replay-file-status";
    fileStatus.className = "pf-replay-file-status";
    fileStatus.setAttribute("role", "status");
    fileStatus.textContent = importedReplay
      ? importName + " verified and visualized"
      : "Compatible canonical files up to 1 MiB are verified before display.";
    fileControls.appendChild(downloadButton);
    fileControls.appendChild(fileLabel);
    fileControls.appendChild(fileStatus);
    inspector.appendChild(fileControls);

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

    var eventNav = document.createElement("div");
    eventNav.className = "pf-replay-event-nav";
    var previousEventButton = document.createElement("button");
    previousEventButton.type = "button";
    previousEventButton.textContent = "Previous event";
    var nextEventButton = document.createElement("button");
    nextEventButton.type = "button";
    nextEventButton.textContent = "Next event";
    var eventHeading = document.createElement("strong");
    eventHeading.textContent = "Events at tick 0";
    eventNav.appendChild(previousEventButton);
    eventNav.appendChild(nextEventButton);
    eventNav.appendChild(eventHeading);
    inspector.appendChild(eventNav);
    var eventList = document.createElement("ul");
    eventList.id = "pf-replay-events";
    eventList.setAttribute("aria-label", "Verified replay events at selected tick");
    inspector.appendChild(eventList);

    var help = document.createElement("p");
    help.className = "pf-help";
    help.textContent =
      "Drag the timeline or jump between event ticks. Positions, per-tick " +
      "SHA-256 hashes, and typed events are emitted by verified re-simulation; " +
      "unverified or incompatible files never replace the current trace.";
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

    var replayEventNames = [
      "NONE",
      "HIT",
      "SHIELD BLOCK",
      "POWERSHIELD",
      "SHIELD BREAK",
      "KO",
      "RESPAWN",
      "SUDDEN DEATH",
      "MATCH RESULT",
      "FORFEIT",
      "TIME LIMIT",
      "GRAB",
      "GRAB ESCAPE",
      "THROW",
      "ITEM PICKUP",
      "ITEM DROP",
      "ITEM THROW",
      "ITEM HIT",
      "ITEM RESET",
      "PROJECTILE FIRE",
      "PROJECTILE HIT",
      "PROJECTILE REFLECT",
      "PUMMEL",
      "REVIVAL DROP",
      "ACTION TRANSITIONS",
    ];
    var replayEventTicks = [];
    for (eventTick = 0; eventTick < checkpointCount; ++eventTick) {
      if (eventCounts[eventTick] > 0) {
        replayEventTicks.push(eventTick);
      }
    }

    function replayEventPlayer(slot) {
      return slot === 255 ? "system" : "P" + (slot + 1);
    }

    function replayEventMaskPlayers(mask) {
      var players = [];
      var slot;

      for (slot = 0; slot < 4; ++slot) {
        if ((mask & (1 << slot)) !== 0) {
          players.push("P" + (slot + 1));
        }
      }
      return players.join(" + ");
    }

    function replayEventsAtTick(tick) {
      var count = Math.max(0, Math.min(16, eventCounts[tick]));
      var events = [];
      var eventIndex;

      for (eventIndex = 0; eventIndex < count; ++eventIndex) {
        var base = (tick * 16 + eventIndex) * 10;
        events.push({
          sequence: eventValues[base],
          tick: eventValues[base + 1],
          type: eventValues[base + 2],
          source: eventValues[base + 3],
          target: eventValues[base + 4],
          value: eventValues[base + 5],
          velocityX: eventValues[base + 6],
          velocityY: eventValues[base + 7],
          flags: eventValues[base + 8],
          detail: eventValues[base + 9],
        });
      }
      return events;
    }

    function replayEventDescription(event) {
      var label = replayEventNames[event.type] || "EVENT " + event.type;
      var source = replayEventPlayer(event.source);
      var target = replayEventPlayer(event.target);
      var value = (event.value / 65536).toFixed(2);
      var velocity =
        "(" +
        (event.velocityX / 65536).toFixed(2) +
        ", " +
        (event.velocityY / 65536).toFixed(2) +
        ")";

      if (event.type === 9) {
        return "FORFEIT · " + replayEventMaskPlayers(event.detail);
      }
      if (event.type === 24) {
        var previousActions = event.velocityX >>> 0;
        var nextActions = event.value >>> 0;
        var transitions = [];
        var slot;

        for (slot = 0; slot < 4; ++slot) {
          if ((event.detail & (1 << slot)) !== 0) {
            transitions.push(
              "P" +
                (slot + 1) +
                " action " +
                ((previousActions >>> (slot * 8)) & 255) +
                " → " +
                ((nextActions >>> (slot * 8)) & 255)
            );
          }
        }
        return label + " · " + transitions.join(" · ");
      }

      return (
        label +
        " · " +
        source +
        " → " +
        target +
        " · value " +
        value +
        " · velocity " +
        velocity +
        " · flags " +
        event.flags +
        ((event.flags & 16) !== 0 ? " · CROUCH CANCEL" : "") +
        " · detail " +
        event.detail
      );
    }

    function renderReplayEvents(tick) {
      var events = replayEventsAtTick(tick);
      var hasPrevious = false;
      var hasNext = false;

      eventHeading.textContent = "Events entering checkpoint " + tick;
      eventList.textContent = "";
      if (events.length === 0) {
        var emptyEvent = document.createElement("li");
        var emptyText = document.createElement("span");
        emptyText.textContent = "No typed events at this checkpoint.";
        emptyEvent.appendChild(emptyText);
        eventList.appendChild(emptyEvent);
      } else {
        events.forEach(function (event) {
          var item = document.createElement("li");
          var key = document.createElement("code");
          var description = document.createElement("span");
          key.textContent = "#" + event.sequence + " · input tick " + event.tick;
          description.textContent = replayEventDescription(event);
          item.appendChild(key);
          item.appendChild(description);
          eventList.appendChild(item);
        });
      }
      replayEventTicks.forEach(function (candidate) {
        hasPrevious = hasPrevious || candidate < tick;
        hasNext = hasNext || candidate > tick;
      });
      previousEventButton.disabled = !hasPrevious;
      nextEventButton.disabled = !hasNext;
    }

    function jumpToReplayEvent(direction) {
      var current = Number(slider.value);
      var target = null;
      var index;

      if (direction < 0) {
        for (index = replayEventTicks.length - 1; index >= 0; --index) {
          if (replayEventTicks[index] < current) {
            target = replayEventTicks[index];
            break;
          }
        }
      } else {
        for (index = 0; index < replayEventTicks.length; ++index) {
          if (replayEventTicks[index] > current) {
            target = replayEventTicks[index];
            break;
          }
        }
      }
      if (target !== null) {
        slider.value = String(target);
        draw(target);
      }
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
      renderReplayEvents(tick);
    }

    downloadButton.addEventListener("click", function () {
      var blob = new Blob([replayBytes], { type: "application/octet-stream" });
      var url = URL.createObjectURL(blob);
      var anchor = document.createElement("a");
      anchor.href = url;
      anchor.download = importedReplay
        ? "verified-imported-replay.pfreplay"
        : "platform-fighter-canonical.pfreplay";
      document.body.appendChild(anchor);
      anchor.click();
      anchor.remove();
      URL.revokeObjectURL(url);
    });
    fileInput.addEventListener("change", async function () {
      var file = fileInput.files && fileInput.files[0];
      var pointer = 0;
      var statusNames = [
        "ok",
        "invalid argument",
        "unsupported version",
        "buffer too small",
        "misaligned memory",
        "invalid config",
        "tick mismatch",
        "episode done",
        "invalid state",
        "deterministic fault",
        "incompatible state",
        "checksum mismatch",
        "replay mismatch",
      ];

      if (!file) {
        return;
      }
      if (file.size === 0 || file.size > 1024 * 1024) {
        fileStatus.textContent = "Replay must be between 1 byte and 1 MiB.";
        return;
      }
      fileStatus.textContent = "Verifying " + file.name + "…";
      try {
        var fileBytes = new Uint8Array(await file.arrayBuffer());
        pointer = Module._malloc(fileBytes.byteLength);
        if (!pointer) {
          throw new Error("WebAssembly allocation failed");
        }
        HEAPU8.set(fileBytes, pointer);
        Module.pfReplayImportName = file.name;
        var importStatus = Module._pf_web_replay_import(
          pointer,
          fileBytes.byteLength
        );
        if (importStatus !== 0) {
          var activeStatus = document.getElementById("pf-replay-file-status");
          if (activeStatus) {
            activeStatus.textContent =
              file.name +
              " rejected: " +
              (statusNames[importStatus] || "status " + importStatus);
          }
        }
      } catch (error) {
        var failureStatus = document.getElementById("pf-replay-file-status");
        if (failureStatus) {
          failureStatus.textContent =
            file.name + " could not be read: " + String(error.message || error);
        }
      } finally {
        if (pointer) {
          Module._free(pointer);
        }
      }
    });
    previousEventButton.addEventListener("click", function () {
      jumpToReplayEvent(-1);
    });
    nextEventButton.addEventListener("click", function () {
      jumpToReplayEvent(1);
    });
    slider.addEventListener("input", function () {
      draw(Number(slider.value));
    });
    draw(0);

    if (status) {
      if (!importedReplay) {
        status.textContent +=
          " replay=pass ticks=" +
          tickCount +
          " winner_mask=" +
          winnerMask +
          " final_sha256=" +
          finalHash;
      }
      status.dataset.replay = "pass";
      status.dataset.replayEventVisualization = "pass";
    }
  },

  pf_web_m4_playtest_install__sig: "viiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
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
    vectorAscentProbePassed,
    aerialLandingLagTicks,
    strongAerialLandingLagTicks
  ) {
    function emptyGamepadInput() {
      return {
        horizontal: 0,
        vertical: 0,
        secondaryHorizontal: 0,
        secondaryVertical: 0,
        jump: false,
        attack: false,
        strongAttack: false,
        special: false,
        taunt: false,
        shield: false,
        shieldStrength: 0,
        leftShieldStrength: 0,
        rightShieldStrength: 0,
      };
    }

    function gamepadButtonValue(gamepad, index) {
      var button =
        gamepad && gamepad.buttons && index < gamepad.buttons.length
          ? gamepad.buttons[index]
          : null;
      if (button === null || button === undefined) {
        return 0;
      }
      var value = Number(button.value);
      if (!Number.isFinite(value)) {
        value = button.pressed === true ? 1 : 0;
      }
      return Math.max(0, Math.min(1, value));
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

    function gamepadRawAxis(gamepad, index) {
      var value =
        gamepad && gamepad.axes && index < gamepad.axes.length
          ? Number(gamepad.axes[index])
          : 0;
      return Number.isFinite(value) ? value : 0;
    }

    function gamepadAxis(gamepad, index) {
      var value = Math.max(-1, Math.min(1, gamepadRawAxis(gamepad, index)));
      if (Math.abs(value) < 0.2) {
        return 0;
      }
      var magnitude = Math.round(Math.abs(value) * dashAxis);
      return value < 0 ? -magnitude : magnitude;
    }

    function mayflashStickAxis(gamepad, index) {
      var value = Math.max(-1, Math.min(1, gamepadRawAxis(gamepad, index)));
      if (Math.abs(value) < 0.2) {
        return 0;
      }
      var magnitude = Math.round(
        Math.min(1, Math.abs(value) / 0.75) * dashAxis
      );
      return value < 0 ? -magnitude : magnitude;
    }

    function isMayflashGameCubeAdapter(gamepad) {
      var id = gamepad && gamepad.id ? String(gamepad.id).toLowerCase() : "";
      return (
        id.indexOf("mayflash gamecube controller adapter") !== -1 ||
        (id.indexOf("0079") !== -1 && id.indexOf("1843") !== -1)
      );
    }

    function mayflashPortHasController(gamepad) {
      var index;
      for (index = 0; gamepad.buttons && index < gamepad.buttons.length; ++index) {
        if (gamepadButtonPressed(gamepad, index)) {
          return true;
        }
      }

      var centeredStickAxes = [0, 1, 2, 5].filter(function (axisIndex) {
        return Math.abs(gamepadRawAxis(gamepad, axisIndex)) < 0.85;
      }).length;
      return centeredStickAxes >= 2;
    }

    function mayflashTriggerValue(gamepad, index) {
      var value = (gamepadRawAxis(gamepad, index) + 1) / 2;
      if (value <= 0.15) {
        return 0;
      }
      return Math.min(1, (value - 0.15) / 0.85);
    }

    function mayflashDpad(gamepad) {
      var up = gamepadButtonPressed(gamepad, 12);
      var right = gamepadButtonPressed(gamepad, 13);
      var down = gamepadButtonPressed(gamepad, 14);
      var left = gamepadButtonPressed(gamepad, 15);
      var hat = gamepadRawAxis(gamepad, 9);
      var hasHatAxis =
        gamepad && gamepad.axes && gamepad.axes.length > 9;
      if (
        !up &&
        !right &&
        !down &&
        !left &&
        hasHatAxis &&
        hat >= -1 &&
        hat <= 1.05
      ) {
        var direction = Math.max(
          0,
          Math.min(7, Math.round(((hat + 1) / 2) * 7))
        );
        up = direction === 0 || direction === 1 || direction === 7;
        right = direction >= 1 && direction <= 3;
        down = direction >= 3 && direction <= 5;
        left = direction >= 5 && direction <= 7;
      }
      return { up: up, right: right, down: down, left: left };
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
      var cStickX = gamepadAxis(gamepad, 2);
      var cStickY = gamepadAxis(gamepad, 3);
      if (cStickX !== 0 || cStickY !== 0) {
        input.secondaryHorizontal = cStickX;
        input.secondaryVertical = cStickY;
        input.strongAttack = true;
      }
      input.attack = gamepadButtonPressed(gamepad, 0);
      input.strongAttack =
        input.strongAttack || gamepadButtonPressed(gamepad, 1);
      input.jump = gamepadButtonPressed(gamepad, 2);
      input.special = gamepadButtonPressed(gamepad, 3);
      input.taunt = gamepadButtonPressed(gamepad, 8);
      input.leftShieldStrength = gamepadButtonPressed(gamepad, 4)
        ? 65535
        : Math.round(gamepadButtonValue(gamepad, 6) * 65535);
      input.rightShieldStrength = gamepadButtonPressed(gamepad, 5)
        ? 65535
        : Math.round(gamepadButtonValue(gamepad, 7) * 65535);
      input.shieldStrength = Math.max(
        input.leftShieldStrength,
        input.rightShieldStrength
      );
      input.shield = input.shieldStrength !== 0;
      return input;
    }

    function mapMayflashGameCubeAdapter(gamepad) {
      var input = emptyGamepadInput();
      if (
        !gamepad ||
        gamepad.connected === false ||
        !isMayflashGameCubeAdapter(gamepad) ||
        !mayflashPortHasController(gamepad)
      ) {
        return input;
      }

      input.horizontal = mayflashStickAxis(gamepad, 0);
      input.vertical = mayflashStickAxis(gamepad, 1);
      var dpad = mayflashDpad(gamepad);
      if (dpad.left || dpad.right) {
        input.horizontal =
          dpad.left === dpad.right ? 0 : dpad.left ? -dashAxis : dashAxis;
      }
      if (dpad.up || dpad.down) {
        input.vertical =
          dpad.up === dpad.down ? 0 : dpad.up ? -dashAxis : dashAxis;
      }

      var cStickX = mayflashStickAxis(gamepad, 5);
      var cStickY = mayflashStickAxis(gamepad, 2);
      if (cStickX !== 0 || cStickY !== 0) {
        input.secondaryHorizontal = cStickX;
        input.secondaryVertical = cStickY;
        input.strongAttack = true;
      }

      var zPressed = gamepadButtonPressed(gamepad, 7);
      input.attack = gamepadButtonPressed(gamepad, 1) || zPressed;
      input.special = gamepadButtonPressed(gamepad, 2);
      input.jump =
        gamepadButtonPressed(gamepad, 0) ||
        gamepadButtonPressed(gamepad, 3);
      input.taunt = gamepadButtonPressed(gamepad, 9);
      input.leftShieldStrength =
        gamepadButtonPressed(gamepad, 4) || zPressed
          ? 65535
          : Math.round(mayflashTriggerValue(gamepad, 3) * 65535);
      input.rightShieldStrength = gamepadButtonPressed(gamepad, 5)
        ? 65535
        : Math.round(mayflashTriggerValue(gamepad, 4) * 65535);
      input.shieldStrength = Math.max(
        input.leftShieldStrength,
        input.rightShieldStrength
      );
      input.shield = input.shieldStrength !== 0;
      return input;
    }

    var wiiUAdapterState = {
      available:
        typeof navigator !== "undefined" &&
        navigator.usb !== undefined &&
        typeof navigator.usb.requestDevice === "function",
      connectedPorts: 0,
      device: null,
      error: "",
      generation: 0,
      inputEndpoint: 0,
      inputs: [
        emptyGamepadInput(),
        emptyGamepadInput(),
        emptyGamepadInput(),
        emptyGamepadInput(),
      ],
      outputEndpoint: 0,
      portConnected: [false, false, false, false],
      status: "disconnected",
    };

    function wiiUAdapterAxis(rawValue, invert) {
      var delta = Number(rawValue) - 128;
      if (invert) {
        delta = -delta;
      }
      if (Math.abs(delta) < 16) {
        return 0;
      }
      var magnitude = Math.round(
        Math.min(1, Math.abs(delta) / 80) * dashAxis
      );
      return delta < 0 ? -magnitude : magnitude;
    }

    function wiiUAdapterTrigger(rawValue) {
      var value = Math.max(0, Math.min(255, Number(rawValue))) / 255;
      if (value <= 0.15) {
        return 0;
      }
      return Math.round(((value - 0.15) / 0.85) * 65535);
    }

    function parseWiiUAdapterReport(report) {
      var parsed = {
        connectedPorts: 0,
        inputs: [
          emptyGamepadInput(),
          emptyGamepadInput(),
          emptyGamepadInput(),
          emptyGamepadInput(),
        ],
        portConnected: [false, false, false, false],
        valid: false,
      };
      if (!report || report.length < 37 || report[0] !== 0x21) {
        return parsed;
      }

      parsed.valid = true;
      for (var port = 0; port < 4; ++port) {
        var base = 1 + port * 9;
        var controllerType = report[base] >> 4;
        if (controllerType === 0) {
          continue;
        }

        var input = parsed.inputs[port];
        var buttons1 = report[base + 1];
        var buttons2 = report[base + 2];
        var dpadLeft = (buttons1 & 0x10) !== 0;
        var dpadRight = (buttons1 & 0x20) !== 0;
        var dpadDown = (buttons1 & 0x40) !== 0;
        var dpadUp = (buttons1 & 0x80) !== 0;
        var zPressed = (buttons2 & 0x02) !== 0;
        var digitalRight = (buttons2 & 0x04) !== 0;
        var digitalLeft = (buttons2 & 0x08) !== 0;

        input.horizontal = wiiUAdapterAxis(report[base + 3], false);
        input.vertical = wiiUAdapterAxis(report[base + 4], true);
        if (dpadLeft || dpadRight) {
          input.horizontal =
            dpadLeft === dpadRight ? 0 : dpadLeft ? -dashAxis : dashAxis;
        }
        if (dpadUp || dpadDown) {
          input.vertical =
            dpadUp === dpadDown ? 0 : dpadUp ? -dashAxis : dashAxis;
        }

        input.secondaryHorizontal = wiiUAdapterAxis(
          report[base + 5],
          false
        );
        input.secondaryVertical = wiiUAdapterAxis(
          report[base + 6],
          true
        );
        input.strongAttack =
          input.secondaryHorizontal !== 0 ||
          input.secondaryVertical !== 0;
        input.attack = (buttons1 & 0x01) !== 0 || zPressed;
        input.special = (buttons1 & 0x02) !== 0;
        input.jump = (buttons1 & 0x0c) !== 0;
        input.taunt = (buttons2 & 0x01) !== 0;
        input.leftShieldStrength =
          digitalLeft || zPressed
            ? 65535
            : wiiUAdapterTrigger(report[base + 7]);
        input.rightShieldStrength = digitalRight
          ? 65535
          : wiiUAdapterTrigger(report[base + 8]);
        input.shieldStrength = Math.max(
          input.leftShieldStrength,
          input.rightShieldStrength
        );
        input.shield = input.shieldStrength !== 0;
        parsed.portConnected[port] = true;
        ++parsed.connectedPorts;
      }
      return parsed;
    }

    function runWiiUAdapterMappingProbe() {
      var report = new Uint8Array(37);
      report[0] = 0x21;
      report[1] = 0x10;
      report[2] = 0x85;
      report[3] = 0x0b;
      report[4] = 208;
      report[5] = 48;
      report[6] = 48;
      report[7] = 208;
      report[8] = 128;
      report[9] = 64;
      var parsed = parseWiiUAdapterReport(report);
      var input = parsed.inputs[0];
      return (
        parsed.valid &&
        parsed.connectedPorts === 1 &&
        parsed.portConnected[0] &&
        input.horizontal === dashAxis &&
        input.vertical === -dashAxis &&
        input.secondaryHorizontal === -dashAxis &&
        input.secondaryVertical === -dashAxis &&
        input.attack &&
        !input.special &&
        input.jump &&
        input.taunt &&
        input.strongAttack &&
        input.shield &&
        input.leftShieldStrength === 65535 &&
        input.rightShieldStrength > 0 &&
        input.rightShieldStrength < 65535
      );
    }

    function updateWiiUAdapterReport(report) {
      var parsed = parseWiiUAdapterReport(report);
      if (!parsed.valid) {
        return;
      }
      wiiUAdapterState.connectedPorts = parsed.connectedPorts;
      wiiUAdapterState.inputs = parsed.inputs;
      wiiUAdapterState.portConnected = parsed.portConnected;
    }

    async function readWiiUAdapter(device, generation) {
      while (
        wiiUAdapterState.device === device &&
        wiiUAdapterState.generation === generation &&
        device.opened
      ) {
        try {
          var result = await device.transferIn(
            wiiUAdapterState.inputEndpoint,
            37
          );
          if (result.status === "ok" && result.data) {
            updateWiiUAdapterReport(
              new Uint8Array(
                result.data.buffer,
                result.data.byteOffset,
                result.data.byteLength
              )
            );
          }
        } catch (error) {
          if (
            wiiUAdapterState.device === device &&
            wiiUAdapterState.generation === generation
          ) {
            wiiUAdapterState.status = "error";
            wiiUAdapterState.error = String(
              error && error.message ? error.message : error
            );
            wiiUAdapterState.connectedPorts = 0;
            wiiUAdapterState.inputs = [
              emptyGamepadInput(),
              emptyGamepadInput(),
              emptyGamepadInput(),
              emptyGamepadInput(),
            ];
            wiiUAdapterState.portConnected = [false, false, false, false];
          }
          return;
        }
      }
    }

    async function openWiiUAdapter(device) {
      wiiUAdapterState.status = "connecting";
      wiiUAdapterState.error = "";
      try {
        if (!device.opened) {
          await device.open();
        }
        if (!device.configuration) {
          await device.selectConfiguration(1);
        }
        var usbInterface = device.configuration.interfaces.find(function (
          candidate
        ) {
          return candidate.interfaceNumber === 0;
        });
        if (!usbInterface) {
          throw new Error("adapter USB interface 0 is unavailable");
        }
        await device.claimInterface(usbInterface.interfaceNumber);
        var alternate = usbInterface.alternate;
        var inputEndpoint = alternate.endpoints.find(function (endpoint) {
          return endpoint.direction === "in";
        });
        var outputEndpoint = alternate.endpoints.find(function (endpoint) {
          return endpoint.direction === "out";
        });
        if (!inputEndpoint || !outputEndpoint) {
          throw new Error("adapter interrupt endpoints are unavailable");
        }

        wiiUAdapterState.device = device;
        wiiUAdapterState.inputEndpoint = inputEndpoint.endpointNumber;
        wiiUAdapterState.outputEndpoint = outputEndpoint.endpointNumber;
        ++wiiUAdapterState.generation;
        await device.transferOut(
          wiiUAdapterState.outputEndpoint,
          new Uint8Array([0x13])
        );
        wiiUAdapterState.status = "connected";
        void readWiiUAdapter(device, wiiUAdapterState.generation);
        return true;
      } catch (error) {
        if (device.opened) {
          try {
            await device.close();
          } catch (closeError) {
            void closeError;
          }
        }
        wiiUAdapterState.device = null;
        wiiUAdapterState.status = "error";
        wiiUAdapterState.error = String(
          error && error.message ? error.message : error
        );
        return false;
      }
    }

    async function requestWiiUAdapter() {
      if (!wiiUAdapterState.available) {
        wiiUAdapterState.status = "unsupported";
        return false;
      }
      try {
        var device = await navigator.usb.requestDevice({
          filters: [{ vendorId: 0x057e, productId: 0x0337 }],
        });
        return await openWiiUAdapter(device);
      } catch (error) {
        if (error && error.name === "NotFoundError") {
          wiiUAdapterState.status = "disconnected";
          wiiUAdapterState.error = "";
          return false;
        }
        wiiUAdapterState.status = "error";
        wiiUAdapterState.error = String(
          error && error.message ? error.message : error
        );
        return false;
      }
    }

    async function reconnectAuthorizedWiiUAdapter() {
      if (
        !wiiUAdapterState.available ||
        typeof navigator.usb.getDevices !== "function"
      ) {
        return false;
      }
      try {
        var devices = await navigator.usb.getDevices();
        var device = devices.find(function (candidate) {
          return (
            candidate.vendorId === 0x057e &&
            candidate.productId === 0x0337
          );
        });
        return device ? await openWiiUAdapter(device) : false;
      } catch (error) {
        wiiUAdapterState.status = "error";
        wiiUAdapterState.error = String(
          error && error.message ? error.message : error
        );
        return false;
      }
    }

    function mergeWiiUAdapterInputs(result) {
      result.wiiUAvailable = wiiUAdapterState.available;
      result.wiiUStatus = wiiUAdapterState.status;
      result.wiiUError = wiiUAdapterState.error;
      result.wiiUPorts = wiiUAdapterState.connectedPorts;
      if (wiiUAdapterState.status !== "connected") {
        return result;
      }
      for (
        var port = 0;
        port < 4 && result.connected < result.inputs.length;
        ++port
      ) {
        if (!wiiUAdapterState.portConnected[port]) {
          continue;
        }
        result.inputs[result.connected] = wiiUAdapterState.inputs[port];
        ++result.connected;
      }
      return result;
    }

    function isSupportedGamepad(gamepad) {
      return (
        gamepad &&
        gamepad.connected !== false &&
        (gamepad.mapping === "standard" ||
          (isMayflashGameCubeAdapter(gamepad) &&
            mayflashPortHasController(gamepad)))
      );
    }

    function mapSupportedGamepad(gamepad) {
      return gamepad && gamepad.mapping === "standard"
        ? mapStandardGamepad(gamepad)
        : mapMayflashGameCubeAdapter(gamepad);
    }

    function collectSupportedGamepads(gamepads) {
      var result = {
        connected: 0,
        mayflashPorts: 0,
        mayflashControllers: 0,
        inputs: [emptyGamepadInput(), emptyGamepadInput()],
      };
      var index;
      for (index = 0; gamepads && index < gamepads.length; ++index) {
        var gamepad = gamepads[index];
        if (gamepad && isMayflashGameCubeAdapter(gamepad)) {
          ++result.mayflashPorts;
        }
        if (isSupportedGamepad(gamepad)) {
          if (isMayflashGameCubeAdapter(gamepad)) {
            ++result.mayflashControllers;
          }
          if (result.connected >= 2) {
            continue;
          }
          result.inputs[result.connected] = mapSupportedGamepad(gamepad);
          ++result.connected;
        }
      }
      return result;
    }

    function pollSupportedGamepads() {
      if (
        typeof navigator === "undefined" ||
        typeof navigator.getGamepads !== "function"
      ) {
        return mergeWiiUAdapterInputs(collectSupportedGamepads([]));
      }
      try {
        return mergeWiiUAdapterInputs(
          collectSupportedGamepads(navigator.getGamepads())
        );
      } catch (error) {
        return mergeWiiUAdapterInputs(collectSupportedGamepads([]));
      }
    }

    function gamepadStatusLabel(gamepads) {
      var cStick = gamepads.inputs[0];
      var cStickHorizontal =
        cStick.secondaryHorizontal < 0
          ? "left"
          : cStick.secondaryHorizontal > 0
          ? "right"
          : "";
      var cStickVertical =
        cStick.secondaryVertical < 0
          ? "up"
          : cStick.secondaryVertical > 0
          ? "down"
          : "";
      var cStickLabel =
        cStickVertical && cStickHorizontal
          ? cStickVertical + "-" + cStickHorizontal
          : cStickVertical || cStickHorizontal || "neutral";
      if (!gamepadApiAvailable) {
        return gamepads.wiiUStatus === "connected"
          ? "Wii U GameCube " + gamepads.wiiUPorts + "/4"
          : "gamepad API unavailable";
      }
      if (gamepads.wiiUStatus === "connected") {
        return (
          "controllers " +
          gamepads.connected +
          "/2 · Wii U GameCube " +
          gamepads.wiiUPorts +
          "/4 · C " +
          cStickLabel
        );
      }
      if (gamepads.wiiUStatus === "connecting") {
        return "connecting Wii U GameCube adapter…";
      }
      if (gamepads.wiiUStatus === "error") {
        return "Wii U adapter error · " + gamepads.wiiUError;
      }
      if (gamepads.mayflashPorts > 0) {
        return (
          "controllers " +
          gamepads.connected +
          "/2 · GameCube " +
          gamepads.mayflashControllers +
          "/" +
          gamepads.mayflashPorts +
          " · C " +
          cStickLabel
        );
      }
      return (
        "controllers " +
        gamepads.connected +
        "/2 · right stick " +
        cStickLabel
      );
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
        axes: [0.5, -0.25, 0.5, -0.5],
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
        id: "Unknown DirectInput Controller",
        axes: [1, 1],
        buttons: dpadButtons,
      };
      var centered = mapStandardGamepad({
        connected: true,
        mapping: "standard",
        axes: [0.1, -0.1],
        buttons: buttons(),
      });
      var mayflashButtons = buttons();
      mayflashButtons[1] = { pressed: true, value: 1 };
      mayflashButtons[3] = { pressed: true, value: 1 };
      mayflashButtons[7] = { pressed: true, value: 1 };
      mayflashButtons[9] = { pressed: true, value: 1 };
      var mayflash = {
        connected: true,
        mapping: "",
        id: "MAYFLASH GameCube Controller Adapter (Vendor: 0079 Product: 1843)",
        axes: [0.75, 0, -0.6, -0.76, -0.76, 0.6, 0, 0, 0, 1.3],
        buttons: mayflashButtons,
      };
      var emptyMayflashPort = {
        connected: true,
        mapping: "",
        id: "0079-1843-Microsoft PC-joystick driver",
        axes: [-1, -1, -1, -1, -1, -1, 0, 0, 0, 1.3],
        buttons: buttons(),
      };
      var mayflashInput = mapMayflashGameCubeAdapter(mayflash);
      var result = collectSupportedGamepads([
        ignored,
        emptyMayflashPort,
        analog,
        null,
        mayflash,
        dpad,
      ]);
      return (
        centered.horizontal === 0 &&
        centered.vertical === 0 &&
        result.connected === 2 &&
        result.mayflashPorts === 2 &&
        result.mayflashControllers === 1 &&
        result.inputs[0].horizontal === Math.round(dashAxis * 0.5) &&
        result.inputs[0].vertical === -Math.round(dashAxis * 0.25) &&
        result.inputs[0].secondaryHorizontal ===
          Math.round(dashAxis * 0.5) &&
        result.inputs[0].secondaryVertical ===
          -Math.round(dashAxis * 0.5) &&
        result.inputs[0].attack &&
        result.inputs[0].strongAttack &&
        result.inputs[0].jump &&
        !result.inputs[0].special &&
        result.inputs[0].taunt &&
        result.inputs[0].shield &&
        result.inputs[0].shieldStrength === Math.round(0.75 * 65535) &&
        result.inputs[0].leftShieldStrength ===
          Math.round(0.75 * 65535) &&
        result.inputs[0].rightShieldStrength === 0 &&
        mayflashInput.horizontal === dashAxis &&
        mayflashInput.vertical === 0 &&
        mayflashInput.secondaryHorizontal ===
          Math.round(dashAxis * 0.8) &&
        mayflashInput.secondaryVertical ===
          -Math.round(dashAxis * 0.8) &&
        mayflashInput.attack &&
        mayflashInput.strongAttack &&
        mayflashInput.jump &&
        mayflashInput.taunt &&
        mayflashInput.shield &&
        mayflashInput.shieldStrength === 65535 &&
        mayflashInput.leftShieldStrength === 65535 &&
        mayflashInput.rightShieldStrength === 0 &&
        result.inputs[1].horizontal === mayflashInput.horizontal &&
        result.inputs[1].vertical === mayflashInput.vertical &&
        result.inputs[1].secondaryHorizontal ===
          mayflashInput.secondaryHorizontal &&
        result.inputs[1].secondaryVertical ===
          mayflashInput.secondaryVertical &&
        result.inputs[1].attack &&
        result.inputs[1].strongAttack &&
        result.inputs[1].jump &&
        !result.inputs[1].special &&
        result.inputs[1].taunt &&
        result.inputs[1].shield &&
        result.inputs[1].shieldStrength === 65535 &&
        result.inputs[1].leftShieldStrength === 65535 &&
        result.inputs[1].rightShieldStrength === 0
      );
    }

    var gamepadApiAvailable =
      typeof navigator !== "undefined" &&
      typeof navigator.getGamepads === "function";
    var gamepadProbePassed = runGamepadMappingProbe();
    var wiiUAdapterProbePassed = runWiiUAdapterMappingProbe();
    var controllerApiAvailable =
      gamepadApiAvailable || wiiUAdapterState.available;
    var status = document.getElementById("pf-status");
    var replayInspector = document.getElementById("pf-replay-inspector");
    var previous = document.getElementById("pf-m4-playtest");
    var ownerChecklist =
      typeof globalThis !== "undefined"
        ? globalThis.PF_M4_OWNER_CHECKLIST
        : null;
    var ownerEvidenceInstaller =
      typeof globalThis !== "undefined"
        ? globalThis.PFInstallM4OwnerEvidence
        : null;
    var ownerChecklistReady =
      ownerChecklist &&
      ownerChecklist.schema === 1 &&
      ownerChecklist.sourceRevision === "2048934" &&
      Array.isArray(ownerChecklist.techniques) &&
      ownerChecklist.techniques.length === 61 &&
      typeof ownerEvidenceInstaller === "function";

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
        ".pf-m4-toolbar button:disabled{cursor:not-allowed;opacity:.45}" +
        ".pf-m4-toolbar button[aria-pressed=true]{background:#214d46;" +
        "border-color:#4ce0a8;color:#dcfff2}" +
        ".pf-m4-tick{color:#8edcff;font:12px/1 ui-monospace,monospace;" +
        "margin-left:auto}" +
        ".pf-m4-gamepads{color:#a9b7ca;font:12px/1 ui-monospace,monospace}" +
        ".pf-m4-overlay-legend{display:flex;flex-wrap:wrap;gap:8px 14px;" +
        "align-items:center;margin:-2px 0 14px;color:#9fb0c7;" +
        "font:11px/1.25 ui-monospace,monospace}" +
        ".pf-m4-overlay-legend strong{color:#edf5ff}" +
        ".pf-m4-overlay-key{display:inline-flex;align-items:center;gap:5px}" +
        ".pf-m4-overlay-swatch{width:13px;height:9px;border:2px solid;" +
        "display:inline-block;border-radius:2px}" +
        ".pf-m4-overlay-stage{border-color:#4ce0a8}" +
        ".pf-m4-overlay-hurt{border-color:#73b7ff;background:#73b7ff22}" +
        ".pf-m4-overlay-shield{border-color:#d8d0ff;background:#a991ff22}" +
        ".pf-m4-overlay-attack{border-color:#ffd089;background:#ffb34733}" +
        ".pf-m4-overlay-grab{border-color:#8cf3ff;background:#62e7ff22}" +
        ".pf-m4-overlay-blast{border-color:#ff6c8f}" +
        ".pf-m4-setup{display:grid;grid-template-columns:1fr auto auto;" +
        "gap:16px;align-items:center;margin:0 0 16px;padding:16px 18px;" +
        "background:#0a1523;border:1px solid #355170;border-radius:13px}" +
        ".pf-m4-setup[hidden]{display:none}.pf-m4-setup-copy strong{" +
        "display:block;color:#f3f7ff;font-size:17px;margin-bottom:4px}" +
        ".pf-m4-setup-copy span{color:#91a5bf;font-size:12px}" +
        ".pf-m4-setup label{display:grid;gap:5px;color:#9fb0c7;" +
        "font:700 11px/1 ui-monospace,monospace;text-transform:uppercase}" +
        ".pf-m4-setup select{min-width:92px;background:#13243a;color:#edf5ff;" +
        "border:1px solid #466587;border-radius:8px;padding:8px 10px;" +
        "font:700 13px/1 system-ui}" +
        ".pf-m4-setup button{background:#1f735f;color:#eafff7;border:1px solid #4ce0a8;" +
        "border-radius:9px;padding:11px 16px;font:800 12px/1 system-ui;" +
        "cursor:pointer;white-space:nowrap}" +
        "#pf-m4-playtest[data-match-flow=setup] #pf-m4-canvas{opacity:.42}" +
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
        ".pf-m4-owner{margin-top:18px;background:#0a1523;border:1px solid #355170;" +
        "border-radius:13px;color:#bed0e8}.pf-m4-owner>summary{cursor:pointer;" +
        "padding:16px 18px;font:800 14px/1.3 system-ui;color:#edf5ff}" +
        ".pf-m4-owner-body{padding:0 18px 18px}.pf-m4-owner-copy{color:#91a5bf;" +
        "font:12px/1.5 system-ui;margin:0 0 14px}.pf-m4-owner-meta{" +
        "display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:12px 0}" +
        ".pf-m4-owner label{display:grid;gap:5px;color:#9fb0c7;" +
        "font:700 11px/1.2 ui-monospace,monospace}.pf-m4-owner input," +
        ".pf-m4-owner textarea,.pf-m4-owner select{width:100%;background:#101d2e;" +
        "color:#edf5ff;border:1px solid #3b5678;border-radius:7px;padding:8px;" +
        "font:12px/1.35 system-ui}.pf-m4-owner textarea{min-height:62px;resize:vertical}" +
        ".pf-m4-owner-progress{display:grid;grid-template-columns:1fr auto;gap:10px;" +
        "align-items:center;margin:12px 0}.pf-m4-owner-progress progress{width:100%;" +
        "accent-color:#4ce0a8}.pf-m4-owner-progress span{font:700 11px/1 " +
        "ui-monospace,monospace;color:#8edcff}.pf-m4-owner-toolbar{display:flex;" +
        "gap:8px;flex-wrap:wrap;align-items:end;margin:12px 0}" +
        ".pf-m4-owner-toolbar label{min-width:130px}.pf-m4-owner button{" +
        "background:#182841;color:#e8f1ff;border:1px solid #385274;" +
        "border-radius:8px;padding:8px 11px;font:700 11px/1 system-ui;cursor:pointer}" +
        ".pf-m4-owner button:hover{background:#203654}.pf-m4-owner-list{" +
        "display:grid;gap:7px;margin:12px 0}.pf-m4-owner-technique{" +
        "background:#0d1a2b;border:1px solid #29415f;border-radius:9px}" +
        ".pf-m4-owner-technique[data-result=pass]{border-color:#27725d}" +
        ".pf-m4-owner-technique[data-result=fail]{border-color:#9b4459}" +
        ".pf-m4-owner-technique>summary{display:flex;justify-content:space-between;" +
        "gap:12px;cursor:pointer;padding:10px 12px;font:700 12px/1.35 system-ui}" +
        ".pf-m4-owner-result{font:800 10px/1 ui-monospace,monospace;" +
        "text-transform:uppercase;color:#8294ad}.pf-m4-owner-technique[data-result=pass] " +
        ".pf-m4-owner-result{color:#77efc6}.pf-m4-owner-technique[data-result=fail] " +
        ".pf-m4-owner-result{color:#ff91a9}.pf-m4-owner-technique-body{" +
        "padding:0 12px 12px}.pf-m4-owner-recipe{color:#b8c8dc;" +
        "font:12px/1.5 system-ui;margin:0 0 10px}.pf-m4-owner-actions{" +
        "display:flex;gap:7px;flex-wrap:wrap;margin-bottom:9px}" +
        ".pf-m4-owner-actions button[data-result=pass]{border-color:#3ba985}" +
        ".pf-m4-owner-actions button[data-result=fail]{border-color:#b6536a}" +
        ".pf-m4-owner-rubric{display:grid;grid-template-columns:repeat(4,1fr);" +
        "gap:9px;margin:14px 0}.pf-m4-owner-collision{display:flex!important;" +
        "grid-column:1/-1;grid-template-columns:auto 1fr!important;align-items:center}" +
        ".pf-m4-owner-collision input{width:auto}.pf-m4-owner-ready{" +
        "border-left:3px solid #6f86a6;padding:9px 11px;background:#101d2e;" +
        "font:12px/1.45 system-ui;color:#b8c8dc}.pf-m4-owner[data-ready=true] " +
        ".pf-m4-owner-ready{border-color:#4ce0a8;color:#cffff0}" +
        "@media(max-width:680px){.pf-m4-heading{flex-direction:column}" +
        ".pf-m4-controls,.pf-m4-state-grid{grid-template-columns:1fr}" +
        ".pf-m4-tick{margin-left:0;width:100%}" +
        "#pf-m4-playtest{padding:16px}.pf-m4-setup{grid-template-columns:1fr;" +
        "align-items:stretch}.pf-m4-owner-meta,.pf-m4-owner-rubric{" +
        "grid-template-columns:1fr}}";
      document.head.appendChild(style);
    }

    var section = document.createElement("section");
    section.id = "pf-m4-playtest";
    section.dataset.ready =
      controllerApiAvailable &&
      gamepadProbePassed &&
      wiiUAdapterProbePassed
        ? "true"
        : "false";
    section.dataset.gamepadProbe = gamepadProbePassed ? "pass" : "fail";
    section.dataset.gamepadApi =
      gamepadApiAvailable ? "available" : "unavailable";
    section.dataset.gamepadProfiles =
      "standard-mayflash-0079-1843-webusb-057e-0337";
    section.dataset.wiiUAdapterApi = wiiUAdapterState.available
      ? "available"
      : "unavailable";
    section.dataset.wiiUAdapterProbe = wiiUAdapterProbePassed
      ? "pass"
      : "fail";
    section.dataset.wiiUAdapter = "disconnected";
    section.dataset.crouchCue = "squat-chevron-label";
    section.dataset.lightShieldCue = "expanded-translucent-percent-label";
    section.dataset.shieldCue = "readable-margin-strength-label";
    section.dataset.shieldHealthCue = "melee-health-density-scale";
    section.dataset.teamLab = "inactive";
    section.dataset.matchFlow = "setup";
    section.dataset.collisionOverlay = "visible";
    section.dataset.collisionOverlaySemantics =
      "stage-hurtbox-shield-attack-grab-item-projectile-blast";
    section.dataset.ownerChecklist = ownerChecklistReady ? "ready" : "fail";
    section.dataset.ownerChecklistSchema = ownerChecklistReady
      ? String(ownerChecklist.schema)
      : "unavailable";
    section.dataset.ownerChecklistRevision = ownerChecklistReady
      ? ownerChecklist.sourceRevision
      : "unavailable";
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
      "Keyboard, Standard Gamepads, and the Mayflash four-port GameCube " +
      "adapter in PC or native Wii U mode drive the same deterministic " +
      "Q16.16 simulation used by native, replay, rollback, and headless " +
      "execution. The collision inspector draws production stage surfaces, " +
      "hurtboxes, shield volumes, attack and grab boxes, item/projectile " +
      "extents, and blast zones.";
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
      controllerApiAvailable &&
      gamepadProbePassed &&
      wiiUAdapterProbePassed
        ? "ALL M4 INPUT + GAMEPAD + COMBAT PROBES PASSED"
        : "RUNTIME PROBE FAILED";
    heading.appendChild(headingCopy);
    heading.appendChild(live);
    section.appendChild(heading);

    var setupPanel = document.createElement("section");
    setupPanel.className = "pf-m4-setup";
    setupPanel.id = "pf-m4-match-setup";
    setupPanel.setAttribute("aria-label", "Local one versus one match setup");
    var setupCopy = document.createElement("div");
    setupCopy.className = "pf-m4-setup-copy";
    var setupTitle = document.createElement("strong");
    setupTitle.textContent = "Local 1v1 match setup";
    var setupSummary = document.createElement("span");
    setupSummary.textContent =
      "Vector vs Vector · Test Stage · 60 Hz · keyboard or supported controllers";
    setupCopy.appendChild(setupTitle);
    setupCopy.appendChild(setupSummary);
    var stockLabel = document.createElement("label");
    stockLabel.textContent = "Stocks";
    var stockSelect = document.createElement("select");
    stockSelect.id = "pf-m4-stock-count";
    stockSelect.setAttribute("aria-label", "Stock count");
    [1, 2, 3, 4].forEach(function (stockCount) {
      var option = document.createElement("option");
      option.value = String(stockCount);
      option.textContent = String(stockCount);
      option.selected = stockCount === 4;
      stockSelect.appendChild(option);
    });
    stockLabel.appendChild(stockSelect);
    var startMatchButton = document.createElement("button");
    startMatchButton.id = "pf-m4-start-match";
    startMatchButton.type = "button";
    startMatchButton.textContent = "Start Local Match";
    setupPanel.appendChild(setupCopy);
    setupPanel.appendChild(stockLabel);
    setupPanel.appendChild(startMatchButton);
    section.appendChild(setupPanel);

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
    var setupButton = document.createElement("button");
    setupButton.type = "button";
    setupButton.textContent = "Match Setup";
    setupButton.setAttribute("aria-pressed", "true");
    setupButton.setAttribute("aria-controls", "pf-m4-match-setup");
    var teamLabButton = document.createElement("button");
    teamLabButton.type = "button";
    teamLabButton.textContent = "Team Wobble Lab";
    teamLabButton.setAttribute("aria-pressed", "false");
    var collisionOverlayButton = document.createElement("button");
    collisionOverlayButton.id = "pf-m4-collision-overlay";
    collisionOverlayButton.type = "button";
    collisionOverlayButton.textContent = "Collision Inspector: On";
    collisionOverlayButton.setAttribute("aria-pressed", "true");
    collisionOverlayButton.setAttribute("aria-controls", "pf-m4-canvas");
    var wiiUAdapterButton = document.createElement("button");
    wiiUAdapterButton.id = "pf-m4-wii-u-adapter";
    wiiUAdapterButton.type = "button";
    wiiUAdapterButton.textContent = "Connect Wii U Adapter";
    wiiUAdapterButton.disabled = !wiiUAdapterState.available;
    wiiUAdapterButton.title = wiiUAdapterState.available
      ? "Release the sticks, then grant access to the 057e:0337 adapter"
      : "WebUSB is unavailable in this browser";
    var tickLabel = document.createElement("span");
    tickLabel.className = "pf-m4-tick";
    tickLabel.textContent = "tick 0 · fixed 60 Hz";
    var gamepadLabel = document.createElement("span");
    gamepadLabel.className = "pf-m4-gamepads";
    gamepadLabel.textContent = gamepadStatusLabel(collectSupportedGamepads([]));
    toolbar.appendChild(pauseButton);
    toolbar.appendChild(stepButton);
    toolbar.appendChild(resetButton);
    toolbar.appendChild(setupButton);
    toolbar.appendChild(teamLabButton);
    toolbar.appendChild(collisionOverlayButton);
    toolbar.appendChild(wiiUAdapterButton);
    toolbar.appendChild(gamepadLabel);
    toolbar.appendChild(tickLabel);
    section.appendChild(toolbar);

    var overlayLegend = document.createElement("div");
    overlayLegend.className = "pf-m4-overlay-legend";
    overlayLegend.id = "pf-m4-collision-legend";
    overlayLegend.setAttribute("aria-label", "Collision inspector legend");
    overlayLegend.innerHTML =
      "<strong>Inspector (I)</strong>" +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-stage"></i>stage/item/projectile</span>' +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-hurt"></i>hurtbox</span>' +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-shield"></i>shield</span>' +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-attack"></i>attack</span>' +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-grab"></i>grab</span>' +
      '<span class="pf-m4-overlay-key"><i class="pf-m4-overlay-swatch pf-m4-overlay-blast"></i>blast zone</span>';
    section.appendChild(overlayLegend);

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
        "Keyboard: A / D dash or DI · Shift + A / D walk · Shift + S reduced-down shield drop · W or Space jump · F light / directional tilt, or hold full direction + F to charge a smash · H immediate uncharged strong · E Pulse Bolt, Down + E Prism Burst reflector, Up + E Vector Ascent recovery from the ground or air, or hold F with Up + E to charge Arc Reservoir · T taunt · G full shield/trigger · F + G grab, or pick up/drop the nearby Relay Rod. Standard Gamepad 1: left stick or D-pad · right stick strong or buffered shield escape · bottom face light / directional tilt or charged smash · right face immediate uncharged strong · left face jump · top face special · Back/View taunt · bumpers full shield · analog triggers pressure-sensitive shield · light + shield grab/item. GameCube adapter: A light · B special · X/Y jump · C-stick strong or buffered shield escape · L/R shield · Z grab/item · Start taunt"
      )
    );
    controls.appendChild(
      controlCard(
        "Player 2",
        "Keyboard: ← / → dash or DI · Shift + horizontal arrows walk · Shift + ↓ reduced-down shield drop · ↑ jump · / or Numpad 0 light / directional tilt, or hold full direction + light to charge a smash · ' or Numpad 2 immediate uncharged strong · ; or Numpad 3 Pulse Bolt, Down + special Prism Burst reflector, Up + special Vector Ascent recovery from the ground or air, or hold light with Up + special to charge Arc Reservoir · , taunt · . or Numpad 1 shield/trigger · light + shield grab/item. Supported controller 2 uses the same controller layout as Player 1"
      )
    );
    section.appendChild(controls);

    var note = document.createElement("p");
    note.className = "pf-m4-note";
    note.textContent =
      "Supported controllers are assigned in browser index order and polled every " +
      "simulation tick, so hot-plugging does not alter canonical state. Left " +
      "stick magnitude preserves analog walk/dash thresholds; the D-pad emits " +
      "full magnitude. A normal quick flick may reach full horizontal over two " +
      "samples and still dash. A slower sweep that takes three or more samples " +
      "to reach the dash threshold becomes the fastest walk. In PC mode, the " +
      "Mayflash 0079:1843 main-stick and C-stick cardinal gate values are " +
      "normalized to full magnitude. In Wii U mode, click Connect Wii U Adapter " +
      "once and grant access to the WUP-028 057e:0337 device; its native stick, " +
      "trigger, button, and four-port reports are read directly. Empty adapter " +
      "ports are skipped. Keyboard and controller buttons may be mixed per player. " +
      "Tap jump and release during the four-tick jump squat for the fixed " +
      "short hop; hold through takeoff for the fixed full hop. Releasing after " +
      "takeoff never changes either apex. Run, press jump while holding " +
      "forward, then immediately hold backward through jump squat to nearly " +
      "cancel horizontal takeoff momentum without turning around. For an " +
      "instant double jump, release " +
      "the first jump during jump squat, then press the other jump key on the " +
      "first airborne frame; the live air-jumps counter changes from 1 to 0. " +
      "Its horizontal speed is replaced by current stick input, so neutral " +
      "stops horizontal momentum. " +
      "Holding one jump key never repeats the input. Tap opposite full directions during " +
      "initial dash to dash-dance; after the state reaches RUN, the same reversal " +
      "enters RUN TURNAROUND instead. To fox-trot, rhythmically tap and release " +
      "one full direction; each fresh tap restarts INITIAL DASH, while holding " +
      "the direction reaches RUN and a reduced-magnitude re-entry only walks. " +
      "To moonwalk, dash and sweep the main stick through the lower half from " +
      "forward through down to back, then finish at straight back; the whole " +
      "GameCube half-moon stays in dash and slides backward without changing " +
      "facing. Pausing at the lower-back notch for two ticks also works. Keyboard " +
      "supports S, then S plus opposite, then opposite; Shift plus opposite " +
      "followed by releasing Shift remains available. " +
      "A faster straight reversal is a dashback. " +
      "To pivot, dash, tap the opposite full direction for one tick, return to " +
      "neutral on the next tick, and immediately attack; the fighter keeps the " +
      "new facing and reversal momentum. Holding the reversal continues the " +
      "dash, while attempting it after RUN enters RUN TURNAROUND. " +
      "For a small-step forward smash, tap and hold a full direction, delay " +
      "one to three simulation ticks, then press the light-attack button; " +
      "hold light to charge for up to 60 ticks, then release it; the " +
      "initial-dash travel extends the strong hitbox's reach. Pressing " +
      "direction plus light simultaneously gives the standing charged comparison, " +
      "while waiting four ticks produces the ordinary non-smash attack. " +
      "For a drop cancel, put both fighters close together on the moving " +
      "platform, press down with the attacker, then press light attack on the " +
      "first airborne tick; a hit returns the attacker to AERIAL LANDING on " +
      "that platform. Waiting one extra tick or whiffing falls through. " +
      "To dash-cancel a run, press down for a traction slide into CROUCH, then " +
      "attack; jump and shield are the other live cancel routes. Shield remains " +
      "locked out during INITIAL DASH and down cannot cancel RUN TURNAROUND. " +
      "Fall beside a ledge while facing inward " +
      "to grab it; after the catch, press inward to climb, a fresh trigger to " +
      "ledge-roll, light or strong attack to ledge-attack, down or away to " +
      "release, or jump to ledge-jump. For an edge hop, tap down from hang, " +
      "release it, then press jump plus inward on the next tick and follow " +
      "with an aerial. F and / perform light attacks: neutral jabs, " +
      "reduced-direction tilts, and full-direction charged smashes from idle " +
      "or walk. Release light early or hold to the 60-tick automatic release; " +
      "the charge scales damage by up to +50%. H and ' perform an immediate " +
      "uncharged directional strong attack on the ground or a slower strong " +
      "attack in the air that " +
      "immediately launches the default fighter into tumble. Translucent boxes " +
      "show active frames, and tumbling " +
      "fighters visibly rotate after hitlag. During ordinary target hitlag, " +
      "change stick direction for SDI and hold a launch direction for DI. Press " +
      "and hold down until CROUCH before a low-percent hit to crouch cancel; " +
      "damage and hitlag stay ordinary, launch and hitstun become two-thirds, " +
      "and the event feed labels CROUCH CANCEL. Hits ending above 40 percent " +
      "keep the ordinary reaction. Press " +
      "a light-attack key while airborne with neutral or reduced stick for the " +
      "neutral aerial. Full vertical-dominant input selects up/down aerial; " +
      "full horizontal-dominant or equal-diagonal input selects forward/back " +
      "aerial relative to facing. The dedicated strong key remains direct. " +
      "For SHFFL, " +
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
      "stock, waits 60 frames, then rides an invulnerable revival platform " +
      "through its 30-frame descent. After it stops, move or press a gameplay " +
      "button to drop; it also releases automatically after its 90-frame hold. " +
      "The 120-frame dashed-ring respawn invulnerability begins on release. " +
      "The HUD shows stocks and both timers; final-stock KOs " +
      "show a result banner and turn Reset into Rematch. Simultaneous final-stock " +
      "KOs enter the deterministic 300% sudden-death fixture. " +
      "Hold G or . on the ground for a full draining shield; Standard Gamepad " +
      "analog triggers provide light shield from 12.5% pressure and become dense " +
      "at 50%. A light shield has a dashed ring and live percentage label. " +
      "Only fresh dense shields powershield during their four-tick window, " +
      "while releases have 15 ticks of lag. During shield hitlag, cross the " +
      "horizontal threshold once for 0.66-scaled shield SDI; holding it or adding " +
      "vertical does not repeat, and the final horizontal input supplies one " +
      "0.66-scaled shield ASDI shift. Press a fresh full horizontal direction " +
      "with the trigger for a " +
      "forward or backward roll relative to facing; press fresh down with the " +
      "trigger for a spot dodge. A forward roll turns the fighter around; a " +
      "backward roll preserves facing, so either ends facing opposite its " +
      "travel direction. Hold the independent GameCube C-stick or Standard " +
      "Gamepad right stick with shield: left/right buffers a roll, down buffers " +
      "spot dodge, and up buffers jump through shield stun and the eligible " +
      "shield frame; no fresh C-stick edge is required. These grounded " +
      "dodges have fixed movement, recovery, " +
      "and invulnerability windows. Tap the " +
      "same trigger shortly before a tumble landing to tech in " +
      "place; hold left or right to tech-roll. " +
      "While airborne, only a fresh dense trigger performs a directional air dodge; " +
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
      "later shield press leaves DASH ATTACK intact. During GRAB HOLD, freshly " +
      "press either attack without a full direction to pummel for 3% while " +
      "retaining the grab. Hold a full direction with that fresh press to throw: " +
      "forward/back are relative " +
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
      "From the ground or air, hold up and freshly press special to spend the " +
      "once-per-airtime Vector Ascent; steer horizontally during its 18-tick " +
      "rise, then " +
      "land or grab a ledge to restore it. The fighter card shows READY or " +
      "SPENT. On the ground, hold light while pressing up plus special to enter " +
      "Arc Reservoir charge instead. To gimp, intercept an opponent's ascent " +
      "with an aerial or Prism " +
      "Burst so they miss the stage; leave the same recovery unchallenged for " +
      "the control. To stage-spike, fight below the raised block and launch the " +
      "opponent into its underside; a missed tech ceiling-bounces downward, " +
      "while a fresh trigger produces the ceiling-tech control. " +
      "The pale upper deck above that block is a stationary second one-way " +
      "surface: land from above, or hold down to pass through it. " +
       "The deterministic event feed below records hits, shield interactions, " +
      "grabs, throws, KOs, revival drops, sudden death, and results in canonical " +
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

    if (ownerChecklistReady) {
      ownerEvidenceInstaller(section, ownerChecklist, status);
    } else {
      var ownerEvidenceFailure = document.createElement("p");
      ownerEvidenceFailure.id = "pf-m4-owner-evidence-failure";
      ownerEvidenceFailure.className = "pf-m4-owner-ready";
      ownerEvidenceFailure.textContent =
        "Owner evidence checklist unavailable: expected registry schema 1, " +
        "source revision 2048934, and exactly 61 generated recipes.";
      section.appendChild(ownerEvidenceFailure);
      section.dataset.ownerEvidence = "unavailable";
    }

    if (replayInspector) {
      document.body.insertBefore(section, replayInspector);
    } else {
      document.body.appendChild(section);
    }

    var state = {
      accumulator: 0,
      aerialLandingLagTicks: aerialLandingLagTicks,
      canvas: canvas,
      collisionOverlayButton: collisionOverlayButton,
      collisionOverlayVisible: true,
      dashAxis: dashAxis,
      eventFeed: eventFeed,
      eventLog: [],
      gamepadLabel: gamepadLabel,
      wiiUAdapterButton: wiiUAdapterButton,
      keys: Object.create(null),
      itemState: itemState,
      projectileState: projectileState,
      lastEventSequence: 0,
      lastTime: 0,
      latest: null,
      pauseButton: pauseButton,
      playerStates: playerStates,
      resetButton: resetButton,
      setupButton: setupButton,
      setupPanel: setupPanel,
      setMatchFlow: setMatchFlow,
      startMatchButton: startMatchButton,
      stepButton: stepButton,
      stockSelect: stockSelect,
      teamLabButton: teamLabButton,
      attackQueued: [false, false],
      strongAttackQueued: [false, false],
      jumpQueued: [false, false],
      shieldQueued: [false, false],
      specialQueued: [false, false],
      tauntQueued: [false, false],
      strongAerialLandingLagTicks: strongAerialLandingLagTicks,
      teamLabActive: false,
      running: false,
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
      var gamepads = pollSupportedGamepads();
      var player0Gamepad = gamepads.inputs[0];
      var player1Gamepad = gamepads.inputs[1];
      state.gamepadLabel.textContent = gamepadStatusLabel(gamepads);
      section.dataset.gamecubeAdapter =
        gamepads.mayflashPorts > 0 || gamepads.wiiUStatus === "connected"
          ? "detected"
          : "not-detected";
      section.dataset.gamecubeControllers = String(
        gamepads.mayflashControllers + gamepads.wiiUPorts
      );
      section.dataset.wiiUAdapter = gamepads.wiiUStatus;
      state.wiiUAdapterButton.textContent =
        gamepads.wiiUStatus === "connected"
          ? "Wii U Adapter Connected"
          : gamepads.wiiUStatus === "connecting"
          ? "Connecting Wii U Adapter…"
          : gamepads.wiiUStatus === "error"
          ? "Retry Wii U Adapter"
          : "Connect Wii U Adapter";
      state.wiiUAdapterButton.disabled =
        !wiiUAdapterState.available ||
        gamepads.wiiUStatus === "connected" ||
        gamepads.wiiUStatus === "connecting";
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
      var player0LeftShieldStrength =
        held("KeyG") || state.shieldQueued[0]
          ? 65535
          : player0Gamepad.leftShieldStrength;
      var player0RightShieldStrength =
        player0Gamepad.rightShieldStrength;
      var player1LeftShieldStrength =
        held("Period") || held("Numpad1") || state.shieldQueued[1]
          ? 65535
          : player1Gamepad.leftShieldStrength;
      var player1RightShieldStrength =
        player1Gamepad.rightShieldStrength;
      var passed = Module._pf_web_m4_playtest_step_dual_trigger_special(
        mergeAxis(
          horizontal("KeyA", "KeyD"),
          player0Gamepad.horizontal
        ),
        mergeAxis(vertical("KeyW", "KeyS"), player0Gamepad.vertical),
        player0Gamepad.secondaryHorizontal,
        player0Gamepad.secondaryVertical,
        player0Jump ? 1 : 0,
        player0Attack ? 1 : 0,
        player0StrongAttack ? 1 : 0,
        player0LeftShieldStrength,
        player0RightShieldStrength,
        mergeAxis(
          horizontal("ArrowLeft", "ArrowRight"),
          player1Gamepad.horizontal
        ),
        mergeAxis(
          vertical("ArrowUp", "ArrowDown"),
          player1Gamepad.vertical
        ),
        player1Gamepad.secondaryHorizontal,
        player1Gamepad.secondaryVertical,
        player1Jump ? 1 : 0,
        player1Attack ? 1 : 0,
        player1StrongAttack ? 1 : 0,
        player1LeftShieldStrength,
        player1RightShieldStrength,
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
      if (running && section.dataset.matchFlow !== "playing") {
        return;
      }
      state.running = running;
      state.pauseButton.textContent = running ? "Pause" : "Resume";
      state.pauseButton.setAttribute("aria-pressed", running ? "false" : "true");
      state.accumulator = 0;
      state.lastTime = 0;
    }

    function clearQueuedInputs() {
      state.keys = Object.create(null);
      state.jumpQueued = [false, false];
      state.attackQueued = [false, false];
      state.strongAttackQueued = [false, false];
      state.shieldQueued = [false, false];
      state.specialQueued = [false, false];
      state.tauntQueued = [false, false];
      state.accumulator = 0;
    }

    function setMatchFlow(flow) {
      var setup = flow === "setup";
      var playing = flow === "playing";

      section.dataset.matchFlow = flow;
      state.setupPanel.hidden = !setup;
      state.setupButton.textContent = flow === "results"
        ? "Change Setup"
        : "Match Setup";
      state.setupButton.setAttribute("aria-pressed", setup ? "true" : "false");
      state.pauseButton.disabled = !playing;
      state.stepButton.disabled = !playing;
      state.resetButton.disabled = setup;
      state.teamLabButton.disabled = !playing;
      if (setup) {
        state.pauseButton.textContent = "Pause";
        state.pauseButton.setAttribute("aria-pressed", "false");
      }
    }

    function openSetup() {
      clearQueuedInputs();
      setRunning(false);
      setMatchFlow("setup");
    }

    function startConfiguredDuel() {
      var stockCount = Number(state.stockSelect.value);

      clearQueuedInputs();
      state.eventLog = [];
      state.lastEventSequence = 0;
      if (!Module._pf_web_m4_playtest_configure_duel(stockCount)) {
        if (status) {
          status.textContent += " match_setup_runtime=fail";
          status.dataset.matchSetupRuntime = "fail";
        }
        return;
      }
      state.teamLabActive = false;
      state.teamLabButton.textContent = "Team Wobble Lab";
      state.teamLabButton.setAttribute("aria-pressed", "false");
      section.dataset.teamLab = "inactive";
      state.gamepadLabel.textContent = gamepadStatusLabel(
        collectSupportedGamepads([])
      );
      setMatchFlow("playing");
      setRunning(true);
    }

    function reset() {
      var completed =
        state.latest &&
        (state.latest[22] !== 0 || state.latest[23] !== 0);
      if (section.dataset.matchFlow === "setup") {
        return;
      }
      clearQueuedInputs();
      if (!Module._pf_web_m4_playtest_reset()) {
        setRunning(false);
        return;
      }
      if (completed) {
        setMatchFlow("playing");
        setRunning(true);
      }
    }

    function toggleTeamLab() {
      var nextActive = !state.teamLabActive;

      if (section.dataset.matchFlow !== "playing") {
        return;
      }
      clearQueuedInputs();
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
        : gamepadStatusLabel(collectSupportedGamepads([]));
      setRunning(true);
    }

    function setCollisionOverlayVisible(visible) {
      state.collisionOverlayVisible = visible;
      state.collisionOverlayButton.textContent = visible
        ? "Collision Inspector: On"
        : "Collision Inspector: Off";
      state.collisionOverlayButton.setAttribute(
        "aria-pressed",
        visible ? "true" : "false"
      );
      section.dataset.collisionOverlay = visible ? "visible" : "hidden";
      if (state.latest && !Module._pf_web_m4_playtest_refresh()) {
        setRunning(false);
        if (status) {
          status.textContent += " collision_overlay_runtime=fail";
          status.dataset.collisionOverlayRuntime = "fail";
        }
      }
    }

    function toggleCollisionOverlay() {
      setCollisionOverlayVisible(!state.collisionOverlayVisible);
    }

    function frame(time) {
      if (state.running) {
        if (!state.lastTime) {
          state.lastTime = time;
        }
        var elapsed = Math.min(time - state.lastTime, 100);
        state.lastTime = time;
        state.accumulator += elapsed;
        if (state.accumulator >= 1000 / 60) {
          step();
          state.accumulator = Math.min(
            state.accumulator - 1000 / 60,
            1000 / 60
          );
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
    setupButton.addEventListener("click", openSetup);
    startMatchButton.addEventListener("click", startConfiguredDuel);
    teamLabButton.addEventListener("click", toggleTeamLab);
    collisionOverlayButton.addEventListener("click", toggleCollisionOverlay);
    wiiUAdapterButton.addEventListener("click", async function () {
      await requestWiiUAdapter();
      if (!state.running) {
        step();
      }
    });

    if (wiiUAdapterState.available) {
      navigator.usb.addEventListener("disconnect", function (event) {
        if (event.device !== wiiUAdapterState.device) {
          return;
        }
        ++wiiUAdapterState.generation;
        wiiUAdapterState.device = null;
        wiiUAdapterState.status = "disconnected";
        wiiUAdapterState.error = "";
        wiiUAdapterState.connectedPorts = 0;
        wiiUAdapterState.inputs = [
          emptyGamepadInput(),
          emptyGamepadInput(),
          emptyGamepadInput(),
          emptyGamepadInput(),
        ];
        wiiUAdapterState.portConnected = [false, false, false, false];
      });
      void reconnectAuthorizedWiiUAdapter().then(function (connected) {
        if (connected && !state.running) {
          step();
        }
      });
    }

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
        if (!wasHeld && event.code === "KeyI") {
          toggleCollisionOverlay();
        }
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
        if (
          event.code === "KeyR" &&
          section.dataset.matchFlow !== "setup"
        ) {
          reset();
        } else if (
          event.code === "KeyP" &&
          section.dataset.matchFlow === "playing"
        ) {
          setRunning(!state.running);
        } else if (
          event.code === "KeyN" &&
          section.dataset.matchFlow === "playing" &&
          !state.running
        ) {
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
        (controllerApiAvailable &&
        gamepadProbePassed &&
        wiiUAdapterProbePassed
          ? "ready"
          : "fail") +
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
        " vector_ascent_probe=" +
        (vectorAscentProbePassed ? "pass" : "fail") +
        " gamepad_probe=" +
        (gamepadProbePassed ? "pass" : "fail") +
        " gamepad_api=" +
        (gamepadApiAvailable ? "available" : "unavailable") +
        " wii_u_adapter_probe=" +
        (wiiUAdapterProbePassed ? "pass" : "fail") +
        " wii_u_adapter_api=" +
        (wiiUAdapterState.available ? "available" : "unavailable") +
        " controls=keyboard-gamepad-webusb-two-controller-duel-team-lab" +
        " owner_checklist=" +
        (ownerChecklistReady ? "ready-61" : "fail");
      status.dataset.playtest =
        controllerApiAvailable &&
        gamepadProbePassed &&
        wiiUAdapterProbePassed
          ? "ready"
          : "fail";
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
      status.dataset.vectorAscentProbe =
        vectorAscentProbePassed ? "pass" : "fail";
      status.dataset.gamepadProbe = gamepadProbePassed ? "pass" : "fail";
      status.dataset.gamepadApi =
        gamepadApiAvailable ? "available" : "unavailable";
      status.dataset.wiiUAdapterProbe = wiiUAdapterProbePassed
        ? "pass"
        : "fail";
      status.dataset.wiiUAdapterApi = wiiUAdapterState.available
        ? "available"
        : "unavailable";
      status.dataset.controls =
        "keyboard-gamepad-webusb-two-controller-duel-team-lab";
      status.dataset.matchFlow = "setup-duel-results-rematch";
      status.dataset.ownerChecklist = ownerChecklistReady ? "ready-61" : "fail";
    }
    openSetup();
    requestAnimationFrame(frame);
  },

  pf_web_m4_playtest_render__sig: "vpi",
  pf_web_m4_playtest_render: function (viewPointer, viewCount) {
    var state = Module.pfM4Playtest;
    if (!state || viewCount !== 503) {
      return;
    }
    var previousTick = state.latest ? state.latest[1] : -1;
    state.latest = new Int32Array(
      HEAP32.subarray(viewPointer >> 2, (viewPointer >> 2) + viewCount)
    );

    var view = state.latest;
    if (view[0] !== 47) {
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
      "VECTOR ASCENT",
      "PUMMEL",
      "UP ATTACK",
      "DOWN ATTACK",
      "FORWARD AERIAL",
      "BACK AERIAL",
      "UP AERIAL",
      "DOWN AERIAL",
      "LEDGE ROLL",
      "LEDGE ATTACK",
      "FORWARD ATTACK",
      "FORWARD STRONG",
      "UP STRONG",
      "DOWN STRONG",
      "FORWARD STRONG CHARGE",
      "UP STRONG CHARGE",
      "DOWN STRONG CHARGE",
      "REVIVAL PLATFORM",
      "FORWARD AERIAL LANDING",
      "BACK AERIAL LANDING",
      "UP AERIAL LANDING",
      "DOWN AERIAL LANDING",
      "FORWARD AERIAL L-CANCEL LANDING",
      "BACK AERIAL L-CANCEL LANDING",
      "UP AERIAL L-CANCEL LANDING",
      "DOWN AERIAL L-CANCEL LANDING",
      "STANDING TURN",
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

    function eventMaskPlayers(mask) {
      var players = [];
      var slot;

      for (slot = 0; slot < 4; ++slot) {
        if ((mask & (1 << slot)) !== 0) {
          players.push("P" + (slot + 1));
        }
      }
      return players.join(" + ");
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
            ((event.flags & 1) !== 0 ? " · TUMBLE" : "") +
            ((event.flags & 16) !== 0 ? " · CROUCH CANCEL" : "")
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
            " entered the revival platform · " +
            event.detail +
            "f post-drop invulnerability" +
            ((event.flags & 8) !== 0 ? " · 300%" : "")
          );
        case 7:
          return "SUDDEN DEATH · " + value + "% · all players respawn";
        case 8:
          return "MATCH RESULT · " + eventWinners(event.detail) + " win";
        case 9:
          return eventMaskPlayers(event.detail) + " forfeited";
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
        case 22:
          return source + " pummeled " + target + " for " + value + "%";
        case 23:
          return (
            target +
            " left the revival platform · " +
            (event.detail === 1 ? "automatic timeout" : "player input")
          );
        case 24:
          var previousActions = event.velocityX >>> 0;
          var nextActions = event.value >>> 0;
          var transitions = [];
          var slot;

          for (slot = 0; slot < 4; ++slot) {
            if ((event.detail & (1 << slot)) !== 0) {
              var previousAction =
                (previousActions >>> (slot * 8)) & 255;
              var nextAction = (nextActions >>> (slot * 8)) & 255;
              transitions.push(
                "P" +
                  (slot + 1) +
                  " " +
                  (actionNames[previousAction] ||
                    "ACTION " + previousAction) +
                  " → " +
                  (actionNames[nextAction] || "ACTION " + nextAction)
              );
            }
          }
          return "ACTION · " + transitions.join(" · ");
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

    var eventCount = Math.max(0, Math.min(16, view[236]));
    var eventIndex;
    for (eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
      var eventBase = 237 + eventIndex * 10;
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

    context.strokeStyle = "#efb8ff";
    context.lineWidth = 5;
    context.beginPath();
    context.moveTo(sx(view[496]), sy(view[498]));
    context.lineTo(sx(view[497]), sy(view[498]));
    context.stroke();

    [0, 1, 2, 3].forEach(function (playerIndex) {
      var revivalBase = 431 + playerIndex * 4;
      if (view[revivalBase] === 0) {
        return;
      }
      var revivalLeft = sx(view[revivalBase + 1]);
      var revivalRight = sx(view[revivalBase + 2]);
      var revivalY = sy(view[revivalBase + 3]);

      context.save();
      context.shadowColor = colors[playerIndex];
      context.shadowBlur = 14;
      context.strokeStyle = colors[playerIndex];
      context.lineCap = "round";
      context.lineWidth = 7;
      context.beginPath();
      context.moveTo(revivalLeft, revivalY);
      context.lineTo(revivalRight, revivalY);
      context.stroke();
      context.shadowBlur = 0;
      context.strokeStyle = "#fff6a8";
      context.lineWidth = 2;
      context.beginPath();
      context.moveTo(revivalLeft, revivalY - 2);
      context.lineTo(revivalRight, revivalY - 2);
      context.stroke();
      if (state.collisionOverlayVisible) {
        context.strokeStyle = "#fff6a8";
        context.lineWidth = 1;
        context.setLineDash([4, 3]);
        context.strokeRect(
          revivalLeft,
          revivalY - 5,
          revivalRight - revivalLeft,
          10
        );
      }
      context.setLineDash([]);
      context.fillStyle = "#fff6a8";
      context.font = "bold 10px ui-monospace, monospace";
      context.textAlign = "center";
      context.fillText(
        "P" + (playerIndex + 1) + " REVIVAL",
        (revivalLeft + revivalRight) / 2,
        revivalY + 18
      );
      context.restore();
    });

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

    if (state.collisionOverlayVisible) {
      context.save();
      context.lineWidth = 2;
      context.strokeStyle = "#4ce0a8";
      context.beginPath();
      context.moveTo(sx(view[2]), sy(view[4]));
      context.lineTo(sx(view[3]), sy(view[4]));
      context.stroke();

      context.strokeStyle = "#67d9ff";
      context.setLineDash([8, 5]);
      context.beginPath();
      context.moveTo(sx(view[5]), sy(view[7]));
      context.lineTo(sx(view[6]), sy(view[7]));
      context.stroke();

      context.strokeStyle = "#efb8ff";
      context.beginPath();
      context.moveTo(sx(view[496]), sy(view[498]));
      context.lineTo(sx(view[497]), sy(view[498]));
      context.stroke();

      context.setLineDash([]);
      context.strokeStyle = "#d68cff";
      context.strokeRect(
        solidLeft,
        solidTop,
        solidRight - solidLeft,
        solidBottom - solidTop
      );

      context.strokeStyle = "#ff6c8f";
      context.setLineDash([7, 8]);
      context.strokeRect(
        sx(view[8]),
        sy(view[10]),
        sx(view[9]) - sx(view[8]),
        sy(view[11]) - sy(view[10])
      );
      context.restore();
    }

    context.save();
    context.fillStyle = "#eaf3ff";
    context.font = "bold 16px ui-monospace, monospace";
    context.textAlign = "center";
    context.fillText(
      "P1 STOCKS " +
        view[25 + 32] +
        "  ·  P2 STOCKS " +
        view[25 + 53 + 32],
      canvas.width / 2,
      25
    );
    context.restore();

    var itemBase = 397;
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

      if (state.collisionOverlayVisible) {
        context.save();
        context.strokeStyle = "#4ce0a8";
        context.lineWidth = 2;
        context.setLineDash([4, 3]);
        context.strokeRect(
          itemX - itemWidth / 2,
          itemY - itemHeight / 2,
          itemWidth,
          itemHeight
        );
        context.restore();
      }

      if (state.collisionOverlayVisible && view[itemBase + 5] !== 0) {
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

    var projectileBase = 415;
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
      var projectileHitboxWidth =
        sx(projectileWorldX + view[projectileBase + 9]) -
        sx(projectileWorldX - view[projectileBase + 9]);
      var projectileHitboxHeight =
        sy(projectileWorldY + view[projectileBase + 10]) -
        sy(projectileWorldY - view[projectileBase + 10]);
      var projectileWidth = Math.max(7, projectileHitboxWidth);
      var projectileHeight = Math.max(7, projectileHitboxHeight);
      var projectileOwner = view[projectileBase + 2];

      if (
        state.collisionOverlayVisible &&
        view[projectileBase + 3] !== 0
      ) {
        context.fillStyle = "#ffb34733";
        context.strokeStyle = "#ffd089";
        context.lineWidth = 2;
        context.fillRect(
          projectileX - projectileHitboxWidth / 2,
          projectileY - projectileHitboxHeight / 2,
          projectileHitboxWidth,
          projectileHitboxHeight
        );
        context.strokeRect(
          projectileX - projectileHitboxWidth / 2,
          projectileY - projectileHitboxHeight / 2,
          projectileHitboxWidth,
          projectileHitboxHeight
        );
      }

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
      var base = 25 + playerIndex * 53;
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
      var proneOrientation = view[499 + playerIndex];
      var respawning = actionState === 44;
      var onRevival = actionState === 94;
      var eliminated = actionState === 45;
      var crouched = actionState === 4 || actionState === 74;
      var tumbling =
        view[base + 22] !== 0 && actionState !== 13;
      var prone =
        actionState === 15 ||
        actionState === 23 ||
        actionState === 26 ||
        actionState === 46 ||
        (actionState === 59 && view[base + 6] !== 0);
      var invulnerable = view[base + 28] !== 0;
      var shielding = view[base + 46] !== 0;

      context.globalAlpha = eliminated ? 0.12 : respawning ? 0.32 : 1;
      if (state.collisionOverlayVisible && !eliminated) {
        var hurtboxLeft = sx(view[base] - view[12]);
        var hurtboxRight = sx(view[base] + view[12]);
        var hurtboxTop = sy(view[base + 1] - view[13]);
        var hurtboxBottom = sy(view[base + 1] + view[13]);

        context.save();
        context.fillStyle = invulnerable ? "#fff6a814" : "#73b7ff1f";
        context.strokeStyle = invulnerable ? "#fff6a8" : "#73b7ff";
        context.lineWidth = 2;
        if (invulnerable) {
          context.setLineDash([5, 4]);
        }
        context.fillRect(
          hurtboxLeft,
          hurtboxTop,
          hurtboxRight - hurtboxLeft,
          hurtboxBottom - hurtboxTop
        );
        context.strokeRect(
          hurtboxLeft,
          hurtboxTop,
          hurtboxRight - hurtboxLeft,
          hurtboxBottom - hurtboxTop
        );
        context.restore();
      }

      if (state.collisionOverlayVisible && view[base + 14]) {
        var hitboxLeft = sx(view[base + 15]);
        var hitboxRight = sx(view[base + 16]);
        var hitboxTop = sy(view[base + 17]);
        var hitboxBottom = sy(view[base + 18]);

        context.fillStyle =
          actionState === 22 || actionState === 41 ||
              (actionState >= 88 && actionState <= 90)
            ? "#ff5f874d"
            : actionState === 26
              ? "#b977ff55"
              : "#ffb34744";
        context.strokeStyle =
          actionState === 22 || actionState === 41 ||
              (actionState >= 88 && actionState <= 90)
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

      if (state.collisionOverlayVisible && view[base + 35]) {
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
        var shieldInputFraction = Math.max(
          0,
          Math.min(1, view[base + 45] / 65535)
        );
        var lightShielding =
          view[base + 27] === 0 && view[base + 45] < 32768;
        var shieldLeft = sx(view[base + 47]);
        var shieldRight = sx(view[base + 48]);
        var shieldTop = sy(view[base + 49]);
        var shieldBottom = sy(view[base + 50]);
        var shieldWidth = Math.max(1, shieldRight - shieldLeft);
        var shieldHeight = Math.max(1, shieldBottom - shieldTop);
        var shieldCenterX = (shieldLeft + shieldRight) / 2;
        var shieldCenterY = (shieldTop + shieldBottom) / 2;
        var shieldPresentationPadding = lightShielding ? 22 : 14;
        var shieldHealthFraction = Math.max(
          0,
          Math.min(1, view[base + 25] / (60 * 65536))
        );
        var shieldDensityScale =
          view[base + 45] <= 8192
            ? 1
            : view[base + 45] >= 32768
              ? 0.5
              : 1 -
                0.5 *
                  ((view[base + 45] - 8192) / (32768 - 8192));
        var shieldMinimumScale = 0.15;
        var shieldFullHealthScale =
          shieldMinimumScale +
          (1 - shieldMinimumScale) * shieldDensityScale;
        var shieldCurrentScale =
          shieldMinimumScale +
          (1 - shieldMinimumScale) *
            shieldHealthFraction * shieldDensityScale;
        var shieldHealthPresentationRatio =
          shieldCurrentScale / shieldFullHealthScale;
        var shieldFullHealthPresentationWidth = Math.max(
          shieldWidth / shieldHealthPresentationRatio,
          width + shieldPresentationPadding
        );
        var shieldFullHealthPresentationHeight = Math.max(
          shieldHeight / shieldHealthPresentationRatio,
          height + shieldPresentationPadding
        );
        var shieldPresentationWidth =
          shieldFullHealthPresentationWidth *
          shieldHealthPresentationRatio;
        var shieldPresentationHeight =
          shieldFullHealthPresentationHeight *
          shieldHealthPresentationRatio;
        var shieldPresentationTop =
          shieldCenterY - shieldPresentationHeight / 2;
        var shieldPercent = Math.round(shieldInputFraction * 100);
        var shieldLabel =
          view[base + 27] !== 0
            ? "POWERSHIELD"
            : lightShielding
              ? "LIGHT SHIELD " + shieldPercent + "%"
              : (shieldPercent >= 99 ? "FULL SHIELD " : "DENSE SHIELD ") +
                shieldPercent +
                "%";
        context.save();
        if (state.collisionOverlayVisible) {
          context.fillStyle = "#a991ff18";
          context.strokeStyle = "#d8d0ff";
          context.lineWidth = 1.5;
          context.setLineDash([3, 3]);
          context.fillRect(
            shieldLeft,
            shieldTop,
            shieldWidth,
            shieldHeight
          );
          context.strokeRect(
            shieldLeft,
            shieldTop,
            shieldWidth,
            shieldHeight
          );
          context.setLineDash([]);
        }
        context.fillStyle =
          view[base + 27] !== 0
            ? "#f7fbff77"
            : colors[playerIndex] + (lightShielding ? "38" : "62");
        context.strokeStyle =
          view[base + 27] !== 0
            ? "#ffffff"
            : lightShielding
              ? "#f2dcff"
              : "#f8fbff";
        context.lineWidth =
          view[base + 27] !== 0 ? 4 : lightShielding ? 3 : 4;
        context.shadowColor = colors[playerIndex];
        context.shadowBlur = lightShielding ? 12 : 18;
        context.beginPath();
        context.ellipse(
          shieldCenterX,
          shieldCenterY,
          shieldPresentationWidth / 2,
          shieldPresentationHeight / 2,
          0,
          0,
          Math.PI * 2
        );
        context.fill();
        context.stroke();
        context.shadowBlur = 0;
        if (lightShielding) {
          context.strokeStyle = colors[playerIndex] + "bb";
          context.lineWidth = 2;
          context.setLineDash([6, 4]);
          context.beginPath();
          context.ellipse(
            shieldCenterX,
            shieldCenterY,
            shieldPresentationWidth / 2 + 4,
            shieldPresentationHeight / 2 + 4,
            0,
            0,
            Math.PI * 2
          );
          context.stroke();
          context.setLineDash([]);
        }
        context.font = "700 11px ui-monospace, SFMono-Regular, monospace";
        context.textAlign = "center";
        context.textBaseline = "bottom";
        context.lineWidth = 4;
        context.strokeStyle = "#08111f";
        context.strokeText(
          shieldLabel,
          shieldCenterX,
          shieldPresentationTop - 7
        );
        context.fillStyle =
          lightShielding ? "#f2dcff" : "#f8fbff";
        context.fillText(
          shieldLabel,
          shieldCenterX,
          shieldPresentationTop - 7
        );
        context.restore();
      }

      context.save();
      if (prone) {
        var standingHeight = height;
        width = Math.max(width * 1.35, height * 1.15);
        height = Math.max(12, width * 0.28);
        y += (standingHeight - height) / 2;
      } else if (crouched) {
        var uprightHeight = height;
        width = Math.max(18, width * 1.18);
        height = Math.max(16, height * 0.58);
        y += (uprightHeight - height) / 2;
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
      if (prone && proneOrientation !== 0) {
        context.fillStyle = proneOrientation === 1 ? "#e9fbff" : "#ffbe78";
        context.fillRect(
          -width * 0.34,
          proneOrientation === 1 ? -height / 2 + 2 : height / 2 - 4,
          width * 0.68,
          2
        );
      }
      context.shadowBlur = 0;
      context.fillStyle = "#07111c";
      context.beginPath();
      context.moveTo(facing * width * 0.45, -5);
      context.lineTo(facing * width * 0.75, 0);
      context.lineTo(facing * width * 0.45, 5);
      context.closePath();
      context.fill();
      context.restore();
      if (crouched) {
        var crouchCueY = y - height / 2 - 21;
        context.save();
        context.globalAlpha = eliminated ? 0.12 : 1;
        context.fillStyle = "#07111ddd";
        context.strokeStyle = colors[playerIndex];
        context.lineWidth = 1.5;
        context.fillRect(x - 27, crouchCueY - 8, 54, 16);
        context.strokeRect(x - 27, crouchCueY - 8, 54, 16);
        context.fillStyle = "#f3f8ff";
        context.font = "800 9px ui-monospace, monospace";
        context.textAlign = "center";
        context.textBaseline = "middle";
        context.fillText("CROUCH", x, crouchCueY);
        context.strokeStyle = colors[playerIndex];
        context.lineWidth = 2;
        context.beginPath();
        context.moveTo(x - 5, crouchCueY + 11);
        context.lineTo(x, crouchCueY + 16);
        context.lineTo(x + 5, crouchCueY + 11);
        context.stroke();
        context.restore();
      }
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
      if (onRevival) {
        context.save();
        context.fillStyle = "#fff6a8";
        context.font = "bold 13px ui-monospace, monospace";
        context.textAlign = "center";
        context.fillText(
          "MOVE / BUTTON TO DROP",
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
      var proneOrientationNames = ["none", "back", "stomach"];
      if (view[base + 22] !== 0) {
        action = "TUMBLE · " + action;
      }
      var staleMoveBase = 447 + playerIndex * 12;
      var staleMoveQueue = [];
      var staleMoveSlot;
      for (
        staleMoveSlot = 0;
        staleMoveSlot < view[staleMoveBase];
        staleMoveSlot += 1
      ) {
        var staleMoveId = view[staleMoveBase + 3 + staleMoveSlot];
        staleMoveQueue.push(
          actionNames[staleMoveId] || "STATE " + staleMoveId
        );
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
        " · prone " +
        (proneOrientationNames[proneOrientation] ||
          "orientation " + proneOrientation) +
        "<br>shield " +
        (view[base + 25] / q16).toFixed(2) +
        " / 60 · shield stun " +
        view[base + 26] +
        " · strength " +
        Math.round((view[base + 45] / 65535) * 100) +
        "% (" +
        view[base + 45] +
        ") · tilt x " +
        view[base + 51] +
        " · y " +
        view[base + 52] +
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
        " / 120f · Smash charge " +
        view[base + 44] +
        " / 60f" +
        " · Vector Ascent " +
        (view[427 + playerIndex] !== 0 ? "READY" : "SPENT") +
        "<br>revival platform " +
        (view[431 + playerIndex * 4] !== 0
          ? "ACTIVE · move/button to drop"
          : "inactive") +
        "<br>stale queue newest first " +
        (staleMoveQueue.length === 0
          ? "empty"
          : staleMoveQueue.join(" ← ")) +
        " · selected move scale " +
        ((view[staleMoveBase + 1] / q16) * 100).toFixed(3) +
        "% · attack registered " +
        view[staleMoveBase + 2];
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
      view[itemBase + 13] +
      " · stale registered " +
      view[495];

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
    state.resetButton.textContent =
      view[22] !== 0 || view[23] !== 0 ? "Rematch" : "Reset";
    if (view[22] !== 0 || view[23] !== 0) {
      state.running = false;
      state.pauseButton.textContent = "Resume";
      state.pauseButton.setAttribute("aria-pressed", "true");
      state.setMatchFlow("results");
    }
  },
});
