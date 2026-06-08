# TP3 – Actividad 02 – 13er Proyecto p/placa NUCLEO-F103RB con FreeRTOS

## Paso 06: 

## 1. Respuesta de la IA Gemini con el prompt:

```
Analizar y explicar (en español), el funcionamiento del código fuente contenido en los archivos adjuntos: app.c , app_it.c , task_a.c , task_b.c , y freertos.c .
```

Este conjunto de archivos en lenguaje C conforma la estructura básica de una aplicación de sistemas embebidos utilizando **FreeRTOS** (un sistema operativo en tiempo real). El código está diseñado para inicializar el sistema, crear tareas concurrentes y manejar funciones de bajo nivel como las interrupciones y los eventos del sistema operativo.

A continuación, presento un análisis detallado del funcionamiento de cada archivo.

---

## 1. Archivo: `app.c` (Punto de entrada de la aplicación)

Este archivo actúa como el núcleo organizador de la aplicación. Su función principal es preparar el entorno antes de que el planificador (scheduler) de FreeRTOS tome el control.

* **Inicialización de contadores:** Pone a cero múltiples variables globales que actúan como contadores del sistema (ticks, tareas, inactividad, etc.).
* **Mensajes de registro:** Utiliza `LOGGER_INFO` para imprimir en consola que el sistema está corriendo e identifica la aplicación como un problema clásico de concurrencia: "Readers-Writers".
* **Creación de Tareas:** Llama a la API de FreeRTOS `xTaskCreate` para instanciar dos hilos de ejecución independientes: `Task A` y `Task B`.
* **Prioridad:** Ambas tareas son creadas con la misma prioridad (`tskIDLE_PRIORITY + 1ul`) y el tamaño mínimo de pila (`configMINIMAL_STACK_SIZE`).
* **Delegación:** Finalmente, invoca funciones externas para inicializar las interrupciones (`app_it_init()`) y un contador de ciclos (`cycle_counter_init()`).

---

## 2. Archivos: `task_a.c` y `task_b.c` (Hilos de ejecución)

Estos archivos contienen la lógica individual de las tareas creadas en `app.c`. Al operar bajo un RTOS, ambas tareas se implementan como bucles infinitos `for (;;)` que nunca deben retornar.

Las tareas son estructuralmente idénticas, pero difieren en sus tiempos de bloqueo:

| Característica | `task_a.c` | `task_b.c` |
| --- | --- | --- |
| **Contador Global** | Incrementa `g_task_a_cnt` en cada iteración. | Incrementa `g_task_b_cnt` en cada iteración. |
| **Mensaje a Consola** | Imprime: `==> Task A - Wait: 250mS`. | Imprime: `==> Task B - Wait: 250mS`. |
| **Tiempo de Bloqueo (Delay)** | Se bloquea llamando a `vTaskDelay` por **250 milisegundos** (`TASK_A_DEL_MAX`). | Se bloquea llamando a `vTaskDelay` por **2500 milisegundos** (`TASK_B_DEL_MAX`). |

**Nota técnica:** Existe una discrepancia menor en `task_b.c`. Aunque la macro de demora está correctamente definida para 2500ms (`2500ul`), la cadena de texto constante que se imprime en consola dice erróneamente "250mS". El uso de `vTaskDelay` es fundamental aquí, ya que permite que el procesador ejecute otras tareas mientras la tarea actual espera a que transcurra el tiempo.

---

## 3. Archivo: `app_it.c` (Manejo de Interrupciones)

Este archivo está reservado para la inicialización y el manejo de las interrupciones específicas de la aplicación.

* **Protección de recursos:** Utiliza instrucciones directamente en lenguaje ensamblador del microcontrolador para deshabilitar (`__asm("CPSID i")`) y habilitar (`__asm("CPSIE i")`) las interrupciones globales.
* **Secciones Críticas:** Estas instrucciones son típicas para crear una "sección crítica" rudimentaria, garantizando que un bloque de código pueda acceder a un recurso compartido sin ser interrumpido por un evento de hardware.

---

## 4. Archivo: `freertos.c` (Funciones Hook del SO)

Este archivo implementa los "Hooks" (ganchos) de FreeRTOS. Los hooks son funciones de "callback" que el sistema operativo llama automáticamente cuando ocurren eventos específicos a nivel de sistema.

* **`vApplicationIdleHook`:** Se ejecuta continuamente cuando el procesador está inactivo (es decir, cuando ni la `Task A`, ni la `Task B`, ni ninguna interrupción necesitan el procesador). Aquí se utiliza simplemente para incrementar un contador de inactividad (`g_task_idle_cnt`).
* **`vApplicationTickHook`:** Se dispara con cada "tick" del reloj del sistema operativo (la interrupción periódica que usa FreeRTOS para medir el tiempo). Incrementa la variable global `g_app_tick_cnt`.
* **`vApplicationStackOverflowHook`:** Es una función de seguridad fundamental. Si el RTOS detecta que una tarea excedió el límite de su memoria RAM asignada (Stack Overflow), ingresa a esta función. Aquí, se deshabilita la conmutación de contexto (`taskENTER_CRITICAL()`), se detiene la ejecución del sistema por completo (`configASSERT( 0 )`) para permitir la depuración, y se incrementa un contador de errores.

--- 
--- 
## Paso 09: Sincronización - Lectores y Escritores

### Configuración
**Modificaciones y lógica de diseño en el código:**
- **Implementación del Patrón Readers-Writers:** Se transformó el lazo asincrónico original del proyecto base en una simulación del problema clásico utilizando la estrategia de diseño del libro *"The Little Book of Semaphores"*. Se configuró a la `Task A` en el rol de **Escritor (Writer)** y a la `Task B` en el rol de **Lector (Reader)**.
- **Mecanismo de Exclusión Mutua para el Acceso al Recurso (`roomEmpty`):** Se utilizó un semáforo binario denominado `xSemaphore_RoomEmpty` para actuar como la "llave de acceso al cuarto" (sección crítica). Debido a que en FreeRTOS `xSemaphoreCreateBinary()` inicializa el token en 0 (vacío), se ejecutó un `xSemaphoreGive()` explícito en `app_init()` para dejar el recurso inicialmente disponible (equivalente a un *Semaphore(1)*). El escritor debe adquirir esta llave de forma bloqueante antes de modificar el contador global, y el primer lector que llega la toma para cerrar el cuarto a los escritores.
- **Resolución de Condiciones de Carrera (`xMutex_Readers`):** Se introdujo la variable compartida `readers_cnt` para llevar la cuenta exacta de lectores activos en la sección crítica. Dado que las operaciones de incremento (`++`) y decremento (`--`) no son atómicas a nivel del procesador ARM Cortex-M3 (conllevan ciclos de lectura, modificación y escritura en registros), se protegió dicha variable mediante un semáforo de exclusión mutua (`xMutex_Readers`). Esto evita inconsistencias de conteo ante cambios de contexto imprevistos por el scheduler.
- **Lógica de Concurrencia de Lectura (First In, Last Out):** En `task_b.c` se estructuró la lógica para habilitar lecturas simultáneas. El **primer lector** en ingresar bloquea el acceso al cuarto (`roomEmpty`), impidiendo la entrada de cualquier escritor. Los lectores subsiguientes entran de forma directa sin alterar la llave del cuarto. El **último lector** en salir decrementa el contador a 0 y es el encargado de devolver la llave (`xSemaphoreGive(xSemaphore_RoomEmpty)`), liberando el cuarto para los escritores.

*Resultado terminal*
```
[info]

[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a seo-tp3_02-application: Readers-Writers
[info]  app is a (Source => TA149 - Sistemas Operativos Embebidos)
[info]

[info]   Task B is running - Tick [mS] = 0
[info]  <- Reader (Task B) reading. Readers inside: 1
[info]

[info]   Task A is running - Tick [mS] = 0
[info]  -> Writer (Task A) writing. Counter: 1
[info]  -> Writer (Task A) writing. Counter: 2
[info]  -> Writer (Task A) writing. Counter: 3
[info]  -> Writer (Task A) writing. Counter: 4
[info]  -> Writer (Task A) writing. Counter: 5
[info]  <- Reader (Task B) reading. Readers inside: 1
[info]  -> Writer (Task A) writing. Counter: 6
[info]  -> Writer (Task A) writing. Counter: 7
[info]  -> Writer (Task A) writing. Counter: 8
[info]  -> Writer (Task A) writing. Counter: 9
[info]  -> Writer (Task A) writing. Counter: 10
[info]  <- Reader (Task B) reading. Readers inside: 1
[info]  -> Writer (Task A) writing. Counter: 11
[info]  -> Writer (Task A) writing. Counter: 12
[info]  -> Writer (Task A) writing. Counter: 13
[info]  -> Writer (Task A) writing. Counter: 14
[info]  -> Writer (Task A) writing. Counter: 15
```
**Observaciones:**
1. **Inicialización y Concurrencia Inicial:** Al arrancar el sistema (`Tick [mS] = 0`), ambas tareas se inician de forma concurrente con igual prioridad. La `Task B` (Lector) logra tomar primero el mutex, incrementa el contador de lectores a `1` y bloquea la sala tomando la llave `roomEmpty` de forma exitosa antes de que la `Task A` intente escribir.
2. **Determinismo Temporal de Ejecución (Relación 5:1):** Se observa experimentalmente un patrón donde por cada lectura de la `Task B`, la `Task A` (Escritor) ejecuta exactamente 5 escrituras seguidas. Esto responde estrictamente a la relación de tiempos configurada por las macros de delay de cada tarea en el código base:
   - El ciclo de escritura dura un total de 500ms (250ms dentro de la sección crítica con `TASK_A_DEL_MAX` y 250ms de descanso en el lazo principal).
   - El ciclo del lector tiene un delay de bloqueo de `TASK_B_DEL_MAX` fijado en 2500ms.
   - Operación: $2500\text{ ms} / 500\text{ ms} = 5$ ciclos de escritura completos por cada ventana de lectura.
3. **Validación de la Exclusión Mutua:** Se constata que los mecanismos de sincronización operan correctamente dado que en la traza de logs **no existe superposición de mensajes**. Nunca un lector interrumpe la sección crítica del escritor (la secuencia incremental de la Task A del 1 al 5, del 6 al 10, etc., jamás se ve fragmentada a la mitad). El contador `Readers inside` se mantiene estable en `1` dado que existe una única tarea lectora concurrente en el firmware de prueba, garantizando una salida y entrada limpia de la zona crítica.



