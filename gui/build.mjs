import * as esbuild from "esbuild";
import { cp, mkdir, rm } from "node:fs/promises";

const isServe = process.argv.includes("--serve");
const outdir = "dist";

const buildOptions = {
  entryPoints: ["src/index.tsx"],
  outdir,
  bundle: true,
  format: "esm",
  target: "es2015",
  sourcemap: false,
  minify: !isServe,
  entryNames: "[name]",
  jsx: "automatic",
  define: {
    "process.env.NODE_ENV": isServe ? '"development"' : '"production"',
  },
  logLevel: "info",
};

await rm(outdir, { recursive: true, force: true });
await mkdir(outdir, { recursive: true });
await cp("public/index.html", `${outdir}/index.html`);

if (isServe) {
  const ctx = await esbuild.context(buildOptions);
  await ctx.watch();
  await ctx.serve({ servedir: outdir, port: 3000 });
} else {
  await esbuild.build(buildOptions);
}
