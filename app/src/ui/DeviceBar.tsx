// Connection to the device, in the header
import { drawsLike } from '../device/protocol';
import type { DeviceState } from '../device/session';

interface Props {
    state: DeviceState;
    appEngine: string | null; // the version of the engine that draws the preview, once loaded
    onConnect: () => void;
    onDisconnect: () => void;
}

export function DeviceBar({ state, appEngine, onConnect, onDisconnect }: Props) {
    if (state.status === 'unsupported') return null; // said by the send note
    const connected = state.status === 'connected';
    const connecting = state.status === 'connecting';
    // As the board says it in its hello, at any length: the bar cuts it, the tooltip shows it whole
    const info = connected ? `${state.info.board}, firmware ${state.info.firmware}, moteur ${state.info.engine}` : '';
    return (
        <div className={connected ? 'device connected' : 'device'}>
            {state.status === 'idle' && state.error && (
                <p className="device-error" role="alert">
                    {state.error}
                </p>
            )}
            <p role="status">
                {connecting && 'Connexion à lilpet…'}
                {connected && (
                    <>
                        <span className="led" aria-hidden="true" />
                        lilpet est branché
                        <small title={info}>
                            {/* First on the line, which is cut at its end: the bar keeps two lines, also on a phone */}
                            {appEngine && !drawsLike(state.info.engine, appEngine) && (
                                <span
                                    className="device-note"
                                    title={`L’aperçu dessine avec le moteur ${appEngine} : les yeux de lilpet peuvent en différer.`}
                                >
                                    autre moteur que l’aperçu
                                    <span className="visually-hidden"> ({appEngine})</span> ·{' '}
                                </span>
                            )}
                            {info}
                        </small>
                    </>
                )}
            </p>
            {/* The same button in every state, so the keyboard focus stays on it. Connecting, a click does nothing */}
            <button type="button" aria-disabled={connecting} onClick={connected ? onDisconnect : onConnect}>
                {connected ? 'Débrancher' : 'Brancher lilpet'}
            </button>
        </div>
    );
}
