# Local HTTP control API

[한국어](HTTP_API.md) | English · [Project](../README_EN.md)

This documents the API used by the dashboard in the current public firmware, 2.1.1. Local programs can call it without an MQTT broker. This documentation update does not change firmware behavior.

## Requirements

- The strip must be connected to the router and online. Web relay commands are rejected in SoftAP mode.
- Use `http://<the strip's current IPv4 address>`. Addresses below are examples.
- There is no authentication token; the `Origin` check is not authentication. Use only on a trusted LAN and do not expose the port to the internet.
- This is not a CORS API for other websites. Run the browser example in the strip dashboard's developer console. External programs must supply the `Origin` header themselves.

## POST /api/control

Send **exactly two binary bytes** with `Content-Type: application/octet-stream`. Do not send JSON, the text `01`, the text `[0,1]`, or form data.

| Byte offset | Meaning | Allowed values |
|---|---|---|
| 0 | Channel | `0`=channel 1, `1`=channel 2, `2`=channel 3, `3`=channel 4, `4`=all |
| 1 | Action | `0`=OFF, `1`=ON |

The required header is `Origin: http://<current IPv4 address>`. Do not include a path, trailing slash or `:80`. The address must match the target strip's current address.

### Python example — standard library

```python
import urllib.request

base = 'http://192.168.0.153'  # Replace with your strip's IP
request = urllib.request.Request(
    base + '/api/control',
    data=bytes([0, 1]),  # Channel 1 ON; all OFF is bytes([4, 0])
    headers={'Content-Type': 'application/octet-stream', 'Origin': base},
    method='POST',
)
with urllib.request.urlopen(request, timeout=10) as response:
    print(response.status, response.read().decode())
```

### JavaScript example — strip dashboard developer console

```javascript
const response = await fetch('/api/control', {
  method: 'POST',
  headers: {'Content-Type': 'application/octet-stream'},
  body: new Uint8Array([0, 1]) // Channel 1 ON
});
console.log(response.status, await response.text());
```

The browser supplies `Origin` for the POST. Running this example on another website or from a local HTML file is not supported.

## Responses and status checks

| HTTP status | Meaning |
|---|---|
| `200` | Command accepted into the control queue; not confirmation of completed relay switching |
| `400` | Missing/mismatched `Origin`, or body length is not two bytes |
| `429` | Rejected: invalid channel/action, full queue, offline state, OTA/configuration work, hardware/relay fault, etc. |
| `405` | Unsupported method other than GET or POST |

Successful response:

```json
{"accepted":true,"contact_feedback":false}
```

`contact_feedback:false` means this is not a result verified using physical contact feedback. Like dashboard/HA commands, these commands are unaffected by physical-button locks; protection cutoffs, timers and other normal behavior still apply.

- `GET /api/device`: logical channel states (`on`) and measurements. `on:null` means unknown. This does not guarantee the physical contact position.
- `GET /api/status`: network and metering diagnostics.
- `GET /api/control`: control diagnostics (`sequence`, `wait_ms`, `run_ms`, `busy`) only; no relay switching. No per-request completion acknowledgement ID is provided.

The public firmware does not currently provide `/api/relay?ch=1&state=1` URL-based control. A request timeout does not prove the command was not accepted: check state rather than retrying blindly. Avoid flooding or unlimited retries.
