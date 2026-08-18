import type { TransportStatus } from './types';

export interface LineTransport {
  readonly label: string;
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  write(line: string): Promise<void>;
  onLine(listener: (line: string) => void): () => void;
  onStatus(listener: (status: TransportStatus, detail?: string) => void): () => void;
}

export class TransportEvents {
  private lineListeners = new Set<(line: string) => void>();
  private statusListeners = new Set<(status: TransportStatus, detail?: string) => void>();

  onLine(listener: (line: string) => void): () => void {
    this.lineListeners.add(listener);
    return () => this.lineListeners.delete(listener);
  }

  onStatus(listener: (status: TransportStatus, detail?: string) => void): () => void {
    this.statusListeners.add(listener);
    return () => this.statusListeners.delete(listener);
  }

  emitLine(line: string): void {
    this.lineListeners.forEach((listener) => listener(line));
  }

  emitStatus(status: TransportStatus, detail?: string): void {
    this.statusListeners.forEach((listener) => listener(status, detail));
  }
}
