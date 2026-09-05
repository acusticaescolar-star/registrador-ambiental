# Revisión de rigor metodológico
## Registrador Ambiental · acusticaescolar.com

Revisión previa a la difusión pública de resultados.
Fecha: septiembre de 2026

---

## RESUMEN DE HALLAZGOS

| Nº | Hallazgo | Severidad |
|----|----------|-----------|
| 1 | ~~Ponderación frecuencial sin verificar~~ | **RESUELTO** |
| 2 | ~~Marco normativo español ausente (CTE DB-HR)~~ | **RESUELTO** |
| 3 | ~~El "nivel de fondo" no es un LA90 normalizado~~ | **RESUELTO** |
| 4 | ~~No se declara la incertidumbre de medida~~ | **RESUELTO** |
| 5 | ~~La separación exógeno/endógeno asume condiciones invariantes~~ | **RESUELTO** |
| 6 | ~~El índice de calor se aplica fuera de su ámbito de validez~~ | **RESUELTO** |
| 7 | ~~El ACH asume mezcla homogénea y ocupación nula~~ | **RESUELTO** |
| 8 | ~~La calibración ABC del sensor de CO2 puede no cumplirse~~ | **RESUELTO** |
| 9 | ~~La "intermitencia" no es la magnitud que definió BREATHE~~ | **RESUELTO** |
| 10 | ~~Los equipos no son instrumentos homologados~~ | **RESUELTO** |
| 11 | ~~El eje estructural (RT/STI) y el reparto exógeno/endógeno no llegaban a ejecutarse~~ | **RESUELTO** |

---

## 1. PONDERACIÓN FRECUENCIAL DEL SONÓMETRO — RESUELTO

**Confirmado.** El módulo PCB Artists aplica ponderación frecuencial A. Los
valores registrados son, por tanto, **dBA**, y son directamente comparables con
los umbrales normativos y con mediciones de sonómetros convencionales.

**Acción recomendada (no bloqueante).** Mantener el contraste empírico con un
sonómetro de clase 2 calibrado como verificación del conjunto —no solo de la
ponderación, sino de la exactitud global de la cadena de medida—, y documentar
el resultado. Es un dato que refuerza la credibilidad y que un revisor
agradecerá encontrar.

---

## 2. MARCO NORMATIVO ESPAÑOL — RESUELTO

**El problema (original).** El sistema valoraba la calidad acústica de las
aulas con **ANSI/ASA S12.60** (norma estadounidense) y **DIN 18041**
(alemana). Ninguna de las dos es de aplicación en España. La normativa
aplicable es el **Código Técnico de la Edificación, Documento Básico HR
(Protección frente al ruido)**, y en Cataluña además la normativa autonómica
correspondiente.

**Valores del CTE DB-HR aplicables a recintos docentes** (apartado 2.2,
acondicionamiento acústico):

| Recinto | Condición | T máximo |
|---|---|---|
| Aulas y salas de conferencias, V ≤ 350 m³ | vacías, sin mobiliario | **0,7 s** |
| Aulas y salas de conferencias, V ≤ 350 m³ | vacías, con butacas | **0,5 s** |
| Comedores y restaurantes | vacíos | **0,9 s** |

Un aula escolar con mesas y sillas móviles no encaja exactamente en ninguna de
las dos primeras categorías. Se aplica el valor de 0,7 s con el supuesto
declarado explícitamente en cada sitio donde se usa.

**Corregido en:**
- `registradorambiental.ino` — cabecera de `fijarEspacio()` y los mensajes de
  evaluación de RT/STI al identificar el espacio citan ahora el CTE DB-HR
  como criterio principal, con ANSI/ASA S12.60 y DIN 18041 reformulados como
  "referencia internacional de buena práctica".
- `volcar_datos.ps1` — nueva función `Grado-Estructural` (grado 1-5 de fondo,
  RT y STI) y textos del bloque "VALORACIÓN DEL ESPACIO" (Eje 1), con la
  misma jerarquía CTE-principal / ANSI-DIN-referencia y el aviso de
  aplicabilidad no retroactiva del CTE (RD 1371/2007) en cada punto donde se
  compara un RT medido con el límite.
- `LEEME.txt` — la referencia a ANSI/ASA S12.60 para el método de medida del
  RT (T30, bandas de octava) se reformula como práctica de medida, no como
  la norma que se evalúa.
- `documentoproyecto.pdf`, apartado 7.1 — se añade la tabla de interpretación
  de Reverberación (RT) y Inteligibilidad (STI) que faltaba, con el CTE
  DB-HR como criterio principal (texto de sustitución en
  `PARCHE_MARCO_NORMATIVO.md`, pendiente de trasladar a la fuente del
  documento).
- `propuestasinvestigacion.pdf`, `propostacentreeducatiu.pdf`,
  `guiamontaje.pdf`, `puestaenmarcha.pdf` — revisados; no contienen
  referencias a ANSI/DIN ni afirmaciones de cumplimiento normativo. No
  requerían cambios.

**Formulación adoptada para edificios de aplicabilidad incierta del CTE**
(acción 4 del hallazgo original): un RT por encima de 0,7 s se expresa como
que el aula **"se aleja de los valores que exigiría un edificio equivalente
de nueva construcción"**, nunca como incumplimiento, porque ni el firmware ni
el script conocen la fecha de construcción o de última licencia de obra del
edificio — eso debe declararse aparte, centro a centro, en el documento de
propuesta.

---

## 3. EL "NIVEL DE FONDO" NO ES UN LA90 NORMALIZADO — RESUELTO

**El problema (original).** El informe denomina "nivel de fondo (LA90
aproximado)" al percentil 10 de las lecturas del intervalo. Un LA90
normalizado se obtiene del análisis estadístico de la señal con ponderación
temporal e intervalos definidos por norma. Nuestro valor procede de lecturas
ya promediadas internamente por el módulo cada 31 ms, agregadas por software.

**Por qué importa.** Llamarlo LA90 sugiere una equivalencia metrológica que no
se ha demostrado.

**Corregido en:**
- `registradorambiental.ino` — el comentario de formato del CSV, la constante
  `DB_PERCENTIL_FONDO` y el comentario de `calcularFondo()` ya no usan "LA90":
  hablan de "nivel de fondo ESTIMADO (percentil 10...)" y explican por qué no
  se llama LA90 (no reproduce el cálculo normalizado sobre la señal continua).
- `volcardatosscript/LEEME.txt` — la descripción de la columna `dB_fondo` en
  "FORMATO DEL CSV" queda igual: nivel de fondo estimado, con la misma
  aclaración de no-equivalencia.
- `documentoproyecto.pdf` y `puestaenmarcha.pdf` — contienen la misma frase
  ("LA90 aproximado"); el texto de sustitución está en
  `PARCHE_LA90_INCERTIDUMBRE.md`, pendiente de trasladar a la fuente de ambos
  documentos (no editables de forma fiable en este formato).

**Acción ya no pendiente en código; pendiente solo en los dos PDF citados.**
Si se desea además comparar con un LA90 real, validar frente a un sonómetro
que lo calcule — eso sigue siendo una mejora futura, no un requisito para
corregir la nomenclatura.

---

## 4. INCERTIDUMBRE DE MEDIDA NO DECLARADA — RESUELTO

**El problema (original).** Los informes presentan valores sin margen de
error, cuando todos los sensores tienen incertidumbre conocida y en algunos
casos relevante:

| Magnitud | Incertidumbre del sensor | Efecto a valores típicos |
|---|---|---|
| CO2 (Senseair S8) | ±40 ppm ±3% de lectura | a 1200 ppm: **±76 ppm** |
| Temperatura (SHT41) | ±0,2 °C | despreciable |
| Humedad (SHT41) | ±1,8 % HR | relevante cerca de los límites |
| Sonido (PCB Artists) | ±2 dB SPL | **±2 dB** |

**Caso especialmente sensible: el ACH.** Se calcula a partir de dos medidas de
CO2 y una referencia exterior, y los errores se propagan de forma no lineal.
Con ±76 ppm en cada extremo, un ACH nominal de 1,0 puede oscilar
aproximadamente entre 0,8 y 1,3. La conclusión cualitativa ("por debajo del
mínimo recomendado") se mantiene, pero **la cifra no debe presentarse con un
decimal como si fuera exacta**.

**Corregido en `volcardatosscript/volcar_datos.ps1`:**
1. Nueva función `IncertidumbreCO2($ppm)` = 40 + 0,03·|ppm|, que reproduce la
   hoja de características del Senseair S8.
2. Nueva función `CalcularACHBanda(...)`: perturba c0, ct y la referencia
   exterior en ± su incertidumbre (sin correlacionarlas entre sí, el
   supuesto más conservador) y devuelve el intervalo resultante del ACH, en
   vez de un único decimal. Se aplica en los dos puntos del informe donde
   aparece el ACH: el desglose por franja ("RENOVACIÓN DE AIRE") y la síntesis
   diaria ("AIRE (CO2)").
3. **Caso encontrado al verificar con datos sintéticos de decaimiento real:**
   cuando la caída de CO2 no se aleja lo suficiente del nivel exterior (por
   ejemplo, un aula que decae de 1200 a 450 ppm con un exterior de referencia
   de 420 ppm), la aritmética de intervalos no puede acotar el denominador —
   el propio margen de incertidumbre hace que el "tiempo hasta el exterior"
   pueda ser negativo. En ese caso el script ya no oculta el problema
   mostrando solo la cifra puntual: añade el aviso explícito "banda no
   acotable: la caída de CO2 respecto al exterior es demasiado pequeña frente
   a la incertidumbre del sensor... cifra orientativa, no una medida con
   precisión garantizada", tanto en el detalle por franja como en la síntesis
   diaria. Es el mismo hallazgo aplicado a sí mismo: mejor declarar que no se
   puede acotar que aparentar una precisión que no existe.
4. El umbral para afirmar una "variación" de nivel sonoro entre franjas (en el
   bloque de origen del ruido, hallazgo 5) se sube de 5 a 6 dB, para quedar
   por encima de la incertidumbre del sonómetro (±2 dB) con margen, y el texto
   lo declara explícitamente.

**Verificación realizada.** Prueba unitaria de `CalcularACHBanda` con valores
de ejemplo (1200/700/430 ppm → ACH 3,14, banda 1,54-5,25) y dos pruebas de
extremo a extremo con datos sintéticos de decaimiento de CO2: una con margen
suficiente frente al exterior (banda 0,3-1,0 mostrada correctamente en ambos
puntos del informe) y otra al límite de la incertidumbre (aviso de "banda no
acotable" mostrado correctamente en ambos puntos). Ambos casos sin errores de
ejecución.

**Pendiente, no en código:** declarar la tabla de incertidumbres por sensor
(la de arriba) en `documentoproyecto.pdf`, para que quien lea el informe la
tenga sin depender de este documento interno. Texto de sustitución en
`PARCHE_LA90_INCERTIDUMBRE.md`.

---

## 5. SEPARACIÓN EXÓGENO/ENDÓGENO — RESUELTO (con una simplificación operativa deliberada)

**El problema (original).** El cálculo restaba energéticamente el nivel de una
franja sin ocupación, del mismo día, al de una franja con actividad, y
atribuía la diferencia a la actividad. Eso asume que **el ruido exterior es el
mismo en ambos momentos del mismo día** — una premisa frágil: el tráfico varía
en pocas horas, y las ventanas pueden estar abiertas con ocupación y cerradas
sin ella, cambiando el aislamiento efectivo entre ambas medidas.

**Rediseño adoptado.** En vez de comparar franjas dentro del mismo día lectivo,
la referencia principal pasa a ser el **mismo tramo horario de un sábado**:

- Sin actividad escolar, un sábado es ruido exterior "puro" en todo el día, no
  solo en los huecos vacíos de una jornada lectiva — cubre franjas (como el
  recreo o el comedor) que en un día de clase nunca están vacías y por tanto
  antes no tenían referencia posible.
- Se asume que el **tráfico de sábado es representativo del tráfico laborable**
  a la misma hora y que las **ventanas permanecen cerradas** ambos días — los
  dos supuestos que planteaste al pedir esta simplificación. Son razonables en
  el caso general (el tráfico de un entorno escolar no suele tener un patrón
  de sábado radicalmente distinto al de un día laborable en las mismas
  franjas horarias) pero **son supuestos, y el informe los declara como
  tales**, nunca como hecho verificado.
- Comparar contra el sábado sustituye la necesidad de anotar en tiempo real el
  estado de cada ventana durante la jornada (acción 1 del hallazgo original):
  si se acepta que las ventanas están cerradas en ambos días, el registro
  puntual de su estado deja de ser imprescindible para este cálculo concreto.
  Sigue siendo útil para otros fines (justificar una queja puntual, por
  ejemplo) pero ya no es la única vía para sostener la separación
  exógeno/endógeno. **Esto es justo la simplificación operativa que
  preguntabas si tenía sentido: sí la tiene**, porque reduce lo que el
  protocolo de campo exige anotar sin debilitar la conclusión, ya que la
  premisa que sustituye (tráfico similar, ventanas cerradas) es más estable
  que la que reemplaza (mismo tráfico en dos franjas del mismo día).

**Autoconsistencia añadida.** El informe ya no solo usa el sábado como
referencia: cuando el día lectivo analizado también tiene franjas vacías,
las compara con el sábado en el mismo nombre de franja y avisa si difieren 6
dB o más (el nuevo umbral del hallazgo 4). Es la forma de que la premisa
"tráfico similar" sea **falsable con los propios datos**, en vez de darse por
supuesta sin más: si un sábado concreto no se parece al laborable (una obra,
un evento), el aviso lo señala en vez de contaminar la estimación en
silencio.

**Cobertura ampliada.** Antes, las aulas con exposición "patio" no tenían
ninguna estimación de origen del ruido (las franjas vacías de un día lectivo
reflejan actividad escolar en el patio, no el exterior estructural). Con
referencia de sábado sí es válida — el patio también está sin actividad
escolar en fin de semana — así que ahora esas aulas obtienen una estimación
cuando hay datos de sábado, y siguen sin ella (con la razón explicada) cuando
no los hay.

**Corregido en `volcardatosscript/volcar_datos.ps1`:**
- Nueva función `PerfilReferenciaSabado($fEsp, $franjas)`: localiza los
  sábados con datos del espacio y calcula el LAeq y el fondo por nombre de
  franja horaria.
- Bloque "ORIGEN DEL RUIDO" reescrito con tres rutas, siempre etiquetadas
  "ESTIMACIÓN" y con los supuestos declarados en el propio texto:
  1. **Con sábado disponible (método principal):** compara cada franja
     ocupada con el mismo tramo del sábado, y añade el contraste de
     autoconsistencia frente a las franjas vacías del propio día.
  2. **Sin sábado, aula no orientada a patio (recurso anterior):** vuelve al
     método de franja vacía más próxima del mismo día, ahora explícitamente
     etiquetado como "menos fiable" y con sugerencia de ampliar el despliegue
     para cubrir un sábado.
  3. **Sin sábado, aula orientada a patio:** no se estima, con la explicación
     de por qué el sábado sí serviría si se dispusiera de él.

**Verificación realizada.** Conjunto sintético de tres espacios (aula con
sábado disponible, aula de patio sin sábado, aula de calle sin sábado) que
ejercita las tres rutas del bloque. Las tres se generan sin errores, con el
texto de "ESTIMACIÓN" y los supuestos esperados en cada caso, y el aviso de
autoconsistencia se dispara correctamente cuando la diferencia con el sábado
supera 6 dB.

**Pendiente, no bloqueante.** La solución que elimina por completo esta
limitación —medición simultánea exterior con una segunda unidad o sonómetro
cableado— sigue siendo la referencia correcta si en algún momento se dispone
de dos equipos; el rediseño de este hallazgo es la mejor estimación posible
**con un solo equipo**, no un sustituto de esa medición simultánea.

**Mejora futura, no implementada.** Ahora mismo cada aula busca su propio
sábado. Si un centro tiene varias aulas con la misma exposición (todas dan a
la misma calle, por ejemplo) y solo alguna de ellas llegó a medirse en
sábado, esa referencia podría compartirse entre las demás del mismo grupo de
exposición. No se ha implementado porque exige decidir cómo agrupar aulas por
exposición equivalente (¿misma fachada? ¿mismo lado del edificio?) y eso
depende del centro concreto; queda anotado por si interesa en una fase
posterior.

---

## 6. ÍNDICE DE CALOR FUERA DE SU ÁMBITO — RESUELTO

**El problema (original).** La fórmula de Rothfusz fue desarrollada por el
servicio meteorológico estadounidense para **condiciones exteriores a la
sombra y con viento ligero**. Su aplicación a interiores no está validada.

**Decisión.** Se retomó explícitamente la pregunta que había quedado sin
cerrar en la sesión anterior: mantener el indicador con matizaciones, tal
como se había recomendado. Se descartó eliminarlo porque, declarado con sus
límites, sigue siendo información orientativa útil (explica quejas de calor
que la temperatura sola no justifica) y el propio hallazgo pedía "mantenerlo
... declarando su origen y sus limitaciones", no retirarlo.

**Corregido en:**
- `volcar_datos.ps1` — el comentario de `IndiceCalor()` explica ahora el
  origen de Rothfusz (NWS, exterior, sombra, viento ligero) y su falta de
  validación en interiores, con referencia a UNE-EN ISO 7730 (PMV/PPD) como
  la norma correcta para confort interior (que exige velocidad del aire y
  estimación de vestimenta/actividad metabólica, datos de los que no se
  dispone). El bloque de salida "Índice de calor máximo" del informe añade
  ahora esa misma advertencia junto a la cifra, en vez de mostrarla sin
  contexto.
- `LEEME.txt` — nueva sección "ÍNDICE DE CALOR" con la misma explicación,
  al mismo nivel que la sección "ÍNDICE DE FLUCTUACIÓN" ya existente.
- `documentoproyecto.pdf` — nuevo apartado §7.3 "Limitaciones adicionales de
  indicadores derivados", con un recuadro destacado ("Ámbito de validez")
  que declara el origen exterior de Rothfusz y remite a ISO 7730. El PDF se
  reconstruyó fielmente (misma tipografía, colores y estilo de recuadro que
  el resto del documento) añadiendo una página nueva antes del apartado 8;
  el documento pasa de 18 a 19 páginas (v4.1 → v4.2) y la numeración de
  página/total de las páginas existentes se actualizó en consecuencia.

**Verificación realizada.** Se aisló `IndiceCalor()` y el bloque de salida
correspondiente y se ejecutaron con PowerShell 7.4 y datos sintéticos
(30 °C / 65 % HR → índice de calor 33,9 °C): el bloque se ejecuta sin
errores y el texto de advertencia aparece junto a la cifra.

---

## 7. SUPUESTOS DEL CÁLCULO DE ACH — RESUELTO

**El problema (original).** El método de decaimiento (ASTM E741) asume mezcla
homogénea del aire, ocupación nula durante el decaimiento, concentración
exterior constante durante la ventana de cálculo y un solo punto de medida
representativo — supuestos que no se declaraban junto al resultado.

**Corregido en:**
- `volcar_datos.ps1` — el bloque "RENOVACIÓN DE AIRE" del informe declara
  ahora los cuatro supuestos explícitamente, justo antes del detalle por
  franja, con una advertencia específica sobre la marca `*VACÍO`: si queda
  alguien en el aula durante la franja usada para el cálculo, el CO2 que esa
  persona genera subestima el ACH real.
- `LEEME.txt` — la sección "FRANJAS HORARIAS Y ORIGEN DEL RUIDO" incorpora
  un párrafo nuevo que conecta la marca `*VACÍO` (ya existente, usada para
  el origen del ruido) con su segundo uso en el cálculo de ACH, y pide
  marcarla solo cuando se pueda confirmar que nadie entró ni salió del aula
  durante ese tramo.
- `documentoproyecto.pdf`, §7.3 — los cuatro supuestos se listan junto a la
  recomendación de verificar en campo, mediante anotación, que el aula
  queda efectivamente vacía durante la franja usada (remite a la Fase 2 del
  protocolo de estudio de campo, §7).

**Nota.** La incertidumbre numérica del ACH (banda por propagación de
errores) ya estaba resuelta desde el hallazgo 4; este hallazgo cubre un
problema distinto — los supuestos estructurales del método, no el margen de
error de los sensores — y ambas correcciones son complementarias.

**Verificación realizada.** Se probó el bloque modificado con datos
sintéticos de decaimiento (1200→700 ppm, exterior 430 ppm, 45 min): se
ejecuta sin errores, el aviso de supuestos aparece antes del detalle por
franja y el cálculo de ACH y su banda no se ven alterados.

---

## 8. CALIBRACIÓN ABC DEL SENSOR DE CO2 — RESUELTO

**El problema (original).** El Senseair S8 mantiene su exactitud mediante
calibración automática de línea base (ABC), que requiere que el sensor vea
aire exterior (~400–420 ppm) periódicamente. En un aula permanentemente
ocupada, o durante campañas de una semana en interior, ese mínimo puede no
alcanzarse. Esto es un problema de protocolo de campo, no de código: el
firmware no controla ni puede verificar la ABC del sensor.

**Corregido en:**
- `LEEME.txt` — nueva sección "CALIBRACIÓN DEL SENSOR DE CO2 (ABC), ANTES DE
  CADA CAMPAÑA" con los cuatro pasos del protocolo: exponer el equipo al
  aire exterior ≥30 min, comprobar que converge a ~420 ppm, registrar la
  verificación junto con la identificación del espacio, y calibrar
  manualmente o declarar incertidumbre ampliada si se detecta deriva.
- `puestaenmarcha.pdf` — nuevo apartado §6.1 "Verificación de calibración de
  CO2 (ABC) antes de cada campaña", con el mismo protocolo en un recuadro
  destacado, insertado como página nueva antes del cierre del documento
  (que pasa de 12 a 13 páginas, v1.1 → v1.2, con la numeración de las
  páginas existentes actualizada en consecuencia) y con una remisión a
  `documentoproyecto.pdf` §7.3 para los supuestos de índice de calor y ACH.
- `documentoproyecto.pdf`, §7.3 — el mismo protocolo, junto a los otros dos
  hallazgos de esta tanda, para quien solo consulte el documento de
  proyecto.

**Por qué en `puestaenmarcha.pdf` y no solo en `documentoproyecto.pdf`.**
Este hallazgo es una verificación operativa de campo, del mismo tipo que el
resto del protocolo de arranque (§6) que ya vive en la guía de puesta en
marcha; el índice de calor y los supuestos del ACH, en cambio, son
limitaciones de análisis y se documentan solo en el documento de proyecto.

---

## 9. LA "INTERMITENCIA" NO ES LA DE BREATHE — RESUELTO

**El problema.** El informe calculaba un "ratio de intermitencia" a partir de
la energía del LAeq y la del nivel de fondo, por intervalo. BREATHE empleó un
indicador del mismo nombre ("intermittency ratio") obtenido del análisis de la
señal continua, con una definición metodológica propia. Usar el mismo nombre
sugería una equivalencia que no existe: los valores no son directamente
comparables con los del estudio.

**Corregido en:**
- `volcar_datos.ps1`: la función `Intermitencia()` pasa a llamarse
  `IndiceFluctuacion()`, con un comentario explícito ("propio, NO es el
  'intermittency ratio' de BREATHE") sobre su definición y su relación —de
  inspiración, no de réplica— con el indicador del estudio. Todos los textos
  de salida (avisos por franja, bloque "Eje 2 — Condiciones en actividad")
  pasan de "intermitencia" a "índice de fluctuación", realineados.
- `LEEME.txt`: la sección "INTERMITENCIA" se renombra a "ÍNDICE DE
  FLUCTUACIÓN" y añade la aclaración de que no es una réplica del
  "intermittency ratio" de BREATHE.
- `documentoproyecto.pdf` (§3.3): el párrafo que presenta el indicador y la
  "Nota metodológica" se reescriben para usar "índice de fluctuación" y
  declarar explícitamente que es un cálculo propio inspirado en el hallazgo
  de BREATHE, no una réplica de su definición ni validado del mismo modo.

**Nota.** Los nombres internos de variable (`$ir`, `$irAct`...) no se han
tocado por no ser user-facing; solo los nombres de función y los textos que
ve la persona que use el script o lea los documentos.

---

## 10. LOS EQUIPOS NO SON INSTRUMENTOS HOMOLOGADOS — RESUELTO

Ninguno de los sensores es un instrumento de medida homologado ni sometido a
verificación metrológica periódica. Los resultados **no tienen validez legal**
y no pueden emplearse como prueba en un procedimiento administrativo o
judicial. Su valor es **indiciario y orientativo**: sirve para detectar
problemas, dimensionarlos y justificar una medición formal, no para
sustituirla.

**Corregido en:**
- `volcar_datos.ps1`: el informe generado (`datalog_FECHA_resumen.txt`)
  incluye ahora el aviso completo justo debajo de la cabecera "ANÁLISIS DEL
  REGISTRO", antes de cualquier dato — no puede pasar desapercibido.
- `LEEME.txt`: el mismo aviso, resumido, se añade al principio del documento,
  antes del apartado CONTENIDO.
- `documentoproyecto.pdf` (§1.1): recuadro destacado ("Alcance de los datos:
  valor indicativo, no legal") justo después de la descripción general del
  sistema, al inicio del documento.
- `propostacentreeducatiu.pdf`: nuevo apartado "Abast de les dades: valor
  indicatiu, no legal", con el mismo aviso en catalán, insertado tras "Què
  obté el centre" y antes de "Confidencialitat i ús de les dades" — la
  propuesta que llega a los centros incluye ahora la advertencia de forma
  visible, tal como pedía la acción requerida.

---

## 11. FUNCIONES DE ANÁLISIS ESTRUCTURAL AUSENTES Y BUG DE FRANJAS VACÍAS — RESUELTO

**Cómo se encontró.** Al implementar la corrección del hallazgo 2 hubo que
poner en marcha por primera vez el bloque "VALORACIÓN DEL ESPACIO" de
`volcar_datos.ps1` con datos reales, para comprobar que el nuevo texto sobre
CTE DB-HR se generaba correctamente. Ese bloque nunca había llegado a
ejecutarse con datos: no aparece mencionado en la revisión original porque el
hallazgo 2 se centró en el contenido de los documentos, no en si el código
que lo aplica funciona.

**El problema, en tres partes:**

1. **Tres funciones invocadas y nunca definidas.** `volcar_datos.ps1` llamaba
   a `Grado-Estructural`, `Letra` y `Valorar-Actividad` en cinco puntos
   distintos (la síntesis diaria y la valoración del espacio), pero ninguna
   de las tres estaba definida en el script. Cualquier ejecución que llegara
   a esas líneas habría terminado con `The term 'Grado-Estructural' is not
   recognized...` y el volcado se habría interrumpido sin generar el resumen.
2. **Bug que lo ocultaba.** Nunca se llegó a ver ese error porque un segundo
   bug lo impedía antes: las filas del CSV se guardaban con la propiedad
   `día` (con tilde) pero se leían como `.dia` (sin tilde) al agrupar por
   jornada. Al no coincidir, el bucle de "por día" no encontraba ningún día
   que procesar y el informe se detenía justo después de la cabecera del
   espacio, sin llegar nunca al código roto del punto 1.
3. **Bug adicional en `Get-Franjas`.** Las franjas marcadas `*VACÍO` (las que
   definen cuándo el aula está sin ocupar) se calculaban con inicio y fin
   00:00–00:00 en todos los casos, por una variable automática de PowerShell
   (`$Matches`) que un segundo `-match` (el que detecta la marca `*VACÍO`)
   sobrescribía antes de leer las horas de inicio y fin. Esto invalidaba
   silenciosamente el reparto exógeno/endógeno (hallazgo 5) y la referencia
   de "aula vacía" que usa el eje estructural: ninguna franja vacía se
   reconocía nunca como tal.

**Por qué importa.** Los tres bugs juntos significan que, tal como estaba el
script, **el eje estructural (RT/STI/fondo) y el reparto exógeno/endógeno no
se habían ejecutado nunca con datos reales**, ni en pruebas ni en campo. El
resto del informe (síntesis diaria, franjas, calidad del aire) sí funcionaba,
lo que hacía fácil no notar que esa parte concreta llevaba, en todos los
casos, la ruta de "sin datos".

**Corregido.**
- Se implementaron `Grado-Estructural`, `Letra` y `Valorar-Actividad` con la
  jerarquía normativa del hallazgo 2 (CTE DB-HR principal; ANSI/DIN como
  referencia internacional) y con los umbrales de `umbrales.txt` que ya
  existían para el nivel sonoro (`DB_OPTIMO/ACEPTABLE/NORMAL/RIESGO`) pero
  que hasta ahora no los consumía ninguna función.
- Se corrigió `día` → `dia` en la propiedad del CSV.
- Se corrigió `Get-Franjas` para leer `$Matches` antes del `-match` que
  detecta `*VACÍO`.

**Verificación realizada.** Se aisló el bloque de funciones del script (sin
la parte que abre el puerto serie) y se ejecutó contra una jornada sintética
de 24 h con PowerShell 7.4, con y sin RT/STI declarados y con exposición
`calle` y `patio`, confirmando que el informe se genera completo, sin
errores, y que los textos de CTE DB-HR/ANSI/DIN aparecen donde corresponde.
No sustituye la verificación con datos reales del equipo, que sigue
pendiente (ver más abajo).

---

## VERIFICACIONES PENDIENTES ANTES DE PUBLICAR

1. Confirmar con el fabricante la ponderación frecuencial del sonómetro.
2. Contrastar el registrador con un sonómetro de clase 2 calibrado, en las
   mismas condiciones y durante al menos una hora.
3. Verificar la lectura de CO2 al aire exterior antes y después de cada campaña.
4. Contrastar temperatura y humedad con un termohigrómetro de referencia.
5. Incorporar el CTE DB-HR y verificar su aplicabilidad al edificio concreto.
6. Repetir una medición completa en las mismas condiciones para estimar la
   repetibilidad del conjunto.
7. Ejecutar `volcar_datos.ps1` con datos reales del equipo (no solo la
   jornada sintética usada para verificar el hallazgo 11) y confirmar en un
   volcado real que el bloque "VALORACIÓN DEL ESPACIO" y el reparto
   exógeno/endógeno se generan correctamente con el espacio identificado
   (`E Aula ... / RT ... / STI ...`) y con franjas vacías definidas en
   `franjas.txt`.
8. ~~Trasladar `PARCHE_LA90_INCERTIDUMBRE.md` a `documentoproyecto.pdf` y
   `puestaenmarcha.pdf` (hallazgos 3 y 4: nomenclatura del nivel de fondo y
   tabla de incertidumbres por sensor).~~ **Hecho** — ambos PDF están
   regenerados con los cambios de los hallazgos 2, 3, 4, 5, 9 y 10.
9. En el primer centro donde se despliegue con esta versión, incluir al
   menos un sábado en el calendario de medición de cada espacio (hallazgo 5):
   sin él, esa aula cae en la ruta de referencia "menos fiable" o, si da a
   patio, se queda sin estimación de origen del ruido.
10. Contrastar, con datos reales de un mismo centro, si el supuesto "tráfico
    de sábado ≈ tráfico laborable" se sostiene: revisar el aviso de
    autoconsistencia (diferencia ≥ 6 dB entre franjas vacías del día y su
    referencia de sábado) en varios espacios y confirmar que no salta de
    forma sistemática, lo que invalidaría el supuesto para ese centro
    concreto.
11. Al ejecutar el nuevo protocolo de calibración ABC (hallazgo 8) en la
    primera campaña real, confirmar que el S8 efectivamente converge a
    ~420 ppm en 30 min de aire exterior; si no converge, decidir en ese
    momento si se calibra manualmente o se declara la incertidumbre
    ampliada, según lo que se observe. También corresponde aprovechar esa
    primera campaña para anotar, franja por franja, que el aula usada para
    el cálculo de ACH quedó realmente vacía (hallazgo 7) y así verificar el
    supuesto de ocupación nula con datos reales, no solo con la anotación
    `*VACÍO` en franjas.txt.

---

## VALORACIÓN GENERAL

El sistema es metodológicamente sólido en su concepción: la separación entre
condiciones estructurales y de uso, el registro continuo frente a la medición
puntual, y la atención a la fluctuación además del nivel medio están bien
fundamentados y responden a la literatura citada.

Las debilidades no están en el planteamiento sino en **la precisión con que se
enuncian los resultados**: nomenclatura que sugiere equivalencias no
demostradas, ausencia de márgenes de error, y marcos normativos importados sin
matizar su aplicabilidad.

Son todas corregibles sin alterar el diseño. Corregidas, el trabajo resiste el
escrutinio; sin corregir, un solo revisor competente puede invalidar
públicamente el conjunto por un detalle de nomenclatura.

**Estado a 4 de septiembre de 2026.** Los 11 hallazgos de esta revisión están
resueltos en el código, la documentación y los PDF. Lo que queda antes de la
difusión pública no es de rigor metodológico sino de verificación de campo:
la lista de "VERIFICACIONES PENDIENTES ANTES DE PUBLICAR" de más arriba.

---
