# ECGR4101-Virtual-Pet-Team12 - REM
Rem is a carefree and troublemaking owl-like creature that loves attention.

---
## Table of Contents
1. [Overview](#1-overview)
2. [Hardware](#2-hardware)
3. [Controls](#3-controls)
4. [Project Structure](#4-project-structure)
5. [Graphics Pipeline](#5-graphics-pipeline)
6. [Build & Flash](#6-build--flash)
7. [Team & Individual Contributions](#7-team--individual-contributions)
8. [AI Tools Used](#8-ai-tools-used)
9. [Known Issues](#9-known-issues)
10. [Credits & References](#10-credits--references)


---

## 1. Overview
### Stats
| Stat      | Range   | Decreases when                    | Increases when |
| --------- | ------- | --------------------------------- | -------------- |
| Hunger    | 0 - 100 | -1 per second or when played with | Fed            |
| Happiness | 0 - 100 | -1 per second                     | Played with    |

### Animations / features
Each mood has its own animation, drawn from the art in `pet_images/`

| State | Art folder             | Animation                                                    |
| ----- | ---------------------- | ------------------------------------------------------------ |
| Idle  | `pet_images/rem_idle`  | 4-step bob: frame sequence `0, 1, 0, 2` (3 unique frames)    |
| Happy | `pet_images/rem_happy` | 4-step crying: frame sequence `0, 1, 2, 3` (4 unique frames) |
| Sad   | `pet_images/rem_sad`   |                                                              |
| Dead  | `pet_images/rem_dead`  |                                                              |

### State machine
<img width="757" height="584" alt="image" src="https://github.com/user-attachments/assets/7d0f7ebd-5d9d-4a71-881e-1e8ba6b21166" />


---

## 2. Hardware

<img width="594" height="322" alt="image" src="https://github.com/user-attachments/assets/be84760c-2443-45f8-9885-18e0b44d44ae" />

### Bill of materials
| Part                                     | Purpose                                  |
| ---------------------------------------- | ---------------------------------------- |
| NUCLEO-L496ZG-P                          | Main MCU board (STM32L496ZG)             |
| X-NUCLEO-GFX01M2                         | 240x320 ILI9341 TFT display shield (SPI) |
| Potentiometer                            | Analog input (ADC)                       |
| Speaker                                  | PWM input                                |
| USB cable (ST-LINK port)                 | Power, programming, serial               |

### Wiring
**Display (SPI1, on the GFX01M2 shield)**

| Signal    | Nucleo pin | Notes                                                                                   |
| --------- | ---------- | --------------------------------------------------------------------------------------- |
| LCD SCK   | PA5        | SPI1_SCK                                                                                |
| LCD MISO  | PA6        | SPI1_MISO                                                                               |
| LCD MOSI  | PA7        | SPI1_MOSI                                                                               |
| LCD CS    | PA9        | GPIO out, active low. Shares the pin with `USB_VBUS` in the .ioc (see `gfx01m2_conf.h`) |
| LCD DC    | PB10       | GPIO out (low = command, high = data)                                                   |
| LCD RESET | PA1        | GPIO out, active low                                                                    |

**Joystick (on the GFX01M2 shield, all active low with internal pull-ups)**

|Direction|Nucleo pin|Notes|
|---|---|---|
|Right|PB0|EXTI0 interrupt, falling edge|
|Left|PB6|EXTI6 interrupt, falling edge|
|Center|PC7|EXTI7 interrupt, falling edge|
|Up|TODO (GPIOC)|Polled in `Joystick_Read()`; pin number is in `gfx01m2_conf.h`|
|Down|TODO (GPIOB)|Polled in `Joystick_Read()`; pin number is in `gfx01m2_conf.h`|

**On-board devices**

| Signal                 | Nucleo pin | Notes                            |
| ---------------------- | ---------- | -------------------------------- |
| User button B1         | PC13       | EXTI13, rising edge              |
| LD2 (blue) / LD3 (red) | PB7 / PB14 | GPIO outputs                     |
| ST-LINK VCP TX/RX      | PG7 / PG8  | LPUART1 (configured in the .ioc) |

### Clock configuration
|Clock|Frequency|Setting|
|---|---|---|
|SYSCLK|56.8 MHz|PLL from MSI (24 MHz), M=5, N=71, R=6|
|HCLK|56.8 MHz|AHB /1|
|APB1|28.4 MHz|/2|
|APB2 (SPI1)|56.8 MHz|/1|
|SPI1 clock|~1.775 MHz|Prescaler /32|

---

## 3. Controls

| Input       | Action |
| ----------- | ------ |
| Menu Select | Play   |
| Menu Select | Pet    |
| Menu Select | Feed   |


---

## 4. Project Structure

```
.
├── EmbeddedCode/                # STM32CubeMX-generated CMake firmware project
│   ├── Core/
│   │   ├── Inc/                 # headers 
│   │   └── Src/                 # main.c, interrupts, MSP init 
│   ├── Drivers/                 # STM32 HAL/CMSIS + ILI9341 display driver 
│   ├── Middlewares/ST/STM32_USB_Host_Library/   # inherited from the  starter
│   ├── USB_HOST/                # inherited from the starter
│   ├── cmake/                   # CMake toolchain files
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── Lab1_NUCLEO_L496.ioc     # CubeMX pin and clock configuration
│   ├── STM32L496xx_FLASH.ld     # linker script
│   └── startup_stm32l496xx.s    # startup code
├── pet_images/                  # source art, one folder per mood
│   ├── rem_idle/
│   ├── rem_happy/
│   ├── rem_sad/
│   └── rem_dead/
├── PNG_Conversion/
│   ├── PngConversion.py         # converts PNG art into C arrays for the display
│   └── PNGs-To-Convert/         # staging folder for the script's input
├── ConvertedImages/             # images converted to C arrays/headers 
└── README.md
```


---

## 5. Graphics Pipeline

The display is a 240x320 ILI9341 driven over SPI in polling mode (`ili9341.c`). Sprites are 177x240 and are stored in flash as C arrays, generated from PNG art at build time.

1. **Source art** lives in `pet_images/<mood>/` as PNGs.
2. **Conversion:** `PNG_Conversion/PngConversion.py` (Python 3 ) converts each PNG to a C array. 
3. **Storage format:** Raw RGB565 (`uint16_t`, 84,960 B per 177x240 frame) 
4. **Duplicate frames** are stored once. The idle animation plays frames `0, 1, 0, 2`, so only 3 unique frames are stored.
5. **Drawing:** `ILI9341_DrawImage` sets the address window and streams the sprite one row at a time with `HAL_SPI_Transmit`.

---

## 6. Build & Flash

### Requirements
- VS Code 
- STM32 for Visual Studio Code Extension (plus necessary bundles)
- STM32CubeMX
- Python (only to regenerate image assets)
- ST-LINK drivers

### Steps
```bash
git clone https://github.com/KellerKain/ECGR4101-Virtual-Pet-Team12.git
cd ECGR4101-Virtual-Pet-Team12/EmbeddedCode
CMake: configure
CMake: Build
```
1. Connect the NUCLEO board to your PC with the USB cable on the **ST-LINK** port.
2. The Debug or Run action in VS Code with the generated `.elf`

---

## 7. Team & Individual Contributions

| Name               | GitHub         | Individual Contributions                                                                                                                                                                                                                                                     |
| ------------------ | -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Christopher Beaman | @Chris-B8      | - We don’t support using AI for art. Art of REM is from Valve’s Deadlock, under fair use. Most of the images and all of the animations frames were hand-edited and animated by me. I additionally worked on README, UI and image rendering, debugging, and the presentation. |
| Javier Guarisma    |                | - Worked on speaker integration, GPIO pin initialization, and presentation. AI helped with developing pin initialization and debugging.                                                                                                                                      |
| Cody Henderson     | @jhende82      | - Implemented the virtual pet’s audio system using STM32 timer interrupts and GPIO control. Worked on incorporating animation into certain states. AI helped with timer initialization and debugging                                                                                                                                                                                                                                                                                                                           |
| Keller Kain        | @KellerKain    | - Contributions: UI, State Development, Presentation, Debugging<br>- AI: Helped with UI implementation and placement, as well as debugging                                                                                                                                   |
| Luke Sherman       | @D-LukeSherman | - Image converter, image driver, state machine and functions, menuing, graphics, incorporating animations, tic-tac-toe game, interrupt functions. <br>    <br>- AI: Any initializations such as interrupts, ADC, image driver, and image converter.                          |

---

## 8. AI Tools Used

| Tool               | Used for                                                                                                                                 |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Claude (Anthropic) | Advice on storing sprite animations (individual const-array frames vs. GIF, palettized vs. raw RGB565 storage, duplicate-frame handling) |
|  Gemeni (Google)   | Initialization code like setting up interrupts, the image driver, image converter. Debugging.                                            |

---

## 9. Known Issues

- **Small stack:** the project stack is 1 KB (`0x400`) and the heap is 512 B (`0x200`).
- **Animation speed is limited by the SPI clock.** At the `.ioc` default of HCLK = 14.2 MHz, a full 177x240 frame takes roughly 100-200 ms to send.

---

## 10. Credits & References

- Instructor-provided LCD starter project
- STM32CubeL4 HAL/CMSIS (STMicroelectronics)
- ILI9341 datasheet; X-NUCLEO-GFX01M2 and NUCLEO-L496ZG-P user manuals
- Valve's Deadlock provided initial art and animation inspiration for REM
