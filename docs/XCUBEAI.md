# Running the model through X-CUBE-AI

The firmware has two inference engines for the same trained network:

| Engine | Where the C comes from | Switch |
|---|---|---|
| Plain C (default) | `train_model.py` writes the weights to `Core/Inc/model_weights.h`; `predictor.c` runs them | `INFERENCE_ENGINE_XCUBEAI 0` |
| X-CUBE-AI | CubeMX generates `network.c` / `network_data.c` from `model_out/model.tflite` and links ST's runtime library | `INFERENCE_ENGINE_XCUBEAI 1` |

With X-CUBE-AI on, the plain C network still runs next to it every tick. The `[SYS]` line then shows both inference times and how many ticks the two disagreed (should stay 0):

```
[SYS] CPU=180MHz ... inference: X-CUBE-AI 14us, plain C 9us, differ 0 ...
```

At boot, `predictor_xcubeai.c` runs three test inputs through both engines, checks they agree, and works out which TFLite output is the temperature head (the export does not guarantee the order). If anything is wrong it logs an error and the board keeps using plain C.

## Steps (about 20 min)

1. **Commit first** (`git status` clean), so you can roll back with `git checkout .` if generation goes wrong.
2. **Install the pack:** CubeIDE → Help → Manage Embedded Software Packages → STMicroelectronics → X-CUBE-AI → install the latest version.
3. Open `Mini_project_v2.ioc`.
4. **Select the component:** Middleware and Software Packs → Select Components (or the Software Packs menu) → STMicroelectronics.X-CUBE-AI → tick **Core**. Leave **Device Application** empty / not selected. OK.
5. **Add the network:** Middleware and Software Packs → X-CUBE-AI → tick the Core checkbox in the mode panel → **Add network**:
   - name: `network` (keep this exact name; the code expects `network.h`)
   - type: TFLite, file: `model_out/model.tflite`
   - compression: None
6. **Analyze** → screenshot the report (MACC, weights flash, activations RAM). **Validate on desktop** → screenshot (error vs the .tflite should be ~0).
7. **Check the stack:** Project Manager → Linker Settings → Minimum Stack Size must be **0x1000** (this repo sets it; CubeMX copies it into the linker script on generation, and 0x400 is too small for this firmware).
8. Save the `.ioc` → **Generate Code**. CubeMX adds `X-CUBE-AI/App/network*.c`, `Middlewares/ST/AI` and the include paths. All our code is in USER CODE blocks or our own files, so it is kept.
9. In `Core/Inc/app_config.h` set `#define INFERENCE_ENGINE_XCUBEAI 1`. Build and flash.
10. At boot you should see:
    ```
    [INFO][ML] X-CUBE-AI network ready: ... B activations, self-check max error 0.000000
    ```

To go back to plain C: set `INFERENCE_ENGINE_XCUBEAI 0` and rebuild. The generated files can stay.

## If something goes wrong

- **CubeMX forces a Device Application:** choose *Application Template*, generate, then delete the two lines it adds to `main.c`: `MX_X_CUBE_AI_Init();` and `MX_X_CUBE_AI_Process();` (the second one lands inside our main loop). Repeat after every regeneration.
- **`network.h: No such file`:** the network was not named `network`, or code was not generated. Re-check step 5 and 8.
- **`#error Generated X-CUBE-AI network does not have 7 inputs...`:** the `.tflite` is from a different model. Regenerate from the current `model_out/model.tflite`.
- **Boot log says "X-CUBE-AI output differs from plain C":** `model_weights.h` and `model.tflite` come from different training runs. Run `train_model.py` once (it writes both), regenerate in CubeMX, rebuild.
- **USART2 interrupt:** do not enable it in the `.ioc` unless you also delete `USART2_IRQHandler` at the bottom of `Core/Src/stm32f4xx_it.c` (otherwise it is defined twice).

## For the report

Measured on the board, same network, same inputs:

| | Plain C | X-CUBE-AI |
|---|---|---|
| Inference time (µs, from `[SYS]`) | | |
| Flash for weights + code (B) | from the `.map` / build size | from Analyze |
| RAM (B) | ~300 (stack) | activations from Analyze |
| Predictions differing (ticks) | — | `differ` count |
