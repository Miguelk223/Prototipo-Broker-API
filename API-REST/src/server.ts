import net from 'net';
import http from 'http';
import express, { Request, Response } from 'express';
import { WebSocketServer, createWebSocketStream } from 'ws';
import Aedes, { Client, PublishPacket } from 'aedes';
import dotenv from 'dotenv';

dotenv.config();

const config = {
  mqttHost: process.env.MQTT_HOST || '0.0.0.0',
  mqttPort: Number(process.env.MQTT_PORT ?? 1883),
  wsPort: Number(process.env.WS_PORT ?? 8883),
  apiPort: Number(process.env.PORT ?? 3000),
  mqttUser: process.env.MQTT_USER || 'Administrador',
  mqttPass: process.env.MQTT_PASS || 'PalmasInn2026',
};

const aedes = new Aedes();
const app = express();
const httpServer = http.createServer(app);

app.use(express.json());

const toPayloadBuffer = (payload: unknown): Buffer => {
  if (Buffer.isBuffer(payload)) return payload;
  if (typeof payload === 'string') return Buffer.from(payload);
  return Buffer.from(JSON.stringify(payload));
};

aedes.authenticate = (client, username, password, callback) => {
  const providedUser = typeof username === 'string' ? username : '';
  const providedPass = password ? password.toString() : '';

  const isValidUser = providedUser === config.mqttUser;
  const isValidPass = providedPass === config.mqttPass;

  if (isValidUser && isValidPass) {
    console.log(`[AUTH] Cliente autenticado: ${client.id}`);
    return callback(null, true);
  }

  console.warn(`[AUTH] Credenciales inválidas: ${client.id}`);
  return callback({ returnCode: 4 } as any, false);
};

aedes.authorizePublish = (client: Client | null, packet: PublishPacket, callback: (error?: Error | null) => void) => {
  if (!client) return callback(new Error('Cliente no identificado'));
  if (packet.topic.startsWith('$SYS') && client.id !== 'internal-system') {
    return callback(new Error('No autorizado para publicar en $SYS'));
  }
  callback(null);
};

aedes.authorizeSubscribe = (_client: Client, sub: any, callback: (error: Error | null, grantedSub?: any) => void) => {
  callback(null, sub);
};

app.get('/health', (_req: Request, res: Response) => {
  res.json({
    ok: true,
    service: 'mqtt-api-broker',
    broker: {
      host: config.mqttHost,
      mqttPort: config.mqttPort,
      wsPort: config.wsPort,
    },
    endpoints: {
      mqtt: `mqtt://${config.mqttHost}:${config.mqttPort}`,
      websocket: `ws://${config.mqttHost}:${config.wsPort}`,
      http: `http://${config.mqttHost}:${config.apiPort}`,
    },
    timestamp: new Date().toISOString(),
  });
});

app.get('/api/status', (_req: Request, res: Response) => {
  res.json({
    ok: true,
    broker: 'online',
    connectedClients: aedes.connectedClients,
    ports: {
      mqtt: config.mqttPort,
      websocket: config.wsPort,
      http: config.apiPort,
    },
  });
});

app.post('/api/publish', (req: Request, res: Response) => {
  const { topic = 'osmosis/test', payload = { ok: true }, qos = 0, retain = false } = req.body ?? {};

  const packet: any = {
    cmd: 'publish',
    qos: Number(qos) as 0 | 1 | 2,
    dup: false,
    retain: Boolean(retain),
    topic: String(topic),
    payload: toPayloadBuffer(payload),
  };

  aedes.publish(packet, (error) => {
    if (error) {
      console.error(`[API ERROR] No se pudo publicar: ${error.message}`);
      return res.status(500).json({ ok: false, error: error.message });
    }

    console.log(`[HTTP -> MQTT] Mensaje publicado en tópico: ${topic}`);
    res.json({ ok: true, topic, published: true });
  });
});

app.post('/api/command', (req: Request, res: Response) => {
  const { deviceId = 'esp32_01', command } = req.body ?? {};

  if (!command || typeof command !== 'string') {
    return res.status(400).json({ ok: false, error: 'El campo command es obligatorio' });
  }

  const topic = `osmosis/device/${deviceId}/command`;
  const packet: any = {
    cmd: 'publish',
    qos: 0,
    dup: false,
    retain: false,
    topic,
    payload: toPayloadBuffer(command),
  };

  aedes.publish(packet, (error) => {
    if (error) {
      console.error(`[API ERROR] No se pudo enviar comando: ${error.message}`);
      return res.status(500).json({ ok: false, error: error.message });
    }

    console.log(`[HTTP -> MQTT] Comando enviado a ${deviceId} en tópico ${topic}: ${command}`);
    res.json({ ok: true, deviceId, command, topic });
  });
});

aedes.on('client', (client: Client) => {
  console.log(`[MQTT CONEXIÓN] Cliente conectado: ${client.id}`);
});

aedes.on('clientDisconnect', (client: Client) => {
  console.log(`[MQTT DESCONEXIÓN] Cliente desconectado: ${client.id}`);
});

aedes.on('publish', (packet: PublishPacket, client: Client | null) => {
  const source = client ? client.id : 'HTTP-API';
  if (packet.topic.startsWith('$SYS')) return;

  const payloadStr = packet.payload ? packet.payload.toString() : '';
  console.log(`[MQTT MENSAJE] origen=${source} | tópico=${packet.topic} | datos=${payloadStr}`);
});

const mqttServer = net.createServer(aedes.handle).listen(config.mqttPort, '0.0.0.0', () => {
  console.log(`[MQTT] Broker TCP activo en puerto ${config.mqttPort}`);
});

const wsServer = new WebSocketServer({ port: config.wsPort });
wsServer.on('connection', (connection) => {
  aedes.handle(createWebSocketStream(connection) as any);
});
console.log(`[WS] Broker WebSocket activo en puerto ${config.wsPort}`);

httpServer.listen(config.apiPort, '0.0.0.0', () => {
  console.log(`[API] Servidor HTTP escuchando en http://localhost:${config.apiPort}`);
});

const shutdown = () => {
  console.log('\n[SISTEMA] Apagando servidores ordenadamente...');

  aedes.close(() => {
    mqttServer.close(() => {
      wsServer.close(() => {
        httpServer.close(() => {
          console.log('[SISTEMA] Todos los servicios se han detenido correctamente.');
          process.exit(0);
        });
      });
    });
  });
};

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);