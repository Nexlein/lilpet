// PC app: React + Vite. The C engine comes from the WebAssembly build (mise run engine:wasm): served in development
// and copied into the build under engine/, next to the app.
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import react from '@vitejs/plugin-react';
import type { Plugin } from 'vite';
import { defineConfig } from 'vitest/config';

const ENGINE_DIR = fileURLToPath(new URL('../engine/build/wasm/', import.meta.url));
const ENGINE_FILES = { 'lilpet.mjs': 'text/javascript', 'lilpet.wasm': 'application/wasm' } as const;
type EngineFile = keyof typeof ENGINE_FILES;

function readEngine(file: EngineFile) {
    try {
        return readFileSync(ENGINE_DIR + file);
    } catch {
        throw new Error(`${ENGINE_DIR}${file} is missing: build the engine with "mise run engine:wasm"`);
    }
}

function engineFiles(): Plugin {
    return {
        name: 'lilpet-engine',
        configureServer(server) {
            server.middlewares.use('/engine/', (req, res, next) => {
                const file = (req.url ?? '').split('?')[0]?.slice(1) ?? '';
                if (!Object.hasOwn(ENGINE_FILES, file)) return next();
                res.setHeader('Content-Type', ENGINE_FILES[file as EngineFile]);
                res.end(readEngine(file as EngineFile));
            });
        },
        generateBundle() {
            for (const file of Object.keys(ENGINE_FILES) as EngineFile[])
                this.emitFile({ type: 'asset', fileName: `engine/${file}`, source: readEngine(file) });
        }
    };
}

export default defineConfig({
    base: './', // relative paths: the build works from any folder (the Playwright tests serve it from /dist/)
    plugins: [react(), engineFiles()],
    server: { port: 8467, strictPort: true }, // the port the README and .claude/launch.json give: fail if it is taken
    test: { include: ['src/**/*.test.ts'] }
});
