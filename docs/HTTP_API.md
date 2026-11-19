# 로컬 HTTP 제어 API

한국어 | [English](HTTP_API_EN.md) · [프로젝트](../README.md)

현재 공개 펌웨어 2.1.1의 웹 대시보드가 사용하는 API입니다. 별도 MQTT 브로커 없이 로컬 프로그램에서도 호출할 수 있습니다. 이 문서 추가로 펌웨어 동작이 변경되지는 않습니다.

## 사용 조건

- 기기가 공유기에 연결되어 정상 온라인 상태여야 합니다. SoftAP에서는 웹 제어 명령이 거절됩니다.
- 주소는 `http://<멀티탭의 현재 IPv4 주소>`입니다. 아래 IP는 예시입니다.
- 인증 토큰은 없으며 `Origin` 검사는 인증이 아닙니다. 신뢰할 수 있는 내부망에서만 사용하고 인터넷에 포트를 공개하지 마십시오.
- 다른 출처의 웹페이지를 위한 CORS API가 아닙니다. 브라우저 예제는 멀티탭 웹페이지의 개발자 콘솔에서 실행합니다. 외부 프로그램은 `Origin` 헤더를 직접 설정해야 합니다.

## POST /api/control

`Content-Type: application/octet-stream`으로 **정확히 2바이트의 바이너리 본문**을 보냅니다. JSON, 문자열 `01`, `[0,1]` 텍스트, 폼 데이터가 아닙니다.

| 바이트 위치 | 의미 | 허용 값 |
|---|---|---|
| 0 | 채널 | `0`=채널 1, `1`=채널 2, `2`=채널 3, `3`=채널 4, `4`=전체 |
| 1 | 동작 | `0`=OFF, `1`=ON |

`Origin: http://<현재 IPv4 주소>`가 반드시 필요합니다. 경로·끝 슬래시·`:80`을 붙이지 않고, IP는 요청 대상 기기의 현재 주소와 일치시킵니다.

### Python 예제 — 표준 라이브러리

```python
import urllib.request

base = 'http://192.168.0.153'  # 본인 멀티탭 IP로 변경
request = urllib.request.Request(
    base + '/api/control',
    data=bytes([0, 1]),  # 채널 1 ON; 전체 OFF는 bytes([4, 0])
    headers={'Content-Type': 'application/octet-stream', 'Origin': base},
    method='POST',
)
with urllib.request.urlopen(request, timeout=10) as response:
    print(response.status, response.read().decode())
```

### JavaScript 예제 — 멀티탭 페이지의 개발자 콘솔

```javascript
const response = await fetch('/api/control', {
  method: 'POST',
  headers: {'Content-Type': 'application/octet-stream'},
  body: new Uint8Array([0, 1]) // 채널 1 ON
});
console.log(response.status, await response.text());
```

브라우저가 POST 요청의 `Origin`을 설정합니다. 이 예제를 다른 웹사이트나 로컬 HTML 파일에서 실행하는 방식은 지원하지 않습니다.

## 응답과 상태 확인

| HTTP 상태 | 의미 |
|---|---|
| `200` | 제어 큐에 명령 접수 성공. 실제 릴레이 전환 완료를 의미하지 않음 |
| `400` | `Origin` 불일치/누락 또는 본문 길이가 2바이트가 아님 |
| `429` | 잘못된 채널/동작 값, 큐 포화, 오프라인, OTA·설정 작업 중 또는 하드웨어/릴레이 오류 등으로 접수 거절 |
| `405` | GET·POST 외의 지원하지 않는 메서드 |

성공 응답:

```json
{"accepted":true,"contact_feedback":false}
```

`contact_feedback:false`는 물리적인 접점 피드백을 읽어 확인한 결과가 아니라는 뜻입니다. 웹/HA 제어와 마찬가지로 물리 버튼 잠금의 영향을 받지 않지만 보호 차단과 타이머 등의 동작은 계속 적용됩니다.

- `GET /api/device`: 채널별 `on` 등 논리 상태와 센서값을 조회합니다. `on:null`은 상태 미확인입니다. 물리 접점 상태의 보증은 아닙니다.
- `GET /api/status`: 네트워크·계측 상태를 조회합니다.
- `GET /api/control`: `sequence`, `wait_ms`, `run_ms`, `busy` 제어 진단을 조회할 뿐 릴레이를 조작하지 않습니다. 개별 요청에 대한 완료 확인 ID는 제공하지 않습니다.

주소창에서 `?ch=1&state=1`로 제어하는 `/api/relay` API는 현재 공개 펌웨어에 없습니다. 요청 타임아웃은 기기에서 명령이 접수되지 않았음을 보장하지 않으므로, 무조건 반복 전송하지 말고 상태를 확인하십시오. 대량·무제한 재시도는 피하십시오.
