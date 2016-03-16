/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 by Sergey Fetisov <fsenok@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * version: 1.0 demo (7.02.2015)
 */

#include "time.h"
#include "hw_config.h"

volatile uint32_t sysTimeTicks;
volatile uint32_t sysTimeDelayCounter;

void time_init(void)
{
  if (SysTick_Config(SystemCoreClock / 1000))
    while (1) {} // Capture error
}

volatile int64_t usAddition = 0;

void HAL_SYSTICK_Callback(void)
{
  usAddition += 1000;
  if ((usAddition & 0x03FF) == 0)
  {
    cpuLoad.result = cpuLoad.cnt;
    cpuLoad.cnt = 0;
  }
}

int64_t utime(void)
{
	uint32_t ctrl;
	static int64_t res;
	uint32_t ticks;

	ctrl = SysTick->CTRL;

read:
	ticks = SysTick->VAL;
	res = usAddition;
	ctrl = SysTick->CTRL;
	if (ctrl & SysTick_CTRL_COUNTFLAG_Msk)
		goto read;

	#define ticksPerUs (SystemCoreClock / 1000000)
	res += 1000 - ticks / ticksPerUs;
	#undef usecPerTick

	return res;
}

int64_t mtime(void)
{
  //return HAL_GetTick();
  return utime() / 1000;
}

void usleep(int us)
{
  uint64_t t = utime();
  while (true)
  {
    uint64_t t1 = utime();
    if (t1 - t >= us) break;
    if (t1 < t) break; // overflow
  }
}

static stmr_t *stmrs = NULL;

void stmr(void)
{
  stmr_t *tmr;
  uint32_t time;
  time = utime();
  tmr = stmrs;
  while (tmr != NULL)
  {
    stmr_t *t;
    uint32_t elapsed;
    t = tmr;
    tmr = tmr->next;
    if ((t->flags & STMR_ACTIVE) == 0)
      continue;
    elapsed = time;
    elapsed -= t->event;
    if (elapsed < t->period)
      continue;
    t->proc(t);
    t->event = utime();
  }
}
/*
void stmr_init(stmr_t *tmr)
{
  tmr->period = 0;
  tmr->event = 0;
  tmr->flags = 0;
  tmr->data = NULL;
  tmr->proc = NULL;
  tmr->next = stmrs;
  stmrs = tmr;
}*/

void stmr_add(stmr_t *tmr)
{
  tmr->next = stmrs;
  stmrs = tmr;
}

/*void stmr_free(stmr_t *tmr)
{
  stmr_t *t;
  
  if (stmrs == NULL)
    return;
  
  if (tmr == stmrs)
  {
    stmrs = tmr->next;
    tmr->next = NULL;
    return;
  }
  
  t = stmrs;
  while (t->next != NULL)
  {
    if (t->next == tmr)
    {
      t->next = tmr->next;
      tmr->next = NULL;
      return;
    }
    t = t->next;
  }
}

void stmr_stop(stmr_t *tmr)
{
  tmr->flags &= ~(uint32_t)STMR_ACTIVE;
}

void stmr_run(stmr_t *tmr)
{
  tmr->flags |= STMR_ACTIVE;
  tmr->event = utime();
}*/
