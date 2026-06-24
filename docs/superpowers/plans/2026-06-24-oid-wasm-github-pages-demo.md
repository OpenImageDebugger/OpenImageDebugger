# OID WASM GitHub Pages Demo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish an auto-play OID WASM demo at `https://openimagedebugger.github.io/` plotting `sample_buffer_1` and `sample_buffer_2` from `test.py`.

**Architecture:** Demo shell (`wasm/demo.html` + `wasm/demo-buffers.js`) lives in the OID repo, is packed into `wasm/dist/index.html`, and deployed to the org-site repo by a `deploy-pages` CI job on `main`.

**Tech Stack:** Qt 6.11 WASM, vanilla JS (ES modules), Node `node:test`, GitHub Actions (`peaceiris/actions-gh-pages`).

**Spec:** `docs/superpowers/specs/2026-06-24-oid-wasm-github-pages-demo-design.md`

---

### Task 1: Buffer generation module

**Files:**
- Create: `wasm/demo-buffers.js`
- Test: `wasm/test/demo-buffers.test.mjs`

- [x] Port `_gen_color` and `_gen_buffers` from `resources/oidscripts/test.py`
- [x] Golden SHA256 parity at 400×200 (Python reference hashes in test)
- [x] Handle Python `min(5.0, nan) == 5.0` (not `Math.min`)

Run: `cd wasm && npm test`  
Expected: 2 passing tests

---

### Task 2: Demo shell page

**Files:**
- Create: `wasm/demo.html`

- [x] iframe → `loader.html`, status bar, 60 s ready timeout
- [x] On `viewer-ready`: `SetAvailableSymbols` then two `PlotBufferContents` frames
- [x] Import `genBuffers` from `demo-buffers.js`

---

### Task 3: Pack and serve scripts

**Files:**
- Modify: `wasm/scripts/pack-viewer-wasm.sh`
- Modify: `wasm/scripts/serve-wasm.sh`
- Modify: `wasm/package.json` (`type: module`, `test` script)

- [x] Pack `demo.html` → `dist/index.html`, copy `demo-buffers.js`
- [x] `serve-wasm.sh` documents demo URL at `/`

---

### Task 4: CI build test + deploy

**Files:**
- Modify: `.github/workflows/wasm.yml`

- [x] `push` to `main` triggers workflow
- [x] `npm test --prefix wasm` in build job
- [x] `deploy-pages` job pushes `wasm/dist` to `openimagedebugger.github.io`

**Manual prerequisite:** `OID_PAGES_DEPLOY_KEY` secret + deploy key on org-site repo.

---

### Task 5: Verify locally (requires WASM build)

Run after `wasm/scripts/build-wasm.sh && wasm/scripts/pack-viewer-wasm.sh`:

```bash
cd wasm && npm run serve
# open http://localhost:8765/
```

Expected: both sample buffers visible in the OID viewer.

---

### Task 6: Post-merge deploy check

After merging to `main`:

1. WASM Build workflow → `deploy-pages` green
2. `openimagedebugger.github.io` repo has new commit
3. https://openimagedebugger.github.io/ loads demo
