// PC app: React + Vite. The C engine comes from the WebAssembly build (mise run engine:wasm): served in development
// and copied into the build under engine/, next to the app.
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import react from '@vitejs/plugin-react';
import { defineConfig } from 'vitest/config';

const ENGINE_DIR = fileURLToPath(new URL('../engine/build/wasm/', import.meta.url));
const ENGINE_FILES = { 'lilpet.mjs': 'text/javascript', 'lilpet.wasm': 'application/wasm' };

function readEngine(file) {
    try {
        return readFileSync(ENGINE_DIR + file);
    } catch {
        throw new Error(`${ENGINE_DIR}${file} is missing: build the engine with "mise run engine:wasm"`);
    }
}

function engineFiles() {
    return {
        name: 'lilpet-engine',
        configureServer(server) {
            server.middlewares.use('/engine/', (req, res, next) => {
                const file = req.url.split('?')[0].slice(1);
                if (!Object.hasOwn(ENGINE_FILES, file)) return next();
                res.setHeader('Content-Type', ENGINE_FILES[file]);
                res.end(readEngine(file));
            });
        },
        generateBundle() {
            for (const file of Object.keys(ENGINE_FILES))
                this.emitFile({ type: 'asset', fileName: `engine/${file}`, source: readEngine(file) });
        }
    };
}

export default defineConfig({
    base: './', // relative paths: the build works from any folder (tests serve it from /app/dist/)
    plugins: [react(), engineFiles()],
    server: { port: 8467 },
    test: { include: ['src/**/*.test.ts'] }
});
