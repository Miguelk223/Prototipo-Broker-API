# API REST y broker MQTT

Servidor Node.js y TypeScript que ofrece endpoints HTTP y un broker MQTT basado en Aedes. Admite conexiones MQTT por TCP y WebSocket.

## Requisitos

- Node.js 20 o superior
- npm

## Instalación

Desde la carpeta del proyecto:

```bash
npm install
```

## Configuración

Configura las variables de entorno en un archivo `.env` en la raíz del proyecto:

```env
MQTT_HOST=0.0.0.0
MQTT_PORT=1883
WS_PORT=8883
PORT=3000
MQTT_USER=tu_usuario
MQTT_PASS=tu_contraseña_segura
```

El servidor usa los puertos MQTT `1883`, WebSocket `8883` y HTTP `3000` si no se configuran otros. Define credenciales propias para los clientes MQTT y no publiques el archivo `.env` ni credenciales reales.

## Ejecución

Modo desarrollo:

```bash
npm run dev
```

Compilar y ejecutar:

```bash
npm run build
npm start
```

## Endpoints HTTP

| Método | Ruta | Descripción |
| --- | --- | --- |
| `GET` | `/health` | Estado del servidor y direcciones de conexión |
| `GET` | `/api/status` | Estado del broker y clientes conectados |
| `POST` | `/api/publish` | Publica un mensaje MQTT en el tópico indicado |
| `POST` | `/api/command` | Publica un comando para un dispositivo |

### Publicar un mensaje

`POST /api/publish`

```json
{
  "topic": "osmosis/test",
  "payload": "mensaje"
}
```

También admite `qos` (`0`, `1` o `2`) y `retain` (`true` o `false`).

### Enviar un comando

`POST /api/command`

```json
{
  "deviceId": "dispositivo_01",
  "command": "LED_ON"
}
```

El comando se publica en `osmosis/device/{deviceId}/command`. El campo `command` es obligatorio.

## Conexiones MQTT

- MQTT TCP: `mqtt://<host>:1883`
- MQTT sobre WebSocket: `ws://<host>:8883`

Los clientes MQTT deben autenticarse con `MQTT_USER` y `MQTT_PASS`.

## Seguridad

Los endpoints HTTP no requieren autenticación en la implementación actual. No expongas el servidor a redes no confiables sin añadir autenticación y controles de acceso.
