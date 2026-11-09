// Browser tests: the built PC app with a fake board on navigator.serial (tests/app.spec.mjs), in headless Chromium, and
// the data scripts on broken copies of the data (tests/data.spec.mjs). The app needs the WebAssembly build (mise run
// engine:wasm). The tests serve this checkout on their own port, 8468, so no other server is ever tested instead.
import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
    testDir: 'tests',
    reporter: process.env.CI ? [['list'], ['html', { open: 'never' }]] : 'list',
    use: { baseURL: 'http://127.0.0.1:8468' },
    projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
    webServer: {
        command: 'python3 -m http.server 8468 --bind 127.0.0.1',
        url: 'http://127.0.0.1:8468/eyes/looks.json', // always there, built or not
        reuseExistingServer: false
    }
});
