# PCM5122 and ESP32-S3: findings that close or narrow the UNVERIFIED items

Status: DATASHEET-SOURCED. Nothing here is HW-VERIFIED. Each finding names the document
section it came from. Sources: TI SLAS763C (PCM5121/PCM5122 datasheet, Oct 2018) and the
ESP-IDF ESP32-S3 GPIO programming guide.

This note answers the UNVERIFIED lists in `docs/hw-pinmap.md` sections 2 and 4. The consolidated breakout pinout, strap/address guide and cross-reference to the Adafruit user guide and TI datasheet are in [docs/hardware/adafruit-pcm5122-breakout.md](hardware/adafruit-pcm5122-breakout.md).

## 1. Resolved from the datasheet

| Item | Finding | Source |
|---|---|---|
| TDM at 48 kHz needs 128 or 256 BCK per frame | Confirmed. TDM/DSP rows: up to 48 kHz, BCK = 128 or 256 x fS. 192 BCK is not allowed, so 8 slots of 32 bit (256 BCK) is right | SLAS763C 8.3.2.1, Table 4 |
| Register 0x28 value | AFMT bits 5:4 = 01 (TDM/DSP), ALEN bits 1:0 = 11 (32 bit), so 0x13 | Table 83 |
| Register 0x29 meaning | AOFS (8 bit) is the number of BCK from the start (MSB) of the audio frame to the start of the wanted sample. Offsets 0, 64 and 128 are valid values | Table 84 |
| Frame start | Rising edge of LRCK is the frame start; LRCK high time must be at least one BCK | 8.3.2.2 note |
| Clock registers | None needed. In software mode, with a valid SCK, BCK and LRCK, the device auto-configures its clock tree and PLL. 48 kHz with SCK = 256 x fS = 12.288 MHz is a listed combination. Clock auto-set is on by default (Reg 0x25 DCAS = 0) | 8.3.6.2, Table 32, Table 82 |
| BCK limits | BCK must be stable between 32 and 256 x fS; 256 x fS is the edge of the range, which is allowed | Table 82 (IDBK), Table 4 |
| Clock-error status mask | The status byte is Page 0 / Register 95 (0x5F), Table 108. Bit 0 is CERF (Clock Error: 0 = clock valid, 1 = clock invalid); bit 2 is CKMF (Clock Missing). Therefore mask 0x05 selects exactly CERF and CKMF. Bit 1 CSRF (Clock Resync Request) and bit 4 LTSH (Latched Clock Halt) are separate status indications and are intentionally not included in this mask. The CERF field description continues on printed page 96. Here 0x05 is a bit mask, not a register address. | SLAS763C Table 108, printed pp. 95–96 |
| MUTE pin polarity at the chip | XSMT low = soft mute then hard analog mute; XSMT high = un-mute. This matches the Adafruit guide ("pull low to mute"). The schematic text "LOW = Mute Off" is the odd one out, so the multimeter check on the real board still decides | 8.3.3, Pin Functions table, 10.2.1 |
| I2S routing to GPIO 4 to 7 | ESP-IDF states that on the ESP32-S3 any peripheral output signal can be routed to any GPIO through the GPIO matrix, IO MUX and RTC IO MUX. GPIO 4 to 7 are ordinary GPIOs (not strapping, not PSRAM, not USB, not JTAG) | ESP-IDF ESP32-S3 GPIO guide, GPIO Summary |

Not resolved by this: whether 12.288 MHz MCLK through the GPIO matrix meets timing on
this board. That is a scope check at bring-up (P6 step 1).

## 2. Still UNVERIFIED

1. **Does each DAC's right sample follow its left sample immediately?** The datasheet
   text for Figures 16 to 18 says only "L-channel = FIRST, R-channel = LAST". The figures
   are images and could not be read here. Assumed: with 32-bit words the right sample is
   the next 32 BCK after the left. Test at bring-up: tone in the right slot only, then
   left slot only, per DAC.
2. **One-BCK ambiguity in the offset.** Figures 16 and 17 show OFFSET = 0 and OFFSET = 1
   as separate cases. The register text says the offset counts from the frame start, but
   not whether the first BCK of the sync pulse counts as 0. The ESP32 short-sync TDM mode
   may therefore need Reg 0x29 = 0, 64, 128 or 1, 65, 129. Test at bring-up: a tone only
   in slot 2 should appear on DAC 1 only.
3. **Whether the ESP-IDF TDM driver's short sync matches the part.** The ESP-IDF I2S TDM
   documentation has not been read yet. Open until the T08 driver design.

## 3. Proposed register sequence for T08 (not built, for review)

Write while the audio clocks are stopped; the part powers down without clocks and still
answers on I2C.

| Step | Write | Meaning |
|---|---|---|
| 1 | Reg 0x00 = 0x00 | Select page 0 |
| 2 | Reg 0x28 = 0x13 | TDM/DSP, 32 bit |
| 3 | Reg 0x29 = 0, 64 or 128 (DAC 0, 1, 2) | Slot offset in BCK (subject to unverified item 2) |
| 4 | Start MCLK, BCK, WS from the ESP32 | Part auto-detects the clocks |
| 5 | After clocks are stable, release the MUTE line | Soft un-mute |

## 4. Readback checks that replace some scope work (candidate checklist steps)

All are read-only I2C reads on page 0. They show what the DAC actually sees.

| Reg | Field | Expect (fS = 48 kHz, 256 BCK, SCK 256 fS) |
|---|---|---|
| 0x5B (91) | DTFS | 011 (32 to 48 kHz) |
| 0x5B (91) | DTSR | 0110 (SCK = 256 fS) |
| 0x5C and 0x5D | DTBR | 256 (BCK per frame) |
| 0x5E (94) | clock status bits | all zero (valid) |
| 0x5F (95) | CERF | 0 (no clock error) |
| 0x76 (118) | PSTM | 0101 (Run) once clocks run and un-muted |
| 0x72 (114) | MTST | 11 with MUTE high (at or above 0.7 VDD); 00 with MUTE pulled low |

The MTST readback gives a second, firmware-visible check of the MUTE polarity and the
470 ohm pull-down level (steps P3.3 to P3.4 and P5.4), but it does not replace the
multimeter check on the board's own MUTE pad.

## 5. Things to fix in the other docs once this is accepted

- `hw-pinmap.md` section 4: replace the three UNVERIFIED bullets with items 1 and 2 above.
- `hw-pinmap.md` section 2: change the "UNVERIFIED" I2S routing paragraph to the ESP-IDF finding, keeping the scope check.
- `hw-pinmap.md` section 4: state that no PLL or clock register writes are planned.
- `prebringup.md` Phase 5: add the readback table above as steps P5.8 onward.
