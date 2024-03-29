/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "delay.h"
#include "../../config.h"
#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Tick Counter united by ms */
static volatile uint32_t _ulTickCount = 0;
#if !defined(NO_DELAY_HIGH_WORD)
static volatile uint32_t _ulTickCountHighWord = 0;
#endif

unsigned long millis(void) {
  // todo: ensure no interrupts
  return _ulTickCount;
}

// Interrupt-compatible version of micros
// Theory: repeatedly take readings of SysTick counter, millis counter and
// SysTick interrupt pending flag. When it appears that millis counter and
// pending is stable and SysTick hasn't rolled over, use these values to
// calculate micros. If there is a pending SysTick, add one to the millis
// counter in the calculation.
unsigned long micros(void) {
  uint32_t ticks, ticks2;
  uint32_t pend, pend2;
  uint32_t count, count2;

  ticks2 = SysTick->VAL;
  pend2 = !!(SCB->ICSR & SCB_ICSR_PENDSTSET_Msk);
  count2 = _ulTickCount;

  do {
    ticks = ticks2;
    pend = pend2;
    count = count2;
    ticks2 = SysTick->VAL;
    pend2 = !!(SCB->ICSR & SCB_ICSR_PENDSTSET_Msk);
    count2 = _ulTickCount;
  } while ((pend != pend2) || (count != count2) || (ticks < ticks2));

  return ((count + pend) * 1000) +
         (((SysTick->LOAD - ticks) * (1048576 / (VARIANT_MCK / 1000000))) >>
          20);
  // this is an optimization to turn a runtime division into two compile-time
  // divisions and a runtime multiplication and shift, saving a few cycles
}

void delay(unsigned long ms) {
  if (ms) {
#if !defined(NO_DELAY_HIGH_WORD)
    uint8_t enableInterrupts = ((__get_PRIMASK() & 0x1) == 0);
    __disable_irq();
#endif

    uint32_t start = _ulTickCount;
#if !defined(NO_DELAY_HIGH_WORD)
    uint32_t targetTickCountHighWord = _ulTickCountHighWord;

    if (enableInterrupts) {
      __enable_irq();
    }

    // Check for _ulTickCount overflow
    uint32_t remaining = (0xFFFFFFFF - start);
    if (ms > remaining) {
      ms -= (remaining + 1);
      start = 0;
      targetTickCountHighWord++;
    }
#endif

    do {
      yield();
#if !defined(NO_DELAY_HIGH_WORD)
    } while (_ulTickCountHighWord < targetTickCountHighWord ||
             (_ulTickCount - start) < ms);
#else
    } while (_ulTickCount - start < ms);
#endif
  }
}

#include "Reset.h" // for tickReset()

void SysTick_DefaultHandler(void) {
  // Increment tick count each ms
  _ulTickCount++;

#if !defined(NO_DELAY_HIGH_WORD)
  if (_ulTickCount == 0) {
    _ulTickCountHighWord++;
  }
#endif
#if defined(CDC_ONLY) || defined(CDC_HID) || defined(WITH_CDC)
  tickReset();
#endif
}
void fast_forward_time(uint32_t ffwd_millis) {
  uint32_t time_to_rollower = UINT32_MAX - _ulTickCount;
  if (time_to_rollower > ffwd_millis) {
    _ulTickCount += ffwd_millis;
  } else {
    _ulTickCountHighWord++;
    _ulTickCount = ffwd_millis - time_to_rollower;
  }
}
#ifdef __cplusplus
}
#endif
