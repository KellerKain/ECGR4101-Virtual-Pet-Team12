/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : X-NUCLEO-GFX01M2 LCD demo for the NUCLEO-L496ZG-P
  *
  * 
  * 
  * 
  * 
  *
  * WIRING - the GFX01M2 plugs directly onto the top of this board's CN11/CN12
  * ST Morpho headers. NOTE: ST's own datasheet (DB4236) only lists Nucleo-64
  * boards as compatible with the GFX01M2 - the NUCLEO-L496ZG-P (Nucleo-144)
  * is not officially validated, so UM2750 has no pin table for this exact
  * combination. Pins were instead cross-referenced against this board's own
  * schematic (mb1312-l4xxzx-a03): RESET=PA1, CS=PA9, DC=PB10, SPI1 SCK/MISO/
  * MOSI=PA5/PA6/PA7. See Drivers/BSP/GFX01M2/gfx01m2_conf.h for the full
  * derivation and the PA9/USB_VBUS_Pin sharing note.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include "gfx01m2_conf.h"
#include <sys/types.h>

// Image Files: Keep limited for space
#include "REM_1_IDLE.h"
#include "REM_2_IDLE.h"
#include "REM_3_IDLE.h"
#include "REM_SAD.h"
#include "REM_Happy.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_tim.h"

#define REM_DEAD REM_1_IDLE

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;

static volatile uint8_t speakerActive = 0;

typedef struct
{
    uint32_t frequency;
    uint32_t duration;
} SpeakerNote;

static const SpeakerNote *currentSound = NULL;
static uint8_t currentNote = 0;
static uint8_t totalNotes = 0;
static uint32_t noteStartTime = 0;

typedef enum {
  STATE_IDLE = 0,
  STATE_FEED,
  STATE_PLAY,
  STATE_PET,
  STATE_DEAD,
  STATE_RUNAWAY,
  STATE_COUNT
} petState_t;

static uint8_t happiness = 100;
static uint8_t hunger = 100;
static uint8_t cursor = 0;
volatile static uint8_t button = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

static void MX_TIM6_Init(void);

static petState_t runStateIdle(void);
static petState_t runStateFeed(void);
static petState_t runStatePlay(void);
static petState_t runStatePet(void);
static petState_t runStateDead(void);
static petState_t runStateRunAway(void);

static void drawMenu(void);
static void drawStatBars(void);

static void Speaker_Tone(uint32_t frequency);
static void Speaker_StartSound(const SpeakerNote *notes, uint8_t count);
static void Speaker_Update(void);
static void Speaker_UpdateIdleSound(void);

static void Speaker_PlayFeedSound(void);
static void Speaker_PlayPlaySound(void);
static void Speaker_PlayPetSound(void);
static void Speaker_PlayDeadSound(void);
static void Speaker_PlayRunAwaySound(void);
static void Speaker_PlayIdleSound(void);
static void Speaker_Delay(uint32_t durationMs);

/* Joystick (B1) bit flags returned by Joystick_Read() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

static uint8_t Joystick_Read(void)
{
  uint8_t state = 0;

  /* All five directions are active low (see gfx01m2_conf.h) */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)     { state |= JOY_LEFT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET) { state |= JOY_CENTER_MASK; }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)     { state |= JOY_DOWN_MASK; }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)   { state |= JOY_RIGHT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)         { state |= JOY_UP_MASK; }

  return state;
}

/**>>
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_TIM6_Init();
  

  /* Initialize the LCD and show a splash screen */
  ILI9341_Init(&hspi1);
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  Speaker_Delay(2000);

  ILI9341_DrawRect(2, 250, 236, 68, ILI9341_COLOR_WHITE);    // Draw menu bar
  ILI9341_DrawRect(2, 227, 236, 24, ILI9341_COLOR_WHITE);    // Draw stats bar
  ILI9341_DrawRect(4, 238, 102, 10, ILI9341_COLOR_WHITE);    // Draw happiness bar
  ILI9341_DrawRect(134, 238, 102, 10, ILI9341_COLOR_WHITE);  // Draw hunger bar
  ILI9341_DrawString(4, 229, "HUNGER",ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);       // Draw hunger label
  ILI9341_DrawString(136, 229, "HAPPINESS",ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);  // Draw happiness label
  drawMenu();

  petState_t current_state = STATE_IDLE;
  petState_t next_state = STATE_IDLE;
  
  /* Infinite loop: cycle through the demo pages */
  while (1)
  {
    drawStatBars();
    Speaker_Update();
    
    switch (current_state)
    {
      case STATE_IDLE:
        next_state = runStateIdle();
        break;
      case STATE_FEED:
        next_state = runStateFeed();
        break;
      case STATE_PLAY:
        next_state = runStatePlay();
        break;
      case STATE_PET:
        next_state = runStatePet();
        break;
      case STATE_DEAD:
        next_state = runStateDead();
        break;
      case STATE_RUNAWAY:
        next_state = runStateRunAway();
        break;
      default:
        next_state = STATE_IDLE;
        break;
    }
    
    current_state = next_state;
  }
}

/* Play a tone using PE9 and GPIO toggling */
/** Play a tone using PE1 and GPIO toggling */
static void Speaker_Tone(uint32_t frequency)
{
    HAL_TIM_Base_Stop_IT(&htim6);
    speakerActive = 0;

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);

    if (frequency == 0)
    {
        return;
    }

    uint32_t timerClock = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0)
    {
        timerClock *= 2;
    }

    uint32_t timerTick = timerClock / (htim6.Init.Prescaler + 1);
    uint32_t period = timerTick / (frequency * 2);

    if (period == 0)
    {
        period = 1;
    }

    __HAL_TIM_SET_AUTORELOAD(&htim6, period - 1);
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

    speakerActive = 1;
    HAL_TIM_Base_Start_IT(&htim6);
}

static void Speaker_StartSound(const SpeakerNote *notes, uint8_t count)
{
    if (count == 0)
    {
        return;
    }

    currentSound = notes;
    totalNotes = count;
    currentNote = 0;
    noteStartTime = HAL_GetTick();

    Speaker_Tone(currentSound[0].frequency);
}
static void Speaker_Update(void)
{
    if (currentSound == NULL)
    {
        return;
    }

    uint32_t currentTime = HAL_GetTick();

    if ((uint32_t)(currentTime - noteStartTime) >= currentSound[currentNote].duration)
    {
        currentNote++;

        if (currentNote >= totalNotes)
        {
            Speaker_Tone(0);
            currentSound = NULL;
            return;
        }

        noteStartTime = currentTime;
        Speaker_Tone(currentSound[currentNote].frequency);
    }
}

static void Speaker_Delay(uint32_t durationMs)
{
    uint32_t startTime = HAL_GetTick();

    while (HAL_GetTick() - startTime < durationMs)
    {
        Speaker_Update();
        HAL_Delay(1);
    }
}

static const SpeakerNote feedSound[] = {
    {500, 100},
    {0, 40},
    {750, 100},
    {0, 40},
    {1000, 150}
};

static const SpeakerNote playSound[] = {
    {1000, 80},
    {0, 40},
    {600, 80},
    {0, 40},
    {1200, 150}
};

static const SpeakerNote petSound[] = {
    {600, 100},
    {0, 50},
    {900, 100}
};

static const SpeakerNote deadSound[] = {
    {600, 250},
    {0, 80},
    {300, 350}
};

static const SpeakerNote runAwaySound[] = {
    {1400, 35},
    {0, 25},
    {1100, 35},
    {0, 25},
    {1500, 30},
    {0, 20},
    {1000, 35},
    {0, 20},
    {800, 40},
    {0, 20},
    {600, 45}
};

static void Speaker_PlayFeedSound(void)
{
    Speaker_StartSound(feedSound, sizeof(feedSound) / sizeof(feedSound[0]));
}

static void Speaker_PlayPlaySound(void)
{
    Speaker_StartSound(playSound, sizeof(playSound) / sizeof(playSound[0]));
}

static void Speaker_PlayPetSound(void)
{
    Speaker_StartSound(petSound, sizeof(petSound) / sizeof(petSound[0]));
}

static void Speaker_PlayDeadSound(void)
{
    Speaker_StartSound(deadSound, sizeof(deadSound) / sizeof(deadSound[0]));
}

static void Speaker_PlayRunAwaySound(void)
{
    Speaker_StartSound(runAwaySound, sizeof(runAwaySound) / sizeof(runAwaySound[0]));
}

static petState_t runStateIdle(void) {
  ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);
  
  uint8_t stateChanged = 0;
  uint32_t lastTick = HAL_GetTick();
  const uint32_t decayIntervalMs = 1000; /* Decrement stats every 1 second */

  while(button != JOY_CENTER_MASK) {

    Speaker_Update();

    switch(button) { 
      case JOY_RIGHT_MASK:
        cursor = (cursor + 1) % 5;
        stateChanged = 1;
        button = 0;
        break;

      case JOY_LEFT_MASK:
        cursor = (cursor == 0) ? 4 : cursor - 1;
        stateChanged = 1;
        button = 0;
        break;

      default:
        break;
    }

    if (stateChanged) {
      drawMenu();
      stateChanged = 0;
    }

    /* Drain stats periodically over time */
    if (HAL_GetTick() - lastTick >= decayIntervalMs) {
      lastTick = HAL_GetTick();

      if (hunger > 0) {
        hunger--;
      }
      if (happiness > 0) {
        happiness--;
      }

      /* Redraw stat bars to reflect the new values */
      drawStatBars();

      if (happiness == 0){
        return STATE_RUNAWAY;
      }
      /* Check for starvation condition */
      if (hunger == 0) {
        return STATE_DEAD;
      }
    }

    if(button == 0) ILI9341_DrawImage(30, 10, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_1_IDLE);
    if(button == 0) ILI9341_DrawImage(30, 10, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_2_IDLE);
    if(button == 0) ILI9341_DrawImage(30, 10, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_3_IDLE);

    /* Small delay to prevent joystick debounciSTATE_HUNGRYng issues */
    HAL_Delay(10);
  }

  button = 0;

  switch(cursor) {
    case 0:
      return STATE_PLAY;
    case 1:
      return STATE_PET;
    case 2:
      return STATE_FEED;
    case 3:
      hunger = 0;
      break;
    case 4:
      happiness = 0;
      break;
    default:
      return STATE_IDLE;
  }
}

static petState_t runStateFeed(void) {
    ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);
    ILI9341_DrawString(10, 10, "FEEDING PET...", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);

    Speaker_PlayFeedSound();

    uint32_t startTime = HAL_GetTick();
    uint32_t lastFrameTime = startTime;
    int y = 40;

    ILI9341_DrawImage(30, y, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_Happy);

    while (HAL_GetTick() - startTime < 2000) {
        Speaker_Update();

        if (HAL_GetTick() - lastFrameTime >= 400 && y > 20) {
            lastFrameTime = HAL_GetTick();

            // Clear bottom strip from previous position
            ILI9341_FillRect(30, y + REM_IDLE_HEIGHT - 10,
                             REM_IDLE_WIDTH, 10, ILI9341_COLOR_BLACK);

            y -= 10;

            // Draw image at new position
            ILI9341_DrawImage(30, y, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_Happy);
        }
    }

    hunger = 100;
    return STATE_IDLE;
}


static petState_t runStatePlay(void) {
  ILI9341_FillRect(0, 0, 240, 100, ILI9341_COLOR_BLACK);
  ILI9341_DrawString(10, 10, "PLAYING WITH PET...", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
 Speaker_PlayPlaySound();
  Speaker_Delay(2000);
  happiness = 100;
  hunger = (hunger >= 0) ? hunger - 10: 0;
  return STATE_IDLE;
}

static petState_t runStatePet(void) {
    ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);
    ILI9341_DrawString(10, 10, "PETTING PET...", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);

    Speaker_PlayPetSound();

    uint32_t startTime = HAL_GetTick();
    uint32_t lastFrameTime = startTime;
    int y = 40;

    ILI9341_DrawImage(30, y, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_Happy);

    while (HAL_GetTick() - startTime < 2000) {
        Speaker_Update();

        if (HAL_GetTick() - lastFrameTime >= 400 && y > 20) {
            lastFrameTime = HAL_GetTick();

            // Clear bottom strip from previous position
            ILI9341_FillRect(30, y + REM_IDLE_HEIGHT - 10,
                             REM_IDLE_WIDTH, 10, ILI9341_COLOR_BLACK);

            y -= 10;

            // Draw image at new position
            ILI9341_DrawImage(30, y, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_Happy);
        }
    }

    happiness = 100;
    return STATE_IDLE;
}

static petState_t runStateDead(void) {
  ILI9341_DrawImage(30, 10, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_DEAD);
  Speaker_PlayDeadSound();
  Speaker_Delay(2000);
  ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);
  
  uint32_t lastTick = HAL_GetTick();
  uint8_t flashState = 0;

  /* Draw initial RESTART? prompt */
  ILI9341_DrawString(64, 50, "TRY NOT TO", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(64, 70, "KILL ME...", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  
  /* Wait for a button press while flashing the text */
  while (button == 0) {
    button = Joystick_Read();

    /* Toggle string color every 500ms */
    if (HAL_GetTick() - lastTick >= 500) {
      lastTick = HAL_GetTick();
      flashState = !flashState;

      uint16_t textColor = flashState ? ILI9341_COLOR_WHITE : ILI9341_COLOR_RED;
      ILI9341_DrawString(73, 110, "RESTART?", textColor, ILI9341_COLOR_BLACK, 2);
    }

    HAL_Delay(10);
  }

  /* Debounce: wait until all joystick inputs are released */
  while (Joystick_Read() != 0) {
    HAL_Delay(20);
  }
  HAL_Delay(50); /* Settling delay after release */

  ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);

  /* Reset pet stats */
  hunger = 100;
  happiness = 100;
  button = 0;
  
  return STATE_IDLE;
}

static petState_t runStateRunAway(void) {
  ILI9341_FillRect(0, 0, 240, 220, ILI9341_COLOR_BLACK);
  Speaker_PlayRunAwaySound();
  // REM runs off screen
  for(uint8_t i = 0; i < 220; i += 10) {
    Speaker_Update();
    if(i < 40) ILI9341_DrawImage(30, i, REM_IDLE_WIDTH, REM_IDLE_HEIGHT, REM_SAD);
    if(i >= 40) ILI9341_DrawImage(30, i, REM_IDLE_WIDTH, 220 - i, REM_SAD);
    ILI9341_FillRect(30, i, REM_IDLE_WIDTH, 10, ILI9341_COLOR_BLACK);
  }
   

  uint32_t lastTick = HAL_GetTick();
  uint8_t flashState = 0;

  ILI9341_DrawString(43, 50, "TRY LOVING ME", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(49, 70, "NEXT TIME...", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);

  while (button == 0) {
    button = Joystick_Read();

    /* Toggle string color every 500ms */
    if (HAL_GetTick() - lastTick >= 500) {
      lastTick = HAL_GetTick();
      flashState = !flashState;

      uint16_t textColor = flashState ? ILI9341_COLOR_WHITE : ILI9341_COLOR_RED;
      ILI9341_DrawString(73, 110, "RESTART?", textColor, ILI9341_COLOR_BLACK, 2);
    }

    HAL_Delay(10);
  }

  while (Joystick_Read() != 0) {
    HAL_Delay(20);
  }

  ILI9341_FillRect(0, 0, 240, 160, ILI9341_COLOR_BLACK);

  HAL_Delay(50); /* Settling delay after release */

  /* Reset pet stats */
  hunger = 100;
  happiness = 100;
  button = 0;
  
  return STATE_IDLE;
}



static void drawMenu(void) {
    if(cursor != 0) ILI9341_DrawString(31, 260, "PLAY", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
    if(cursor != 1) ILI9341_DrawString(103, 260, "PET", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
    if(cursor != 2) ILI9341_DrawString(163, 260, "FEED", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
    if(cursor != 3) ILI9341_DrawString(45, 290, "KILL", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
    if(cursor != 4) ILI9341_DrawString(113, 290, "NEGLECT", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

    switch(cursor) {
      case 0:
        ILI9341_DrawString(31, 260, "PLAY", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
        HAL_Delay(50);
        break;
      case 1:
        ILI9341_DrawString(103, 260, "PET", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
        HAL_Delay(50);
        break;
      case 2:
        ILI9341_DrawString(163, 260, "FEED", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
        HAL_Delay(50);
        break;
      case 3:
        ILI9341_DrawString(45, 290, "KILL", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
        HAL_Delay(50);
        break;
      case 4:
        ILI9341_DrawString(113, 290, "NEGLECT", ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
        HAL_Delay(50);
        break;
    }
}

static void drawStatBars(void) {
  ILI9341_FillRect(5, 239, hunger, 8, ILI9341_COLOR_GREEN);
  ILI9341_FillRect(5 + hunger, 239, 100 - hunger, 8, ILI9341_COLOR_BLACK);

  ILI9341_FillRect(135, 239, happiness, 8, ILI9341_COLOR_BLUE);
  ILI9341_FillRect(135 + happiness, 239, 100 - happiness, 8, ILI9341_COLOR_BLACK);
}







static void MX_TIM6_Init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();

    htim6.Instance = TIM6;
    htim6.Init.Prescaler = 55;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = 499;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}











/**
  * @brief SPI1 Initialization Function
  *        SCK = PA5, MISO = PA6, MOSI = PA7 - connects to the GFX01M2 LCD.
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  /* PCLK2 is now ~56.8 MHz (HCLK was previously quartered by a stray AHB
   * DIV4 - see SystemClock_Config). /32 keeps the same ~1.775 MHz target
   * that gave comfortable margin over the ILI9341's 10 MHz write ceiling
   * on this jumper-wired, unsupported-board setup. */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 71;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  /* DIV4 here previously left HCLK at 56.8/4 = 14.2 MHz despite the PLL
   * being configured for 56.8 MHz - DIV1 makes HCLK match the PLL output. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* HCLK is now ~56.8 MHz - VOS Range 1 requires 3 wait states for
   * 48-64 MHz (RM0351), not the 0 WS that was only valid up to 16 MHz. */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE(); // Initialize PE9

  /* Configure PE9 for the speaker */
HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);

GPIO_InitStruct.Pin = GPIO_PIN_1;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin, GPIO_PIN_RESET);

  /* LCD CS/RESET idle high, DC state does not matter until the first transfer */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OverCurrent_Pin SMPS_PG_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin|SMPS_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_PowerSwitchOn_Pin SMPS_V1_Pin SMPS_EN_Pin SMPS_SW_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC is on a different port than LCD_CS (GPIOB vs GPIOA) - must be a separate HAL_GPIO_Init call */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_LEFT_Pin JOY_DOWN_Pin JOY_RIGHT_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_LEFT_Pin|JOY_DOWN_Pin|JOY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_CENTER_Pin JOY_UP_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_CENTER_Pin|JOY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Configure PB0, PB6, PC7 as interrupt inputs */
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Enable NVIC interrupts */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6 && speakerActive)
    {
        HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_1);
    }
}

void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        button |= JOY_RIGHT_MASK;  // Set the button flag for PB0
    }
    else if (GPIO_Pin == GPIO_PIN_6)
    {
        button |= JOY_LEFT_MASK;  // Set the button flag for PB6
    }
    else if (GPIO_Pin == GPIO_PIN_7)
    {
        button |= JOY_CENTER_MASK;  // Set the button flag for PC7
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
