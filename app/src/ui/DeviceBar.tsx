// Connection to the device, in the header
import type { DeviceState } from './hooks';

interface Props {
    state: DeviceState;
    onConnect(): void;
    onDisconnect(): void;
}

export function DeviceBar({ state, onConnect, onDisconnect }: Props) {
    switch (state.status) {
        case 'unsupported':
            return <p className="device">Envoi à lilpet : ouvre l’app dans Chrome ou Edge sur ordinateur.</p>;
        case 'connecting':
            return <p className="device">Connexion à lilpet…</p>;
        case 'connected':
            return (
                <div className="device connected">
                    <p>
                        <span className="led" aria-hidden="true" />
                        lilpet est branché
                        <small>
                            firmware {state.info.firmware}, moteur {state.info.engine}
                        </small>
                    </p>
                    <button type="button" onClick={onDisconnect}>
                        Débrancher
                    </button>
                </div>
            );
        case 'idle':
            return (
                <div className="device">
                    {state.error && (
                        <p className="device-error" role="alert">
                            {state.error}
                        </p>
                    )}
                    <button type="button" onClick={onConnect}>
                        Brancher lilpet
                    </button>
                </div>
            );
    }
}
