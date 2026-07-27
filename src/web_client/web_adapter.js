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
});
