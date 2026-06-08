# TP3 – Actividad 04 – 15to Proyecto p/placa NUCLEO-F103RB con FreeRTOS

## Paso 06: 

## 1. Respuesta de la IA Gemini con el prompt:

```
Analizar y explicar (en español), el funcionamiento del código fuente contenido en los archivos adjuntos: app.c , app_it.c , task_gate_a.c , task_test.c y freertos.c.
```

Este conjunto de archivos en lenguaje C forma la estructura principal de una aplicación basada en un Sistema Operativo de Tiempo Real (RTOS), específicamente **FreeRTOS**. Según los metadatos y comentarios, el proyecto implementa un sistema activado por eventos (Event-Triggered System) para controlar una esclusa de seguridad ("Security airlock").

A continuación, presento un desglose detallado del propósito y funcionamiento de cada archivo .

---

### `app.c` - Inicialización y Orquestación Principal

Este archivo actúa como el punto de entrada para la configuración del sistema y la creación de los hilos de ejecución (tareas).

* **Inicialización de Variables:** La función `app_init()` comienza reiniciando varios contadores globales (como `g_app_cnt`, `g_task_idle_cnt`, etc.) que sirven para monitorear el rendimiento y la actividad de la aplicación.
* **Creación de Tareas:** Utiliza la API de FreeRTOS `xTaskCreate` para instanciar cinco tareas distintas, asignándoles diferentes niveles de prioridad:
* **Task Gate A** y **Task Gate C**: Prioridad 3 (Alta).
* **Task Gate B** y **Task Gate D**: Prioridad 2 (Media).
* **Task Test**: Prioridad 1 (Baja, aunque luego modifica su propia prioridad).


* **Configuración del Hardware:** Llama a `app_it_init()` para inicializar las interrupciones y a `cycle_counter_init()` para métricas de tiempo.

### `app_it.c` - Gestión de Interrupciones (Hardware)

Este archivo se encarga de manejar los eventos asíncronos provenientes del hardware (por ejemplo, cuando un usuario presiona un botón físico).

* **`app_it_init()`:** Contiene instrucciones en ensamblador (`CPSID i` y `CPSIE i`) para deshabilitar y volver a habilitar las interrupciones a nivel global. Esto se usa comúnmente para proteger la configuración inicial de recursos compartidos.
* **`HAL_GPIO_EXTI_Callback()`:** Es una rutina de servicio de interrupción (ISR). Evalúa qué pin del microcontrolador generó la interrupción. Si el evento proviene del pin asociado al botón A (`BTN_A_PIN`), se reservó el espacio para ejecutar la lógica correspondiente (actualmente vacía en el código base).

### `freertos.c` - Funciones Hook (Callbacks del Sistema)

Contiene funciones de "enganche" (hooks) que FreeRTOS llama automáticamente en momentos específicos del ciclo de vida del sistema operativo. Son fundamentales para el monitoreo y la depuración.

* **`vApplicationIdleHook()`:** Se ejecuta cuando ninguna otra tarea tiene trabajo que hacer (estado inactivo). Aquí simplemente incrementa el contador `g_task_idle_cnt`. Es el lugar ideal para enviar el microcontrolador a un modo de bajo consumo.
* **`vApplicationTickHook()`:** Se ejecuta en cada "tick" (interrupción del temporizador del sistema) del RTOS. Actualiza el contador `g_app_tick_cnt`.
* **`vApplicationStackOverflowHook()`:** Una medida de seguridad crítica. Si el RTOS detecta que una tarea ha excedido la memoria asignada para su pila (stack), llama a esta función. El código entra en una sección crítica y detiene la ejecución (`configASSERT(0)`) para permitir al desarrollador depurar la falla.

### `task_gate_a.c` - Tarea de Control (Ejemplo)

Define el comportamiento de una de las tareas operativas de la esclusa (Compuerta A).

* **Bucle Infinito:** Como la mayoría de las tareas en un RTOS, reside en un bucle `for(;;)` que nunca termina.
* **Bloqueo y Espera:** Incrementa un contador de ejecuciones, imprime un mensaje en el registro (logger) y luego llama a `vTaskDelay(TASK_GATE_A_DEL_MAX)`. Esta función bloquea la tarea durante 2500 milisegundos, cediendo el uso del procesador a otras tareas de menor prioridad durante ese tiempo.

### `task_test.c` - Generador de Estímulos

Es una tarea diseñada para probar y excitar a las demás tareas del sistema simulando eventos (como solicitudes de apertura de puertas).

* **Manipulación de Prioridades:** Al arrancar, consulta su propia prioridad y la incrementa temporalmente (`+2ul`) usando `vTaskPrioritySet()`. Esto garantiza que tome el control inicial del sistema para configurar las pruebas antes de ser interrumpida.
* **Vectores de Prueba:** Utiliza un bloque de compilación condicional (`#if (E_TASK_TEST_X == ...)`) para seleccionar un arreglo de eventos (`e_task_test_array`). Estos eventos simulan la actividad de la esclusa (ej: `OPEN_REQUEST_A`, `DOOR_CLOSED_B`).
* **Ejecución Periódica Estricta:** A diferencia de la Tarea A, que usa un retraso relativo, la tarea de prueba utiliza un bloque `switch` para procesar los eventos simulados y luego llama a `vTaskDelayUntil()`. Esto le indica al RTOS que la tarea debe ejecutarse *exactamente* cada 5000 milisegundos absolutos, evitando la deriva temporal (jitter).

---
---

## Paso 09: Problema de Sincronización - Esclusa de Seguridad (Security Airlock)

### Configuración

**Modificaciones y lógica de diseño en el código:**

* **Sincronización Reactiva (Event-Driven):** Se eliminaron los retardos de tiempo autónomos de las tareas de control de las compuertas. En su lugar, se implementaron semáforos binarios globales de solicitud y cierre (ej. `xSem_OpenRequest_B` y `xSem_DoorClosed_B`) inicializados en 0 en `app.c`. La tarea de simulación `task_test` actúa como generadora de eventos utilizando primitivas `xSemaphoreGive()`, mientras que las tareas de las compuertas permanecen bloqueadas en `xSemaphoreTake(..., portMAX_DELAY)` esperando los estímulos físicos correspondientes.
* **Exclusión Mutua del Recurso Compartido:** El habitáculo de la esclusa se modela como un recurso de acceso único. Para garantizar la seguridad e impedir que dos puertas se abran a la vez, se implementó un Mutex denominado `xMutex_Airlock` que gestiona el derecho de apertura concurrente de todas las compuertas.
* **Control de Intertrabado y Bloqueo por Condición:** Cuando una compuerta recibe una solicitud de apertura, primero debe tomar obligatoriamente el `xMutex_Airlock`. Si otra compuerta ya se encuentra abierta, el Mutex estará ocupado y la tarea solicitante pasará a estado *Blocked* de manera segura, manteniendo su puerta físicamente trabada para prevenir la violación del aislamiento de seguridad.
* **Liberación Dinámica de la Esclusa:** Al procesarse el evento de que una compuerta se ha cerrado físicamente por completo, la tarea devuelve el token de exclusión mediante `xSemaphoreGive(xMutex_Airlock)`. Esto despierta de forma inmediata a cualquier otra compuerta que estuviera en la lista de listos esperando por el acceso a la cámara.

*Resultado terminal con (`E_TASK_TEST_X = 3`)*

```
[info]  
[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a seo-tp3_04-application: Security airlock
[info]  app is a (Source => TA149 - Sistemas Operativos Embebidos)
[info]  
[info]   Task Gate B is running - Tick [mS] = 0
[info]   Task Gate A is running - Tick [mS] = 0
[info]   Task Gate C is running - Tick [mS] = 0
[info]   Task Gate D is running - Tick [mS] = 0
[info]  
[info]   Task Test is running - Tick [mS] = 1
[info]   <=> Task Test - Priority: Task Test 3
[info]  
[info]   <=> Task Test - e_task_test_array: index 0
[info] [Simulator] Event: Door Opening Request B
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Gate B] Opening request detected. Checking airlock...
[info]  ==> [Gate B] Light: GREEN. Door OPEN. Enter the cabin.
[info]  ==> [Gate B] Waiting for the door to close completely...
[info]  
[info]   <=> Task Test - e_task_test_array: index 1
[info] [Simulator] Event: Door Opening Request C
[info]   <=> Task Test - Wait:   5000mS
[info]  ==> [Gate C] Opening request detected. Checking airlock...
[info]  
[info]   <=> Task Test - e_task_test_array: index 2
[info] [Simulator] Event: Door B closed
[info]   <=> Task Test - Wait:   5000mS
[info]  <== [Gate B] Door CLOSED and secured.
[info]  ==> [Gate C] Light: GREEN. Door OPEN. Enter the cabin.
[info]  ==> [Gate C] Waiting for the door to close completely...
[info]  
[info]   <=> Task Test - e_task_test_array: index 3
[info] [Simulator] Event: Door C closed
[info]   <=> Task Test - Wait:   5000mS
[info]  <== [Gate C] Door CLOSED and secured.

```

**Observaciones:**

1. **Validación de la Exclusión Mutua:** Se verifica experimentalmente el intertrabado de seguridad en el `index 1`. Al intentar abrir la Puerta C mientras la Puerta B aún se encuentra abierta y retiene el Mutex, el planificador (*Scheduler*) suspende de forma determinística a la tarea C. La puerta C se mantiene inactiva en el log, comprobando la preservación del aislamiento.
2. **Desbloqueo Dinámico por Liberación de Recurso:** En el `index 2`, al recibirse el evento de cierre físico de la Puerta B (`Door B closed`), la misma suelta el Mutex. Inmediatamente, la Tarea C recobra su lugar en la *Ready List*, toma el control y abre su compuerta de manera fluida (`Light: GREEN. Door OPEN.`). La contigüidad de ambos registros en el log ratifica la capacidad reactiva inmediata de FreeRTOS.
3. **Estabilidad y Prevención de Violaciones de Seguridad:** La estricta exigencia de adquirir `xMutex_Airlock` previo a cualquier apertura mecánica sincroniza la concurrencia eficientemente. Esto garantiza que la lógica del RTOS controle íntegramente las restricciones físicas del hardware, evitando condiciones de carrera que puedan exponer el habitáculo.
