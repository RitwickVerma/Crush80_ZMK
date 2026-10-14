# RGB / OpenRGB — Status & Plan

## Status (2026-09-14)
- ✅ **ZMK installed and running on the keyboard.** USB `1d50:615e`
  "ZMK Project Crush 80" (HID + CDC ACM console). USB + BLE + matrix + ZMK Studio.
  RGB **off** (`CONFIG_CRUSH80_RGB=n` in `conf/app.conf`).
- Build/install tooling fixed in this repo:
  - `scripts/prepare_ota.py` — patches the OTA image (CRC type @0x06=0x5d02,
    16-byte align body, size @0x18, CRC32 trailer).
  - `scripts/flash_combined.py` — writes MCUboot+app via the bridge's custom
    `flash_mgmt` SMP group (64) and COMMITs.
  - Repo's `install_zmk.sh` Stage 2 is broken (bridge has no `img` group).

## Hardware target (verified)
- LED driver: **2× AW20216S**, 154 LEDs (91 + 63), GSPI/HSPI bus.
  - `PE0` = CS0, `PE1` = CLK (HSPI FUNC_C), `PE2` = MOSI (HSPI FUNC_C)
  - `PC0` = CS1, `PC2` = LED power MOSFET (active-high)
- Matrix shares `PE0/PE1/PE2/PC0` (cols) — must be time-multiplexed with LED SPI.

## Why RGB is currently blocked (from docs/rgb-debug-history.md)
- HSPI hardware reports `STATUS=0x40` (transfer complete, FIFOs drained) but the
  AW20216S never lights up. Prime suspect: **HSPI signals never reach the
  PE1/PE2 pads** — a missing pad-driver / output-mux step.

## New finding from disassembly (this session)
Disassembled the stock firmware (extracted `code_2M.bin` → `inner_fw.bin`, RISC-V).
The stock LED init at **`0xF2B4`** and its pin-config helper at **`0xF33C`** write
these GPIO registers before any SPI traffic (load addr `0x20000000`):

| Register | Addr | Notes |
|---|---|---|
| PE_OUT | `0x80140320` | PE data out |
| PE_OEN | `0x80140322` | `andi 0x88` in helper — output-enable bits |
| PE_IE | `0x80140323` | `ori 0x77` — input enable |
| PC_OUT | `0x80140310` (?) | |
| PC_OEN | `0x8014030A` | `andi 0x80` |
| PC_IE | `0x80140313` | `ori 0x13` |
| PB_OEN | `0x801402xx` (?) | |

The helper (`0xF33C`) **clears specific OEN bits then re-writes them**, with
`jal 0x2000EE30` (delay) between writes — the exact sequence the ZMK port is
missing. This confirms the pin-mux hypothesis; the stock firmware manipulates
OEN/IE directly around HSPI use, not just `gpio_function_dis()`.

## Concrete next steps (need hardware access)
1. **Disassemble `0xF33C` fully** and transcribe the exact OEN/IE bit sequence
   for PE and PC (and check if PB/PD are touched).
2. **Replicate that sequence in `zmk/src/crush80_rgb.c`** around each HSPI
   transaction (save/restore OEN+IE, not just alt-function).
3. **Flash** (`bash update.sh`) and watch the console; if still dark, attach a
   logic analyzer to PE1/PE2 on the PCB.
4. Once LEDs light: validate the **154-LED → (chip, SW, CS) map** with
   `CONFIG_AW20216S_CHANNEL_SCAN` (in `zmk/drivers/led/aw20216s.c`).
5. Port `rainy_rgb` effects engine (from `scholzri/rainy75-zmk`) onto the
   AW20216S backend.
6. **OpenRGB:** implement the QMK OpenRGB protocol (RAW HID usage `0xFF60`,
   commands `0x00–0x07`) in ZMK, feeding it from the AW20216S matrix. Per-key
   zone via `LED_FLAG_KEYLIGHT`.

## Safety
- NEVER unbounded busy-wait inside `irq_lock()` (bricked the board before).
  Always timeout + bail.
- Keep a known-good image (`dist/crush80-zmk-app.signed.bin`) and
  `scripts/force_recovery.py` handy.
