import type { LineTransport } from './transport';
import { TransportEvents } from './transport';
import type { TransportStatus } from './types';

type SerialPortLike = {
  readable: ReadableStream<Uint8Array> | null;
  writable: WritableStream<Uint8Array> | null;
  open(options: { baudRate: number; dataBits?: number; stopBits?: number; parity?: string }): Promise<void>;
  close(): Promise<void>;
};

type SerialApiLike = {
  requestPort(): Promise<SerialPortLike>;
};

function getSerialApi(): SerialApiLike | null {
  return (navigator as Navigator & { serial?: SerialApiLike }).serial ?? null;
}

export class WebSerialTransport implements LineTransport {
  readonly label = 'Web Serial';
  private readonly events = new TransportEvents();
  private readonly baudRate: number;
  private port: SerialPortLike | null = null;
  private reader: ReadableStreamDefaultReader<string> | null = null;
  private readLoopPromise: Promise<void> | null = null;
  private buffer = '';

  constructor(baudRate = 115200) {
    this.baudRate = baudRate;
  }

  get supported(): boolean {
    return getSerialApi() !== null;
  }

  onLine(listener: (line: string) => void): () => void {
    return this.events.onLine(listener);
  }

  onStatus(listener: (status: TransportStatus, detail?: string) => void): () => void {
    return this.events.onStatus(listener);
  }

  async connect(): Promise<void> {
    const serial = getSerialApi();
    if (!serial) {
      const message = 'Web Serial is unavailable. Use Chrome or Edge on HTTPS or localhost.';
      this.events.emitStatus('error', message);
      throw new Error(message);
    }

    this.events.emitStatus('connecting');
    try {
      this.port = await serial.requestPort();
      console.info('[WebSerial] port selected', this.port);
      await this.port.open({ baudRate: this.baudRate, dataBits: 8, stopBits: 1, parity: 'none' });
      console.info('[WebSerial] port opened', { baudRate: this.baudRate, readable: Boolean(this.port.readable), writable: Boolean(this.port.writable) });
      this.events.emitStatus('connected', `USB serial at ${this.baudRate} baud`);
      this.readLoopPromise = this.readLoop();
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unable to open serial port';
      this.events.emitStatus('error', message);
      this.port = null;
      throw error;
    }
  }

  async disconnect(): Promise<void> {
    const reader = this.reader;
    this.reader = null;
    if (reader) {
      await reader.cancel().catch(() => undefined);
      reader.releaseLock();
    }

    await this.readLoopPromise?.catch(() => undefined);
    this.readLoopPromise = null;

    if (this.port) {
      await this.port.close().catch(() => undefined);
      this.port = null;
    }

    this.buffer = '';
    this.events.emitStatus('disconnected');
  }

  async write(line: string): Promise<void> {
    if (!this.port?.writable) throw new Error('Serial port is not connected');
    const writer = this.port.writable.getWriter();
    try {
      await writer.write(new TextEncoder().encode(line));
    } finally {
      writer.releaseLock();
    }
  }

  private async readLoop(): Promise<void> {
    if (!this.port?.readable) return;

    const decoder = new TextDecoderStream();
    const decoded = this.port.readable.pipeThrough(decoder as unknown as ReadableWritablePair<string, Uint8Array>);
    const reader = decoded.getReader();
    this.reader = reader;

    try {
      while (this.reader === reader) {
        const { value, done } = await reader.read();
        if (done) {
          console.info('[WebSerial] read loop ended');
          break;
        }
        console.info('[WebSerial] decoded chunk', JSON.stringify(value));
        this.buffer += value;

        let newlineIndex = this.buffer.indexOf('\n');
        while (newlineIndex >= 0) {
          const line = this.buffer.slice(0, newlineIndex).replace(/\r$/, '');
          this.buffer = this.buffer.slice(newlineIndex + 1);
          if (line.length > 0) {
            console.info('[WebSerial] RX line', JSON.stringify(line));
            this.events.emitLine(line);
          }
          newlineIndex = this.buffer.indexOf('\n');
        }

        if (this.buffer.length > 4096) {
          this.buffer = '';
          this.events.emitStatus('error', 'Serial frame exceeded 4 KB and was discarded');
        }
      }
    } catch (error) {
      if (this.reader === reader) {
        const message = error instanceof Error ? error.message : 'Serial read failed';
        console.error('[WebSerial] read error', error);
        this.events.emitStatus('error', message);
      }
    } finally {
      console.info('[WebSerial] read loop cleanup');
      if (this.reader === reader) this.reader = null;
      reader.releaseLock();
    }
  }
}
