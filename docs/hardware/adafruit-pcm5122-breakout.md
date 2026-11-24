# Adafruit PCM5122 I2S DAC breakout — hardware and integration reference

**Status:** SOURCE-MAPPED / PROPOSED. This is a consolidated engineering reference, not a board approval or a hardware verification record. The supplied front- and back-side photos identify the Adafruit PCM5122 I2S DAC breakout. The official Adafruit guide and TI datasheet are the source authorities; repository wiring choices are identified separately as project proposals.

## 1. Sources and authority

| ID | Source | Authority |
|---|---|---|
| A1 | [Adafruit guide — Overview](https://learn.adafruit.com/adafruit-pcm5122-i2s-dac/overview) (published/edited 2025-09-26) | Default-mode quick-start wiring, optional MCLK, format/word-width overview, analog-output constraints, hardware vs software configurability |\n| A1a | [Adafruit guide — Pinouts](https://learn.adafruit.com/adafruit-pcm5122-i2s-dac/pinouts) (published/edited 2025-09-26) | Board pad names, mode multiplexing, address straps, clock-direction jumper |\n| A1b | [Adafruit guide — full guide](https://learn.adafruit.com/adafruit-pcm5122-i2s-dac) | CircuitPython, Arduino and Raspberry Pi examples; schematic/download resources |
| A2 | [TI PCM5121/PCM5122 datasheet SLAS763C](https://www.ti.com/lit/ds/symlink/pcm5122.pdf) (revised 2018-10) | Silicon pin functions, electrical limits, formats, TDM timing, registers and status |
| A3 | Project-supplied front-side board photo | Physical silk labels and pad layout |
| A4 | Project-supplied back-side board photo | Board identity, feature markings and CLK Dir Sel jumper |
| P1 | [Project hardware pin map](../hw-pinmap.md) | Proposed ESP32 GPIO wiring, three-board addresses and slot plan |
| P2 | [PCM5122/ESP32-S3 verification notes](../hw-verification-notes.md) | Datasheet findings and remaining bench checks |
| P3 | [Pre-bring-up checklist](../prebringup.md) | Inspection, wiring, power-up and first-tone procedure |
| P4 | [Engineering task catalog](../engineering/03_task_catalog.md) | T08 and T27–T33 integration work/gates |

Precedence: A2 governs silicon behaviour/register definitions; A1 governs how Adafruit exposes the IC on this breakout; P1 governs the current project proposal. If these sources appear to conflict, record the discrepancy and verify against the schematic/actual board. Photos establish visible labels, not electrical continuity or measured strap states.

## 2. Board identity and visible pad groups

The front photo shows the PCM5122 IC, 3.5 mm line-out jack and two rows of breakout pads. The rear photo is marked “PCM5122 I2S DAC”, “112-dB 16/20/24/32-Bit”, “I2S, LJ, RJ, TDM/DSP”, “Hardware / I2C / SPI Control”, “3.3V Logic, MCLK-optional”, and “Line Out 1KΩ Min. Not AC-coupled”. It also shows the CLK Dir Sel solder-jumper position.

### 2.1 Power, audio and analog-output pads

| Board silk | Function | TI signal / notes |
|---|---|---|
| VIN | Breakout power input | Adafruit guide: 3.3–5 VDC input; this does not permit 5-V signal levels |
| GND (power side) | Common power/logic ground | Board ground |
| WSEL | Word select / left-right clock | LRCK |
| DIN | Serial audio data input | DIN; sampled on rising BCK |
| BCK | Audio bit clock | BCK |
| MCK | Optional main/system clock input | SCK; optional per Adafruit guide |
| MUTE | Hardware mute input | XSMT; low = mute, high = unmute |
| Lout | Left analog line output | OUTL |
| GND (audio side) | Analog output ground | Adafruit describes this as clean analog ground |
| Rout | Right analog line output | OUTR |
| 3.5-mm jack | Stereo line output | Same line-level output path; not a headphone driver |

Adafruit's Overview and Pinouts pages say the output is ground-centred and not AC-coupled. It specifies a load of at least 1 kΩ and explicitly says the jack cannot drive headphones. Check downstream input/load and ground reference; do not connect headphones directly. The Overview says the default path needs only BCK, WSEL and DIN: no MCLK and no I²C configuration are required for a basic hardware-mode I²S stream. The chip internally generates its main clock from BCK unless MCK is supplied.

### 2.2 Configuration and control pads

| Board silk | Adafruit guide meaning | Mode dependency / caution |
|---|---|---|
| MOD1 | Control-mode strap 1 | Combined with MOD2 |
| MOD2 | Control-mode strap 2; SPI chip-select in SPI mode | Combined with MOD1 |
| A1 | I²C address bit; SPI MISO in SPI mode | Adds +1 to 7-bit I²C address |
| A2 | I²C address bit | Adds +2 to 7-bit I²C address |
| SDA | I²C data; SPI MOSI; ATT2 in hardware mode | Same pad, mode-dependent function |
| SCL | I²C clock; SPI clock; ATT1 in hardware mode | Same pad, mode-dependent function |
| ATT0 | Hardware-mode gain/attenuation control | Together with ATT1 (SCL) and ATT2 (SDA), selects output gain/attenuation; Adafruit Overview describes the range as −6 dB to +15 dB (see TI datasheet Table 3 for exact combinations) |
| AGN | Hardware-mode analog gain selector | Low = 0 dB; high = −6 dB per Adafruit |
| FMT | Hardware-mode audio format selector | Low = I²S; high = left-justified per Adafruit; TDM is selected in software mode |
| FILT | Hardware-mode interpolation filter | Low = normal latency; high = low latency |
| DEEM | Hardware-mode 44.1-kHz de-emphasis | Default off per Adafruit |

**Multiplexing warning:** do not interpret SDA/SCL as I²C wires while the breakout is strapped into hardware mode: Adafruit documents these pads as ATT2/ATT1 in hardware mode. In I²C mode they are the I²C bus. Hardware pin controls are not a substitute for the PCM5122 register interface in software-control mode.

## 3. Control modes and straps

Adafruit guide Pinouts pages 7–9:

| Mode | MOD1 | MOD2 |
|---|---|---|
| Hardware | Low | Low |
| I²C | Low | High |
| SPI | High | N/A (MOD2 is chip-select) |

Both MOD pins open by default select hardware mode. Adafruit's I²C example wires MOD2 high.

**Two distinct operating paths:** (1) Basic stereo I²S quick-start: default hardware mode, with BCK/WSEL/DIN, no MCLK required, and no I²C setup required. Adafruit's Overview says the device detects the audio word format from the WSEL/BCK ratio; it lists 16-, 24- and 32-bit audio. (2) Configurable/TDM path: volume, software mute, EQ/filter settings use register control; TI datasheet section 8.3.2.2 says TDM/DSP and right-justified formats are available only in software-control mode. Therefore this project's TDM/DSP path requires I²C or SPI register configuration. Project proposal remains I²C: MOD1 low / MOD2 high. Do not use hardware-only FMT to select TDM. The supplied rear-board silk separately prints 16/20/24/32-bit; record this as a board marking, not as a substitute for the Overview's advertised 16/24/32-bit quick-start statement or TI's mode-specific word-length tables.

The hardware MUTE input remains usable in all control modes. Adafruit says pulling MUTE low sets outputs to ground. TI identifies XSMT low as soft mute and high as soft unmute (section 6.1, Pin Functions).

## 4. I²C address selection

Adafruit guide pages 8–9: default 7-bit address 0x4C. A1 adds 1; A2 adds 2.

| A2 | A1 | 7-bit address |
|---|---|---:|
| Low/open | Low/open | 0x4C |
| Low/open | High | 0x4D |
| High | Low/open | 0x4E |
| High | High | 0x4F |

The guide describes pulling address straps high by connecting to VIN. However, digital signals are 3.3-V logic only. The project pin map proposes tying straps to ESP32 3V3, not a potentially 5-V VIN. Verify voltage before connection. These are 7-bit I²C addresses.

Current project proposal in P1:

| Board | MOD1 | MOD2 | A1 | A2 | Address |
|---|---|---|---|---|---:|
| DAC 0 | Low/open | 3V3/high | Low/open | Low/open | 0x4C |
| DAC 1 | Low/open | 3V3/high | 3V3/high | Low/open | 0x4D |
| DAC 2 | Low/open | 3V3/high | Low/open | 3V3/high | 0x4E |

This is proposed assembly wiring, not a measured fact. Check each strap with a meter before powering the assembled bus.

## 5. Clock-direction jumper

Adafruit guide Pinouts page 9 calls the rear solder jumper “CLK Dir Set”:
- Open by default: expects clock input from the microcontroller (secondary mode).
- Soldered closed: sets the pin high and makes the board generate its own clock (primary mode).

The supplied rear photo shows the location labelled “CLK Dir Sel”. For the proposed ESP32-master / DAC-secondary topology, the jumper is expected to remain open on all three boards. This is not HW-VERIFIED; inspect and photograph each board and record the result in the bring-up log.

Do not confuse this jumper with MCK: the jumper selects clock direction; MCK is the optional system-clock signal. Confirm the actual master/slave behaviour against the breakout schematic and TI pin-mode behaviour before changing the jumper.

## 6. Electrical and analog constraints

| Topic | Source-backed detail | Integration implication |
|---|---|---|
| VIN | Adafruit: 3.3–5 VDC | Supply range applies to VIN only |
| Digital logic | Adafruit: 3.3-V logic only | Never drive signal pads at 5 V |
| MCK | Optional; PCM5122 can generate its internal main clock from BCK per Adafruit | Do not assume MCK is mandatory; project proposes it and must validate timing |
| Line output | Ground-centred, not AC-coupled | Check downstream input topology and ground reference |
| Minimum load | At least 1 kΩ | Not suitable for direct headphone drive |
| MUTE | Low mutes per Adafruit; TI XSMT low = mute | Keep mute asserted until system safety gates permit release |
| Audio timing | TI samples DIN on rising BCK | Validate timing against ESP-IDF peripheral configuration |

Do not infer regulator topology, current consumption, input protection, output drive beyond the published load guidance, or safe power sequencing from the photos.

## 7. PCM5122 audio-interface facts relevant to Xover-ESP32

Sources: TI SLAS763C section 8.3.2.1 / Table 4, section 8.3.2.2 and Figures 13–18 (printed pages 15–18); Adafruit guide Overview and Pinouts pages.

- Serial interface signals are LRCK, BCK and DIN. DIN is sampled on the rising edge of BCK.
- Audio words are MSB-first, two's-complement, up to 32 bits.
- TDM/DSP is available in software-control mode. Supported TDM BCK rates depend on sample rate: up to 48 kHz, 128 or 256 × fS; at 96 kHz, 128 or 256 × fS; at 192 kHz, 128 × fS.
- In TDM, LRCK duty cycle must be at least one BCK; the rising edge is frame start.
- TDM figures identify left as FIRST and right as LAST, with offset examples 0, 1 and N. Validate exact slot alignment on hardware; do not infer ESP-IDF framing from prose alone.
- Standard I²S/LJ and software-mode TDM/RJ are distinct. Hardware FMT selects I²S vs left-justified; it does not configure TDM.

### 7.1 Registers used by the current project proposal

All addresses below are Page 0 unless stated otherwise. Page Select Register is Page 0 / Register 0.

| Register | Field | Datasheet meaning | Project reference |
|---|---|---|---|
| 0x28 (40) | AFMT[1:0], ALEN[1:0] | AFMT 01 = TDM/DSP; ALEN 11 = 32-bit | P2; PCM5122 register/control modules |
| 0x29 (41) | AOFS[7:0] | Offset in BCKs from audio-frame MSB to desired sample start | P1 slot proposal; P2 offset caveat |
| 0x25 (37) | IDFS, IDBK, IDSK, IDCH, IDCM, DCAS, IPLK | Clock-detection ignore controls and clock-divider autoset control; reset defaults zero | Do not change ignore bits without reviewed decision |
| 0x5F (95) | LTSH, CKMF, CSRF, CERF | Read-only clock status: bit 0 CERF, bit 1 CSRF, bit 2 CKMF, bit 4 LTSH | P2 documents selected 0x05 mask as CERF + CKMF |

**Register 0x05 is reserved** in the Page 0 register map (TI Table 49, printed page 67). Do not confuse that register address with the value 0x05 used as a bit mask against Page 0 / Register 0x5F.

For Page 0 / Register 0x5F, TI Table 108 (printed page 95) defines bit 0 CERF, bit 1 CSRF, bit 2 CKMF and bit 4 LTSH. CERF's description continues on printed page 96: 0 = clock valid, 1 = clock invalid. Mask 0x05 selects CERF and CKMF; it does not select CSRF or LTSH. LTSH is cleared on read, so reads have a side effect for that bit.

The clock-ignore register is not a way to make invalid clocks acceptable. Default zero means the detections are regarded. Any future change to ignore bits requires explicit review.

## 8. Adafruit guide examples and limits

The official Adafruit guide is organized into Overview, Pinouts, CircuitPython Hardware Mode, CircuitPython I²C Mode, Arduino Hardware Mode, Arduino I²C Mode, Raspberry Pi and Downloads. Its Overview establishes the simplest three-signal I²S path and the optional-MCLK behaviour; Pinouts is the authority for the pad functions, mode-dependent pin reuse, address straps and clock-direction jumper. The code/example sections are useful for basic stereo bring-up and software-control patterns, but they do not demonstrate this project's ESP32-S3 multi-board TDM implementation.

These examples help bring up a basic stereo breakout and software-control interface. They do **not** prove the project's ESP32-S3 two-lane TDM arrangement, eight-slot framing, three-DAC synchronisation, clock integrity or output-slot mapping. Those remain separate integration/bench tasks.

## 9. Project wiring crosswalk

The proposed ESP32 GPIO wiring and slot plan live in P1, not here. This table maps signal names only.

| ESP32/project signal in P1 | Adafruit pad | TI device signal |
|---|---|---|
| I2S MCLK | MCK | SCK |
| I2S BCLK | BCK | BCK |
| I2S WS / frame sync | WSEL | LRCK |
| I2S data output | DIN | DIN |
| I²C data | SDA | SDA (software-control mode) |
| I²C clock | SCL | SCL (software-control mode) |
| DAC mute | MUTE | XSMT |
| Analog left output | Lout | OUTL |
| Analog right output | Rout | OUTR |
| Ground | GND | AGND/DGND joined on breakout |

The visible breakout pad set has no pads labelled GPIO1–GPIO6. Do not plan a PCM5122 GPIO status-wire connection from these photos. Internal chip GPIO functions are not equivalent to breakout GPIO pads.

## 10. ESP32-S3 TRM cross-reference: peripheral signals, GPIO matrix and registers

**Source:** Espressif, *ESP32-S3 Technical Reference Manual*, Version 1.8 (2026-03-04), supplied project copy `esp32-s3_technical_reference_manual_en.pdf`. This is the silicon reference. The module datasheet still governs module-specific unavailable pins and strapping restrictions; the project pin map governs the current proposal.

### 10.1 Proposed DAC signal to ESP32-S3 signal path

The current proposal in `docs/hw-pinmap.md` / `firmware/main/board_config.h` assigns GPIO4–7 to the shared DAC serial bus. TRM Chapter 6, Table 6.11-1 (printed pages 483–484) identifies the I2S0 output signals available through the GPIO matrix. These are peripheral signal names, not fixed GPIO numbers.

| Project signal | ESP32-S3 GPIO proposal | I2S0 peripheral output in TRM Table 6.11-1 | Adafruit pad | TI PCM5122 pin |
|---|---:|---|---|---|
| MCLK | GPIO4 | I2S0_MCLK_out (signal 23) | MCK | SCK |
| BCLK | GPIO5 | I2S0O_BCK_out (signal 22) | BCK | BCK |
| Frame sync / WS | GPIO6 | I2S0O_WS_out (signal 24) | WSEL | LRCK |
| Serial audio data | GPIO7 | I2S0O_SD_out (signal 25) | DIN | DIN |
| I2C SDA | GPIO8 | I2C controller output routed through GPIO matrix / open-drain pad configuration | SDA | SDA |
| I2C SCL | GPIO9 | I2C controller output routed through GPIO matrix / open-drain pad configuration | SCL | SCL |
| Hardware mute | GPIO10 | ordinary GPIO output | MUTE | XSMT |

**What the TRM proves:** Table 6.11-1 lists the I2S0 MCLK, BCK, WS and SD output signals as GPIO-matrix outputs; it marks these signals as not direct IO-MUX output functions. Table 6.12-1 lists GPIO4–10 as ordinary GPIO-capable pads (GPIO8–10 have additional alternate functions, but GPIO-matrix routing remains available). Thus the proposed GPIO4–7 assignment is structurally routable through the GPIO matrix. It does not prove that a particular board header is electrically connected as expected, nor that the current firmware configures the matrix correctly.

### 10.2 GPIO matrix register model

TRM Chapter 6:
- Section 6.11 and Table 6.11-1 (printed pages 482–492): peripheral signal numbers, valid input/output signals and output-enable semantics.
- Section 6.5 / register descriptions (GPIO chapter register map, printed pages 496 onward): GPIO pin configuration, peripheral input selection and per-GPIO peripheral output selection.
- GPIO peripheral input selection registers are `GPIO_FUNCn_IN_SEL_CFG_REG`; output selection registers are `GPIO_FUNCn_OUT_SEL_CFG_REG`. The TRM register map lists their address offsets in the GPIO block. GPIO peripheral base is `0x6000_4000` (TRM Table 4.3-3).
- For GPIO-matrix outputs, the output-enable selection bit determines whether enable comes from the GPIO enable register or from the peripheral output-enable signal. Follow the TRM semantics; do not assume a mux selection alone enables the pad.

This reference deliberately does not prescribe raw GPIO register values. Use the ESP-IDF I2S/GPIO driver for the selected ESP-IDF release unless a separately reviewed low-level implementation task requires direct register programming.

### 10.3 I2S controller register cross-reference

TRM Chapter 28 (I2S Controller):
- I2S0 peripheral base: `0x6000_F000`; I2S1 base: `0x6002_D000` (TRM Table 4.3-3, printed pages 408–409).
- `I2S_TX_CONF_REG`, offset `0x0024`: TX enable/start/reset and `I2S_TX_TDM_EN`, plus alignment, bit order and channel mode controls (Register 28.12, printed pages 1068–1069).
- `I2S_TX_CONF1_REG`, offset `0x002C`: `I2S_TX_TDM_WS_WIDTH`, `I2S_TX_BCK_DIV_NUM`, `I2S_TX_BITS_MOD`, `I2S_TX_HALF_SAMPLE_BITS`, `I2S_TX_TDM_CHAN_BITS`, `I2S_TX_MSB_SHIFT` and `I2S_TX_BCK_NO_DLY` (Register 28.13, printed page 1069).
- `I2S_TX_CLKM_CONF_REG`, offset `0x0034`: TX clock source/divider configuration (Register 28.14, printed page 1070).
- Chapter 28 also defines the TX TDM channel control register and DMA/FIFO path. Consult the exact field definitions for the silicon revision before configuring slot masks/channel count.

For the project's 48-kHz, eight-slot, 32-bit proposal, the frame has 256 BCK per sample and a 12.288-MHz BCK. The corresponding intended I2S TX setup is TDM enabled, 32 valid bits per channel/slot and eight total slots. The exact WS pulse width, MSB shift, BCK divider and clock-source values must be derived together from the TRM, ESP-IDF version and PCM5122 timing diagrams; they are not finalized by this cross-reference. Do not translate the descriptive target into raw register constants without a dedicated review and waveform test.

### 10.4 I2C controller cross-reference

The ESP32-S3 TRM Table 4.3-3 places I2C0 at `0x6001_3000` and I2C1 at `0x6002_7000`. GPIO8/GPIO9 are the project-selected SDA/SCL pads; the I2C controller signals are routed through the GPIO matrix and configured open-drain. The Adafruit breakout is in I2C software-control mode (MOD1 low, MOD2 high) for TDM configuration. The I2C bus frequency remains 100 kHz for bring-up, with 400 kHz only after signal-integrity checks, per `docs/hw-pinmap.md`.

### 10.5 Mapping confidence and remaining checks

| Mapping / configuration | Evidence status | What remains |
|---|---|---|
| GPIO4–7 can route I2S0 MCLK/BCK/WS/SD through GPIO matrix | TRM-SOURCED | Confirm actual ESP-IDF configuration and scope waveforms |
| GPIO8/9 available as proposed I2C pads | TRM + module-pin-map sourced | Confirm board header wiring and I2C scan |
| GPIO10 as mute output | TRM + board pin-map proposal | Measure MUTE polarity/boot level on actual breakout |
| I2S0 TDM TX capability and control fields | TRM-SOURCED | Confirm ESP-IDF release, exact driver semantics and selected clock/framing values |
| 48 kHz × 8 × 32-bit target = 12.288 MHz BCK | Calculated from proposal | Scope-verify clock and frame |
| Three DACs receive correct slots/offsets | PROPOSED | Bench-test with low-level unique slot tones, amps/speakers disconnected |
| Register configuration values and GPIO matrix routing in firmware | NOT YET VERIFIED HERE | Trace actual firmware/ESP-IDF driver implementation and compare to TRM |

The TRM is a register and peripheral reference, not a substitute for the ESP32-S3 module datasheet, dev-board schematic, ESP-IDF programming guide or bench measurements. This section does not upgrade the pin map or TDM plan from PROPOSED to HW-VERIFIED.

## 11. Known, proposed and still requiring measurement

| Item | Classification | Evidence / next action |
|---|---|---|
| Board identity and silk labels | PHOTO-OBSERVED | Supplied front/back photos |
| Default stereo wiring and optional MCLK | GUIDE-SOURCED | Adafruit Overview |\n| Pad functions, mode/address scheme and CLK Dir jumper | GUIDE-SOURCED | Adafruit Pinouts |
| PCM5122 TDM capability and register meanings | DATASHEET-SOURCED | TI SLAS763C citations above |
| Three-board ESP32 GPIO wiring and address allocation | PROPOSED | P1; maintainer approval required by AGENTS.md |
| CLK Dir jumper open on all three boards | UNVERIFIED | Inspect/photograph each board (P0.4) |
| Actual MOD1/MOD2/A1/A2 strap voltages | UNVERIFIED | Measure before connection (P3.2/P3.5) |
| MUTE polarity and pull-down voltage | UNVERIFIED | Meter/tone checks (P3.3/P3.4/P5.4) |
| I²C addresses on assembled bus | UNVERIFIED | Scan at P5.6 |
| MCLK/BCK/WS waveform and TDM alignment | UNVERIFIED | Scope and low-level slot test; amps/speakers disconnected |
| Analog output level/load compatibility | UNVERIFIED | Measure with actual downstream input |

No statement here upgrades a proposed pin map or unmeasured board characteristic to HW-VERIFIED. Record measurements only in docs/bringup-log.md, with person, instrument, setup and measured result.

## 12. Related repository files and change discipline

- docs/hw-pinmap.md — project pin numbers, proposed wiring and slot plan.
- docs/hw-verification-notes.md — datasheet findings and unresolved questions.
- docs/prebringup.md — receiving inspection, strap checks, assembly and first power-up.
- docs/bringup-log.md — human-entered measurements; sole source of HW-VERIFIED status.
- docs/engineering/03_task_catalog.md and docs/engineering/task_state.json — implementation tasks and gates.
- firmware/components/pcm5122/ — register definitions, TDM setup and status reads.
- firmware/main/board_config.h — board-level signal assignments (human-owned).

If the physical board revision, straps, clock direction, I²C addresses, ESP32 pin assignment, TDM slot layout or mute electrical design changes, update the relevant human-owned pin map and obtain maintainer approval. Update the pre-bring-up procedure and this reference together where applicable. Do not change firmware pin constants from this document alone.
