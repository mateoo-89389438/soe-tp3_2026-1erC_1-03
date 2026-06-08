/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include "cmsis_os.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"

/********************** macros and definitions *******************************/
#define G_TASK_ENTRY_B_CNT_INI	0ul

#define TASK_ENTRY_B_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_ENTRY_B_DEL_MAX	(pdMS_TO_TICKS(2500ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_entry_b_wait_2500mS		= "   ==> Task Entry B - Wait:   2500mS";

/********************** external data declaration *****************************/
uint32_t g_task_entry_b_cnt;
extern SemaphoreHandle_t xSem_Entry_B;

extern SemaphoreHandle_t xSem_Entry_B;
extern SemaphoreHandle_t xMutex_Crossing;
extern SemaphoreHandle_t xSem_SpaceAvailable;
extern uint32_t g_crossing_cnt;

#define MAX_CAPACITY    2ul  // Capacidad máxima definida para el cruce
/********************** external functions definition ************************/
/* Task thread */
void task_entry_b(void *parameters)
{
	/*  Declare & Initialize Task Function variables */
	g_task_entry_b_cnt = G_TASK_ENTRY_B_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Espera bloqueante hasta que task_test genere el evento Entry_B */
		if (xSemaphoreTake(xSem_Entry_B, portMAX_DELAY) == pdTRUE)
		{
			/* Update Task Counter */
        g_task_entry_b_cnt++;
        
        uint32_t entered = 0;
        
        while (entered == 0)
        {
            xSemaphoreTake(xMutex_Crossing, portMAX_DELAY); // Protección del recurso
            
            if (g_crossing_cnt < MAX_CAPACITY)
            {
                /* ¡Semaforo Vial en VERDE! Hay espacio disponible */
                g_crossing_cnt++;
                LOGGER_INFO(" ==> [Light B: GREEN] - Car ENTERS. Cars inside: %lu", g_crossing_cnt);
                entered = 1; // Rompe el lazo de reintento
                xSemaphoreGive(xMutex_Crossing);
            }
            else
            {
                /* ¡Semaforo Vial en ROJO! Cruce lleno, hay que esperar */
                LOGGER_INFO(" ==> [Light B: RED] - Intersection FULL. Car waiting...");
                xSemaphoreGive(xMutex_Crossing); // Liberar antes de bloquearse
                
                /* Bloqueo acá hasta que una tarea de salida avise que hay lugar */
                xSemaphoreTake(xSem_SpaceAvailable, portMAX_DELAY);
            }
        }
        vTaskDelay(TASK_ENTRY_B_DEL_ZERO);
		}
	}
}

/********************** end of file ******************************************/

