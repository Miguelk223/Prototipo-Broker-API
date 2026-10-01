# IoT API + ESP32

Proyecto que incluye:
- una API REST/MQTT con Aedes y WebSocket
- un cliente ESP32 con WiFi + MQTT

## Estructura

- `src/` — API Node/TypeScript
- `Prueba-ESP32/` — proyecto PlatformIO para ESP32

## Requisitos

- Node.js 20+
- npm
- PlatformIO

## Configuración

1. Copia `.env.example` a `.env` y completa tus valores reales.
2. Ajusta la configuración del ESP32 en `Prueba-ESP32/src/main.cpp` con tu SSID, contraseña WiFi y datos MQTT.
3. Instala dependencias:

```bash
npm install
```

## Ejecutar API

```bash
npm run dev
```

## Endpoint principal

```http
POST /api/publish
```

Ejemplo:

```json
{
  "topic": "dispositivos/esp32_simulado_01/comando",
  "payload": "LED_ON"
}
```

## Importante

No subas credenciales reales a GitHub. Usa valores de ejemplo o variables de entorno.
# API-REST-Prototipo-aedes
# Prototipo-API-Broker-aedes
