# TP3 – Actividad 01 – 12vo Proyecto p/placa NUCLEO-F103RB con FreeRTOS

## Paso 06: 

## 1. Respuesta de la IA Gemini con el prompt:

```
Analizar y explicar (en español), el funcionamiento del código fuente contenido en los archivos adjuntos: app.c ,app_it.c , task_a.c , task_b.c y freertos.c.
```

Este código forma parte de un proyecto de software embebido estructurado sobre **FreeRTOS**, diseñado para demostrar el funcionamiento de un sistema controlado por eventos (Event-Triggered System). Aunque los comentarios del código hacen referencia al clásico problema de sincronización del "Productor-Consumidor", la implementación actual establece la arquitectura base del sistema operativo y ejecuta dos tareas periódicas independientes.

A continuación, te explico en detalle el funcionamiento y propósito de cada archivo.


### 1. `app.c` (Inicialización y Configuración General)

Este archivo es el núcleo de la configuración de la aplicación antes de que el planificador (scheduler) de FreeRTOS comience a administrar el tiempo del procesador.

* **Inicialización Global:** La función principal `app_init()` reinicia varios contadores globales (como `g_app_cnt`, `g_app_tick_cnt`, etc.) y emite mensajes de registro (logs) indicando que la aplicación ha arrancado.
* **Creación de Tareas:** Utiliza la API de FreeRTOS `xTaskCreate` para instanciar las dos tareas principales del sistema: `Task A` y `Task B`.
* **Prioridades:** Ambas tareas son creadas con el tamaño de pila mínimo recomendado (`configMINIMAL_STACK_SIZE`) y se les asigna exactamente la misma prioridad de ejecución (`tskIDLE_PRIORITY + 1ul`).
* **Configuración de Hardware:** Llama a las funciones `app_it_init()` y `cycle_counter_init()` para preparar las interrupciones del microcontrolador y el conteo de ciclos de reloj.

### 2. `task_a.c` y `task_b.c` (Las Tareas o Hilos de Ejecución)

Estos archivos contienen la lógica individual de las dos rutinas que el sistema operativo ejecutará de manera concurrente.

* **Bucle Infinito:** Como es estándar en los RTOS, ambas tareas están encapsuladas dentro de un bucle infinito (`for (;;)`) para que nunca terminen su ejecución.
* **Comportamiento de la Tarea A:** Incrementa un contador global dedicado (`g_task_a_cnt`), envía el mensaje "Wait: 250mS" a la consola de registro y, fundamentalmente, llama a `vTaskDelay(TASK_A_DEL_MAX)`. Esta función bloquea la tarea (liberando el procesador) durante 250 milisegundos.
* **Comportamiento de la Tarea B:** Su funcionamiento es idéntico estructuralmente al de la Tarea A, pero incrementa su propio contador (`g_task_b_cnt`) y su retardo de bloqueo es diez veces mayor: 2500 milisegundos (2.5 segundos) a través de la macro `TASK_B_DEL_MAX`.

### 3. `freertos.c` (Hooks del Sistema Operativo)

Este archivo permite inyectar código personalizado dentro de las rutinas internas del propio FreeRTOS mediante funciones conocidas como "Hooks".

* **`vApplicationIdleHook`:** Se ejecuta de forma continua únicamente cuando no hay tareas de mayor prioridad listas para usar el procesador (por ejemplo, cuando tanto la Tarea A como la Tarea B están bloqueadas por su retardo). El código aprovecha este tiempo inactivo para incrementar el contador `g_task_idle_cnt`.
* **`vApplicationTickHook`:** FreeRTOS basa su medición del tiempo en una interrupción periódica llamada "Tick". Esta función se ejecuta automáticamente con cada Tick e incrementa la variable `g_app_tick_cnt`.
* **`vApplicationStackOverflowHook`:** Es un mecanismo de seguridad crítico. Si el RTOS detecta que una tarea consumió más memoria de la que se le asignó al crearla (desbordamiento de pila), llama a esta función. Aquí se detiene abruptamente la ejecución del programa mediante `configASSERT( 0 )` y se incrementa el contador de errores `g_app_stack_overflow_cnt` para facilitar la depuración.

### 4. `app_it.c` (Gestión de Interrupciones)

Este archivo se encarga de configuraciones de bajo nivel vinculadas al microcontrolador.

* **Secciones Críticas:** La función `app_it_init()` incluye directivas en lenguaje ensamblador (`__asm("CPSID i")` y `__asm("CPSIE i")`). Estas instrucciones deshabilitan y vuelven a habilitar temporalmente las interrupciones globales del procesador. Esto asegura que el proceso de configuración no sea interrumpido, protegiendo así la integridad de los recursos compartidos durante el arranque.

---

### Resumen del Flujo de Ejecución

1. Al iniciar, `app_init()` configura los contadores y le indica a FreeRTOS cómo y cuándo debe ejecutar `Task A` y `Task B`.
2. Una vez que el planificador (scheduler) arranca, evalúa las tareas. Como ambas tienen la misma prioridad, las ejecuta basándose en la disponibilidad de tiempo.
3. La **Tarea A** se ejecuta rápidamente, se imprime su mensaje, y se "duerme" por 250ms. Luego la **Tarea B** se ejecuta, hace lo mismo, y se "duerme" por 2500ms.
4. Mientras ambas tareas están dormidas, el procesador queda libre, por lo que entra a la tarea *Idle* de FreeRTOS, ejecutando repetidamente `vApplicationIdleHook`.
5. Cada vez que el temporizador interno del sistema cuenta un milisegundo, se dispara el `vApplicationTickHook`.
6. Al cumplirse 250ms, la **Tarea A** despierta, repite su ciclo y vuelve a dormir. Este ciclo de alternancia y espera continúa de manera infinita.


--- 
--- 

## Paso 09: Sincronización - Productor-Consumidor

### Configuración
**Modificaciones y lógica de diseño en el código:**
- **Implementación del Modelo Productor-Consumidor:** Se reestructuró el lazo asincrónico original del proyecto base para implementar una solución de sincronización unidireccional utilizando la estrategia de diseño del libro *"The Little Book of Semaphores"*. Se configuró a la `Task A` como el **Productor** y a la `Task B` como el **Consumidor**.
- **Mecanismo de Señalización mediante Semáforo Binario:** Se declaró e inicializó un semáforo binario global denominado `h_sem_producer_consumer` en `app.c`. Al crearse de manera nativa con `xSemaphoreCreateBinary()`, el token se inicializa en 0 (vacío), lo que garantiza el bloqueo preventivo del consumidor en caso de que no existan recursos disponibles al arrancar el sistema.
- **Sincronización del Productor (`task_a.c`):** La `Task A` ejecuta su bloque de procesamiento, simula la generación del recurso retardándose un tiempo máximo definido por la macro `TASK_A_DEL_MAX` (250 ms), y posteriormente invoca a la función **`xSemaphoreGive()`**. Esta acción incrementa el token del semáforo (lo pone en 1), notificando y desbloqueando de forma inmediata a la tarea consumidora.
- **Bloqueo Eficiente del Consumidor (`task_b.c`):** Se eliminó el retardo autónomo e independiente de la `Task B` reemplazándolo por la función **`xSemaphoreTake()`** con un tiempo de *timeout* configurado en `portMAX_DELAY`. Esto asegura que el consumidor no realice una espera activa (*busy waiting*), pasando automáticamente al estado **Blocked** para liberar el 100% de los recursos de la CPU hasta recibir la señal del productor. Al tomar el semáforo con éxito, consume el recurso, incrementa su contador y ejecuta un delay mínimo (`TASK_B_DEL_ZERO`) para habilitar la alternancia de contexto.

*Resultado terminal:*
```
[info]

[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a seo-tp3_01-application: Producer-Consumer
[info]  app is a (Source => TA149 - Sistemas Operativos Embebidos)
[info]

[info]   Task B is running - Tick [mS] = 0
[info]

[info]   Task A is running - Tick [mS] = 0
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
[info]    ==> Task    A - Wait:   250mS
[info]    ==> Task    B - Wait:   250mS
```

**Observaciones:**
1. **Inicialización y Estado Inicial de Bloqueo:** Al arrancar el sistema (`Tick [mS] = 0`), el planificador inicializa concurrentemente `Task B` y `Task A`. Se observa que la `Task A` (Productor) ejecuta dos ciclos iniciales de 250ms de forma consecutiva antes de habilitar al consumidor. Esto valida el diseño: el semáforo binario nace en cero, forzando a la `Task B` a permanecer bloqueada en la cola del semáforo hasta que la `Task A` completa la producción física y libera el primer token con `xSemaphoreGive()`.
2. **Establecimiento del Régimen de Sincronismo Intercalado:** Una vez superado el transitorio inicial de arranque, se verifica experimentalmente un régimen estacionario perfectamente entrelazado: un mensaje de la `Task A` es seguido inmediatamente por un mensaje de la `Task B` (`Task A -> Task B -> Task A -> Task B`). Esto demuestra que el consumidor ya no corre bajo su propia base de tiempo autónoma, sino que su frecuencia de ejecución está estrictamente gobernada (*event-driven*) por la velocidad de producción de la `Task A`.
3. **Optimización del Uso de la CPU:** A través de la traza de logs se confirma la desaparición de desfasajes temporales erráticos. Al utilizar las primitivas bloqueantes de FreeRTOS (`xSemaphoreTake` con `portMAX_DELAY`), el sistema operativo remueve la tarea consumidora de la lista de tareas listas (*Ready list*) mientras espera la señal. Esto garantiza un comportamiento determinístico y un uso eficiente de los recursos del microcontrolador STM32F103RB.
