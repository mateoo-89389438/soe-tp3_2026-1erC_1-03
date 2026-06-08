# TP3 – Actividad 03 – 14to Proyecto p/placa NUCLEO-F103RB con FreeRTOS

## Paso 06: 

## 1. Respuesta de la IA Gemini con el prompt:

## Analizar y explicar (en español), el funcionamiento del código fuente contenido en los archivos adjuntos: app.c , app_it.c , task_enrtry_a.c , task_exit_a.c , task_test.c y freertos.c .

El código proporcionado corresponde a una aplicación embebida basada en el sistema operativo en tiempo real (RTOS) **FreeRTOS**. Específicamente, está diseñada como un Sistema Disparado por Eventos (ETS) para simular la lógica de un "Cruce Vehicular" en el contexto de un proyecto académico.

A continuación, se detalla la función principal de cada uno de los archivos fuente proporcionados:

### `app.c` (Inicialización de la Aplicación)

Este archivo es el núcleo configurador de la aplicación y se encarga de preparar el entorno del RTOS.

* Inicializa variables globales utilizadas para el monitoreo de tareas y el conteo de *ticks* (ej. `g_app_cnt`, `g_app_task_cnt`).
* Crea cinco tareas independientes de FreeRTOS (`task_entry_a`, `task_exit_a`, `task_entry_b`, `task_exit_b` y `task_test`) mediante la función `xTaskCreate`.
* Asigna distintas prioridades a las tareas: las tareas "Entry A" y "Entry B" tienen la prioridad más alta (nivel 3), las tareas "Exit" tienen prioridad 2, y la tarea "Test" se crea inicialmente con la prioridad más baja (nivel 1).
* Llama a rutinas de inicialización de interrupciones (`app_it_init()`) y prepara un contador de ciclos para medir el rendimiento.

---

### `app_it.c` (Manejo de Interrupciones)

Este archivo gestiona las interrupciones del hardware y sus devoluciones de llamada (callbacks).

* Proporciona la función `app_it_init()`, que utiliza instrucciones de ensamblador (`__asm`) para deshabilitar y volver a habilitar las interrupciones, protegiendo así los recursos compartidos durante la inicialización del sistema.
* Define el callback de interrupción externa `HAL_GPIO_EXTI_Callback`, que contiene la lógica a ejecutar cuando el hardware detecta un evento en un pin específico, como la pulsación del botón `BTN_A_PIN`.

---

### `task_entry_a.c` y `task_exit_a.c` (Tareas Funcionales)

Ambos archivos implementan hilos de ejecución individuales que simulan partes del cruce vehicular.

* Se ejecutan en bucles infinitos (`for (;;)`) estándar para tareas de RTOS.
* En cada ciclo, incrementan sus contadores globales específicos (`g_task_entry_a_cnt` y `g_task_exit_a_cnt`).
* Imprimen un mensaje informativo mediante el registrador de eventos (logger).
* Utilizan la API `vTaskDelay` para bloquear la tarea (ponerla a dormir) durante 2500 milisegundos, cediendo así el procesador a otras tareas de menor prioridad.

---

### `task_test.c` (Generación de Estímulos)

Este archivo actúa como un banco de pruebas que introduce eventos periódicos para evaluar la respuesta de las demás tareas.

* Durante su arranque, la tarea lee su prioridad actual y se reasigna a sí misma un nivel de prioridad superior (sumando 2), lo que obliga al planificador (scheduler) a ejecutarla inmediatamente.
* Itera sobre un arreglo de pruebas llamado `e_task_test_array` que contiene diferentes eventos (como `Entry_A`, `Exit_A`, `Error`), el cual cambia dependiendo del valor pre-compilado de la macro `E_TASK_TEST_X`.
* Utiliza un bloque switch para determinar qué señal imprimir o despachar según el índice del evento actual.
* Emplea `vTaskDelayUntil` para garantizar un tiempo de ejecución absolutamente preciso cada 5000 milisegundos, a diferencia del retardo relativo usado en las otras tareas.

---

### `freertos.c` (Integración del RTOS)

Este archivo implementa las funciones "Hook" (ganchos) definidas por el usuario, que permiten inyectar código directamente en las rutinas internas de FreeRTOS.

* **`vApplicationIdleHook`**: Se ejecuta continuamente en la tarea de menor prioridad posible (Idle) cuando ninguna otra tarea necesita el procesador, incrementando el contador `g_task_idle_cnt`.
* **`vApplicationTickHook`**: Es llamada durante la rutina de servicio de interrupción (ISR) del reloj del sistema, actualizando un contador global por cada "tick" de la aplicación.
* **`vApplicationStackOverflowHook`**: Es un mecanismo de seguridad que se dispara si el RTOS detecta que la memoria RAM de alguna tarea ha superado sus límites, colgando intencionalmente la ejecución con `configASSERT(0)` para facilitar la depuración.



# -------------------------- # ------------------------- # -------------------------- # ------------------------- # -------------------------- # ------------------------- #
# Paso 09: Problema de Sincronización - Cruce Vehicular

### Configuración
**Modificaciones y lógica de diseño en el código:**
- **Sincronización Reactiva (Event-Driven):** Se eliminaron los retardos de tiempo autónomos de las tareas de control de los puestos A y B. En su lugar, se implementaron 4 semáforos binarios globales (`xSem_Entry_A`, `xSem_Exit_A`, `xSem_Entry_B`, `xSem_Exit_B`) inicializados en 0 en `app.c`. La tarea de simulación de alta prioridad `task_test` actúa como generadora de eventos utilizando primitivas `xSemaphoreGive()`, mientras que las tareas de ingreso y egreso permanecen bloqueadas en `xSemaphoreTake(..., portMAX_DELAY)` hasta ser requeridas.
- **Exclusión Mutua del Recurso Compartido:** La ocupación del cruce se modela con una variable global compartida (`g_crossing_cnt`). Para evitar condiciones de carrera (*race conditions*) derivadas de la concurrencia, se implementó un Mutex denominado `xMutex_Crossing` que garantiza que solo una tarea a la vez pueda evaluar o modificar el estado del contador.
- **Control de Capacidad Máxima y Bloqueo por Condición:** Se definió un límite físico estricto de capacidad (`MAX_CAPACITY = 2`). Cuando un puesto de entrada detecta un vehículo pero el contador ha alcanzado la capacidad máxima, se señaliza el estado del semáforo vial en **ROJO**. Para evitar una espera activa o un bloqueo mutuo (*deadlock*), la tarea libera el Mutex inmediatamente y se auto-suspende bloqueada en un semáforo binario de condición (`xSem_SpaceAvailable`).
- **Liberación Dinámica de la Intersección:** Al procesarse un evento de egreso, la tarea de salida decrementa el contador bajo la protección del Mutex e inmediatamente emite una señalización mediante `xSemaphoreGive(xSem_SpaceAvailable)`. Esto despierta de forma inmediata a cualquier tarea de ingreso que estuviera retenida en luz roja, alternando instantáneamente el semáforo vial a **VERDE** y habilitando el acceso seguro.

*Resultado con (`E_TASK_TEST_X = 3`)*
[info]

[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a seo-tp3_03-application: Vehicular crossing
[info]  app is a (Source => TA149 - Sistemas Operativos Embebidos)
[info]

[info]   Task Entry B is running - Tick [mS] = 0
[info]

[info]   Task Entry A is running - Tick [mS] = 0
[info]

[info]   Task Exit A is running - Tick [mS] = 0
[info]

[info]   Task Exit B is running - Tick [mS] = 0
[info]

[info]   Task Test is running - Tick [mS] = 1
[info]   <=> Task Test - Priority: Task Test 3
[info]

[info]   <=> Task Test - e_task_test_array: index 0
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Light A: GREEN] - Car ENTERS. Cars inside: 1
[info]

[info]   <=> Task Test - e_task_test_array: index 1
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Light A: GREEN] - Car ENTERS. Cars inside: 2
[info]

[info]   <=> Task Test - e_task_test_array: index 2
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Light A: RED] - Intersection FULL. Car waiting...
[info]

[info]   <=> Task Test - e_task_test_array: index 3
[info]   <=> Task Test - Wait:   5000mS
[info]  <== [Exit A] - Car exited. Cars inside: 1
[info]  ==> [Light A: GREEN] - Car ENTERS. Cars inside: 2
[info]

[info]   <=> Task Test - e_task_test_array: index 4
[info]   <=> Task Test - Wait:   5000mS
[info]  <== [Exit A] - Car exited. Cars inside: 1
[info]

[info]   <=> Task Test - e_task_test_array: index 5
[info]   <=> Task Test - Wait:   5000mS
[info]  <== [Exit A] - Car exited. Cars inside: 0
[info]

[info]   <=> Task Test - e_task_test_array: index 0
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Light A: GREEN] - Car ENTERS. Cars inside: 1

**Observaciones:**
1. **Validación del Límite de Capacidad:** Se verifica experimentalmente el comportamiento preventivo en el `index 2`. Al intentar ingresar un tercer vehículo consecutivo estando el cruce ocupado al máximo (`Cars inside: 2`), el sistema responde de forma determinística cambiando el estado del Semáforo Vial a **RED** e interrumpiendo el flujo. La tarea ingresa al estado *Blocked* liberando recursos de cómputo.
2. **Desbloqueo por Evento de Egreso:** En el `index 3`, ante la llegada del estímulo de salida `Exit A`, el contador disminuye a 1 de manera transitoria. De inmediato, gracias al semáforo de condición `xSem_SpaceAvailable`, la tarea de entrada que se encontraba retenida recobra su lugar en la lista de listos (*Ready List*), toma el control, cambia el estado a **GREEN** e ingresa, volviendo a ocupar el puente al máximo de su capacidad (`Cars inside: 2`). Ambos logs ocurren de forma consecutiva bajo la misma ventana de activación temporal, validando el correcto funcionamiento de las variables de condición sobre FreeRTOS.
3. **Estabilidad y Prevención de Deadlocks:** La correcta secuenciación de tomar y liberar el Mutex (`xMutex_Crossing`) previo a la suspensión en el semáforo binario de condición evita la retención indefinida del recurso. Esto permite que las tareas de egreso puedan ejecutarse libremente, modificando la variable compartida y garantizando la vivacidad del sistema de control del microcontrolador STM32F103RB.