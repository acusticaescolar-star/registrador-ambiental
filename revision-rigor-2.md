# Segunda revisión de rigor metodológico
## Registrador Ambiental · acusticaescolar.com

Revisión previa a la difusión pública de resultados — segunda ronda.
Fecha: 5 de septiembre de 2026

Esta revisión es independiente de la primera (`REVISION_RIGOR.md`, hallazgos
1-11, todos resueltos). Los hallazgos se numeran a partir del 12 para que las
referencias cruzadas que ya existen en el código y en los PDF (`hallazgo 4`,
`hallazgo 7`...) sigan siendo inequívocas.

**Dónde ha mirado esta ronda y la anterior no.** La primera revisión cerró con
la frase «`registradorambiental.ino` (sin cambios — no contiene código
relacionado con ningún hallazgo)». Esa afirmación era prematura: el firmware es
donde empieza la cadena de medida, y siete de los once hallazgos que siguen
están ahí. La primera revisión se centró en **cómo se enuncian** los resultados;
esta se centra en **cómo se obtienen**.

---

## RESUMEN DE HALLAZGOS

| Nº | Hallazgo | Severidad |
|----|----------|-----------|
| 12 | ~~El sonómetro solo observa el 25 % del tiempo: se pierden los eventos breves~~ | **RESUELTO** |
| 13 | ~~La reconstrucción de la ponderación «Fast» usa una constante que no corresponde al ritmo de muestreo~~ | **RESUELTO** |
| 14 | ~~La referencia «exterior» de CO2 es el mínimo interior: sesga el ACH al alza~~ | **RESUELTO** |
| 15 | ~~El módulo se opera fuera de su configuración documentada: el ±2 dB declarado no está garantizado~~ | **RESUELTO** |
| 16 | ~~El ACH se calcula con dos puntos en lugar de la regresión que especifica ASTM E741~~ | **RESUELTO** |
| 17 | ~~Una lectura de CO2 = 0 durante el calentamiento se registra como dato válido~~ | **RESUELTO** |
| 18 | ~~Resultados con un decimal a partir de datos cuantizados a 1 dB~~ | **RESUELTO** |
| 19 | ~~La marca de tiempo se trunca a minutos y el orden dentro del minuto no está garantizado~~ | **RESUELTO** |
| 20 | ~~El umbral de eventos hereda el fondo del día anterior o de otra aula~~ | **RESUELTO** |
| 21 | ~~La ruta de respaldo sin muestras fabrica un intervalo de fluctuación nula~~ | **RESUELTO** |
| 22 | ~~La ventana 07:00-19:00 excluye el único decaimiento con ocupación nula garantizada~~ | **RESUELTO** |
| 23 | ~~El documento de proyecto documenta etiquetas del CSV que ningún firmware emite~~ | **RESUELTO** |

Los hallazgos 12, 13 y 15 compartían una misma causa raíz y **se han corregido
juntos con la opción A** el 5 de septiembre de 2026 (ver «Corrección aplicada»,
al final del hallazgo 15). Los hallazgos **14, 16, 17 y 18 se han corregido
después**, el mismo día. El hallazgo **23 apareció al corregir el 17** y se
corrigió a la vez. El **22 se corrigió después**, junto con la revisión del 23.
Los tres restantes —**19, 20 y 21**, de severidad BAJA— se corrigieron en último
lugar, junto con el nuevo comando `C` de verificación y calibración del sensor
de CO2 (hallazgo 8). **Los doce hallazgos de esta revisión quedan cerrados.**

Sigue abierto lo que ninguna corrección de código puede cerrar: las
verificaciones de campo del final de este documento y de la primera revisión.

---

## 12. EL SONÓMETRO SOLO OBSERVA EL 25 % DEL TIEMPO — RESUELTO

**El problema.** Tres constantes del firmware deberían ser coherentes entre sí
y no lo son:

| Línea | Constante / comentario | Dice |
|---|---|---|
| 140 | `#define DBM_TAVG_MS 31` | el promediado interno del módulo es de **31 ms** |
| 178 | `#define DB_SAMPLE_MS 125UL` | el firmware lee el módulo cada **125 ms** |
| 176-177 | comentario | «Se muestrea al mismo ritmo que el promediado interno **(125 ms)**» |
| 430 | comentario | «Configurar el promediado interno del sonómetro a **125 ms** ("Fast")» |
| 149 | `ALPHA_FAST 0.2196f` | `1 - exp(-31/125)`, es decir, calculada para un paso de **31 ms** |

El código afirma en dos comentarios que el promediado interno es de 125 ms,
pero la constante que se escribe en el módulo es 31. El resultado es que el
promediado interno (31 ms) y el intervalo de lectura (125 ms) **no coinciden**.

El fabricante documenta el registro `DECIBEL` como «*Latest sound intensity
value in decibels, averaged over the last Tavg time period*»: una media móvil
sobre los últimos Tavg milisegundos. Leyéndolo cada 125 ms con Tavg = 31 ms,
cada lectura resume los 31 ms anteriores y **los 94 ms restantes de cada ciclo
no quedan representados en ningún valor**. Se observa el 25 % del tiempo.

El comentario de las líneas 176-177 demuestra que el problema se había
identificado —«a 1 s de intervalo quedaban 875 ms sin observar»— y se corrigió
a medias: se bajó el intervalo de lectura de 1000 a 125 ms, pero no se subió el
promediado interno de 31 a 125 ms. El hueco pasó de 875 a 94 ms; no se cerró.

**Por qué importa.** Todo el proyecto se apoya en el hallazgo de BREATHE de que
lo que se asocia con el desarrollo cognitivo es la **fluctuación**, no el nivel
medio. La fluctuación se mide con eventos breves, y un evento breve tiene una
probabilidad alta de caer entero en el hueco no observado:

| Duración del evento | Probabilidad de perderlo por completo |
|---|---|
| 10 ms | 67 % |
| 30 ms | 51 % |
| 60 ms (portazo típico) | 27 % |
| 80 ms | 11 % |
| ≥ 94 ms | 0 % |

(Fórmula: `(125 − 31 − d) / 125` para d < 94 ms; comprobada con 200.000
posiciones aleatorias por duración, coincidencia con la analítica dentro del
0,2 %.)

El **LAeq** resiste mejor de lo que cabría temer, y conviene decirlo con
precisión para no exagerar el hallazgo: un submuestreo del 25 % es un estimador
prácticamente insesgado del nivel medio *cuando el ruido es estacionario*. En
1.000 realizaciones de un intervalo de 30 s con cuatro portazos de 60 ms a
85 dB sobre un fondo de 45 dB, el error medio fue de **−0,6 dB**. Pero la
**dispersión** es el problema: desviación típica de **2,8 dB**, un **29 %** de
los intervalos con más de 2 dB de error y un peor caso de **−19 dB**. Es decir:
el LAeq del día estará bien, pero el LAeq de *una franja concreta* con ruido
impulsivo puede estar muy mal, y las franjas son la unidad de análisis del
informe.

El **contador de eventos** y el **índice de fluctuación** son los que se llevan
la peor parte: en la simulación de un intervalo con 8 portazos de 60 ms, solo
5 eran observables. Un infracontaje sistemático de un tercio en el indicador
que el proyecto declara como el más relevante.

**Dónde lo afirma la documentación (y es incorrecto).** Cuatro sitios dicen que
se muestrea cada 31 ms:

- `documento-proyecto.pdf` §3.3: «El sonómetro se muestrea a 31 ms, más rápido
  que la ponderación normalizada, para no perder impulsos breves.»
- `documento-proyecto.pdf` §3.3: «dB_max es el pico con las muestras rápidas de
  31 ms.»
- `LEEME.txt`: «El sonómetro se muestrea cada 31 ms; la ponderación Fast se
  reconstruye por software con un filtro exponencial sobre la energía.»
- `puesta-en-marcha.pdf` §5: «El sonómetro se muestrea cada 31 ms y su
  promediado interno se ajusta en el arranque: verifícalo en la fase 1, debe
  indicar "promediado interno ajustado a 31 ms".»

La última es la más delicada: el protocolo de puesta en marcha pide al operador
que **confirme** el estado inconsistente y lo dé por bueno.

**Corregido.** Ver «Corrección aplicada» al final del hallazgo 15.

---

## 13. LA RECONSTRUCCIÓN «FAST» NO RECONSTRUYE UNA PONDERACIÓN FAST — RESUELTO

**El problema.** El filtro exponencial de la línea 1149

```
dbEnergiaFast += ALPHA_FAST * (energia - dbEnergiaFast);
```

usa `ALPHA_FAST = 0,2196 = 1 − exp(−31/125)`, que es el coeficiente correcto
**si el filtro se actualiza cada 31 ms**. Pero se actualiza cada 125 ms
(hallazgo 12). Aplicando un α calculado para un paso de 31 ms a un paso real de
125 ms, la constante de tiempo efectiva es

```
τ_efectiva = −125 / ln(1 − 0,2196) = 504 ms
```

**504 ms, no 125 ms.** Cuatro veces más lenta que «Fast», y más cerca de «Slow»
(1 s) que de la ponderación que dice reconstruir.

**Por qué importa.** `dB_maxF` es, según la propia documentación, «el
comparable con un sonómetro comercial y con los umbrales de la norma» y el
valor «sobre el que se evalúan las alertas acústicas». Es el único indicador
acústico que el proyecto presenta como normativamente comparable, y no lo es.

En la simulación (fondo 45 dB, 8 portazos de 60 ms a 80 dB en 30 s), el máximo
con ponderación Fast real —filtro τ = 125 ms integrando de forma continua a
1 ms— fue de **75,9 dB**; el que produce el firmware, **73,0 dB**: **−2,9 dB**.
El error no es constante: depende de la forma temporal del ruido, de modo que
no puede corregirse a posteriori con un offset.

Un matiz para no sobredimensionar el hallazgo: la atenuación de un impulso
aislado que cita la documentación —«un golpe de 31 ms se atenúa ~6,6 dB al
aplicar Fast»— sí es aritméticamente correcta, porque `10·log10(0,2196) =
−6,58 dB` y ese número depende solo de α. Lo que está mal no es el ataque del
filtro sino su **relajación**: tras un pico, el nivel reconstruido cae cuatro
veces más despacio de lo que caería en un sonómetro en modo Fast.

**Corregido.** Ver «Corrección aplicada» al final del hallazgo 15.

---

## 14. LA REFERENCIA «EXTERIOR» DE CO2 ES EL MÍNIMO INTERIOR — RESUELTO

**El problema.** `volcar_datos.ps1`, líneas 1159-1167 (y 1166-1170 para la
síntesis diaria):

```powershell
# Nivel exterior de referencia: el mínimo registrado sin ocupación.
if ($minsVacias.Count -gt 0) { $cExt = ($minsVacias | Measure-Object -Minimum).Minimum }
else { $cExt = ($fCO2 | Measure-Object -Property co2 -Minimum).Minimum }
```

`cExt` —la concentración exterior que entra en la fórmula de decaimiento y en
el criterio diferencial del RITE— es **el valor de CO2 más bajo medido dentro
del aula**. No procede de ninguna medida exterior. El script no tiene ninguna
noción de una segunda unidad: `grep -ci "unidad exterior" volcar_datos.ps1`
devuelve 0.

**Por qué importa — primer efecto: el ACH sale siempre alto.** El interior casi
nunca baja hasta el exterior real, así que `cExt` está sistemáticamente por
encima del valor verdadero. En la fórmula `ACH = ln((C0−Cext)/(Ct−Cext))/t`,
sobrestimar Cext encoge el denominador más deprisa que el numerador y el
resultado sube. Con un decaimiento real de 1150 → 780 ppm en 40 min y un
exterior real de 420 ppm (ACH = 1,06):

| `cExt` empleado | ACH informado | Desviación |
|---|---|---|
| 420 ppm (exterior real) | 1,06 | — |
| 480 ppm | 1,21 | +14 % |
| 520 ppm | 1,33 | +25 % |
| 560 ppm | 1,48 | +40 % |
| 620 ppm | 1,80 | **+69 %** |

El sesgo va **siempre** en la dirección de hacer parecer el aula mejor
ventilada de lo que está. Es la dirección peligrosa: un aula por debajo del
mínimo recomendado puede quedar informada como si estuviera más cerca de
cumplirlo. La banda de incertidumbre del hallazgo 4 no protege de esto, porque
es un sesgo, no un error aleatorio: la banda entera se desplaza con él.

**Por qué importa — segundo efecto: se descarta el mejor decaimiento.** Como
`cExt` es el mínimo de las franjas vacías y `Ct` es la última lectura de cada
decaimiento, la franja que mejor ventila define `cExt` con su propio valor
final. Entonces `Ct ≤ cExt` y `CalcularACH` devuelve `$null` (línea 333 del
script; el mismo filtro está en la línea 364 de `CalcularACHBanda`). Ejecutado
con las funciones reales del script:

```
   cExt = mínimo de las franjas vacías = 520 ppm  (procede de 'Tarde vacía')

   Recreo        1150 -> 780 ppm   ACH = 1.33
   Comedor       1050 -> 690 ppm   ACH = 1.71
   Tarde vacía    980 -> 520 ppm   ACH = DESCARTADO (ct <= cExt)
```

**La franja mejor ventilada del día es la única que no obtiene ACH.** El
informe se queda con las peores y no avisa de la exclusión.

**Por qué importa — tercer efecto: la documentación afirma lo contrario.**
`documento-proyecto.pdf` §7.1, recuadro «Criterio diferencial»:

> «Al medir simultáneamente dentro y fuera, este sistema permite aplicar el
> criterio normativo correcto —el diferencial—, algo que los medidores
> comerciales de una sola unidad no pueden hacer.»

Esa frase describe la ventaja competitiva del diseño frente a un medidor
comercial. La configuración de despliegue (§1.3) contempla una «unidad
exterior», y el firmware puede desde luego instalarse en dos equipos — pero
**el análisis no combina dos series**. El criterio diferencial del RITE
(línea 1174: `$umbralIDA2 = $cExt + 500`) se aplica contra el mínimo interior.
Con una sola unidad, el sistema hace exactamente lo que la frase dice que los
medidores de una sola unidad no pueden hacer.

**Acción recomendada.**
1. Distinguir en el informe dos casos y no llamarlos igual: `cExt` **medido**
   (segunda unidad, o una lectura exterior anotada por el operador al instalar)
   y `cExt` **estimado** (mínimo interior). En el segundo caso, etiquetar el
   ACH y el umbral IDA 2 como estimaciones con sesgo conocido al alza y
   declarar la dirección del sesgo.
2. Permitir introducir la concentración exterior como dato: una lectura tomada
   al aire libre durante la verificación ABC del hallazgo 8 ya está prevista en
   el protocolo y sirve exactamente para esto, sin hardware adicional.
3. Excluir del cálculo de `cExt` la franja cuyo ACH se está evaluando, o
   calcular `cExt` con el mínimo de los **otros** días de la campaña, para que
   el mejor decaimiento deje de autoexcluirse.
4. Corregir la frase de §7.1 mientras el análisis no combine dos unidades.

---

## 15. EL MÓDULO SE OPERA FUERA DE SU CONFIGURACIÓN DOCUMENTADA — RESUELTO

**El problema.** El manual de programación del módulo documenta dos modos:
1000 ms («slow», el valor por defecto) y 125 ms («fast»). No documenta ni
caracteriza valores por debajo de 125 ms. El firmware escribe 31 ms.

El registro **acepta** el valor —el firmware lo comprueba releyéndolo, líneas
435-438— pero que un registro admita un valor no significa que el fabricante
haya caracterizado el módulo en esa configuración.

**Por qué importa.** La tabla de incertidumbres del hallazgo 4, que ahora está
en `documento-proyecto.pdf` §7.2 y sostiene todo el discurso de márgenes de
error del proyecto, atribuye al sonómetro **±2 dB SPL**. Esa cifra procede de
la hoja de características, es decir, del módulo funcionando en su
configuración documentada. A 31 ms de promediado, con la cuarta parte de
muestras de audio por valor, el ruido de estimación del propio módulo es
previsiblemente mayor, y no hay ninguna especificación del fabricante que lo
acote. El proyecto declara una incertidumbre que no le ha sido garantizada para
el modo en que usa el instrumento.

Esto interactúa con la primera revisión: el umbral de 6 dB adoptado en el
hallazgo 4 para afirmar una «variación real» entre franjas se justificó como
«por encima de la incertidumbre del sonómetro (±2 dB) con margen». Si el ±2 dB
no aplica, la justificación del umbral queda sin base numérica (el umbral
probablemente siga siendo prudente, pero ya no está fundamentado).

### Corrección aplicada (5 de septiembre de 2026) — opción A

**Decisión tomada: opción A.** Se abandona la medición de impulsividad.

Cambios en `registrador-ambiental.ino`:
- `DBM_TAVG_MS` pasa de **31 a 125 ms**, con lo que el promediado interno y el
  intervalo de lectura (`DB_SAMPLE_MS`) coinciden: cada lectura resume
  exactamente los 125 ms transcurridos desde la anterior y **no queda ningún
  tramo sin observar**. El módulo queda además en su modo «fast» documentado.
- Se eliminan `DBM_TAU_FAST_MS`, `ALPHA_FAST` y el filtro exponencial software
  (variables `dbEnergiaFast`, `dbMaxFast`, `dbFastIniciado`): ya no hace falta
  reconstruir nada, porque el promediado de 125 ms lo hace el propio módulo.
- Desaparece la columna `dB_max` del CSV, que pasa de 10 a **9 columnas**. El
  pico del intervalo es ahora `dB_maxF`, medido sobre el promediado de 125 ms.
- Se reescriben los comentarios que se contradecían entre sí (líneas 140,
  176-177 y 430 del fichero original), que eran la causa raíz del hallazgo, y
  se añade una nota `PONDERACIÓN TEMPORAL` en la cabecera que deja constancia
  de que ambas constantes deben cambiarse siempre juntas.

Cambios en `volcar_datos.ps1`:
- El lector admite **los dos formatos**: 9 columnas (firmware actual) y 10
  (anterior). En el formato nuevo se hace `max = maxF`, de modo que el rango
  dinámico y el resto del análisis siguen funcionando sin cambios.
- Si el volcado contiene registros del formato antiguo, el informe abre con un
  **aviso destacado** indicando qué porcentaje son, que su recuento de eventos
  está infraestimado y que no deben mezclarse con los nuevos para comparar
  fluctuación entre espacios.

Cambios en la documentación: `LEEME.txt` (formato del CSV, sección de
impulsividad retirada, muestreo), `documento-proyecto.pdf` (§3.3 y la fila F09
de la tabla de funciones; v4.2 → **v4.3**) y `puesta-en-marcha.pdf` (§5.4 y el
párrafo del análisis automático; v1.2 → **v1.3**). En particular, el protocolo
de puesta en marcha ya no pide confirmar «promediado interno ajustado a 31 ms»
sino **125 ms**.

**Matiz que aparece al implementar la corrección.** Al consultar el manual del
fabricante para redactar los textos se comprobó que, para las versiones con
micrófono MEMS —la de este proyecto—, **no se documenta el tipo de promediado**
(solo la variante con micrófono externo se describe como exponencial) y **no se
declara conformidad con IEC 61672**. Por eso `dB_maxF` no se describe ya como
«comparable con un sonómetro comercial y con los umbrales de la norma», sino
como «la mejor aproximación disponible a la ponderación temporal Fast», con la
salvedad explícita. La opción A resuelve el hueco de muestreo y devuelve el
módulo a su configuración caracterizada, pero no convierte el equipo en un
sonómetro normalizado: eso sigue dependiendo de la verificación con un
sonómetro de clase 2 que la primera revisión ya dejó pendiente.

**Verificación realizada.**
- El firmware pasa una comprobación de sintaxis con `g++ -fsyntax-only` sobre
  el fichero completo con la API de Arduino simulada (0 errores).
- Se comprobó que no queda ninguna referencia huérfana a los identificadores
  eliminados y que las **tres cabeceras** que el firmware escribe en el CSV
  coinciden ahora con los 9 campos del `snprintf` (una comprobación que detectó
  tres cabeceras que se habían quedado con el formato antiguo).
- El script pasa la validación del analizador de PowerShell y se probó el
  lector con un CSV mixto de filas de 9 y 10 columnas: los campos se asignan
  correctamente en ambos y el rango dinámico sigue calculándose igual.
- En los dos PDF se verificó que **la capa de texto coincide con lo que se ve**
  (numeración, versión y textos corregidos), comprobación que reveló que los
  parches de la entrega anterior tapaban el texto con una imagen sin eliminar
  el subyacente; ambos documentos se han rehecho desde los originales con
  redacción real.

### Cómo se corrigen 12, 13 y 15 de una vez (análisis previo a la decisión)

Los tres son la misma incoherencia entre `DBM_TAVG_MS`, `DB_SAMPLE_MS` y el `dt`
con que se calculó `ALPHA_FAST`. Hay dos formas coherentes de dejarlo, y son
excluyentes:

**Opción A — `TAVG = 125 ms`, lectura cada 125 ms.** El registro se lee una vez
por cada ventana de promediado: cobertura del 100 % del tiempo, sin huecos. El
módulo queda en su modo «fast» documentado, con lo que el ±2 dB vuelve a estar
respaldado. `dB_maxF` pasa a ser un nivel con ponderación Fast **real**, medido
por el propio módulo, y el filtro software `ALPHA_FAST` **desaparece** junto con
el hallazgo 13. Coste: se pierde `dB_max` como pico de 31 ms, y con él el
indicador de impulsividad `dB_max − dB_maxF`. Es la opción metrológicamente
defendible.

**Opción B — `TAVG = 31 ms`, lectura cada 31 ms.** Cobertura del 100 % también,
y `ALPHA_FAST = 0,2196` pasa a ser **correcta** sin tocarla, porque el paso real
del filtro sería entonces 31 ms. Se conservan los dos picos y el indicador de
impulsividad, que es lo distintivo del proyecto. Coste: cuadruplica el tráfico
I²C y el consumo asociado (habría que rehacer la estimación de autonomía), y
deja abierto el hallazgo 15, porque el módulo sigue fuera de su rango
documentado.

**Recomendación.** Para lo que se vaya a publicar o a presentar a un centro,
opción A. Si el indicador de impulsividad se considera irrenunciable, opción B,
pero entonces hay que (a) verificar empíricamente el ±2 dB a 31 ms contra el
sonómetro de clase 2 —la verificación 2 de la lista pendiente ya prevista, que
pasaría de recomendable a **bloqueante**— y (b) etiquetar `dB_max` y la
impulsividad como indicadores exploratorios, no como magnitudes comparables con
norma.

En cualquiera de los dos casos, corregir las cuatro afirmaciones de la
documentación que dicen «se muestrea cada 31 ms» y el comentario de la línea 430.

### ¿Cuánto aleja la opción A del marco BREATHE?

Es la objeción natural, porque la opción A sacrifica el pico de 31 ms y con él
el indicador de impulsividad, y todo el encuadre del proyecto se apoya en
BREATHE. La respuesta, simulando los tres escenarios con la misma señal
(5 minutos de aula, contador de eventos con la lógica exacta del firmware):

**Escenario 1 — solo ruido de tráfico (lo que midió BREATHE):** las pasadas de
vehículos duran segundos, muy por encima de los 94 ms del hueco. Las cuatro
configuraciones dan **exactamente el mismo resultado**:

| configuración | LAeq | fondo | eventos | índice fluct. |
|---|---|---|---|---|
| referencia (Fast real continuo) | 53,8 | 45,1 | 16 | 86,4 % |
| actual (31/125) | 53,8 | 45,0 | 16 | 86,7 % |
| **opción A (125/125)** | **53,8** | **45,0** | **16** | **86,7 %** |
| opción B (31/31) | 53,8 | 45,0 | 16 | 86,7 % |

**Todo lo que BREATHE midió se conserva intacto con la opción A.** No hay
alejamiento: hay identidad.

**Escenario 2 — solo impulsos internos del aula (lo que el proyecto añade por
su cuenta):** aquí la configuración actual es la peor de las tres.

| configuración | eventos | desviación |
|---|---|---|
| referencia | 38 | — |
| actual (31/125) | 31 | **−18 %** |
| opción A (125/125) | 40 | +5 % |
| opción B (31/31) | 40 | +5 % |

La opción A **mejora** el contador de eventos respecto a lo que hay hoy. Es
contraintuitivo pero tiene una explicación simple: promediando 125 ms sin
huecos, un portazo de 60 ms sigue elevando la media de su ventana muy por
encima del umbral de «fondo + 10 dB», mientras que hoy ese mismo portazo cae
entero en el hueco no observado el 27 % de las veces.

**Lo único que la opción A degrada de verdad** es la *amplitud* con que se
registra un impulso corto, no su detección:

| duración del impulso (85 dB) | con TAVG 31 ms | con TAVG 125 ms | pérdida |
|---|---|---|---|
| 10 ms | 80,1 dB | 74,0 dB | −6,1 dB |
| 30 ms | 84,9 dB | 78,8 dB | −6,1 dB |
| 60 ms | 85,0 dB | 81,8 dB | −3,2 dB |
| 100 ms | 85,0 dB | 84,0 dB | −1,0 dB |
| ≥ 125 ms | 85,0 dB | 85,0 dB | 0 dB |

**Conclusión.** La opción A no aleja del marco BREATHE: el nivel, el fondo, el
número de eventos y el índice de fluctuación quedan igual o mejor. Lo que se
pierde es la capacidad de decir **cuán seco** es un impulso —el indicador
`dB_max − dB_maxF`—, que nunca fue una magnitud de BREATHE, sino una aportación
propia del proyecto, y que la documentación ya etiqueta como cálculo propio
inspirado en el estudio y no como réplica (hallazgo 9 de la primera revisión).

Dicho de otro modo: la decisión entre A y B **no debe tomarse por fidelidad a
BREATHE**, porque en ese terreno son equivalentes. Debe tomarse por si se quiere
conservar el descriptor de impulsividad como línea propia de investigación
—entonces B, asumiendo el hallazgo 15— o si se prioriza que cada cifra que
salga del equipo esté respaldada por la especificación del fabricante
—entonces A.

*(Advertencia sobre estas cifras: el recuento de eventos de la fila
«referencia» depende de la convención con que se cuenten eventos sobre una
señal continua, de modo que las desviaciones del ±5-12 % están dentro del ruido
de esa convención. Lo robusto es lo cualitativo: los eventos de escala
«tráfico» son idénticos en las cuatro configuraciones, y los impulsos breves
solo los infracuenta la configuración actual.)*

---

## 16. EL ACH USA DOS PUNTOS DONDE ASTM E741 ESPECIFICA UNA REGRESIÓN — RESUELTO

**El problema.** `volcar_datos.ps1`, líneas 1193-1195:

```powershell
$c0   = $ff[0].co2
$ct   = $ff[$ff.Count-1].co2
$mins = $ff[$ff.Count-1].minutos - $ff[0].minutos
```

De una franja vacía de 30 minutos registrada cada 30 s —**60 lecturas**— el
cálculo usa **dos**: la primera y la última. Las 58 intermedias se descartan.

ASTM E741 no calcula así el decaimiento: ajusta `ln(C − Cext)` frente al tiempo
por **regresión lineal** sobre toda la serie, y la pendiente es la tasa de
renovación.

**Por qué importa.**
1. **Precisión.** Con ±40 ppm ±3 % por lectura, el resultado depende
   íntegramente del ruido de dos medidas concretas. Una regresión sobre 60
   puntos reduce la componente aleatoria aproximadamente en √n. La banda de
   incertidumbre que añadió el hallazgo 4 **cuantifica correctamente** ese
   problema, pero lo trata como algo inevitable cuando es una consecuencia del
   método elegido: la banda es el síntoma, la regresión es la cura.
2. **Verificabilidad del hallazgo 7.** El hallazgo 7 de la revisión anterior se
   resolvió *declarando* el supuesto de mezcla homogénea. Una regresión lo hace
   además **comprobable**: si el aire está bien mezclado, `ln(C − Cext)` es una
   recta y el R² es alto; si hay estratificación o cortocircuito de
   ventilación, la curva se aparta y el R² lo delata. Con dos puntos, el
   supuesto es indemostrable por construcción — cualesquiera dos puntos definen
   siempre una recta perfecta.
3. **Robustez.** Un único valor anómalo en el primer o el último registro
   (alguien que abre la puerta al entrar a recoger algo) desplaza el resultado
   entero sin dejar rastro.

**Acción recomendada.** Sustituir el cálculo de dos puntos por una regresión de
`ln(C − Cext)` sobre el tiempo en toda la franja, informar la pendiente como
ACH, el intervalo de confianza de la pendiente como banda (sustituyendo a la
aritmética de intervalos actual, que quedaría redundante) y el **R² como
indicador de cumplimiento del supuesto de mezcla**, con un aviso cuando baje de,
por ejemplo, 0,95. Es un cambio de unas 15 líneas que cierra a la vez la
precisión del hallazgo 4 y la verificabilidad del hallazgo 7.

---

## 17. UNA LECTURA DE CO2 = 0 SE REGISTRA COMO DATO VÁLIDO — RESUELTO

**El problema.** `registrador-ambiental.ino`, líneas 734-739:

```cpp
if (s8_ok) {
    int16_t r = sensor_S8->get_co2();
    if (r > 0) co2 = r;
    else if (r < 0) errCO2 = true;
    // r == 0 durante el calentamiento inicial: no se marca como error
}
```

Si `r == 0`, `co2` conserva su valor inicial **0** y **no se marca error**. La
fila se escribe con `co2_ppm = 0` y `estado = OK`.

Lo llamativo es que el propio firmware explica, en su cabecera (líneas
160-162), por qué esto no debe ocurrir:

> «El campo "estado" del CSV solo informa de la FIABILIDAD del dato, que NO es
> derivable de los valores: **si una lectura de CO2 vale 0, sin la marca no
> habría forma de saber si fue un fallo del sensor o una medida real.**»

Es exactamente el caso que el comentario describe, y es el que queda sin marcar.
El fabricante indica además que la lectura «is only valid after about 1 second
of module power-up», y el firmware ya avisa por consola de que el S8 «tarda ~30
s en responder» (línea 477): la ventana de ceros no es hipotética.

**Alcance real.** El script **sí** filtra los ceros: `$_.co2 -gt 0` en las
líneas 537, 758 y 1154. Las estadísticas del informe no están contaminadas. El
problema queda en tres sitios:

1. El CSV es un entregable por derecho propio —la documentación insiste en que
   «se abre en Excel con las columnas separadas»— y quien lo analice por su
   cuenta verá ceros etiquetados como fiables.
2. El recuento de «Registros con lecturas no fiables» del informe se hace con
   `$_.estado -ne "OK"` (línea 1394), de modo que estas filas **no se cuentan**
   y el informe subestima los registros no utilizables.
3. Contradice la regla de diseño que el propio firmware declara, que es lo que
   un revisor externo señalaría.

**Acción recomendada.** Marcar `ERR_CO2` también cuando `r == 0`, o añadir una
marca propia (`CO2_CALENTANDO`) si se quiere distinguir el calentamiento de un
fallo. Un renglón de código.

---

## 18. RESULTADOS CON UN DECIMAL A PARTIR DE DATOS CUANTIZADOS A 1 dB — RESUELTO

**El problema.** El firmware escribe los cuatro niveles acústicos como enteros
(`%d`, línea 783) porque los almacena en `uint8_t` y redondea el LAeq con
`lround` (línea 746). El CSV no tiene decimales en dB. El informe, en cambio,
presenta el reparto exógeno/endógeno con un decimal y el peso del exterior con
porcentaje (líneas 1046 y 1108):

```
Lectivo mañana   total 55.0 dB = actividad 54.9 dB + exterior 38.0 dB (2 %)
```

**Por qué importa.** La resta energética amplifica el redondeo de forma no
lineal cuando los dos niveles están próximos. Para un aula cuyos valores reales
fueran 55,4 dB de total y 53,6 dB de referencia exterior (66 % exterior):

| total | exterior | actividad | % exterior |
|---|---|---|---|
| 55,4 | 53,6 | 50,7 | **66 %** (valores reales) |
| 55 | 54 | 48,1 | **79 %** |
| 55 | 53 | 50,7 | **63 %** |
| 56 | 54 | 51,7 | **63 %** |

La misma aula se informa con un 63 %, un 66 % o un 79 % de ruido exterior según
dónde caiga el redondeo de 1 dB — y eso **antes** de aplicar la incertidumbre
de ±2 dB del sensor. El reparto se presenta sin banda y con una cifra decimal
que sugiere una resolución que el dato no tiene.

Es el mismo problema que resolvió el hallazgo 4 para el ACH, aplicado a la otra
magnitud derivada del sistema. La corrección de aquel hallazgo elevó a 6 dB el
umbral para **comparar franjas entre sí**, pero el reparto exógeno/endógeno
dentro de una franja siguió presentándose con precisión fina.

**Acción recomendada.** Dos opciones, compatibles entre sí: (a) registrar el
LAeq con un decimal en el firmware —el acumulador ya es `double`, solo se pierde
en el `lround` de la línea 746 y en el `uint8_t`—, y (b) presentar el reparto
con banda, o redondeado a tramos («mayoritariamente exterior» / «mixto» /
«mayoritariamente actividad»), y omitirlo cuando la diferencia entre total y
referencia sea menor que la incertidumbre.

---

## CORRECCIONES APLICADAS A 14, 16, 17, 18 Y 23 (5 de septiembre de 2026)

Los cuatro hallazgos se corrigieron en la misma tanda porque se tocan entre sí:
la referencia exterior (14) es un dato de entrada de la regresión del ACH (16),
y el ceros de calentamiento (17) contaminan esa misma serie de CO2.

### 14 — Referencia exterior de CO2

- **Nuevo campo `CEXT` al identificar el espacio**, en firmware y script:
  `E Aula 3B / calle / RT 0.85 / CEXT 420`. Es el CO2 medido al aire libre junto
  al aula. El protocolo lo encadena con la verificación de calibración ABC
  (hallazgo 8 de la primera revisión), donde el equipo ya está fuera: es el
  mismo dato, sin trabajo adicional.
- El firmware avisa por consola cuando se identifica un espacio **sin** CEXT y
  explica la consecuencia.
- El informe distingue ahora **«CO2 exterior MEDIDO»** de **«CO2 exterior
  ESTIMADO»**, y en el segundo caso declara la dirección del sesgo: la
  referencia queda por encima del valor real, el ACH sale alto y el aula parece
  mejor ventilada de lo que está. El aviso se repite junto al ACH y junto al
  criterio RITE.
- **La franja evaluada se excluye del cálculo de la referencia estimada**, de
  modo que la franja mejor ventilada deja de autoexcluirse. Comprobado con las
  funciones reales del script: en el caso del ejemplo del hallazgo, la franja
  que antes salía «DESCARTADO» ahora obtiene ACH.
- Corregido también el recuadro de §7.1 del documento de proyecto, que afirmaba
  que el sistema mide «simultáneamente dentro y fuera».

### 16 — ACH por regresión

- Nueva función `CalcularACHRegresion`: ajusta `ln(C − Cext)` frente al tiempo
  sobre **todas** las lecturas de la franja, como especifica ASTM E741, y
  devuelve la pendiente (ACH), el intervalo de confianza de la pendiente, el
  **R²** y el número de lecturas usadas.
- La banda ya no es aritmética de intervalos: combina la incertidumbre
  **estadística** del ajuste con la **sistemática** de la referencia exterior
  (que no se promedia con más lecturas, porque desplaza toda la curva).
- El ajuste se detiene cuando el exceso sobre el exterior baja del mayor de
  «2 × la incertidumbre del sensor» y «el 10 % del exceso inicial»: por debajo
  de ese suelo la diferencia es ruido y su logaritmo hundiría el R² sin que el
  aula tenga ningún problema. Es la práctica habitual en ensayos de decaimiento
  de trazador, y sin ese corte el aviso de R² saltaba en falso.
- **El R² hace comprobable el supuesto de mezcla homogénea** (hallazgo 7 de la
  primera revisión), que hasta ahora solo se declaraba. El informe avisa por
  debajo de 0,95.

**Verificación:** con 200 decaimientos sintéticos de ACH real 2,0 y ruido de
±15 ppm por lectura, el método de dos puntos daba una desviación típica de
**0,061** y la regresión **0,015** — cuatro veces más preciso, sin sesgo en
ninguno de los dos. Un decaimiento partido en dos tramos (estratificación
simulada) da R² = 0,81 y dispara el aviso; uno exponencial limpio, R² = 0,995.

### 17 — CO2 = 0 durante el calentamiento

Nueva marca **`CO2_CALENTANDO`** en el campo `estado`. La fila deja de salir
como «OK» con 0 ppm, el recuento de registros no fiables del informe la cuenta,
y quien abra el CSV en Excel la ve marcada. Un renglón de código, como se
anticipaba.

### 18 — Precisión declarada

- El firmware guarda el **LAeq con un decimal**. Las muestras del módulo son
  enteras, pero el promedio energético de ~240 recupera resolución por debajo
  del dB, y es justo la que necesita la resta exógeno/endógeno. El fondo y el
  pico siguen siendo enteros por construcción (histograma de 1 dB y máximo de
  muestras enteras): fingir decimales ahí sería el error contrario.
- El reparto exógeno/endógeno se presenta **como banda**, no como cifra: se
  evalúan los dos extremos razonables perturbando total y referencia en ±2 dB.
  Según lo que la banda permita afirmar, el informe da un rango numérico, una
  etiqueta cualitativa («mayoritariamente exterior/actividad») o dice
  expresamente que **no es separable** cuando el total no supera a la referencia
  por encima de la incertidumbre.

**Verificación:** barrido de diferencias de 1 a 20 dB. Con 1-2 dB el informe no
reparte nada; con 3-10 dB da una etiqueta cualitativa y el rango; a partir de
14 dB da la cifra con una banda estrecha (2-10 %).

### 23 — Etiquetas inexistentes en el documento de proyecto

Apareció al corregir el 17: §3.3 del documento de proyecto tenía una tabla
titulada «Marcas del campo de estado (fiabilidad del dato)» que listaba
`T_BAJA`, `HR_BAJA`, `CO2_DEFIC`, `DB_ELEV`… **etiquetas que ningún firmware de
este proyecto escribe en el CSV**. El firmware solo emite marcas de fiabilidad
(`ERR_*`, `CO2_CALENTANDO`, `FLASH_BAJA`), y el propio código explica por qué
se eliminaron las de condición: «serían redundantes, T_ALTA se deduce de
temp_C». Los umbrales que la tabla mostraba sí existen, pero viven en
`umbrales.txt` y los aplica el análisis.

La tabla se ha retitulado **«Categorías de valoración que aplica el análisis (no
se escriben en el CSV)»** y su contenido se ha reformulado como rangos, sin
nombres de etiqueta inventados. La tabla siguiente, «Etiquetas de fiabilidad del
dato», que sí es correcta, incorpora ahora `CO2_CALENTANDO`.

### Verificación del conjunto

- Firmware: `g++ -fsyntax-only` sobre el fichero completo con la API de Arduino
  simulada, 0 errores; cabecera del CSV y `snprintf` coherentes (9 columnas,
  LAeq con `%.1f`).
- Script: validado con el analizador de PowerShell y ejecutado **de extremo a
  extremo** contra una jornada lectiva sintética más un sábado de referencia
  (2.880 registros). El informe se genera completo y el ACH recuperado (2,2)
  coincide con el que se usó para generar los datos, con R² de 0,996.
- Un fallo real detectado por esa prueba y corregido: en PowerShell la coma liga
  más fuerte que la resta, de modo que `@($cext - $u, $cext + $u)` se
  interpretaba como `$cext - ($u, $cext) + $u` y fallaba en ejecución. La
  comprobación de sintaxis no lo detecta; solo la prueba funcional.
- PDF: verificado que la capa de texto coincide con lo que se ve, sin restos de
  la versión anterior. `documento-proyecto.pdf` v4.4 (19 páginas),
  `puesta-en-marcha.pdf` v1.4 (13 páginas).

---

## 19. LA MARCA DE TIEMPO SE TRUNCA A MINUTOS — RESUELTO

`volcar_datos.ps1`, línea 664:

```powershell
minutos = [int]$c[0].Substring(11,2)*60 + [int]$c[0].Substring(14,2)
```

El campo `minutos` toma hora y minuto y **descarta los segundos**. Con registro
cada 30 s, **dos filas de cada minuto comparten el mismo valor**. Consecuencias:

- La duración del decaimiento del ACH (`$mins`, línea 1195) se cuantiza a
  minutos enteros: hasta ±1 minuto sobre una ventana de 15, un 7 % de error
  directo sobre el ACH.
- `Sort-Object minutos` (línea 1191) no es estable por defecto en PowerShell
  —para eso existe el modificador `-Stable`—, de modo que con claves repetidas
  el orden dentro del minuto no está garantizado: `$ff[0]` y `$ff[último]`
  pueden no ser la primera y la última lectura reales del decaimiento.

Con el cálculo de dos puntos del hallazgo 16, esto entra directamente en el
resultado. Con una regresión, dejaría de importar casi por completo — otra
razón para hacer ese cambio.

**Acción recomendada.** Usar segundos desde medianoche (o el `DateTime`
completo) en lugar de minutos enteros, y añadir `-Stable` a la ordenación.

**Corregido (5 de septiembre de 2026).**

- Cada fila lleva ahora un campo `segundos` (segundos desde medianoche) además
  de `minutos`. El reparto por franjas sigue usando `minutos` —las franjas se
  declaran al minuto y ahí los segundos no aportan nada—, pero el cálculo de
  renovación de aire ordena y mide en segundos.
- `CalcularACHRegresion` ordena con `Sort-Object segundos -Stable`, toma `$t0`
  en segundos y calcula cada abscisa como `($r.segundos - $t0) / 3600`. Igual en
  el bucle de la componente sistemática y en la serie de temperatura.

**Verificación, y una corrección al propio hallazgo.** Las dos consecuencias que
enunciaba el hallazgo no pesan lo mismo, y conviene decirlo con números en vez
de darlas ambas por buenas:

- *El error del 7 % sobre el ACH no se materializa.* En 500 decaimientos
  sintéticos de 30 min con lecturas cada 30 s (ACH real 2,2, ruido de 8 ppm), el
  ajuste con abscisas en segundos da media 2,2016 y desviación 0,0216; con
  abscisas truncadas a minutos enteros, media 2,1998 y desviación 0,0215. La
  diferencia es indistinguible. La razón es que la pendiente de una regresión no
  depende de la duración total sino del conjunto de puntos, y el truncado
  desplaza todas las abscisas casi por igual. Ese 7 % era real con el estimador
  de dos puntos del hallazgo 16, que ya no existe: el hallazgo 19 fue redactado
  antes de aplicar el 16 y arrastró su aritmética.
- *El problema de ordenación sí era real.* Comprobado en PowerShell 7.4:
  ordenando diez elementos con claves repetidas de dos en dos,
  `Sort-Object minutos` devuelve `2,1,3,6,5,4,7,10,9,8` —las parejas salen
  invertidas—, mientras que `Sort-Object segundos -Stable` devuelve
  `1,2,...,10`. Es decir, `$ord[0]` y `$ord[último]` **no eran** la primera y la
  última lectura del decaimiento, y de ellos salen las cifras `C0` y `Ct` que el
  informe imprime. Eso queda corregido.
- La duración informada deja de cuantizarse: la franja nocturna del conjunto de
  prueba pasa a informarse con su duración real.

La prueba de integración completa (dos días sintéticos, 2.038 filas, franja
nocturna incluida) recupera ACH 2,2 con R² 0,996 frente al 2,2 con que se
generaron los datos.

---

## 20. EL UMBRAL DE EVENTOS HEREDA EL FONDO DE OTRO DÍA O DE OTRA AULA — RESUELTO

`dbFondoPrevio` (línea 218) guarda el nivel de fondo del intervalo anterior y
sirve de umbral para contar eventos (líneas 1156-1159). `reiniciarAcumuladoresDb()`
(líneas 226-231) **no lo reinicia**, y es lo correcto dentro de una jornada:
mantiene la referencia entre intervalos consecutivos.

Pero tampoco se reinicia en los dos momentos en que la referencia deja de ser
válida:

- **Al entrar en la ventana horaria cada mañana** (línea 1127): el umbral de
  eventos de los primeros 30 s del día usa el fondo de las 18:59 del día
  anterior.
- **Al cambiar de aula** con el comando `E`: el umbral arrastra el fondo del
  espacio anterior, que puede diferir en 10-15 dB entre un aula interior y una
  a fachada.

El efecto se autocorrige en un intervalo (30 s), así que el impacto es pequeño;
pero como el contador de eventos es un indicador central, conviene que no
arranque contaminado.

**Acción recomendada.** Poner `dbFondoPrevio = 0` en el cambio de ventana
horaria y en el comando de identificación de espacio. Con valor 0 el detector
queda inhibido un intervalo (línea 1156, `if (dbFondoPrevio > 0)`), que es
justo el comportamiento deseado.

**Corregido (5 de septiembre de 2026).** Aplicada tal cual: `dbFondoPrevio = 0`
en los dos puntos, en el cambio de estado de la ventana horaria dentro de
`loop()` y al final de `fijarEspacio()`, cada uno con el comentario que explica
por qué la referencia deja de ser válida ahí. El detector queda inhibido un
intervalo (30 s) y vuelve a arrancar con el fondo real del tramo nuevo.

---

## 21. LA RUTA DE RESPALDO FABRICA UN INTERVALO DE FLUCTUACIÓN NULA — RESUELTO

`registrador-ambiental.ino`, líneas 749-752:

```cpp
} else if (dbmeter_ok) {
    dbEq = readDBMeter(); dbMax = dbEq; dbMaxF = dbEq; dbFondo = dbEq;
    if (dbEq == 0) errDB = true;
}
```

Cuando un intervalo no ha acumulado ninguna muestra, se hace una lectura suelta
y se escriben los cuatro niveles con **el mismo valor**. El registro resultante
es válido para el script (estado `OK`) y tiene rango dinámico 0 e impulsividad 0
—el perfil exacto que la documentación describe como «zumbido constante»— sin
que eso se haya medido.

Ocurre en el primer intervalo tras cada arranque y tras cada cambio de ventana
horaria: poco volumen, pero son registros fabricados que entran en los promedios
y en el índice de fluctuación.

**Acción recomendada.** Marcar esas filas con una etiqueta propia
(`DB_MUESTRA_UNICA`) para que el análisis pueda excluirlas de los indicadores de
fluctuación, o no escribir los campos derivados en ese caso.

**Corregido (5 de septiembre de 2026).**

- El firmware marca esas filas con `DB_MUESTRA_UNICA` en el campo `estado`. La
  marca se emite solo cuando la lectura de respaldo es válida: si vale 0 sigue
  siendo `ERR_DB`, como antes. `construirEstado()` recibe el nuevo indicador y
  el buffer de estado pasa de 48 a 80 bytes, porque la cadena más larga posible
  (`ERR_RTC;ERR_TH;ERR_CO2;DB_MUESTRA_UNICA;FLASH_BAJA`) ocupa 51 y se habría
  truncado en silencio.
- El análisis las excluye de **todos** los indicadores acústicos, no solo de los
  de fluctuación: con una sola muestra el LAeq del intervalo tampoco es un LAeq.
  El criterio de exclusión se añadió a los diez filtros del script que ya
  descartaban `ERR_DB`. Temperatura, humedad y CO2 de esas filas se siguen
  usando con normalidad.
- El bloque de fiabilidad del informe cuenta esas filas por separado y explica
  en tres líneas que no son un error, sino un intervalo sin muestras acumuladas.

**Verificación.** Prueba de integración con el mismo conjunto sintético en dos
variantes: en una, la primera fila de cada día es un registro normal; en la
otra, esa fila se sustituye por una marcada `DB_MUESTRA_UNICA` con LAeq, fondo y
pico puestos a 95 dB —un valor absurdo, elegido para que cualquier fuga sea
visible. El LAeq horario (41,1 dB) y el pico (54 dB) salen idénticos en las dos
variantes; la única diferencia es el fondo medio de esa hora, 33,0 frente a
33,1 dB, que es el efecto de tener una fila normal menos en el promedio, no una
fuga del valor marcado.

---

## 22. LA VENTANA 07:00-19:00 EXCLUYE EL MEJOR DECAIMIENTO POSIBLE — RESUELTO

El firmware registra solo de 07:00 a 19:00 (líneas 195-196), y la documentación
lo describe correctamente. La decisión está bien justificada para el objetivo
principal —autonomía y ruido en horario lectivo— pero tiene una consecuencia
que no se ha valorado en ninguna de las dos revisiones:

**La noche es el único periodo con ocupación nula garantizada**, que es
precisamente el supuesto que el hallazgo 7 pide verificar y que durante la
jornada solo puede asumirse. Es también el decaimiento más largo y el único que
permite que el CO2 interior se aproxime de verdad al exterior — es decir, lo
único que, con una sola unidad, daría una estimación honesta de `cExt` y
desactivaría buena parte del hallazgo 14.

Dicho de otro modo: los hallazgos 14, 16 y 22 se refuerzan entre sí. No medir de
noche obliga a estimar el exterior desde dentro (14), y estimarlo mal se nota
más porque el método de dos puntos no tiene forma de detectarlo (16).

**Corregido (5 de septiembre de 2026).**

Al ir a aplicar la acción recomendada —«documentar que la primera noche conviene
ampliar la ventana a 24 h»— apareció el motivo real de que nadie fuera a hacerlo:
`HORA_INICIO` y `HORA_FIN` eran `#define`, de modo que cambiar el horario exigía
**recompilar y reprogramar el equipo**. Para una unidad instalada una semana en
un aula, eso convierte la recomendación en papel mojado. Documentar sin quitar
ese obstáculo habría sido cerrar el hallazgo en falso.

- **Nuevo comando `W`** en el firmware: `W 0-24` fija la ventana en caliente,
  `W 7-19` vuelve al horario del centro y `W` a secas la muestra y explica por
  qué conviene la noche. La ventana **se guarda en la flash** (fichero
  `/ventana.txt`), igual que la identificación del espacio, así que sobrevive a
  un corte de alimentación a mitad de campaña. Al fijar una ventana de 20 h o
  más, el firmware avisa de que la memoria dura aproximadamente la mitad.
- El cambio queda registrado en el CSV como marca `#VENTANA`, de modo que al
  analizar se sabe con qué horario se tomó cada tramo.
- **El informe avisa cuando ningún decaimiento usado para el ACH es nocturno**,
  explica por qué importa (la noche es el único tramo con ocupación nula
  garantizada, que es el supuesto del hallazgo 7) e indica los dos pasos
  concretos: `W 0-24` y descomentar la franja nocturna.
- `franjas.txt` incluye ya la franja nocturna comentada, con la explicación de
  por qué basta `19:00-23:59`: el decaimiento útil se completa en una o dos
  horas, así que no hace falta cruzar la medianoche —lo que además evitó tener
  que resolver el reparto de un decaimiento entre dos fechas.
- Documentado en `LEEME.txt` (nuevo paso 2 de la operativa), en §3.2 y §7.3 del
  documento de proyecto y en §5.3 de la guía de puesta en marcha, que ya no dice
  «editar HORA_INICIO y HORA_FIN al principio del firmware».

**Verificación.** Prueba de integración con dos conjuntos sintéticos: sin franja
nocturna el informe emite el aviso; con ella, analiza el decaimiento nocturno
(298 min, 104 lecturas ajustadas, ACH 2,2, R² 0,996) y el aviso desaparece. El
firmware pasa la comprobación de sintaxis con la API de Arduino simulada.

**Lo que este hallazgo deja como lección.** La acción recomendada original era
correcta pero incompleta: describía *qué* hacer sin comprobar si era *practicable*
con el equipo en el aula. Conviene aplicar ese mismo filtro a las verificaciones
de campo que siguen pendientes de la primera revisión.

---

## 23. ETIQUETAS DEL CSV QUE NINGÚN FIRMWARE EMITE — RESUELTO

Detectado al corregir el hallazgo 17. `documento-proyecto.pdf` §3.3 incluía una
tabla titulada «Marcas del campo de estado (fiabilidad del dato)» con las
etiquetas `T_BAJA`, `T_ALTA`, `T_RIESGO`, `HR_BAJA`, `HR_ALTA`, `CO2_DEFIC`,
`CO2_MALA`, `CO2_RIESGO`, `DB_ELEV` y `DB_RIESGO`. Ninguna aparece en el
firmware salvo en un comentario que explica por qué se quitaron. Un lector que
programara un análisis propio contra esa tabla no encontraría nunca esas marcas
en el CSV.

Es el mismo patrón del hallazgo 12 —documentación que describe un estado que el
código abandonó— y refuerza la conclusión de esta revisión: conviene contrastar
periódicamente cada afirmación de los PDF contra el código, no solo cuando se
cambia algo.

**Corregido.** Ver «Correcciones aplicadas», más arriba. Comprobado además que
las etiquetas inexistentes no aparecen en ninguno de los cinco PDF del proyecto
—incluidos `propuestas-investigacion.pdf`, `guia-montaje.pdf` y
`proposta-centre-educatiu.pdf`, que no se habían tocado— ni en `LEEME.txt`. En el
firmware solo quedan citadas en el comentario que explica por qué se retiraron.

---

## LO QUE ESTA REVISIÓN HA COMPROBADO Y ESTÁ BIEN

Para que la lista de arriba no dé una impresión desequilibrada, conviene dejar
constancia de lo que se ha verificado y resiste:

- **El promedio energético del LAeq es correcto** (líneas 1141-1146 del
  firmware y 290 del script): se promedia energía, no decibelios, en los dos
  extremos de la cadena. Es el error más común en este tipo de proyectos y aquí
  no está.
- **El percentil de fondo por histograma** (líneas 238-248) está bien
  implementado y es una solución elegante para un microcontrolador: coste
  constante en memoria y una pasada.
- **Las once correcciones de la primera revisión están efectivamente aplicadas**
  en el código y en los textos; se han contrastado una a una.
- **La histéresis del detector de eventos** (líneas 1158-1159) evita el
  recuento múltiple de un mismo evento, que es un fallo clásico.
- **La estimación de consumo de almacenamiento** de la documentación (847 KB
  para una semana) es consistente con el formato real de fila y el intervalo de
  registro.
- **El filtrado de errores del script** (`estado -notlike "*ERR_*"`) se aplica
  variable a variable, no fila a fila, que es lo correcto: una fila con
  `ERR_CO2` sigue aportando su dato acústico.

---

## VERIFICACIONES Y DECISIONES PENDIENTES

1. **Decidir entre la opción A y la opción B del hallazgo 15.** Es la decisión
   que condiciona todo lo demás: de ella dependen los hallazgos 12, 13 y 15, y
   si se elige B, la verificación con sonómetro de clase 2 pasa a ser
   bloqueante.
2. **Confirmar empíricamente el hueco de muestreo.** Reproducir un tren de
   impulsos breves conocido (una app de generador de tonos con ráfagas de 30 ms
   sirve) y comprobar que el contador de eventos registra del orden de la mitad.
   Es una comprobación de media hora que confirma o descarta el hallazgo 12 en
   el equipo real, no en simulación.
3. **Medir la concentración exterior de CO2 al instalar** en cada aula y
   anotarla. Ya no requiere un paso aparte: el comando `C`, con el equipo al
   aire libre, da la lectura que se anota como `CEXT` en el comando `E`. Es la
   misma operación que verifica la calibración ABC (hallazgo 8).
   Queda una limitación que conviene declarar al publicar: `CEXT` es una lectura
   puntual del día de instalación, y el CO2 exterior varía a lo largo del día
   —el máximo de madrugada supera al mínimo de la tarde entre 11 y 31 ppm según
   el emplazamiento en las medidas del área de París (Xueref-Remy et al., *ACP*
   18, 3335, 2018)—. Sobre un decaimiento típico, 25 ppm de error en la
   referencia mueven el ACH un 8 %, dentro de la banda que el informe ya
   propaga, pero no es cero.
4. Las verificaciones de campo pendientes de la primera revisión siguen
   vigentes y no se repiten aquí.

---

## VALORACIÓN GENERAL

La primera revisión concluyó que las debilidades no estaban en el planteamiento
sino en la precisión con que se enunciaban los resultados. Esta segunda ronda
matiza esa conclusión: al bajar al firmware aparece un problema que sí afecta al
dato de origen, y afecta justamente al indicador que el proyecto ha elegido como
más relevante. Los eventos y la fluctuación —lo que distingue este trabajo de un
sonómetro cualquiera y lo que conecta con BREATHE— se están midiendo con una
cuarta parte del tiempo observada.

La buena noticia es que la causa es una sola y es trivial de corregir: tres
constantes que dejaron de estar de acuerdo entre sí durante una optimización a
medio hacer, y que el propio código documenta de forma contradictoria en dos
comentarios. No hay ningún error de concepto detrás. La cadena de medida está
bien pensada —el promedio energético, el histograma de percentiles, la
histéresis de eventos son decisiones acertadas— y lo que falla es una
coherencia de configuración.

El hallazgo 14 es de otra naturaleza y merece más atención de la que su
severidad sugiere: no es un error de implementación sino una **estimación
presentada como medida**, con un sesgo de dirección conocida y con una frase en
la documentación que afirma justo lo contrario de lo que el análisis hace. Es el
tipo de detalle por el que un revisor competente descarta el conjunto, que es
exactamente el riesgo que estas revisiones existen para evitar.

Ninguno de los once hallazgos exige rediseñar nada. Cinco son de una línea de
código.

**Nota añadida al cerrar la revisión (5 de septiembre de 2026).** Dos cosas que
merecen quedar escritas por encima de los hallazgos concretos:

La primera es que en dos ocasiones el obstáculo real no era el que describía el
hallazgo. En el 22, la recomendación —«registrar la primera noche»— era
impracticable porque el horario estaba fijado en compilación, y documentarla sin
más habría cerrado el hallazgo en falso. En el 8, el protocolo terminaba en «si
hay deriva, calibrar», una instrucción que nadie puede ejecutar; ahora es un
comando que además se niega a calibrar cuando la lectura no es de aire exterior
o el sensor declara error. Merece la pena aplicar ese filtro —¿es *ejecutable*
lo que estoy recomendando?— al resto de la documentación.

La segunda es que el hallazgo 19 estaba parcialmente equivocado y solo se supo
al medirlo: el error del 7 % que anunciaba desaparece con la regresión del
hallazgo 16, mientras que el problema de ordenación, presentado como secundario,
sí era real y demostrable. Cuantificar antes de corregir no solo dimensiona el
arreglo: a veces cambia cuál es el arreglo.
