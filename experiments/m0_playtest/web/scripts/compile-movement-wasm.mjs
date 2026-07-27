import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { runClang } from "@yowasp/clang";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const sourcePaths = {
  "movement_model.c": path.join(root, "..", "movement_model.c"),
  "movement_model.h": path.join(root, "..", "movement_model.h"),
  "movement_web.c": path.join(root, "native", "movement_web.c"),
  "movement_web_runtime.c": path.join(root, "native", "movement_web_runtime.c"),
};
const sources = {};

for (const [name, sourcePath] of Object.entries(sourcePaths)) {
  sources[name] = await readFile(sourcePath, "utf8");
}

const outputName = "movement_core.wasm";
const outputs = await runClang(
  [
    "clang",
    "-std=c17",
    "-O3",
    "-ffreestanding",
    "-fno-builtin-memcpy",
    "-fno-builtin-memset",
    "-nostdlib",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Werror",
    "-Wl,--no-entry",
    "-Wl,--strip-all",
    "movement_model.c",
    "movement_web.c",
    "movement_web_runtime.c",
    "-o",
    outputName,
  ],
  sources,
);

const wasm = outputs[outputName];
if (!wasm) {
  throw new Error("Clang completed without producing movement_core.wasm");
}

const publicDir = path.join(root, "public");
await mkdir(publicDir, { recursive: true });
await writeFile(path.join(publicDir, outputName), wasm);

const sourceHashes = Object.fromEntries(
  Object.keys(sourcePaths).map((name) => [
    name,
    createHash("sha256").update(sources[name]).digest("hex"),
  ]),
);
const manifest = {
  abi: 2,
  compiler: "@yowasp/clang 22.0.0-git20542-10",
  sourceHashes,
  wasmBytes: wasm.byteLength,
  wasmSha256: createHash("sha256").update(wasm).digest("hex"),
};
await writeFile(
  path.join(publicDir, "movement_core.manifest.json"),
  `${JSON.stringify(manifest, null, 2)}\n`,
);
console.log(
  `Compiled ${outputName}: ${wasm.byteLength} bytes, sha256=${manifest.wasmSha256}`,
);
