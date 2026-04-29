> **이 코드는 Claude Sonnet 4.6으로 바이브코딩으로 작성되었습니다.**  
> **ESP32-C3에서 테스트 되었습니다.**

# ESPHome 외부 컴포넌트 ? HLK-LD6002

HLK-LD6002 60 GHz FMCW 레이더 센서용 ESPHome 외부 컴포넌트입니다.  
UART를 통해 심박수(Heart Rate), 호흡수(Breath Rate), 거리(Distance)를 수신합니다.

---

## 하드웨어 사양

| 항목 | 값 |
|---|---|
| 제품명 | HI-LINK HLK-LD6002 |
| 주파수 | 57?64 GHz FMCW |
| 전원 | 3.3 V (별도 DCDC 권장: ≥1 A, 리플 ≤50 mV) |
| 통신 | UART 115200 bps |
| 측정 항목 | 심박수(bpm), 호흡수(bpm), 거리(cm) |

---

## 배선

| LD6002 핀 | ESP32 핀 | 설명 |
|---|---|---|
| 3V3 | 3.3V | 전원 (별도 DCDC 권장) |
| GND | GND | 공통 그라운드 |
| **P19** | **GND** | **반드시 GND 연결** (파워온 시 LOW 필요) |
| TX0 | RX (예: GPIO4) | 센서 → MCU 데이터 |
| RX0 | TX (예: GPIO5) | MCU → 센서 (선택) |

> **주의:** P19 핀을 GND에 연결하지 않으면 센서가 데이터를 전송하지 않습니다.

---

## 통신 프로토콜

### 프레임 구조

```
Byte   Field         설명
──────────────────────────────────────────────────────
[0]    Start         고정값 0x01
[1-2]  Reserved      0x00 0x00 (항상 0)
[3-4]  DataLen       페이로드 길이 (Big-Endian, 4 또는 8)
[5-6]  FrameType     프레임 타입 ID (Big-Endian)
[7]    HeaderXor     헤더 체크섬 (XOR-Inverse of [0..6])
[8..]  Payload       데이터 페이로드
[last] PayloadXor    페이로드 체크섬 (XOR-Inverse of payload)
```

### 프레임 타입

| 타입 코드 | 설명 | 페이로드 |
|---|---|---|
| `0x0A15` | 심박수 | 4-byte IEEE 754 float (bpm) |
| `0x0A14` | 호흡수 | 4-byte IEEE 754 float (bpm) |
| `0x0A16` | 거리 | 4-byte uint32 flag + 4-byte float (cm) |

---

## ESPHome 설정

### 1. 컴포넌트 복사

프로젝트 디렉터리에 컴포넌트 파일을 배치합니다.

```
your-project/
├── your-device.yaml
└── external_components/
    └── ld6002/
        ├── __init__.py
        ├── ld6002.h
        └── ld6002.cpp
```

### 2. 최소 설정 예시

```yaml
external_components:
  - source:
      type: local
      path: external_components
    components: [ld6002]

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: arduino

uart:
  id: uart_ld6002
  rx_pin: GPIO4       # LD6002 TX0 연결
  tx_pin: GPIO5       # LD6002 RX0 연결 (선택)
  baud_rate: 115200

ld6002:
  uart_id: uart_ld6002
  heart_rate:
    name: "Heart Rate"
  breath_rate:
    name: "Breath Rate"
  distance:
    name: "Distance"
```

## 설정 파라미터

### `ld6002:` 블록

| 파라미터 | 필수 | 설명 |
|---|---|---|
| `uart_id` | 필수 | UART 컴포넌트 ID |
| `heart_rate` | 선택 | 심박수 센서 설정 |
| `breath_rate` | 선택 | 호흡수 센서 설정 |
| `distance` | 선택 | 거리 센서 설정 (cm) |

각 센서는 ESPHome 표준 [Sensor 스키마](https://esphome.io/components/sensor/index.html)를 모두 지원합니다 (`name`, `id`, `filters`, `on_value` 등).

---

## 참고

- HLK-LD6002 원본 라이브러리: [phuongnamzz/HLK-LD6002](https://github.com/phuongnamzz/HLK-LD6002)
