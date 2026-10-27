// Browser tests: the parity test renders every reference scene with the prototype and the C engine (WebAssembly)
// in headless Chromium. Needs the WebAssembly build: mise run engine:wasm.
import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
    testDir: 'tests',
    reporter: process.env.CI ? [['list'], ['html', { open: 'never' }]] : 'list',
    use: { baseURL: 'http://127.0.0.1:8466' },
    projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
    webServer: {
        command: 'python3 -m http.server 8466 --bind 127.0.0.1',
        url: 'http://127.0.0.1:8466/prototype/reference.html',
        reuseExistingServer: !process.env.CI
    }
});
