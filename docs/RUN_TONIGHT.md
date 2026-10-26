# Closed-loop build: how to run it

Everything runs on the Nucleo-F446RE alone. The fan is optional and can be wired later.

## 0. What changed

| File | What it does |
|---|---|
| `Core/Src/main.c` | 1 Hz pipeline: sense → features → predict → react → battery → log. Main loop runs the workload duty cycle, then sleeps (`__WFI`). All edits are inside `USER CODE` blocks. |
| `Core/Inc/app_config.h` | Every tunable value (limits, duty cycles, fan pins, battery defaults). |
| `Core/Src/sensor_acquisition.c` | Adds the internal die temperature sensor (factory calibrated). The chip-taped LM35 is no longer needed. |
| `Core/Src/workload_generator.c` | Levels are now duty cycles of each second (0 / 30 / 65 / 100 %), and IDLE sleeps. Adds the AUTO schedule for unattended data collection. |
| `Core/Src/feature_extraction.c` | 7 sliding-window features (same maths as the Python script). |
| `Core/Src/predictor.c` | Runs the trained network in plain C, plus the slope-rule baseline. Falls back to the rule until a model is trained. |
| `Core/Src/reaction.c` | Stage 1: caps the workload and drives the fan by predicted risk, with a fixed safety cut-off that always applies. |
| `Core/Src/battery.c` | What-if battery: the user enters capacity and voltage, and the board estimates % left and hours left. |
| `Core/Src/fan.c` | L293D fan PWM on TIM1 (set up in code, no CubeMX change). |
| `Core/Src/status_led.c` | RGB LED: blue = measuring baseline, green / yellow / red = Normal / Caution / Critical. |
| `Core/Src/uart_cmd.c` | Commands typed in the serial terminal (below). |
| `Core/Src/logging.c` | DATA line now has 17 fields. |
| `Core/Inc/model_weights.h` | Placeholder until training overwrites it. |
| `STM32F446RETX_FLASH.ld` | Stack raised from 1 KB to 4 KB (float printf + inference). |
| `scripts/capture_data.py` | New 17-field schema, plus `--send` to send a command at start. |
| `scripts/train_model.py` | Labels data, trains the model, compares it with the rule, and writes `model_weights.h` plus a report. |

## 1. Wiring

Only the INA219 is required (the firmware halts at boot without it). The die temperature sensor is inside the MCU.

| Part | Connections |
|---|---|
| INA219 (required) | VCC → 3V3, GND → GND, SDA → PB9 (D14), SCL → PB8 (D15). JP6 (IDD) jumper removed: Vin+ → JP6 regulator side, Vin− → JP6 MCU side |
| RGB LED, common cathode | R → 470 Ω → PC7 (D9), G → 470 Ω → PB6 (D10), B → 470 Ω → PA7 (D11), cathode → GND |
| LM35 ambient (optional) | +Vs → 5V, Vout → PA1 (A1), GND → GND, in open air |
| Fan via L293D (optional) | See section 8 |
| OLED | Not used by this firmware yet; leave unplugged |

## 2. Build and flash

1. Copy the files over your CubeIDE project (same folder layout). New `.c` files in `Core/Src` are picked up automatically.
2. Make sure float printf is on: Project → Properties → C/C++ Build → Settings → MCU Settings → tick **Use float with printf**.
3. Build and flash. Open a serial terminal at **115200** (the CubeIDE console, PuTTY or Tera Term).
4. At boot the LED is **blue for 10 s** while the idle baseline is measured. Don't touch the board during this time.
5. Every second you get one `DATA,...` line and one readable line like:
   `T=38.2C rise=+0.4C slope=+0.01C/s P=95mW WL=IDLE/IDLE RULE:NORMAL rule:NORMAL P:NORMAL fan=0% bat=100.0% 66.9h inf=0us`

If the debugger loses the board after flashing (the CPU sleeps a lot now), set Debug Configuration → Debugger → Reset behaviour = **Connect under reset**.

## 3. Commands (type in the terminal, then Enter)

| Command | Effect |
|---|---|
| `HELP` | List commands |
| `MODE RUN` | Predict and react (default after reset, for the demo) |
| `MODE COLLECT` | Log only, no reaction; set the workload yourself |
| `MODE AUTO` | Log only, with a random workload schedule (for unattended data collection) |
| `WL 0` … `WL 3` | Workload IDLE / LOW / MEDIUM / HIGH (the B1 button also cycles it) |
| `BAT 2000 3.7` | Set the what-if battery (mAh, volts); `BAT` alone shows it |
| `LIM 5 8` | Caution / critical limits in °C above baseline |
| `BASE` | Re-measure the idle baseline (do this once the board settles in a new room) |
| `FAN 100` / `FAN AUTO` | Manual fan test / back to automatic |

## 4. Heat test (about 15 min, do this first)

1. `MODE COLLECT`, then `WL 0` and wait 3 min.
2. `WL 3` and wait 8 min. Watch `rise=`.
3. `WL 0` for 5 min to watch it cool.

Note the highest `rise` value and the power at IDLE vs HIGH. Around 4 °C of rise or more is fine. Under 3 °C, train with lower limits (step 6) or cover the board loosely.

## 5. Collect data (leave it running)

Close the terminal first (only one program can hold the port). Then:

```
python scripts/capture_data.py --port COM5 --samples 10800 --output data/auto1.csv --send "MODE AUTO"
```

10800 samples = 3 hours, and 2 hours is the useful minimum. Keep the laptop awake and plugged in. Replace `COM5` with your port (`/dev/ttyACM0` on Linux).

## 6. Train

```
python -m pip install tensorflow pandas numpy matplotlib
python scripts/train_model.py --data data/*.csv
```

This writes `Core/Inc/model_weights.h` and `model_out/report.md` (ML vs slope rule: accuracy, false alarms, critical recall, warning lead time), plus `model_out/prediction_example.png` for the report.
It picks limits from the data (50 % / 80 % of the observed max rise). To set them yourself:

```
python scripts/train_model.py --data data/*.csv --caution-rise 3 --critical-rise 5
```

Google Colab works too: upload the CSVs and the script, run it, and download `model_weights.h`.

## 7. Rebuild with the model and demo

Rebuild and flash. The readable line now shows `ML:` and an inference time in µs. The board starts in `MODE RUN`.

Demo script (about 3 min):
1. Idle: point out the baseline, the power and the battery hours left.
2. `WL 3` (or press B1 until HIGH). Power jumps at once; the model moves to CAUTION or CRITICAL **before** the temperature gets there, while `rule:` usually lags.
3. The reaction caps the workload (`WL=HIGH/MED`), the fan starts, and the temperature levels off. The LED changes colour.
4. `BAT 220 3.0` (coin cell) vs `BAT 10000 3.7` (power bank) shows the battery estimate change live.

Before reviewers arrive: let it settle at idle, type `BASE`, and optionally run HIGH for a couple of minutes so things happen sooner.

## 8. Fan wiring (L293D chip, channel 1)

| L293D pin | Connect to |
|---|---|
| 1 (EN1) | PA8 / Arduino D7 |
| 2 (IN1) | PB5 / Arduino D4 |
| 7 (IN2) | PB4 / Arduino D5 |
| 3 (OUT1), 6 (OUT2) | Fan + and − |
| 16 (VCC1) | Nucleo 5V |
| 8 (VCC2) | 12 V supply + |
| 4, 5, 12, 13 (GND) | Nucleo GND **and** 12 V supply − (common ground) |

Test with `FAN 100`, then `FAN AUTO`. If the fan spins backwards, swap its two wires. Keep the fan current out of the INA219 path.
