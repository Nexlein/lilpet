// What a page reports as an error: tests end by expecting none
export function collectErrors(page) {
    const errors = [];
    page.on('pageerror', (err) => errors.push(err.message));
    page.on('console', (msg) => msg.type() === 'error' && errors.push(msg.text()));
    return errors;
}
