declare module 'ws' {
  export class WebSocketServer {
    constructor(options?: any);
    on(event: 'connection', listener: (socket: any, request?: any) => void): this;
    on(event: 'error', listener: (error: any) => void): this;
    close(callback?: (error?: Error) => void): void;
  }

  export function createWebSocketStream(ws: any, options?: any): any;
}
