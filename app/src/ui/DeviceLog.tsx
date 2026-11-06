// What the device writes, in a folding panel that follows the last line unless it was scrolled up
import { useLayoutEffect, useRef, type UIEvent } from 'react';

const END_MARGIN = 8; // px from the end that still count as the end

export function DeviceLog({ lines }: { lines: readonly string[] }) {
    const pre = useRef<HTMLPreElement>(null);
    const atEnd = useRef(true);

    const follow = () => {
        if (pre.current && atEnd.current) pre.current.scrollTop = pre.current.scrollHeight;
    };
    useLayoutEffect(follow, [lines]);

    const noteEnd = ({ currentTarget: el }: UIEvent<HTMLPreElement>) => {
        atEnd.current = el.scrollHeight - el.scrollTop - el.clientHeight < END_MARGIN;
    };

    return (
        <section aria-label="Journal de lilpet">
            <details className="log">
                <summary>Journal de lilpet</summary>
                <pre ref={pre} aria-live="off" onScroll={noteEnd}>
                    {lines.length ? lines.join('\n') : 'Rien pour l’instant.'}
                </pre>
            </details>
        </section>
    );
}
