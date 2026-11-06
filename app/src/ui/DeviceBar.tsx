// Connection to the device, in the header
import type { DeviceState } from '../device/session';

interface Props {
    state: DeviceState;
    onConnect(): void;
    onDisconnect(): void;
}

export function DeviceBar({ state, onConnect, onDisconnect }: Props) {
    if (state.status === 'unsupported') return null; // said by the send note
    const connected = state.status === 'connected';
    const connecting = state.status === 'connecting';
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
                        <small>
                            {state.info.board && `${state.info.board}, `}firmware {state.info.firmware}, moteur{' '}
                            {state.info.engine}
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
