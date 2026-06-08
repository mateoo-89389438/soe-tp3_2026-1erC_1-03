# TP3 – Actividad 04 – 15to Proyecto p/placa NUCLEO-F103RB con FreeRTOS

## Paso 06: 

## 1. Respuesta de la IA Gemini con el prompt:

## Analizar y explicar (en español), el funcionamiento del código fuente contenido en los archivos adjuntos: app.c , app_it.c , task_gate_a.c , task_test.c y freertos.c.

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





# -------------------------- # ------------------------- # -------------------------- # ------------------------- # -------------------------- # ------------------------- #
# Paso 09: Problema de Sincronización - Sistema de Control de Acceso del tipo puerta esclusa

## 1. Objetivos
- Implementar un mecanismo de exclusión mutua utilizando un **Mutex de FreeRTOS** para coordinar de forma segura el acceso al habitáculo de una esclusa controlado por múltiples tareas independientes (`task_gate_a`, `task_gate_b`, `task_gate_c`, `task_gate_d`).
- Evitar que más de una compuerta se abra simultáneamente, garantizando las condiciones de seguridad e intertrabado del sistema físico.
- Analizar el comportamiento dinámico del sistema reactivo guiado por eventos frente a solicitudes concurrentes mediante logs de *Semihosting / Logger*.

## 2. Descripción del Problema y Lógica de Control
El sistema simula una esclusa de seguridad perimetral compuesta por cuatro compuertas individuales (A, B, C y D). Para resguardar la presurización o seguridad del habitáculo interno, se debe cumplir de forma mandatoria la regla de diseño: **nunca puede haber dos o más puertas abiertas en un mismo instante**.

Para solucionar este problema de acceso concurrente a un recurso común abstracto (el interior de la esclusa), se define un Mutex compartido llamado `xMutex_Airlock`. Cada una de las tareas controladoras implementa el siguiente ciclo reactivo estructurado en torno al RTOS:

1. **Espera de Estímulo:** La tarea permanece en estado *Blocked* suspendida en un semáforo binario de solicitud (por ejemplo, `xSem_OpenRequest_B`), esperando la acción de un usuario o del simulador.
2. **Verificación de Seguridad (Intertrabado):** Una vez recibido el estímulo, la tarea intenta tomar el token de exclusión mutua mediante `xSemaphoreTake(xMutex_Airlock, portMAX_DELAY)`. Si otra compuerta está abierta, la tarea se congela de forma segura en este punto, bloqueando la apertura física.
3. **Acción de Apertura:** Tras adquirir con éxito el Mutex, la puerta pasa al estado abierto (Luz Verde).
4. **Espera de Cierre Físico:** La tarea aguarda a que el sensor de fin de carrera físico emita la señal de puerta cerrada por medio de otro semáforo binario de sincronización (por ejemplo, `xSem_DoorClosed_B`).
5. **Liberación del Recurso:** Al completarse el cierre y asegurar el perímetro, la tarea devuelve el Mutex mediante `xSemaphoreGive(xMutex_Airlock)`, permitiendo que cualquier otra compuerta en espera tome el control.

## 3. Configuración del Escenario de Prueba (`E_TASK_TEST_X == 3`)
Con el propósito de evaluar la robustez del intertrabado ante una solicitud de violación concurrente, se configuró el arreglo de estímulos `e_task_test_array` dentro de `task_test.c` empleando el perfil `3`. Dicha secuencia fuerza un solapamiento directo entre las compuertas B y C:

1. `OPEN_REQUEST_B` (Se solicita la apertura de la Puerta B. La tarea toma el Mutex y abre la compuerta).
2. `OPEN_REQUEST_C` (**Punto de conflicto:** Se intenta abrir la Puerta C *mientras* la B sigue abierta. C debe quedar retenida esperando el Mutex).
3. `DOOR_CLOSED_B` (La persona termina el tránsito por la Puerta B, que se cierra físicamente y libera el Mutex).
4. `DOOR_CLOSED_C` (Se cierra finalmente la Puerta C).

## 4. Log de la Terminal (Resultado obtenido)
A continuación, se transcribe el registro de eventos capturado por la consola de depuración al ejecutar el programa bajo el perfil de conflicto controlado:

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


## 5. Análisis del Comportamiento y Conclusión

* **Exclusión Mutua Validada:** En el `index 1`, el simulador emitió la señal `Door Opening Request C`. La bitácora demuestra que `Gate C` capturó el evento e inició la verificación de la esclusa, pero el flujo se detuvo allí de manera segura. Al estar el `xMutex_Airlock` retenido por la Puerta B, la Tarea C fue puesta en estado de bloqueo por el núcleo del sistema operativo, previniendo de forma efectiva que ambas compuertas se abrieran en simultáneo.
* **Sincronización Determinista:** En el `index 2`, en el momento exacto en que la Puerta B completó su cierre (`Door B closed`) y ejecutó la devolución del Mutex, el planificador (*Scheduler*) de FreeRTOS transfirió de inmediato el recurso a la tarea pendiente de mayor prioridad en la lista de espera. Como resultado, la Puerta C se destrabó automáticamente pasando a `Light: GREEN. Door OPEN`, logrando un funcionamiento coordinado y un comportamiento predecible y determinista acorde a un sistema operativo de tiempo real.

