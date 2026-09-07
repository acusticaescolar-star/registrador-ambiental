# TERCERA REVISIÓN DE RIGOR METODOLÓGICO
## Registrador Ambiental · acusticaescolar.com
### 7 de septiembre de 2026 · hallazgos 24 a 36 · todos corregidos

---

Esta revisión se hizo sobre el estado posterior a las correcciones de las dos
anteriores: firmware con el comando `C`, análisis con la regresión ASTM E741 y
los veintitrés hallazgos previos cerrados. A diferencia de las dos primeras,
que buscaban errores en el **camino del dato** —del sensor al CSV y del CSV al
número—, esta baja al último tramo: **del número a la conclusión**. Ahí es donde
están los tres hallazgos graves.

Se añade además, a petición expresa, un bloque de **simplificaciones sin merma
de eficacia**: código y texto que puede desaparecer sin que el proyecto pierda
nada.

Todo lo que se afirma aquí está medido, simulado o ejecutado. Cuando una
sospecha no se ha podido confirmar con números, se dice.

---

## RESUMEN DE HALLAZGOS

| Nº | Hallazgo | Severidad |
|----|----------|-----------|
| 24 | ~~La categoría FAVORABLE del eje acústico es inalcanzable, y con ella las notas A y B de la jornada~~ | **RESUELTO** |
| 25 | ~~La recomendación de obra se dispara con una cifra sin banda que la propia revisión anterior declaró no fiable~~ | **RESUELTO** |
| 26 | ~~Un fallo de reloj fabrica una jornada fantasma y la califica con A~~ | **RESUELTO** |
| 27 | ~~«Condiciones adecuadas en las cuatro variables» se afirma sin comprobar cuántas hubo~~ | **RESUELTO** |
| 28 | ~~El intervalo de registro se deduce de dos filas: un reinicio de 5 min divide por diez los eventos/hora~~ | **RESUELTO** |
| 29 | ~~Un sensor de CO2 que no responde se lleva por delante el 17 % de las muestras acústicas del intervalo, sin marca~~ | **RESUELTO** |
| 30 | ~~El nivel de fondo se promedia en decibelios dentro de un cociente de energías~~ | **RESUELTO** |
| 31 | ~~La autonomía de «peor caso» se calcula con 86 B por fila; el peor caso real son 111 B~~ | **RESUELTO** |
| 32 | ~~Comentarios que describen un respaldo del cálculo de ACH que no existe~~ | **RESUELTO** |
| 33 | ~~Las marcas `#VENTANA` se leen y no se usan~~ | **RESUELTO** |
| 34 | ~~La cabecera del firmware no lista tres comandos ni una marca del CSV~~ | **RESUELTO** |
| 35 | ~~La primera noche dependía de dos acciones humanas y la segunda no la hace nadie~~ | **RESUELTO** |
| 36 | ~~umbrales.txt dice que los tramos de ruido se evalúan sobre el pico; el análisis los aplica a niveles sostenidos~~ | **RESUELTO** |

**Los trece hallazgos quedan cerrados**, junto con las seis simplificaciones
del bloque siguiente. El 35 se corrigió durante la propia revisión (automatismo
de la primera jornada), con el 33 y el 34 como consecuencia directa; el 36
apareció al aplicar las correcciones del 24. Todo lo demás se corrigió después,
el 7 de septiembre de 2026, y está verificado con pruebas de ejecución cuyos
resultados se citan en cada apartado.

**Los tres ALTO comparten una misma raíz**, y conviene verla antes que los
hallazgos por separado: el informe **razona bien y concluye mal**. Los cálculos
intermedios son correctos —el LAeq es energético, el ACH es una regresión con
banda, el reparto exógeno/endógeno tiene su intervalo—, pero el último paso, el
que convierte números en una letra y en una recomendación, se hizo con reglas
que no se comprobaron numéricamente. El resultado es un informe cuya parte más
visible —la síntesis de la jornada, lo único que va a leer un equipo directivo—
es la menos fiable del conjunto.

---

## 24. LA CATEGORÍA «FAVORABLE» ES INALCANZABLE — RESUELTO

`volcar_datos.ps1`, `Valorar-Actividad`:

```powershell
$fluctuacionAlta = ($rango -ge 20) -or ($eventosHora -ge 12) -or ($indiceFluctuacion -ge 40)
if ($fluctuacionAlta) {
    if ($base -eq "FAVORABLE")      { $base = "INTERMEDIA" }
    elseif ($base -eq "INTERMEDIA") { $base = "DESFAVORABLE" }
}
```

El índice de fluctuación es, por definición,

```
IF = (E_total - E_fondo) / E_total × 100 = (1 - 10^(-(LAeq - fondo)/10)) × 100
```

es decir, **una función únicamente de la diferencia LAeq − fondo**. Invirtiéndola:

| IF | equivale a LAeq − fondo |
|----|--------------------------|
| 40 % | **2,2 dB** |
| 60 % | 4,0 dB |
| 80 % | 7,0 dB |
| 90 % | 10,0 dB |

El umbral de 40 % que dispara «fluctuación alta» es, por tanto, **una diferencia
de 2,2 dB entre el nivel equivalente y el percentil 10 del mismo intervalo**. No
existe recinto ocupado en el que eso no se cumpla.

**Comprobación.** Simulación de cinco escenarios, 240 muestras de 125 ms por
intervalo, fondo estacionario más ráfagas de voz:

| escenario | LAeq | p10 | Δ | IF | ¿«fluctuación alta»? |
|-----------|-----:|----:|--:|---:|:---:|
| aula en clase (fondo 45, voz 62) | 59,7 | 43,2 | 16,5 | 97,8 % | SÍ |
| aula tranquila (fondo 38, voz 55) | 49,3 | 36,4 | 12,9 | 94,8 % | SÍ |
| aula vacía con tráfico (fondo 42) | 42,7 | 40,2 | 2,5 | 43,3 % | SÍ |
| **biblioteca (fondo 33, casi nada)** | 34,1 | 31,3 | 2,8 | **47,8 %** | **SÍ** |
| zumbido constante (fondo 60, sin voz) | 60,3 | 58,2 | 2,1 | 39,0 % | no |

El resultado es perverso en los dos extremos. Un espacio **a 34 dB, en silencio
de biblioteca**, se marca como de fluctuación alta y baja un escalón. Un
**zumbido constante a 60 dB** —acústicamente el peor entorno posible para un
aula, y el caso que este indicador se escribió para detectar— **no** lo hace.

**Consecuencias en cadena.** `Valorar-Actividad` solo devuelve tres valores, y
`GradoRuido` los traduce a `FAVORABLE→2`, `INTERMEDIA→3`, `DESFAVORABLE→5`. Si
FAVORABLE es inalcanzable, el grado acústico solo puede valer **3 o 5**: nunca 1,
nunca 2, nunca 4. Y como la nota del día es el **máximo** de los cuatro ejes
(`$gDia = max(gT, gH, gA, gRu)`), **ninguna jornada con datos acústicos puede
sacar mejor que una C**, por perfectas que sean la temperatura, la humedad y la
ventilación. En la prueba de integración, un aula con LAeq 52 dB —una clase
normal y tranquila— sale «C, mejorable» con los otros tres ejes en A.

Dicho de otro modo: una escala de cinco letras de la que solo se pueden emitir
dos, y ninguna de las buenas.

**Los otros dos términos de la condición no salvan nada.** `rango ≥ 20` exige
maxF − fondo ≥ 20 dB, y `eventosHora ≥ 12` es un umbral razonable, pero ambos son
**mucho más exigentes** que IF ≥ 40 (2,2 dB): en la práctica la disyunción entera
se reduce al tercer término, y los dos primeros no llegan nunca a decidir nada.

**Acción recomendada.** Tres piezas, en este orden:

1. **Retirar `indiceFluctuacion` de la condición.** No aporta información: es
   una recodificación monótona de LAeq − fondo, dos cifras que el informe ya
   imprime en la misma línea (ver simplificación S4).
2. **Fijar el umbral de fluctuación sobre el rango dinámico**, que es la
   magnitud que el propio proyecto declara como indicador principal, con un
   valor defendible. Los datos de campo del proyecto darán el número; mientras
   tanto, 15 dB separa razonablemente el aula interrumpida del zumbido, y debe
   declararse como criterio propio, no normativo.
3. **Revisar `GradoRuido` para que use los cinco grados**, o declarar
   explícitamente que el eje acústico es de tres niveles y no mezclarlo con
   ejes de cinco en un máximo.

**Corregido (7 de septiembre de 2026), con una decisión distinta de la que
proponía la acción recomendada.** Al ir a fijar el umbral de fluctuación se vio
que el problema no era el valor sino la existencia del umbral: **para la
fluctuación no hay ninguna escala**, ni normativa ni propia con respaldo, y
cualquier corte que se elija es una invención que mueve una letra. El nivel, en
cambio, sí tiene una escala declarada con el efecto de cada tramo (documento de
proyecto, 7.1: «aula ocupada en silencio», «actividad docente habitual»,
«esfuerzo vocal del docente», «comunicación comprometida»).

De modo que el eje se partió en dos:

- **La letra sale del NIVEL**, con los tramos `DB_*` de `umbrales.txt`, los
  mismos que ya usaba el eje estructural. `GradoNivelDb` devuelve 1 a 5 y usa
  los cinco grados. `Valorar-Actividad` y `GradoRuido` han desaparecido.
- **La fluctuación se informa aparte**, con sus tres cifras —LAeq, rango
  dinámico y eventos por hora— y un perfil descriptivo: «ambiente tranquilo y
  estable», «ruido SOSTENIDO, sin interrupciones: revisar instalaciones» o
  «ambiente con interrupciones frecuentes». **No toca la letra.**
- El informe declara, cada vez que emite ese eje, que **el criterio es propio y
  no normativo**.

Con esto la escala vuelve a discriminar. Simulando el nivel de siete arquetipos
con el detector de eventos real del firmware:

| escenario | LAeq | fondo | rango | ev/h | nota |
|-----------|-----:|------:|------:|-----:|:----:|
| biblioteca / estudio | 35,8 | 35,0 | 8,8 | 108 | **B** |
| aula vacía con tráfico | 42,5 | 40,0 | 9,5 | 54 | **B** |
| aula tranquila (poca voz) | 44,1 | 36,1 | 22,1 | 471 | **B** |
| aula normal (voz moderada) | 51,6 | 40,1 | 23,2 | 996 | **C** |
| aula en clase (voz continua) | 58,9 | 43,5 | 25,5 | 1497 | **D** |
| zumbido constante (climatización) | 60,3 | 58,0 | 6,2 | 0 | **D** |
| aula muy interrumpida | 67,3 | 42,9 | 33,5 | 1833 | **E** |

Las cinco letras se emiten, la biblioteca deja de estar penalizada y el zumbido
—que antes se salvaba— cae en D por su nivel. En la prueba de integración, un
aula a 52 dB pasa de «C forzada por el modificador» a **C por su nivel**, y una
a 44 dB, que antes también era C, ahora es **B**.

**El umbral que sí sobrevive** es el de 12 dB de rango dinámico, pero solo para
elegir la frase del perfil, no para calificar. Está declarado en el código como
criterio propio, con la nota de que debe recalibrarse con datos de campo. En los
arquetipos separa limpiamente: los ambientes con voz dan 22-34 dB y los estables
6-10 dB, con un hueco entre medias donde el corte no es crítico.

---

## 25. LA RECOMENDACIÓN DE OBRA SE DISPARA CON UNA CIFRA QUE LA REVISIÓN ANTERIOR DECLARÓ NO FIABLE — RESUELTO

El hallazgo 18 de la segunda revisión estableció que el reparto
exógeno/endógeno **no puede darse como cifra**, porque restar dos niveles
próximos con ±2 dB de incertidumbre mueve el porcentaje decenas de puntos. La
corrección fue `RepartoConBanda`, que calcula los dos extremos y decide si
puede afirmarse una cifra, solo un sentido, o nada.

Esa corrección se aplicó **en la sección de detalle** y no en la síntesis
diaria. La síntesis sigue haciendo, en `Get-Análisis`:

```powershell
$refMin = ($pv | Measure-Object -Minimum).Minimum
$pesoExt = PesoEnergetico $refMin $lqAct2      # cifra puntual, sin banda
...
if ($pesoExt -ne $null -and $pesoExt -ge 30) {
    $inf.Add("       -> Actuar sobre la envolvente: ventanas y fachada")
}
```

Es decir: **la única recomendación del informe que cuesta dinero** —cambiar
ventanas y actuar sobre la fachada— se dispara con un umbral del 30 % aplicado a
una cifra sin banda.

**Cuánto vale esa banda.** Con ±2 dB en el nivel de referencia y ±2 dB en el
total (±4 dB en la diferencia):

| cifra que se imprime | rango real compatible |
|---------------------:|----------------------|
| 15 % | 6,0 – 37,7 % |
| **30 %** | **11,9 – 75,4 %** |
| 50 % | 19,9 – 100 % |
| 70 % | 27,9 – 100 % |

Un 30 % impreso es compatible con cualquier valor entre el 12 % y el 75 %. El
umbral cae dentro de su propia banda en los cuatro casos: **la comparación no
distingue nada**.

**Y la referencia se elige peor que en el detalle.** La síntesis toma
`$refMin`, el **mínimo** de los LAeq de todas las franjas vacías del día. La
sección de detalle, en cambio, elige la franja vacía **más próxima en el tiempo**
a la franja evaluada (`Sort-Object { abs($_.centro - $centroFr) }`), que es lo
correcto: el ruido de tráfico de las 8:00 no es el de las 14:00. La síntesis
compara la actividad de media mañana contra el tramo más silencioso del día, lo
que **sobreestima sistemáticamente la parte endógena** y, por tanto,
infraestima el peso exterior. Los dos sesgos —banda ignorada y referencia
demasiado baja— empujan en direcciones opuestas y no se compensan de forma
controlada.

**Acción recomendada.** Sustituir las dos líneas de la síntesis por una llamada
a `RepartoConBanda` con la misma referencia por proximidad temporal que usa el
detalle, y condicionar la recomendación de obra al tipo devuelto: emitirla solo
cuando el reparto sea `'cifra'` con el extremo inferior por encima del umbral,
o `'cualitativo'` con etiqueta «mayoritariamente EXTERIOR». Es además una
**simplificación** (S6): elimina la segunda vía de cálculo.

**Corregido (7 de septiembre de 2026).** La síntesis usa ahora `RepartoConBanda`
con la franja vacía **más próxima en el tiempo** al centro de la actividad, la
misma regla que la sección de detalle. La segunda vía de cálculo ha
desaparecido: hay una sola función de reparto en todo el script.

La recomendación de obra se emite únicamente cuando la banda la sostiene:

- `'cifra'` con el extremo **inferior** ≥ 30 % → «Actuar sobre la envolvente».
- `'cualitativo'` con el extremo inferior > 60 % → lo mismo.
- Cuando el extremo superior llega al 30 % pero el inferior no, el informe **no
  recomienda obra**: dice que el origen no queda separado, cuánto admite la
  banda, y qué hacer para cerrarlo (registrar un sábado, o medir el RT).

**Verificación.** En la prueba de integración con un aula a 44 dB y su franja
vacía a 38 dB, el reparto sale `10-63 %`: banda ancha, tipo `'cualitativo'`. El
informe **no** propone obra; imprime «El origen no queda separado: la banda
admite hasta un 63 % exterior». Con el aula a 52 dB y la misma referencia, la
banda es `3-20 %` y tampoco la propone, correctamente. Antes, con la cifra
puntual, el primer caso habría dado un valor cercano al 25 % y el segundo un
6 %, ambos sin banda y ambos comparados contra un 30 % que no significaba nada.

---

## 26. UN FALLO DE RELOJ FABRICA UNA JORNADA FANTASMA Y LA CALIFICA CON A — RESUELTO

Cuando el DS3231 no responde, el firmware escribe la marca de tiempo literal
`0000-00-00T00:00:00` y marca la fila con `ERR_RTC`. El análisis **no excluye
esas filas**: las agrupa por su campo `dia`, que vale `0000-00-00`, y las trata
como una jornada más.

**Comprobación.** Prueba de integración con 40 filas (20 minutos) marcadas
`ERR_RTC` dentro de un día por lo demás normal. Salida literal del informe:

```
  --- 0000-00-00 ()   40 registros ---

  +-- SÍNTESIS DE LA JORNADA -----------------------------------------+

    TEMPERATURA    22.5 - 22.5 C                A    óptimo
       -> Sin actuación

    HUMEDAD          48 -   48 %                 A    óptimo
       Dentro del rango legal toda la jornada
       -> Sin actuación

    AIRE (CO2)      662 -  796 ppm               -    sin datos

  +-- VALORACIÓN DE LA JORNADA:  A ---------------------------------+
     Condiciones adecuadas en las cuatro variables.
```

Un fallo de reloj produce **una jornada inventada, con día de la semana en
blanco, calificada con A** y con la frase «condiciones adecuadas en las cuatro
variables» cuando solo dos tenían datos.

Y hay una ironía que conviene no perder: esa jornada fantasma es **la única del
informe que saca una A**, y la saca precisamente porque le falta el eje
acústico —todas sus filas caen en el minuto 0, fuera de cualquier franja, de
modo que `valR` queda nulo y el eje que impone el techo de C (hallazgo 24) no
llega a evaluarse. **Los dos defectos se combinan para dar la valoración más
halagüeña del informe al conjunto de datos más inservible.**

Es exactamente la clase de error que el hallazgo 17 de la revisión anterior
corrigió en el firmware —una lectura de 0 registrada como dato válido—
reaparecida un nivel más arriba: un día entero de marcas de tiempo inservibles
registrado como jornada válida.

**Acción recomendada.** En el parser, desviar las filas cuyo `dia` no sea una
fecha válida a un cubo aparte, y en el informe emitir un bloque de aviso
—«N registros con marca de tiempo no fiable, excluidos del análisis por
jornadas»— en vez de una jornada. Sus valores de T, HR y CO2 pueden seguir
contando en los totales globales del espacio, que no dependen de la fecha; lo
que no puede es fabricarse un día.

**Corregido (7 de septiembre de 2026).** El parser valida la marca de tiempo
—formato completo, año ≥ 2020, mes y día en rango— y desvía las filas que no la
cumplen a un cubo aparte. El informe emite un bloque propio:

```
====================================================================
  MARCA DE TIEMPO NO VÁLIDA
====================================================================
  40 registros (1.1 % del total) no llevan una fecha utilizable.
  Ocurre cuando el DS3231 no responde o ha perdido la hora: el firmware
  escribe 0000-00-00T00:00:00 y marca la fila con ERR_RTC.
  QUEDAN FUERA del análisis por jornadas y por franjas: sin hora no se
  sabe si corresponden a una clase, a un recreo o a la noche.
  Rango registrado: T 22.5-22.5 C
                    CO2 663-795 ppm
  Ajusta el reloj con el comando T y comprueba la pila CR2032 del DS3231.
```

**Verificación.** El mismo conjunto sintético que producía la jornada fantasma
con una A ya no la produce: las 40 filas aparecen en este bloque, con el rango
de lo que registraron, y el informe pasa directamente al día real.

---

## 27. «CONDICIONES ADECUADAS EN LAS CUATRO VARIABLES» SE AFIRMA SIN COMPROBAR CUÁNTAS HUBO — RESUELTO

```powershell
$grados = @($gT,$gH,$gA,$gRu) | Where-Object { $_ -gt 0 }
$gDia = if ($grados.Count -gt 0) { ($grados | Measure-Object -Maximum).Maximum } else { 0 }
...
} elseif ($gDia -le 2) {
    $inf.Add("     Condiciones adecuadas en las cuatro variables.")
}
```

Dos problemas en cuatro líneas:

1. **La frase se emite sin contar los ejes disponibles.** Con dos ejes medidos y
   dos ausentes afirma las cuatro. Lo demuestra el ejemplo del hallazgo 26.
2. **Los ejes sin datos se eliminan del máximo**, de modo que **no medir una
   variable mejora la nota**. Un día en el que no se pudo calcular el ACH sale
   mejor calificado que uno idéntico en el que sí se calculó y resultó malo. Es
   ausencia de evidencia leída como evidencia de ausencia, y es justo lo que
   este proyecto lleva dos revisiones corrigiendo en otros sitios.

**Acción recomendada.** Contar los ejes con dato y nombrarlos: «Condiciones
adecuadas en las 3 variables medidas (falta: ventilación)». Y en la cabecera de
la valoración, indicar sobre cuántos ejes se ha calculado la letra.

**Corregido (7 de septiembre de 2026).** Los cuatro ejes se construyen ahora
como una lista de objetos con nombre, grado y banda. La valoración declara
siempre sobre cuántos se ha calculado, y nombra los que faltan con la
advertencia de que la letra podría empeorar al medirlos:

```
  +-- VALORACIÓN DE LA JORNADA:  D ---------------------------------+
     Calculada sobre 3 de 4 ejes.
     SIN DATO: ventilación. La letra podría empeorar si se midieran.
```

La frase de cierre pasó de «las cuatro variables» a «Condiciones adecuadas en
N de las 4 variables medidas», con el número real.

---

## 28. EL INTERVALO DE REGISTRO SE DEDUCE DE DOS FILAS — RESUELTO

```powershell
$intervaloSeg = 30
if ($filas.Count -gt 2) {
    $t1 = ...$filas[1].ts...; $t2 = ...$filas[2].ts...
    $d = ($t2 - $t1).TotalSeconds
    if ($d -gt 0 -and $d -lt 600) { $intervaloSeg = $d }
}
```

El intervalo con el que se normaliza **todo el informe** —eventos por hora,
horas de actividad— se deduce de la distancia entre la segunda y la tercera
fila del fichero. Si justo ahí hay un hueco (un reinicio, un cambio de espacio,
una escritura fallida), el valor es erróneo para el conjunto.

**Comprobación.** Al mismo conjunto sintético de 1.440 filas se le añaden
**tres filas** al principio, con la segunda y la tercera separadas cinco minutos
—un reinicio de cinco minutos al empezar—:

| | intervalo deducido | eventos por hora |
|---|---:|---:|
| sin el hueco | 30 s | **117** |
| con el hueco | 300 s | **12** |

Tres filas de 1.443 —el 0,2 % del fichero— dividen por diez el indicador. Y
12 ev/h es exactamente el umbral de `Valorar-Actividad`, así que el error puede
además cambiar la valoración acústica.

**Acción recomendada.** Tomar la **mediana** de las diferencias entre filas
consecutivas del mismo día, descartando las negativas y las mayores de 600 s.
Es robusta por construcción y cuesta tres líneas. Añadir un aviso si la mediana
no coincide con el valor esperado del firmware.

**Corregido (7 de septiembre de 2026).** Mediana de las diferencias entre filas
consecutivas del mismo día, descartando las no positivas y las mayores de 600 s,
y solo si hay al menos tres diferencias utilizables.

**Verificación.** El mismo fichero con el reinicio de cinco minutos al principio
—el que antes daba 300 s— ahora deduce **30 s**, y los eventos por hora vuelven
a los 117 correctos en vez de 12.

---

## 29. UN SENSOR DE CO2 QUE NO RESPONDE SE LLEVA POR DELANTE EL 17 % DE LAS MUESTRAS ACÚSTICAS — RESUELTO

`registrador-ambiental.ino`. El muestreo del sonómetro y el registro de fila
comparten el mismo hilo:

```cpp
if (dbmeter_ok && (now - lastDbSample >= DB_SAMPLE_MS)) { ... }   // cada 125 ms
if (now - lastLog >= LOG_INTERVAL_MS) { lastLog = now; logRow(); }
```

`logRow()` es bloqueante, y dentro llama a `sensor_S8->get_co2()`. La librería
`S8_UART` define `S8_TIMEOUT 5000ul`: si el sensor no contesta —cable suelto,
alimentación floja, sensor averiado— **la lectura tarda cinco segundos**, y
durante esos cinco segundos no se toma ni una muestra de dB.

| bloqueo de `logRow()` | muestras perdidas de 240 | fracción del intervalo |
|---|---:|---:|
| lectura normal (~15 ms) | 0,1 | 0,1 % |
| escritura lenta en flash (200 ms) | 1,6 | 0,7 % |
| **S8 sin responder (5.000 ms)** | **40** | **16,7 %** |

Los dos primeros son irrelevantes. El tercero no: **el 17 % del intervalo deja
de observarse**, y el recuento de eventos —que es proporcional al tiempo
observado— cae en la misma proporción. Es el mismo mecanismo del hallazgo 12 de
la segunda revisión, que se calificó de ALTO, aunque aquí solo se activa con el
sensor de CO2 averiado.

Lo que lo hace un hallazgo y no una nota es que **la fila sale marcada
`ERR_CO2`, no `ERR_DB`**, y el análisis excluye `ERR_CO2` solo de las
estadísticas de CO2: los datos acústicos de esa fila, medidos sobre el 83 % del
intervalo, entran íntegros en los promedios y en el recuento de eventos.

**Acción recomendada.** Dos opciones, no excluyentes:

- **Contar las muestras y marcarlo.** `dbNumMuestras` ya existe: si el intervalo
  reúne menos del 90 % de las esperadas (`LOG_INTERVAL_MS / DB_SAMPLE_MS`),
  añadir una marca `DB_INCOMPLETO` con el mismo tratamiento que
  `DB_MUESTRA_UNICA`. Es la solución robusta, porque cubre cualquier causa de
  pérdida, no solo esta.
- **Reducir el bloqueo.** Saltarse la lectura de CO2 tras varios fallos
  consecutivos, o rebajar el timeout del S8. Ataca la causa pero solo esta.

La primera opción es preferible: convierte un fallo silencioso en un dato
declarado, que es la línea del proyecto.

**Corregido (7 de septiembre de 2026)** con la primera opción. El firmware
compara `dbNumMuestras` con `DB_MUESTRAS_ESPERADAS` (= `LOG_INTERVAL_MS /
DB_SAMPLE_MS` = 240) y marca la fila con **`DB_INCOMPLETO`** si no alcanza el
90 %. El análisis excluye esas filas de todos los indicadores acústicos, igual
que `DB_MUESTRA_UNICA`, y el bloque de fiabilidad explica qué significan y qué
revisar si aparecen muchas.

El umbral del 90 % (216 de 240 muestras) deja margen para la sobrecarga normal
del bucle sin marcar falsos positivos, y captura cualquier causa de pérdida, no
solo el timeout del S8.

---

## 30. EL NIVEL DE FONDO SE PROMEDIA EN DECIBELIOS DENTRO DE UN COCIENTE DE ENERGÍAS — RESUELTO

```powershell
$sf = Stats ($vD.fondo)                    # media ARITMÉTICA de dB
$ir = IndiceFluctuacion $laeq $sf.avg      # se usa como si fuera energía
```

`$laeq` es un promedio **energético** (correcto) y `$sf.avg` una media
**aritmética de decibelios**. El índice los divide como si fueran homogéneos.
La media aritmética de dB queda siempre por debajo de la energética, y la
diferencia crece con la dispersión: para niveles con desviación σ,

```
L_energético - L_aritmético ≈ 0,115 · σ²  dB
```

| dispersión del fondo entre intervalos | subestimación |
|---|---:|
| 2 dB | 0,5 dB |
| 4 dB | 1,8 dB |
| 6 dB | 4,1 dB |
| 8 dB | 7,4 dB |

En un bloque que mezcle tramos tranquilos y ruidosos la dispersión del fondo
llega con facilidad a 5–6 dB, de modo que el fondo empleado queda 3–4 dB por
debajo del que corresponde y **el índice sale sesgado al alza**. No cambia
ninguna decisión —el hallazgo 24 muestra que el umbral se supera de todos
modos—, pero es una incoherencia de método en una magnitud que se publica.

**Acción recomendada.** Si el índice se conserva (ver S4), promediar el fondo
energéticamente, igual que `LaeqGlobal`. Si se retira, el problema desaparece
con él.

**Corregido (7 de septiembre de 2026).** Las dos cosas. El índice se retiró
(S4), de modo que la incoherencia desaparece en su origen; y el fondo que el
informe **sí** publica en cada bloque se promedia ahora energéticamente, con la
misma aritmética que `LaeqGlobal`, porque es un nivel sonoro y promediar niveles
en decibelios es incorrecto se use para lo que se use.

---

## 31. LA AUTONOMÍA DE «PEOR CASO» SE CALCULA CON UNA FILA QUE YA NO EXISTE — RESUELTO

`infoCSV()` estima la autonomía con `capacidad = libre / 86`, y el comentario
dice «peor caso (86 B/fila, todas las alertas)». Midiendo las filas reales que
el firmware genera hoy:

| fila | bytes (con CRLF) |
|---|---:|
| normal (`OK`) | 52 |
| con una marca de error | 57 |
| con dos marcas | 68 |
| **peor caso real** (`ERR_RTC;ERR_TH;ERR_CO2;DB_MUESTRA_UNICA;FLASH_BAJA`, valores extremos) | **111** |

Los 86 B se quedaron de antes de que el campo `estado` incorporara
`CO2_CALENTANDO` (hallazgo 17) y `DB_MUESTRA_UNICA` (hallazgo 21). Con 111 B:

| supuesto | autonomía informada (ventana de 12 h) |
|---|---:|
| 86 B/fila (lo que dice hoy) | 11,4 días |
| **111 B/fila (peor caso real)** | **8,8 días** |
| 52 B/fila (funcionamiento normal) | 18,9 días |

El error es del **+30 % en el peor caso**, y afecta también a las cifras
publicadas en los dos PDF: la tabla «11,4 días» del documento de proyecto y el
recuadro «una semana necesita 847 KB, queda un 39 % de margen» de la guía de
puesta en marcha. Recalculado, una semana en el peor caso necesita 1.104 KB de
los 1.380 disponibles: el margen real es del **20 %**, no del 39 %.

Sigue habiendo margen suficiente —esa es la razón de la severidad baja— pero es
una cifra publicada que no cuadra con el código.

**Acción recomendada.** Sustituir la constante por `sizeof(row)` real medido, o
por 112 declarado con su desglose, y rehacer las dos cifras de los PDF. Conviene
además notar que **la extensión automática de la primera jornada** (hallazgo 35)
duplica el consumo de ese día: la estimación de `infoCSV()` debería mencionarlo.

**Corregido (7 de septiembre de 2026).** `infoCSV()` deduce el tamaño de fila
**del propio fichero** —bytes entre registros—, que es exacto, persistente entre
reinicios y no necesita ninguna constante que se quede obsoleta. Informa de dos
autonomías: al ritmo actual y en el peor caso (112 B). Y avisa cuando la jornada
en curso está ampliada a 24 h, que consume el doble.

Las cifras de los dos PDF se han rehecho: la tabla de autonomía pasa a
**18,9 / 14,4 / 8,8 días** (sin alertas, ocasionales, peor caso) y el recuadro,
a «una semana completa requiere **1.102 KB** de los 1.380 disponibles en el peor
caso: queda un margen del **20 %**; en funcionamiento normal, 512 KB».

---

## 32. COMENTARIOS QUE DESCRIBEN UN RESPALDO DEL ACH QUE NO EXISTE — RESUELTO

Sobre `CalcularACHBanda`, en el script:

> «NOTA: solo se usa como respaldo cuando la regresión no es aplicable (menos de
> 5 lecturas). El método principal es `CalcularACHRegresion`.»

**Es falso.** `CalcularACHBanda` no se invoca en ninguna parte del fichero, ni
`CalcularACH` tampoco. Comprobado con búsqueda por límite de palabra: ambas
aparecen una sola vez, su propia definición. Cuando la regresión no es
aplicable, la franja se descarta en silencio y no hay ACH: no existe el
respaldo que el comentario promete.

Un revisor que lea ese comentario concluirá que los decaimientos cortos reciben
un tratamiento degradado pero existente. No lo reciben. Y como el comentario
está redactado con más autoridad que el código —cita el hallazgo, explica el
criterio—, es más creíble que la realidad.

**Acción recomendada.** Borrar las dos funciones y su comentario (40 líneas, ver
S2), y añadir en la sección de ACH del informe una línea que diga cuántas
franjas se descartaron y por qué. Que el usuario sepa que hubo decaimientos no
usados es más útil que un respaldo imaginario.

**Corregido (7 de septiembre de 2026).** Las dos funciones y su comentario han
desaparecido. En su lugar, la cabecera de la sección de ACH dice ahora lo que
realmente ocurre: el cálculo es **siempre** por regresión, no hay respaldo de
dos puntos, y un decaimiento que no cumple los requisitos se descarta. Se añade
la advertencia de no reintroducir el método de dos puntos «por parecer más
simple», con el motivo: con dos puntos el R² vale siempre 1 y el supuesto de
mezcla homogénea deja de ser comprobable.

---

## 33. LAS MARCAS `#VENTANA` SE LEEN Y NO SE USAN — RESUELTO

En la segunda revisión escribí, al cerrar el hallazgo 22:

> «El cambio queda registrado en el CSV como marca `#VENTANA`, de modo que al
> analizar se sabe con qué horario se tomó cada tramo.»

El firmware escribe la marca. El script la parsea:

```powershell
$ventanas = @()
...
if ($p.Count -ge 3) { $ventanas += [PSCustomObject]@{ ts=$p[1]; valor=$p[2].Trim() } }
```

Y `$ventanas` **no vuelve a aparecer en las 1.800 líneas del fichero**. La marca
se lee, se guarda en memoria y se descarta. La frase que escribí describía una
intención, no el código: al analizar no se sabía con qué horario se tomó cada
tramo.

Es un error mío de la ronda anterior, y del mismo tipo que el 32: documentación
que corre por delante de la implementación.

**Corregido (7 de septiembre de 2026).** El informe muestra ahora, bajo la
cabecera de cada jornada, las marcas `#VENTANA` de ese día, distinguiendo la
extensión automática, su fin y los cambios manuales:

```
  --- 2026-09-10 (jueves)   2038 registros ---
      Primera jornada en el aula: registro ampliado a 24 h (automático).
      Es la noche con ocupación nula que exige el cálculo de renovación.

  --- 2026-09-11 (viernes)   1440 registros ---
      07:00  fin del registro de 24 h; vuelve al horario configurado.
```

---

## 34. LA CABECERA DEL FIRMWARE NO LISTA TRES COMANDOS NI UNA MARCA DEL CSV — RESUELTO

La cabecera de `registrador-ambiental.ino` es la primera documentación que ve
quien abre el fichero, y es la que va a leer quien lo descargue del repositorio.
Estaba incompleta:

- La lista de comandos daba `T H D I L W X ?` y **omitía `E`, `N` y `C`** —
  incluido `E`, que la propia cabecera describe en otro punto como obligatorio.
- La lista de marcas del campo `estado` **omitía `DB_MUESTRA_UNICA`**, que el
  firmware emite desde la corrección del hallazgo 21.

Las dos omisiones son mías: la de `C` y la de `DB_MUESTRA_UNICA` son de la ronda
anterior, en la que documenté las dos novedades en los PDF y en el `LEEME` y no
en el propio fichero.

**Corregido (7 de septiembre de 2026).** Cabecera completa, con los tres
comandos, la marca y el automatismo del hallazgo 35.

---

## 35. LA PRIMERA NOCHE DEPENDÍA DE DOS ACCIONES HUMANAS, Y LA SEGUNDA NO LA HACE NADIE — RESUELTO

El hallazgo 22 de la segunda revisión hizo *practicable* registrar la primera
noche: antes exigía recompilar, después bastaba con `W 0-24`. Pero dejó el
procedimiento en **dos acciones manuales**:

1. escribir `W 0-24` al instalar el equipo, y
2. **volver a escribir `W 7-19` a la mañana siguiente.**

La segunda es la que rompe el diseño. El equipo se queda solo en un aula durante
días; para ejecutar el paso 2 alguien tiene que volver, conectar un portátil y
teclear. En la práctica solo hay dos desenlaces: o no se hace el paso 1 y no hay
noche que analizar —y el informe emite su aviso, correctamente, todas las
semanas—, o no se hace el paso 2 y el equipo registra 24 h durante toda la
campaña, gastando el doble de memoria sin ninguna ganancia a partir de la
primera noche.

Es la misma lección del hallazgo 22, un nivel más adentro: **una recomendación
que exige una acción que nadie va a estar allí para hacer no es una corrección.**

**Corregido (7 de septiembre de 2026).** La primera jornada en cada aula se
registra 24 h **automáticamente**. El disparador es el comando `E`, que ya es
obligatorio al instalar: no añade ningún paso a la operativa.

**Diseño y sus cuatro guardas:**

- **Caduca por FECHA, no por horas.** Se guarda en la flash el día en que se
  armó (`/ventext.txt`, `AAAA-MM-DD`) y `enVentanaHoraria()` devuelve `true`
  mientras la fecha del RTC coincida. Un corte de alimentación no regala otra
  noche, y un equipo olvidado no se queda registrando 24 h para siempre.
- **Solo al cambiar el NOMBRE del espacio.** Repetir `E Aula 3B / RT 0.85` tres
  días después para añadir la reverberación es la operativa documentada; si eso
  volviera a extender la jornada, gastaría memoria sin motivo. Se compara el
  primer campo, antes de la primera barra, sin distinguir mayúsculas.
- **Un `W` explícito el mismo día la cancela.** Una instrucción manual manda
  sobre una automática.
- **No se arma sin reloj fiable** (`ds3231_ok` y año ≥ 2020): sin fecha no hay
  día contra el que caducar. El firmware lo dice por consola y pide ajustar la
  hora con `T` antes de repetir el `E`.

**La caducidad se comprueba en `logRow()`**, no en el cambio de ventana horaria.
Es deliberado: con una ventana de 24 h configurada a mano no hay transición a
medianoche en la que engancharse, y `logRow()` ya tiene la fecha calculada y
pasa por ahí una vez por intervalo.

**Que el registro se corte a medianoche no es un problema.** Con ACH 0,3 h⁻¹
—un aula mal ventilada— partiendo de 800 ppm de exceso a las 19:00, a las 00:00
quedan 178 ppm, por encima del suelo de ajuste de la regresión (105 ppm con
Cext 420). El ajuste dispone de cinco horas y de rango dinámico de sobra. Y
`franjas.txt` ya traía la franja nocturna como `19:00-23:59` precisamente para
no cruzar medianoche: el diseño previo ya era coherente con esto.

**Coste.** Una jornada de 24 h por aula: unas 2.880 filas, ~150 KB de los
~1.380 KB disponibles (11 %). El aviso de memoria del firmware ya lo cubre.

**Verificación.** Prueba de integración con dos días —el primero de 24 h con la
marca `#VENTANA 0-24 auto`, el segundo de 12 h con `7-19 fin auto`—: el informe
etiqueta correctamente ambas jornadas (hallazgo 33), analiza el decaimiento
nocturno (298 min, 104 lecturas ajustadas, **ACH 2,2 con R² 0,996** frente al
2,2 con que se generaron los datos) y el aviso de «ningún decaimiento nocturno»
desaparece. El firmware pasa la comprobación de sintaxis con la API de Arduino
simulada.

**Documentación actualizada:** cabecera del firmware, ayuda de los comandos `E`
y `W`, `LEEME.txt` (paso 2 de la operativa, reescrito: ya no manda teclear
nada), §3.2 y §7.3 del documento de proyecto y §5.3 de la guía de puesta en
marcha. El aviso del informe cuando no encuentra decaimiento nocturno también se
reescribió: ya no dice «pon `W 0-24`», sino que explica que debería haberse
grabado sola y qué comprobar si no aparece.

---

---

## 36. LA DOCUMENTACIÓN DICE QUE LOS TRAMOS DE RUIDO SE EVALÚAN SOBRE EL PICO — RESUELTO

Apareció al aplicar la corrección del hallazgo 24, y no se habría visto de otro
modo: al ir a usar los tramos `DB_*` para graduar el nivel, hubo que comprobar
sobre qué magnitud estaban definidos. `umbrales.txt` decía:

> «Se evalua sobre dB_maxF (pico con ponderación Fast), comparable con un
> sonómetro normalizado.»

Y la tabla del documento de proyecto, en la misma línea: «≥ 55 elevado · ≥ 65 dB
riesgo — **evaluadas sobre el pico**».

**El análisis nunca los ha aplicado al pico.** `Grado-Estructural` los aplica al
nivel de fondo del aula vacía, y el eje de actividad al LAeq. Ambos son niveles
sostenidos.

Y el código tiene razón: los tramos están descritos con efectos que solo tienen
sentido sobre un nivel sostenido —«actividad docente habitual», «esfuerzo vocal
del docente»—, y un **pico** de 65 dB en un aula no es nada; se alcanza con una
silla al arrastrarse. Aplicarlos al pico marcaría de «riesgo» cualquier aula del
país. La documentación describía una versión del análisis que no existió.

**Corregido (7 de septiembre de 2026).** `umbrales.txt` y la tabla del documento
de proyecto dicen ahora «sobre niveles sostenidos (LAeq y fondo)». El comentario
de `umbrales.txt` incluye además el desglose de los cinco tramos con su efecto y
la advertencia de que el único con respaldo externo es el de 35 dB de fondo con
el aula **vacía** (OMS, ANSI/ASA S12.60); el resto son criterio propio.

---

# SIMPLIFICACIONES SIN MERMA DE EFICACIA

El proyecto ha acumulado tres rondas de correcciones y arrastra el sedimento
propio de eso: código escrito para un problema que dejó de existir, dos nombres
para una misma magnitud, una compatibilidad con un formato que nunca se publicó.
Nada de esto rompe nada; todo cuesta atención al que lo lea.

Las seis simplificaciones siguientes suman **unas 90 líneas de código y un
indicador**, y ninguna cambia un solo número del informe salvo donde se dice.

**Las seis se han aplicado el 7 de septiembre de 2026.** El script pasó de 1.842
a 1.933 líneas: bajó 90 por las simplificaciones y subió 180 por las
correcciones de los hallazgos 24 a 32, que añaden las bandas por eje, la
validación de fecha y los textos que declaran lo que antes se daba por supuesto.
El saldo neto es más código, pero con menos vías: una sola función de reparto,
una sola de ACH, un solo campo de tiempo y un solo formato de CSV.

---

### S1. Eliminar la compatibilidad con el CSV de 10 columnas — APLICADA

El parser admite dos formatos, el actual de 9 columnas y el anterior de 10, con
una bandera `antiguo`, un campo `max` duplicado y un bloque de diez líneas de
aviso en el informe.

El formato de 10 columnas **lo produjo una versión del firmware que nunca se ha
publicado**: el proyecto se está preparando ahora para su primera difusión
pública. No hay ningún usuario con datos en ese formato, y no puede haberlo.

- **Se elimina:** la bandera `$formatoAntiguo`, el campo `antiguo`, el campo
  `max` (idéntico a `maxF` en el formato actual), los tres ternarios del parser
  y el bloque «ATENCIÓN — DATOS DE FIRMWARE ANTERIOR».
- **Se gana:** ~25 líneas, y el informe deja de contener un aviso que ningún
  usuario verá nunca.
- **Riesgo:** ninguno, siempre que se haga **antes** de publicar. Después, no
  vale la pena tocarlo.

Nota: el campo `max` se usa en dos sitios (`$_.max - $_.fondo`, el rango
dinámico). Al eliminarlo hay que renombrar esos usos a `maxF`, que es lo que ya
contienen.

---

### S2. Eliminar `CalcularACH` y `CalcularACHBanda` — APLICADA

Código muerto, con un comentario que además afirma que se usa (hallazgo 32).
**40 líneas** entre las dos funciones y sus comentarios. Ninguna llamada.

Merece la pena conservar en un comentario de dos líneas, junto a
`CalcularACHRegresion`, la razón por la que el método de dos puntos se abandonó
—remite al hallazgo 16— para que nadie lo reintroduzca por parecer más simple.

---

### S3. Un solo campo de tiempo por fila — APLICADA

Cada fila lleva hoy `minutos` (minutos desde medianoche, para el reparto por
franjas) y `segundos` (segundos desde medianoche, para la regresión de ACH). Son
la misma magnitud con dos unidades, y la duplicación existe solo porque
`segundos` se añadió después, al corregir el hallazgo 19, sin tocar los
diecisiete sitios que ya usaban `minutos`.

Con un único campo `segundos`, los filtros de franja pasan de
`$_.minutos -ge $fr.ini` a `$_.segundos -ge $fr.ini*60`. Son diecisiete
sustituciones mecánicas.

**Beneficio real:** no es solo estética. Hoy conviven dos resoluciones de tiempo
y nada impide que un cálculo futuro use la equivocada, que es exactamente el
error del hallazgo 19. Un campo, un riesgo menos.

**Precaución:** hacerlo con una prueba de integración antes y después, y
comparar los dos informes línea a línea. Deben ser idénticos.

**Aplicada.** El campo `minutos` ha desaparecido; los diecisiete filtros de
franja comparan `$_.segundos` contra `$fr.ini*60`. La duración que devuelve la
regresión de ACH se renombró a `durMin` para que no se confunda con el campo
eliminado. Las tres pruebas de integración dan los mismos números que antes.

---

### S4. Retirar el índice de fluctuación — APLICADA

Es la simplificación con más contenido de todas, porque el índice **no es un
indicador**: como demuestra el hallazgo 24,

```
IF = (1 - 10^(-(LAeq - fondo)/10)) × 100
```

es una función biyectiva de `LAeq − fondo`, **dos cifras que el informe ya
imprime en la misma línea**. Decir «índice de fluctuación 84 %» es decir «LAeq
menos fondo son 8 dB» con más palabras y menos claridad.

Y tiene tres costes:

1. Se presta a leerse como el *intermittency ratio* de BREATHE, hasta el punto
   de que hay **cuatro descargos de responsabilidad** repartidos por el código,
   el `LEEME` y el documento de proyecto explicando que no lo es. Cuatro
   descargos para un número redundante es mala relación.
2. Arrastra el defecto de método del hallazgo 30 (mezclar una media aritmética
   de dB con un promedio energético).
3. Su umbral es el que degenera la valoración acústica (hallazgo 24).

**Qué se pierde:** nada cuantitativo. **Qué se gana:** el informe deja de
publicar un indicador propio que hay que defender, y la valoración acústica
pasa a apoyarse en el rango dinámico, que es lo que el proyecto declara desde el
principio como su indicador de fluctuación.

**Alternativa conservadora**, si se prefiere no perder la idea: sustituirlo por
la diferencia en decibelios, «LAeq − fondo = 8,0 dB», que es lo mismo, se
entiende sin explicación y no necesita ningún descargo.

---

### S5. Un solo nombre para el pico del intervalo — APLICADA

La misma magnitud se llama `dbMaxIntervalo` en el firmware, `dB_maxF` en el CSV
y `maxF` —más un alias `max`— en el script. Tres nombres y un alias para el
máximo de las muestras de 125 ms del intervalo.

**Aplicada.** Con S1 desapareció el alias `max` y los dos usos que quedaban
—los dos cálculos del rango dinámico— pasan a `maxF`. Queda `dbMaxIntervalo` en
el firmware, que es una variable interna y no sale a ninguna parte: la cabecera
documenta la correspondencia con `dB_maxF` del CSV, que es lo que el usuario ve.

---

### S6. Una sola vía para el reparto exógeno/endógeno — APLICADA

Hay dos implementaciones del mismo cálculo: `RepartoConBanda` en la sección de
detalle y las dos líneas de `PesoEnergetico` en la síntesis diaria. La segunda
es peor —sin banda, con la referencia peor elegida— y es la que emite la
recomendación cara (hallazgo 25).

Unificar en `RepartoConBanda` **corrige un hallazgo ALTO y elimina una
duplicación en el mismo movimiento**. Es la simplificación con mejor relación
entre esfuerzo y resultado de la lista, y la única que cambia números del
informe: los cambia porque los de ahora están mal.

**Aplicada**, junto con el hallazgo 25. `PesoEnergetico` sigue existiendo porque
`RepartoConBanda` lo usa internamente, pero ya no se llama desde ningún sitio
más: hay una sola vía y una sola regla de elección de la referencia.

---

# CONSISTENCIA ENTRE EL RIGOR Y LAS REGLAS DE DECISIÓN

El encargo de esta ronda no era solo corregir hallazgos sueltos, sino asegurar
que **las reglas con las que se decide sean tan rigurosas como las medidas sobre
las que deciden**. Conviene dejar escrito el criterio que ha resultado de eso,
porque es lo que debe aplicarse a cualquier regla futura.

**1. Toda comparación contra un umbral se hace con banda, y la banda llega hasta
la conclusión.** Antes el proyecto acotaba con banda el ACH y el reparto
exógeno/endógeno, y después comparaba esas cifras contra umbrales como si fueran
exactas. Ahora cada eje devuelve su grado *y* el rango de grados compatible con
la incertidumbre de su sensor (±0,2 °C, ±1,8 % HR, ±2 dB, y la banda ya
calculada del ACH), y la valoración lo dice cuando la letra no es firme:

```
     Con la incertidumbre de los sensores la nota está entre C y D:
     alguna variable cae junto a un umbral. Léase la categoría, no la letra.
```

**2. Se gradúa lo que tiene escala; lo que no la tiene, se describe.** Ésta es
la decisión de fondo del hallazgo 24, y va contra la tentación natural de meterlo
todo en una letra. El nivel sonoro tiene una escala con el efecto declarado de
cada tramo; la fluctuación no tiene ninguna. Fabricar un umbral para la
fluctuación es lo que degeneró la escala entera. Ahora la fluctuación se informa
con sus tres cifras y un perfil en palabras, que es exactamente tan preciso como
lo que se sabe.

**3. Cada regla declara su respaldo.** Temperatura y humedad salen del
RD 486/1997; la ventilación, del RITE y de las guías internacionales. El eje de
ruido en actividad **no tiene respaldo normativo**, y el informe lo dice cada vez
que lo emite, en la misma línea que la nota. No es un descargo genérico enterrado
en un anexo: va donde se lee la conclusión.

**4. La ausencia de dato no mejora el resultado.** Un eje sin medir se nombra y
se advierte de que la letra podría empeorar al medirlo. Antes se descartaba en
silencio del máximo.

**5. Una recomendación que cuesta dinero exige que la banda la sostenga.** La de
actuar sobre la envolvente solo se emite si el extremo **inferior** del reparto
supera el umbral. Cuando la banda no decide, el informe dice que no decide y qué
hacer para cerrarlo. Es preferible un «no lo sé, haz esto» a un número que
aparenta saberlo.

**6. Una sola vía por cálculo.** Las duplicaciones fueron el origen de dos
hallazgos ALTO (el 25 y, en la ronda anterior, el 18 a medio aplicar): existía la
versión buena y la versión rápida, y la síntesis usaba la rápida. Ahora hay una
función de reparto, una de ACH, un campo de tiempo y un formato de CSV.

---

# VERIFICACIONES PENDIENTES

Ninguna de estas puede hacerse sin el equipo delante, y siguen siendo el
principal punto débil del conjunto.

1. **Recalibrar el corte de fluctuación con datos reales.** El criterio de
   12 dB de rango dinámico elige la frase del perfil, no la nota, así que su
   impacto es mucho menor que antes — pero sigue siendo un número elegido a
   ojo entre dos arquetipos simulados. Con una decena de aulas medidas se
   podrá fijar mirando la distribución real.
2. **Confirmar el hueco de muestreo con el S8 desconectado** (hallazgo 29).
   Media hora: desconectar el UART del S8, registrar diez minutos y comprobar
   que aparecen filas `DB_INCOMPLETO` y que el recuento de eventos de las
   filas restantes no cae.
3. **Comprobar el automatismo de la primera jornada en campo** (hallazgo 35):
   instalar con el comando `E`, verificar al día siguiente que el CSV tiene la
   noche y que el equipo ha vuelto solo al horario.
4. **Las verificaciones de campo de la primera y la segunda revisión** siguen
   vigentes: el tren de impulsos breves, la comparación con sonómetro de clase 2
   y la medida del CO2 exterior en cada instalación.

---

# VALORACIÓN GENERAL

Las dos primeras revisiones encontraron errores en el camino del dato y los
corrigieron. Esta encuentra que **el camino del dato está bien y el último paso
no**: la síntesis de la jornada, que es lo único que va a leer un equipo
directivo, se construyó con reglas de decisión que nunca se comprobaron
numéricamente. Tres de ellas —el umbral del índice de fluctuación, el umbral del
peso exterior y el tratamiento de las variables sin dato— resultan degeneradas
al evaluarlas: una hace inalcanzables las dos mejores notas, otra compara contra
una banda que la contiene, la tercera premia no medir.

Hay una asimetría instructiva en eso. El proyecto ha sido escrupuloso al
declarar la incertidumbre de cada magnitud —bandas en el ACH, bandas en el
reparto, percentiles en vez de LA90— y no aplicó el mismo escrúpulo a los
umbrales con los que después compara esas magnitudes. Una cifra bien acotada
comparada contra un umbral arbitrario produce una conclusión tan poco fiable
como una cifra mal medida, y con peor apariencia de rigor.

Ninguno de los nueve hallazgos abiertos exige rediseñar nada. Seis son de una a
tres líneas. Los dos más laboriosos —el 25 y el 26— consisten en usar en la
síntesis el código que ya existe y funciona en la sección de detalle.

Y merece la pena registrar el patrón que se repite por tercera vez: **en los
tres casos en que una corrección quedó a medias, lo que faltaba era la parte que
dependía de una persona.** El hallazgo 22 documentó un procedimiento que exigía
recompilar; el 35, uno que exigía volver al aula a la mañana siguiente; el 25 y
el 33 aplicaron una corrección en un sitio y no en el otro, confiando en que
alguien recordara la simetría. El filtro que ha resultado más productivo en
estas tres revisiones no es «¿es correcto?», sino **«¿quién tiene que hacer esto,
y va a estar ahí para hacerlo?»**.

---

**Cierre (7 de septiembre de 2026).** Los trece hallazgos y las seis
simplificaciones están aplicados y verificados. De las tres revisiones sale un
proyecto en el que las tres cosas que importan están alineadas: **se mide con
cuidado, se calcula con la incertidumbre a la vista, y se concluye solo lo que
esa incertidumbre sostiene**. Lo que queda pendiente ya no es código: son las
cuatro verificaciones de campo, que ningún análisis puede sustituir.

Una última observación sobre el método de revisar. Los tres hallazgos graves de
esta ronda no se encontraron leyendo el código sino **evaluando sus reglas con
números**: invirtiendo la fórmula del índice para ver a qué diferencia en dB
correspondía su umbral, propagando ±2 dB por el reparto para ver cuánto valía
realmente un 30 %, y ejecutando el análisis con cuarenta filas de reloj perdido
para ver qué imprimía. Ninguno era visible a simple vista; los tres eran obvios
en cuanto se les puso un número delante. Es el mismo aprendizaje del hallazgo 19
de la segunda revisión, ampliado: **cuantificar no solo dimensiona el arreglo,
también revela cuál es el problema.**
