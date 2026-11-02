// The built PC app (npm run app:build) opens, runs the C engine and picks looks. CI builds it before this test, and
// fails it if the build is missing; locally the test is skipped until the app is built.
import { existsSync } from 'node:fs';
import { expect, test } from '@playwright/test';

test('the PC app shows live eyes and picks looks', async ({ page }) => {
    test.skip(!existsSync('app/dist/index.html') && !process.env.CI, 'build the app first: npm run app:build');
    const errors = [];
    page.on('pageerror', (err) => errors.push(err.message));
    page.on('console', (msg) => msg.type() === 'error' && errors.push(msg.text()));

    await page.goto('/app/dist/index.html');
    await expect(page.locator('.grid .chip')).toHaveCount(100);
    await expect(page.locator('canvas.screen')).toBeVisible();

    // The engine draws: some pixels of the screen are lit
    await expect
        .poll(() =>
            page.locator('canvas.screen').evaluate((canvas) => {
                const data = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
                let lit = 0;
                for (let i = 0; i < data.length; i += 4) if (data[i] + data[i + 1] + data[i + 2] > 0) lit++;
                return lit / (canvas.width * canvas.height);
            })
        )
        .toBeGreaterThan(0.05);

    await page.locator('.grid .chip', { hasText: 'socks' }).click();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
    await page.locator('.pupil', { hasText: 'Gland' }).click(); // the label: the radio itself is hidden
    await page.locator('.favorite').click();
    await expect(page.locator('.favorites .chip')).toHaveText(['socks gland']);

    // The choice survives a reload
    await page.reload();
    await expect(page.locator('.look-name h1')).toHaveText('socks');
    await expect(page.getByLabel('Gland')).toBeChecked();

    expect(errors, 'console errors in the PC app').toEqual([]);
});
