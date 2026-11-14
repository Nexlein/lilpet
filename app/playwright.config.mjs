// Browser tests of the PC app: its build (npm run app:build) with a fake board on navigator.serial (tests/app.spec.mjs),
// in headless Chromium. The build needs the WebAssembly engine (mise run engine:wasm). The tests serve this folder on
// their own port, 8468, so no other server is ever tested instead; their reports stay in this folder too.
import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
    testDir: 'tests',
    reporter: process.env.CI ? [['list'], ['html', { open: 'never' }]] : 'list',
    use: { baseURL: 'http://127.0.0.1:8468' },
    projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
    webServer: {
        command: 'python3 -m http.server 8468 --bind 127.0.0.1',
        url: 'http://127.0.0.1:8468/index.html', // always there, built or not
        reuseExistingServer: false
    }
});
