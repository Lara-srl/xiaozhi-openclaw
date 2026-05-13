# Guida: Aggiungere MCP Hardware Tools (XiaoZhi ↔ Device)

> Riferimento: commit `5630288dda`, `14f20a681f`, `24557757a0` (2026-04-16)

## Architettura

```
LLM Agent  ──tool call──▶  tools.ts  ──callDeviceMcp()──▶  bridge.ts  ──WS──▶  Device ESP32
                                                                        ◀──WS──  MCP response
                                                           handleMcpResponse()
                                                                  ▼
                                                           resolve Promise
                                                                  ▼
                                                         tool result → LLM
```

**Flusso completo:**

1. L'LLM decide di usare un tool (es. `ada_led`)
2. `tools.ts` → `getBridge()` → `bridge.callDeviceMcp("tools/call", { name, arguments })`
3. `bridge.ts` → serializza JSON-RPC → invia frame `type:"mcp"` via WebSocket
4. Device firmware esegue l'azione hardware
5. Device risponde con frame `type:"mcp"` contenente JSON-RPC response
6. `bridge.handleMcpResponse()` → risolve la Promise
7. Tool handler riceve il risultato → lo formatta per l'LLM

## File coinvolti

| File                                 | Ruolo                                                 |
| ------------------------------------ | ----------------------------------------------------- |
| `extensions/xiaozhi/src/tools.ts`    | Definizione e registrazione tool                      |
| `extensions/xiaozhi/src/bridge.ts`   | WS server + `callDeviceMcp()` + pending call tracking |
| `extensions/xiaozhi/src/protocol.ts` | `buildMcpRequest()` + `parseMessage()` (type:"mcp")   |
| `extensions/xiaozhi/src/types.ts`    | `McpJsonRpcResponse`, `McpPendingCall`                |
| `extensions/xiaozhi/src/channel.ts`  | Singleton bridge via `Symbol.for()`                   |
| `extensions/xiaozhi/src/ui-state.ts` | `buildUiState()` per feedback visivo device           |
| `extensions/xiaozhi/index.ts`        | Lifecycle: `setActiveBridge()` + `registerAdaTools()` |

## Come aggiungere un nuovo tool

### Step 1: Verificare il tool MCP firmware

Il device deve esporre il tool nel suo MCP server firmware. Il nome segue la convenzione `self.<component>.<action>`, es:

- `self.led.set` — LED
- `self.sensor.read` — sensori batteria/volume
- `self.camera.take_photo` — camera
- `self.haptic.feedback` — vibrazione
- `self.audio_speaker.set_volume` — volume

### Step 2: Aggiungere il tool in `tools.ts`

Template copia-incolla per un tool MCP standard:

```typescript
api.registerTool({
  name: "ada_NOME", // prefisso "ada_" obbligatorio
  label: "Ada NOME",
  description: "Descrizione chiara per l'LLM — cosa fa e quando usarlo.",
  parameters: Type.Object({
    // Parametri con tipi Typebox — vincoli min/max dove serve
    param1: Type.String({ description: "..." }),
    param2: Type.Optional(
      Type.Number({
        description: "...",
        minimum: 0,
        maximum: 100,
      }),
    ),
  }),
  async execute(_id, params) {
    const bridge = getBridge();
    if (!bridge) return notConnected();
    try {
      // 1. Feedback visivo sul device
      bridge.sendToActiveSession(buildUiState(AdaUiState.ACTING, { text: "Azione..." }));

      // 2. Chiamata MCP al device firmware
      const result = await bridge.callDeviceMcp(
        "tools/call",
        {
          name: "self.componente.azione", // ← nome MCP firmware
          arguments: {
            param1: params.param1,
            param2: params.param2 ?? defaultValue,
          },
        },
        5000,
      ); // timeout: 5s default, 10s per camera/vision

      // 3. Reset UI + ritorno risultato
      bridge.sendToActiveSession(buildUiState(AdaUiState.IDLE));
      return ok({ ok: true, ...params, result });
    } catch (err) {
      bridge.sendToActiveSession(buildUiState(AdaUiState.IDLE));
      return ok({ ok: false, error: String(err) });
    }
  },
});
```

### Step 3: Non servono modifiche ad altri file

I seguenti pezzi sono già in place e **non** vanno toccati per aggiungere un nuovo tool:

- **`bridge.ts`** — `callDeviceMcp()`, `handleMcpResponse()`, `rejectAllPendingMcp()` sono generici
- **`protocol.ts`** — `buildMcpRequest()` e parsing `type:"mcp"` sono generici
- **`types.ts`** — `McpJsonRpcResponse` e `McpPendingCall` coprono qualsiasi tool
- **`channel.ts`** — singleton bridge immutato
- **`index.ts`** — `registerAdaTools()` già chiamato al boot

## Gotcha critici

### 1. Bridge Singleton — `Symbol.for()` (NON usare closure)

```typescript
// ❌ SBAGLIATO — _getBridge è null nel contesto jiti separato
export function registerAdaTools(api, _getBridge) {
  const getBridge = _getBridge; // null!
}

// ✅ CORRETTO — usa il singleton process-global
export function registerAdaTools(api, _getBridge) {
  const getBridge = () => getActiveBridge(); // Symbol.for("openclaw.xiaozhi.bridge")
}
```

**Perché:** Il gateway avvia il servizio in un contesto jiti → `setActiveBridge()`. Ma `resolvePluginTools()` carica `tools.ts` in un **altro** contesto jiti. Le closure catturate sono scollegate. `Symbol.for()` crea una chiave globale al processo, condivisa tra tutti i moduli.

### 2. Sempre check null bridge

```typescript
const bridge = getBridge();
if (!bridge) return notConnected(); // device non connesso
```

### 3. Sempre reset UI nel catch

```typescript
} catch (err) {
  bridge.sendToActiveSession(buildUiState(AdaUiState.IDLE));  // ← obbligatorio
  return ok({ ok: false, error: String(err) });
}
```

### 4. Timeout appropriati

| Tipo operazione              | Timeout          |
| ---------------------------- | ---------------- |
| LED, haptic, volume, sensori | 5000ms (default) |
| Camera + vision API          | 10000ms          |
| Operazioni di rete (future)  | 15000ms+         |

### 5. Schema parametri — NO Union, NO anyOf

Per policy del repo (`CLAUDE.md`), gli schemi tool devono usare:

- `Type.String()`, `Type.Number()`, `Type.Boolean()` — tipi base
- `Type.Optional(...)` — parametri opzionali (no `... | null`)
- `stringEnum`/`optionalStringEnum` — per liste enum (no `Type.Union`)
- Sempre `Type.Object({...})` al top level

### 6. Helper `ok()` per risposte consistenti

```typescript
const ok = (payload: unknown) => ({
  content: [{ type: "text" as const, text: JSON.stringify(payload, null, 2) }],
  details: payload,
});
```

## Protocollo WS — Frame MCP

### Request (gateway → device)

```json
{
  "session_id": "uuid-...",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "self.led.set",
      "arguments": { "hex_color": "FF0000", "mode": "pulse" }
    },
    "id": 1
  }
}
```

### Response (device → gateway)

```json
{
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
      "content": [{ "type": "text", "text": "{\"ok\": true}" }],
      "isError": false
    }
  }
}
```

### Error response

```json
{
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "id": 1,
    "error": { "code": -1, "message": "LED not available" }
  }
}
```

## UI States disponibili

```typescript
AdaUiState.BOOT = 0; // Avvio
AdaUiState.IDLE = 100; // Inattivo — reset dopo ogni tool
AdaUiState.LISTENING = 200; // Registrazione audio
AdaUiState.THINKING = 300; // LLM in elaborazione
AdaUiState.ACTING = 400; // Tool in esecuzione ← usare per i tool
AdaUiState.SPEAKING = 500; // TTS in riproduzione
AdaUiState.COMPACTION = 600; // Compattazione sessione
AdaUiState.SHUTDOWN = 900; // Spegnimento
```

## Tool esistenti (riferimento)

| Tool                | MCP name firmware               | Parametri                        | Timeout  |
| ------------------- | ------------------------------- | -------------------------------- | -------- |
| `ada_status`        | — (solo bridge check)           | nessuno                          | —        |
| `ada_speak`         | — (gestito da audio pipeline)   | `text`                           | —        |
| `ada_emoji`         | — (solo UI state)               | `emotion`                        | —        |
| `ada_volume`        | `self.audio_speaker.set_volume` | `level: 0-100`                   | 5s       |
| `ada_play`          | `self.audio_player.play`        | `url, repeat?`                   | deferred |
| `ada_led`           | `self.led.set`                  | `hex_color, mode?, duration_ms?` | deferred |
| `ada_haptic`        | `self.haptic.feedback`          | `pattern, repeat?`               | deferred |
| `ada_sensor`        | `self.sensor.read`              | nessuno                          | 5s       |
| `ada_factory_reset` | `self.system.factory_reset`     | `confirm: true`                  | 5s       |
| `ada_sleep`         | `self.system.sleep`             | nessuno                          | 5s       |
| `ada_photo`         | `self.camera.take_photo`        | `question?`                      | 10s      |

## Esempio completo: aggiungere `ada_display`

Supponiamo di voler controllare il display LCD del device con un tool MCP firmware `self.display.show_text`.

```typescript
// In extensions/xiaozhi/src/tools.ts, dentro registerAdaTools():

api.registerTool({
  name: "ada_display",
  label: "Ada Display",
  description:
    "Show custom text on the Ada LCD display. Use for status messages, alerts, or information.",
  parameters: Type.Object({
    text: Type.String({ description: "Text to display (max 120 chars)." }),
    font_size: Type.Optional(
      Type.Number({
        description: "Font size: 16, 24, 32 (default 24).",
        minimum: 16,
        maximum: 32,
      }),
    ),
    duration_s: Type.Optional(
      Type.Number({
        description: "Display duration in seconds (0 = indefinite, default 5).",
        minimum: 0,
        maximum: 60,
      }),
    ),
  }),
  async execute(_id, params) {
    const bridge = getBridge();
    if (!bridge) return notConnected();
    try {
      bridge.sendToActiveSession(buildUiState(AdaUiState.ACTING, { text: "Display..." }));
      const result = await bridge.callDeviceMcp("tools/call", {
        name: "self.display.show_text",
        arguments: {
          text: params.text,
          font_size: params.font_size ?? 24,
          duration_s: params.duration_s ?? 5,
        },
      });
      bridge.sendToActiveSession(buildUiState(AdaUiState.IDLE));
      return ok({ ok: true, text: params.text, result });
    } catch (err) {
      bridge.sendToActiveSession(buildUiState(AdaUiState.IDLE));
      return ok({ ok: false, error: String(err) });
    }
  },
});
```

## Tool senza MCP firmware (solo lato gateway)

Alcuni tool non chiamano il device — eseguono logica solo lato gateway:

```typescript
// Esempio: ada_speak — TTS gestito dall'audio pipeline, non dal device MCP
api.registerTool({
  name: "ada_speak",
  label: "Ada Speak",
  description: "Speak text aloud on the Ada device speaker via TTS.",
  parameters: Type.Object({
    text: Type.String({ description: "Text to speak on the device." }),
  }),
  async execute(_id, params) {
    if (!getBridge()) return notConnected();
    // Nessuna callDeviceMcp — la audio pipeline gestisce il TTS
    return ok({ ok: true, queued: params.text });
  },
});
```

## Debug

- **Log MCP:** `[XZ bridge] MCP request id=N method=tools/call` / `[XZ bridge] MCP response id=N ok`
- **Log tool trace:** `/tmp/xiaozhi-llm-trace.jsonl`
- **Timeout:** se il device non risponde entro il timeout, la Promise viene rigettata con `MCP call timeout after Nms`
- **Disconnect:** se il device si disconnette, `rejectAllPendingMcp("Device disconnected")` rigetta tutte le call pendenti
