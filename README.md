# ML-Based Predictive Power and Thermal Management System for Embedded Computing Platforms

A closed-loop, on-device machine learning system for real-time power and thermal management on microcontroller-based embedded platforms. Built on the STM32F446RE (Nucleo), the system continuously monitors electrical and thermal parameters, predicts near-future power draw and temperature risk, estimates remaining energy budget, and proactively throttles workload to prevent overheating — entirely on-device, with no host PC in the loop.

## Overview

Embedded and IoT platforms often lack the intelligence to anticipate power or thermal issues before they occur. This project builds a low-cost monitoring and prediction pipeline that:

- Measures real-time electrical parameters (via INA219) and system telemetry (workload/activity level)
- Trains and deploys an on-device ML model (via X-CUBE-AI) to classify near-future **risk state** — Normal / Caution / Critical — for both power and temperature
- Estimates remaining battery/energy budget using a deterministic rule-based calculation derived from predicted power draw
- Issues early warnings and automatically throttles workload (staged response: workload balancing → low-power states → dynamic clock scaling) before thresholds are breached
- Displays live status via OLED, UART, and RGB LED indicators

The MVP targets a full closed-loop demo — prediction driving actual mitigation — rather than a passive dashboard.

## Hardware

- STM32 Nucleo-F446RE
- INA219 power monitor module (I2C)
- 2x LM35DZ analog temperature sensors — one board-mounted (taped to MCU package), one ambient
- 0.96" SSD1306 OLED display (I2C)
- Common-cathode RGB LED (status indication: normal/warning/critical)
- Push button (onboard B1)
- Breadboard, jumper wires, 470Ω resistors

### Pin Assignments

| Peripheral | Pins | Notes |
|---|---|---|
| INA219 + OLED | I2C1 — PB9 (D14/SDA), PB8 (D15/SCL) | Shared bus |
| LM35 (board temp) | PA0 (A0) | Taped to MCU package |
| LM35 (ambient) | PA1 (A1) | Physically separate from board |
| RGB LED (R/G/B) | PC7 (D9), PB6 (D10), PA7 (D11) | Contiguous PWM pins |
| Push button | PC13 (B1) | Onboard |
| UART debug | PA2/PA3 | Onboard ST-Link VCP, no extra wiring |

## Project Structure

```
Mini_project_v2/
├── Core/
│   ├── Inc/
│   │   ├── adc.h                  # ADC config (LM35 sensor reads)
│   │   ├── gpio.h
│   │   ├── i2c.h                  # INA219 / OLED bus
│   │   ├── logging.h              # UART/debug logging
│   │   ├── sensor_acquisition.h   # Unified sensor read pipeline
│   │   ├── tim.h                  # Timers (PWM for RGB LED, scheduling)
│   │   ├── usart.h
│   │   └── workload_generator.h   # Synthetic load generation for demo
│   └── Src/
│       └── (corresponding .c files)
├── Drivers/
│   └── STM32F4xx_HAL_Driver/      # HAL modules incl. ADC, TIM
├── Mini_project_v2.ioc            # CubeMX configuration
├── LICENSE
└── README.md
```

**Note on restructuring:** earlier standalone modules (`ina219.c/h`, `telemetry.c/h`, `system_monitor.c/h`, `workload.c/h`) have been consolidated and renamed into `sensor_acquisition`, `logging`, `adc`/`i2c`/`tim`/`usart`, and `workload_generator` to better reflect a layered HAL-driven architecture.

## ML Approach

- **Task framing:** classification (not regression) — predicts risk state (Normal/Caution/Critical) at a fixed future prediction horizon *H*, determined empirically from initial data collection
- **Outputs:** separate risk heads for power and temperature (shared single model, two output heads); battery/energy risk is computed deterministically from predicted power, not ML-predicted
- **Deployment:** X-CUBE-AI (STM32Cube expansion pack) generates optimized C inference code from the trained model — no external TFLite Micro runtime

## Status

🚧 Actively in development — see project board / commit history for current progress.

## Team

Collaborative academic mini-project, team of 3.

## License

See [LICENSE](./LICENSE).
