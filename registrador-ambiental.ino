// ============================================================
// datalogger_test_v1.ino
// Monitor Ambiental - firmware de prueba, verificación y logging
//
// Placa: YD-ESP32-S3 - WROOM-1-N16R8 - PCB 2022-V1.3
// Board Arduino IDE: "ESP32S3 Dev Module"
// Flash Size: 16MB (128Mb) | PSRAM: OPI PSRAM | Core 3.2.0
// USB CDC On Boot: ENABLED (el monitor serie sale por USB nativo)
//
// IMPORTANTE (hallazgos de la puesta en marcha):
//  - Cerrar el puente IN-OUT de la placa para que el pin 5V de VBUS
//    alimente el S8. Con el puente abierto el S8 no mide (CO2 = 0).
//  - USB CDC On Boot debe estar en Enabled o no se ve el monitor.
//
// Librerías (Library Manager):
//   - Sensirion I2C SHT4x  (Sensirion)
//   - S8_UART              (jcomas)  -> el fichero es s8_uart.h (minúsculas)
//   - RTClib               (Adafruit)
//   - LittleFS             (incluida en el core ESP32)
//
// Pines verificados:
//   SDA  GPIO8  col53B  |  SCL  GPIO9  col56B
//   S8 TX GPIO1 col45J -> S8 UART_RxD col2J
//   S8 RX GPIO2 col46J <- S8 UART_TxD col3J
//   S8 5V ESP32 col62B -> S8 G+ col1A  |  S8 GND -> S8 G0 col2A
//   LED GPIO48 col57J
//
// CONSUMO:
//   La CPU trabaja a 80 MHz en lugar de los 240 MHz por defecto. El firmware
//   pasa casi todo el tiempo esperando, de modo que la reducción no afecta al
//   funcionamiento y baja el consumo de ~84 a ~64 mA: unos 7,2 días de
//   autonomía con un banco de 20.000 mAh, frente a 5,5 días a 240 MHz.
//
// VENTANA HORARIA:
//   Por defecto solo se registra entre las 07:00 y las 19:00, todos los días
//   (incluidos sábados y domingos, para permitir estudios de ruido exterior en
//   fin de semana). Fuera de ese horario el sistema queda en espera sin
//   escribir. Autonomía a 30 s de intervalo: >11 días incluso en el peor caso.
//   La ventana se cambia con el comando W (p. ej. W 0-24) y se guarda en la
//   flash. LA PRIMERA JORNADA EN CADA AULA SE REGISTRA 24 h AUTOMÁTICAMENTE:
//   la dispara el comando E al identificar un aula nueva, caduca sola al
//   cambiar de día y la cancela un W explicito. La noche es el único tramo con
//   ocupación nula garantizada, que es justo el supuesto que exige el cálculo
//   de renovación de aire (revision-rigor-2.md, 22; revision-rigor-3.md, 35).
//
// FORMATO CSV:
//   timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,eventos,estado
//   9 columnas. La columna dB_max (pico crudo de 31 ms) se eliminó al
//   corregir los hallazgos 12, 13 y 15 de revision-rigor-2.md: ver la nota
//   "PONDERACIÓN TEMPORAL" más abajo.

//   El campo "estado" marca la FIABILIDAD del dato, no la valoración de las
//   condiciones (esa se hace al analizar, con la tabla del documento de
//   proyecto). Vale "OK" o una o más de estas marcas separadas por ';':
//     ERR_TH ERR_CO2 ERR_DB   sensor sin respuesta valida en esa lectura
//     ERR_RTC                 marca de tiempo no fiable
//     CO2_CALENTANDO          el S8 aun no da lectura (~30 s tras encender):
//                             co2_ppm vale 0 y NO debe usarse como medida
//     DB_INCOMPLETO           el intervalo se observo solo en parte: el hilo
//                             estuvo bloqueado (típicamente el sensor de CO2
//                             sin responder, 5 s de timeout). El LAeq vale;
//                             el recuento de eventos esta infraestimado
//     DB_MUESTRA_UNICA        el intervalo no acumulo muestras (primer
//                             registro tras arrancar o tras cambiar de
//                             ventana): los cuatro niveles acústicos salen de
//                             UNA lectura, con rango dinámico 0 por
//                             construcción. No es un error, pero el análisis
//                             lo excluye de los indicadores acústicos
//     FLASH_BAJA              queda poco espacio de almacenamiento
//   Si aparece una marca ERR_, esa variable debe excluirse del análisis en
//   las filas afectadas.
//   Umbrales según RD 486/1997 Anexo III, RITE (IDA2) y CTE DB-HS 3.
//   dB_LAeq  nivel continuo equivalente del intervalo, CON UN DECIMAL.
//            Promedia ENERGÍA (no decibelios), que es la magnitud correcta en
//            acústica y la que exige la normativa. Las muestras del módulo son
//            enteras, pero promediar ~240 de ellas recupera resolución por
//            debajo del dB, necesaria para el reparto exógeno/endógeno
//            (revision-rigor-2.md, hallazgo 18). El fondo y el pico siguen
//            siendo enteros por construcción.
//   dB_fondo nivel de fondo ESTIMADO: percentil 10 de las muestras del
//            intervalo (el nivel que se supera el 90 % del tiempo). Describe
//            el ruido sostenido del aula. No es un LA90 normalizado: un LA90
//            se calcula por norma sobre la señal continua con ponderación
//            temporal e intervalos definidos; este valor parte de lecturas
//            ya promediadas por el módulo cada 125 ms y agregadas por
//            software. Se usa el nombre "nivel de fondo estimado" para no
//            sugerir una equivalencia metrológica que no se ha demostrado
//            (REVISION_RIGOR.md, hallazgo 3).
//   dB_maxF  pico del intervalo, medido sobre el promediado de 125 ms del
//            propio módulo (su "fast mode"). Es la mejor aproximación
//            disponible a la ponderación temporal Fast y sobre el se evalúan
//            las alertas acústicas. NO es una ponderación Fast certificada:
//            el fabricante no documenta si su promediado es exponencial ni
//            declara conformidad con IEC 61672 (revision-rigor-2.md, 15).
//   eventos  número de veces que el nivel sube por encima del fondo más 10 dB.
//
//   El fondo, el pico y los eventos capturan la FLUCTUACIÓN del ruido. El estudio BREATHE
//   (Foraster et al., 2022, PLOS Medicine) halló que dentro del aula la
//   fluctuación se asocia de forma consistente con el desarrollo cognitivo,
//   mientras que el nivel medio apenas lo hace. Registrar solo el LAeq
//   dejaría fuera la dimensión que la investigación señala como decisiva.
//   El rango dinámico (dB_maxF - dB_fondo) es el indicador de fluctuación más
//   directo que puede derivarse de estos datos.
//
// PONDERACIÓN TEMPORAL (revision-rigor-2.md, hallazgos 12, 13 y 15):
//   El promediado interno del módulo y el intervalo de lectura son AMBOS de
//   125 ms. Es la condición que hace que el registro sea continuo: cada
//   lectura resume exactamente los 125 ms transcurridos desde la anterior y
//   no queda ningún tramo sin observar.
//   Antes el módulo promediaba 31 ms y se leía cada 125 ms, de modo que solo
//   se observaba el 25 % del tiempo y los impulsos de menos de 94 ms se
//   perdían con probabilidad (94-duración)/125. Se eliminó también el filtro
//   exponencial que reconstruía "Fast" por software: su constante estaba
//   calculada para un paso de 31 ms y, aplicada cada 125 ms, daba una
//   constante de tiempo efectiva de 504 ms en vez de 125 ms.
//   Contrapartida asumida: al promediar 125 ms se pierde el pico crudo de
//   31 ms y con el el indicador de impulsividad (dB_max - dB_maxF). No era
//   una magnitud del estudio BREATHE —que midió ruido de tráfico, de escala
//   de segundos— y su perdida no afecta al LAeq, al fondo, al recuento de
//   eventos ni al índice de fluctuación.
//
// NO INTERFERENCIA CON EL ESTUDIO:
//   Durante el registro el LED queda a brillo mínimo (3/255) como simple
//   testigo de funcionamiento: verde tenue = todo correcto. Si algo falla
//   (sensor sin respuesta, error de escritura, flash casi llena) pasa a
//   ROJO INTERMITENTE y brillante: una avería debe verse desde lejos para
//   que el operador intervenga, y no informa de condiciones ambientales.
//   NO codifica condiciones ambientales ni la ventana horaria. Si lo hiciera,
//   los ocupantes sabrían cuando se mide o que el aire está cargado, y
//   podrían ventilar o bajar la voz: el dato dejaría de reflejar el aula
//   real (reactividad). Se mantiene encendido porque el módulo DS3231 ya
//   lleva un LED de alimentación permanente e inevitable; el equipo se ve
//   encendido igualmente. El comando L da un destello puntual más visible
//   para que el operador compruebe el estado cuando lo necesite.
//
// COMANDOS por el monitor serie (escribir y pulsar Enter):
//   T2026-08-06 06:50:00  ajustar reloj (formato exacto TAAAA-MM-DD hh:mm:ss)
//   E Aula 3B / calle / RT 0.85 / STI 0.62 / CEXT 420
//       identificar el espacio. OBLIGATORIO al instalar en un aula nueva.
//   N obras en el pasillo   anotar una incidencia con su hora
//   C   verificar el sensor de CO2 (C CALIBRAR fuerza la calibración de
//       fondo; solo al aire libre)
//   W 0-24  fijar la ventana horaria de registro (se guarda en la flash)
//   H   mostrar hora del RTC
//   D   volcar el CSV guardado
//   I   info del fichero y flash
//   L   destello de comprobación del estado
//   X   BORRAR todos los datos (pide confirmación)
//   ?   ayuda
// El registro de datos sigue activo mientras se aceptan comandos.
// ============================================================
// Autoría: www.acusticaescolar.com - Licencia MIT
// ============================================================

#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include <s8_uart.h>
#include <RTClib.h>
#include <LittleFS.h>

// -- Pines --
#define PIN_SDA       8
#define PIN_SCL       9
#define PIN_S8_TX     1
#define PIN_S8_RX     2
#define PIN_LED       48

// -- Direcciones I2C --
#define ADDR_SHT41    0x44
#define ADDR_DBMETER  0x48
#define ADDR_DS3231   0x68

// -- Registros DBMETER (PCB Artists) --
#define DBM_REG_VERSION   0x00
#define DBM_REG_TAVG_HIGH 0x07   // byte alto del tiempo de promediado (ms)
#define DBM_REG_TAVG_LOW  0x08   // byte bajo
#define DBM_REG_DECIBEL   0x0A

// Tiempo de promediado interno del sonómetro.
// 125 ms es el "fast mode" que documenta el fabricante y el valor mas bajo
// que documenta: por debajo (el firmware usaba 31 ms) el registro acepta la
// escritura pero el módulo queda fuera de la configuración caracterizada, y
// con el la precisión de +/-2 dB de la hoja de características.
// Por defecto el módulo viene a 1000 ms ("slow mode").
// DEBE COINCIDIR CON DB_SAMPLE_MS: si el promediado es mas corto que el
// intervalo de lectura, la diferencia es tiempo que no se observa.
#define DBM_TAVG_MS    125

// -- Sobre la valoración de las condiciones --
// El firmware NO clasifica las condiciones ambientales: registra los valores
// medidos y deja la valoración para el análisis posterior. Así los datos no
// caducan si cambian los umbrales normativos (RITE y CTE están en revisión),
// y las etiquetas de condición serían además redundantes: T_ALTA se deduce de
// temp_C, CO2_MALA de co2_ppm, etc.
// La tabla de interpretación (RD 486/1997 Anexo III, RITE IDA2, CTE DB-HS 3,
// NTP 742) figura en el documento de proyecto, que es donde puede mantenerse
// actualizada.
// El campo "estado" del CSV solo informa de la FIABILIDAD del dato, que NO es
// derivable de los valores: si una lectura de CO2 vale 0, sin la marca no
// habría forma de saber si fue un fallo del sensor o una medida real.

// -- Indicadores de fluctuación (marco BREATHE / Foraster et al. 2022) --
// El estudio BREATHE (ISGlobal, 2.680 escolares de Barcelona) encontró que
// dentro del aula la FLUCTUACIÓN del ruido se asocia de forma consistente con
// el desarrollo cognitivo, mientras que el nivel medio apenas lo hace. Por eso
// no basta con registrar el LAeq: se añaden el nivel de fondo y un contador de
// eventos, que capturan esa dimensión.
#define DB_HIST_SIZE       128   // histograma de niveles (1 dB por casilla)
#define DB_PERCENTIL_FONDO  10   // percentil 10 = nivel superado el 90 % del tiempo
#define EVENTO_MARGEN_DB    10   // un evento supera el fondo en esta cantidad
#define EVENTO_HISTERESIS    3   // margen de salida, evita contar rebotes

// -- Muestreo del sonómetro --
// Se lee al mismo ritmo que el promediado interno (DBM_TAVG_MS) para que el
// registro sea continuo: cada lectura resume los 125 ms anteriores y no queda
// ningún tramo sin observar. Cambiar uno de los dos valores sin cambiar el
// otro reintroduce el hallazgo 12 (huecos) y el 13 (ponderación falseada).
#define DB_SAMPLE_MS  125UL

// Muestras que debe reunir un intervalo completo, y mínimo aceptable.
// logRow() es bloqueante y dentro llama a get_co2(): la librería S8_UART tiene
// un timeout de 5 s, de modo que un sensor de CO2 que no responde deja el hilo
// parado cinco segundos y se lleva por delante 40 de las 240 muestras del
// intervalo -- el 17 %. El LAeq apenas lo nota, pero el recuento de eventos es
// proporcional al tiempo observado y cae en la misma proporción. La fila salia
// marcada ERR_CO2, que el análisis excluye solo del CO2, y sus datos acústicos
// entraban intactos (revision-rigor-3.md, hallazgo 29).
// Contar las muestras cubre esta causa y cualquier otra.
#define DB_MUESTRAS_ESPERADAS (LOG_INTERVAL_MS / DB_SAMPLE_MS)   // 240
#define DB_MUESTRAS_MIN_PCT   90

// -- Logging --
#define LOG_INTERVAL_MS  30000UL
#define CSV_FILENAME     "/datalog.csv"

// -- Frecuencia de la CPU --
// A 80 MHz el consumo baja de ~45 a ~25 mA sin afectar al funcionamiento: el
// firmware pasa casi todo el tiempo esperando, no calculando. Supone un 38 %
// más de autonomía con la misma batería. Subir a 240 solo si se añadieran
// tareas exigentes (WiFi, procesado de señal).
#define CPU_FREQ_MHZ  80

// -- Ventana horaria de registro --
// Solo se registra entre HORA_INICIO y HORA_FIN (todos los días, incluidos
// fines de semana: permite estudios de ruido exterior en sábado y domingo).
// Fuera de la ventana el sistema permanece en espera sin escribir en flash.
#define HORA_INICIO_DEF   7    // 07:00 h
#define HORA_FIN_DEF     19    // 19:00 h (se registra hasta las 18:59:59)
// La ventana es AJUSTABLE EN CALIENTE con el comando W y se guarda en la
// flash: no hace falta recompilar ni volver a programar el equipo. Antes era
// un #define, y eso hacía impracticable la recomendación de registrar la
// primera noche de cada campaña (revision-rigor-2.md, hallazgo 22).
// Nota: la ventana no admite cruce de medianoche (fin debe ser mayor que
// inicio). Para registro continuo 24 h: W 0-24.
uint8_t horaInicio = HORA_INICIO_DEF;
uint8_t horaFin    = HORA_FIN_DEF;
#define VENTANA_FILE "/ventana.txt"

// -- Objetos globales --
SensirionI2cSht4x sht4x;
S8_UART           *sensor_S8;
HardwareSerial    S8_serial(1);
RTC_DS3231        rtc;

// -- Estado --
bool sht41_ok = false, dbmeter_ok = false, ds3231_ok = false;
bool s8_ok = false, littlefs_ok = false;
unsigned long lastLog = 0;
uint32_t rowCount = 0;

// -- Seguimiento de picos de ruido dentro del intervalo --
unsigned long lastDbSample = 0;
uint8_t  dbMaxIntervalo = 0;    // pico del intervalo (promediado de 125 ms)
double   dbSumaEnergia = 0.0;   // suma de energías para el LAeq
uint16_t dbNumMuestras = 0;     // número de muestras acumuladas
uint16_t dbHistograma[DB_HIST_SIZE];  // distribución de niveles del intervalo
uint8_t  dbFondoPrevio = 0;     // fondo del intervalo anterior (umbral eventos)
uint16_t dbEventos = 0;         // eventos detectados en el intervalo
bool     dbEnEvento = false;    // estado del detector de eventos

// Vacía el histograma y los acumuladores del intervalo
void reiniciarAcumuladoresDb() {
  dbMaxIntervalo = 0; dbSumaEnergia = 0.0; dbNumMuestras = 0;
  dbEventos = 0; dbEnEvento = false;
  memset(dbHistograma, 0, sizeof(dbHistograma));
}

// Nivel de fondo ESTIMADO del intervalo: percentil bajo de la distribución
// (percentil 10, el nivel que se supera el 90 % del tiempo). Describe el
// ruido sostenido sobre el que destacan los eventos. No se denomina "LA90"
// porque no reproduce el cálculo normalizado de esa magnitud (ver nota del
// formato CSV, más arriba, y REVISION_RIGOR.md, hallazgo 3).
uint8_t calcularFondo() {
  if (dbNumMuestras == 0) return 0;
  uint32_t objetivo = (uint32_t)dbNumMuestras * DB_PERCENTIL_FONDO / 100;
  if (objetivo == 0) objetivo = 1;
  uint32_t acumulado = 0;
  for (uint8_t i = 0; i < DB_HIST_SIZE; i++) {
    acumulado += dbHistograma[i];
    if (acumulado >= objetivo) return i;
  }
  return 0;
}

// -- Control de espacio en flash --
// Si el espacio libre baja de este umbral, el sistema avisa (LED rojo tenue)
// y marca los registros. Evita que un estudio desatendido pierda datos en
// silencio al llenarse la memoria.
#define FLASH_MIN_KB   60      // umbral de aviso
uint16_t fallosEscritura = 0;  // registros que no se pudieron guardar
bool flashCasiLlena = false;

// -- Identificación del espacio medido --
// Imprescindible cuando el equipo rota entre aulas: sin esta marca, al volcar
// varios días de varios espacios los datos quedarían mezclados sin remedio.
// Se guarda como línea de marca en el CSV, no como columna, para no repetir
// el nombre en cada registro.
#define ESPACIO_MAX 96
#define ESPACIO_FILE "/espacio.txt"
char espacioActual[ESPACIO_MAX] = "sin identificar";

// El espacio se guarda en la flash y se recupera al arrancar. Sin esto, un
// reinicio (corte de alimentación, fallo puntual) dejaría el equipo "sin
// identificar" en mitad de una campaña, con el riesgo de atribuir después los
// registros al aula equivocada.
void guardarEspacio() {
  if (!littlefs_ok) return;
  File f = LittleFS.open(ESPACIO_FILE, "w");
  if (f) { f.print(espacioActual); f.close(); }
}

void cargarEspacio() {
  if (!littlefs_ok) return;
  if (!LittleFS.exists(ESPACIO_FILE)) return;
  File f = LittleFS.open(ESPACIO_FILE, "r");
  if (!f) return;
  String v = f.readString();
  f.close();
  v.trim();
  if (v.length() > 0) {
    v.toCharArray(espacioActual, ESPACIO_MAX);
    Serial.printf("  [OK] Espacio recuperado de la flash: %s\n", espacioActual);
  }
}

// -- Ventana horaria --
bool enVentanaAnterior = true;  // para avisar solo en los cambios de estado
bool escribirMarca(const char* tipo, const char* texto);   // definida más abajo

// -- Extensión automática de la primera jornada en cada aula --
// El cálculo de renovación de aire (ACH) exige ocupación nula, y el único
// tramo con ocupación nula GARANTIZADA es la noche. Registrarla requería antes
// dos acciones humanas: poner W 0-24 al instalar y volver a W 7-19 a la mañana
// siguiente. La segunda no la hace nadie, porque el equipo se queda solo en el
// aula: en la práctica, o se gastaba memoria de más durante toda la campaña o
// no había noche que analizar.
//
// Ahora la primera jornada en cada aula se registra 24 h automáticamente. El
// disparador es el comando E, que ya es obligatorio al instalar: no añade
// ningún paso. La extensión se guarda con la FECHA en que se armó y caduca
// sola al cambiar de día, de modo que un corte de alimentación no regala otra
// noche ni deja el equipo registrando 24 h para siempre.
//
// Cuatro reglas que evitan que se dispare cuando no debe:
//   1. Solo si CAMBIA EL NOMBRE del espacio. Repetir E con el mismo nombre
//      para añadir RT o STI más tarde es la operativa documentada y no
//      vuelve a extender.
//   2. Un comando W explícito el mismo día la CANCELA: una instrucción
//      manual manda sobre una automática.
//   3. No se arma sin reloj fiable: sin fecha no hay día contra el que
//      caducar. (Sin RTC el firmware ya registra continuamente.)
//   4. No se arma si la ventana configurada ya cubre 24 h.
#define VENTEXT_FILE "/ventext.txt"
char ventanaExtDia[11] = "";   // "AAAA-MM-DD" de la jornada extendida, o ""
void cancelarExtension(const char* motivo, bool marcar);   // definida más abajo

// La ventana sobrevive a un reinicio, igual que la identificación del espacio:
// un corte de alimentación a mitad de campaña no debe devolver el equipo al
// horario por defecto sin que nadie se entere.
void guardarVentana() {
  if (!littlefs_ok) return;
  File f = LittleFS.open(VENTANA_FILE, "w");
  if (f) { f.printf("%u-%u", horaInicio, horaFin); f.close(); }
}

void cargarVentana() {
  if (!littlefs_ok) return;
  if (!LittleFS.exists(VENTANA_FILE)) return;
  File f = LittleFS.open(VENTANA_FILE, "r");
  if (!f) return;
  String v = f.readString();
  f.close();
  int g = v.indexOf('-');
  if (g <= 0) return;
  int ini = v.substring(0, g).toInt();
  int fin = v.substring(g + 1).toInt();
  if (ini >= 0 && fin > ini && fin <= 24) {
    horaInicio = (uint8_t)ini; horaFin = (uint8_t)fin;
    Serial.printf("  [OK] Ventana recuperada de la flash: %02d:00-%02d:00\n",
                  horaInicio, horaFin);
  }
}

// Comando W: fija la ventana horaria de registro.
void fijarVentana(const String& cmd) {
  String v = cmd.substring(1);
  v.trim();
  if (v.length() == 0) {
    Serial.printf("Ventana actual: %02d:00-%02d:00 (todos los días)\n", horaInicio, horaFin);
    Serial.println(F(""));
    Serial.println(F("Formato:  W 7-19     registra de 07:00 a 18:59"));
    Serial.println(F("          W 0-24     registro continuo las 24 h"));
    Serial.println(F(""));
    if (ventanaExtDia[0] != '\0')
      Serial.printf("HOY (%s) se registra 24 h: primera jornada en el aula.\n\n", ventanaExtDia);
    Serial.println(F("NO HACE FALTA PONER W 0-24 PARA LA PRIMERA NOCHE. El comando E,"));
    Serial.println(F("al identificar un aula nueva, ya extiende esa jornada a 24 h y"));
    Serial.println(F("la deshace solo al día siguiente. La noche es el único tramo con"));
    Serial.println(F("ocupación nula GARANTIZADA, que es el supuesto que exige el"));
    Serial.println(F("cálculo de renovación de aire (ACH)."));
    Serial.println(F("Usar W aquí CANCELA esa extensión: manda lo que fijes a mano."));
    Serial.println(F("Añade una franja nocturna en franjas.txt, por ejemplo:"));
    Serial.println(F("   19:00-23:59=Noche *VACÍO"));
    return;
  }
  int g = v.indexOf('-');
  if (g <= 0) {
    Serial.println(F("Formato incorrecto. Ejemplo:  W 0-24"));
    return;
  }
  int ini = v.substring(0, g).toInt();
  int fin = v.substring(g + 1).toInt();
  if (ini < 0 || fin <= ini || fin > 24) {
    Serial.println(F("[!] Valores no validos. El inicio debe ser menor que el fin"));
    Serial.println(F("    y el fin como máximo 24. La ventana no cruza medianoche."));
    return;
  }
  horaInicio = (uint8_t)ini; horaFin = (uint8_t)fin;
  guardarVentana();
  // Una ventana fijada a mano manda sobre la extensión automática de la
  // primera jornada: es una instrucción explícita del operador.
  if (ventanaExtDia[0] != '\0') {
    Serial.println(F("  [i] Se cancela la extensión automática de esta jornada:"));
    Serial.println(F("      manda la ventana que acabas de fijar."));
    cancelarExtension("W manual", false);
  }
  Serial.printf("  [OK] Ventana de registro: %02d:00-%02d:00\n", horaInicio, horaFin);
  uint8_t horas = horaFin - horaInicio;
  if (horas >= 20) {
    Serial.println(F("  [i] Registro casi continuo: la memoria dura aproximadamente"));
    Serial.println(F("      la MITAD que con la ventana de 12 h. Vuelca los datos"));
    Serial.println(F("      antes o vuelve al horario del centro tras la primera noche."));
  }
  char vbuf[16];
  snprintf(vbuf, sizeof(vbuf), "%u-%u", horaInicio, horaFin);
  escribirMarca("VENTANA", vbuf);
}

void guardarVentanaExt() {
  if (!littlefs_ok) return;
  File f = LittleFS.open(VENTEXT_FILE, "w");
  if (f) { f.print(ventanaExtDia); f.close(); }
}

void cargarVentanaExt() {
  if (!littlefs_ok) return;
  if (!LittleFS.exists(VENTEXT_FILE)) return;
  File f = LittleFS.open(VENTEXT_FILE, "r");
  if (!f) return;
  String v = f.readString();
  f.close();
  v.trim();
  if (v.length() == 10) {
    v.toCharArray(ventanaExtDia, sizeof(ventanaExtDia));
    Serial.printf("  [OK] Primera jornada extendida a 24 h, día %s\n", ventanaExtDia);
  }
}

// Cancela la extensión y deja constancia. `motivo` va a la consola; la marca
// del CSV lleva la ventana que queda vigente, para que al analizar se sepa con
// qué horario se tomó cada tramo.
void cancelarExtension(const char* motivo, bool marcar) {
  if (ventanaExtDia[0] == '\0') return;
  ventanaExtDia[0] = '\0';
  guardarVentanaExt();
  if (marcar) {   // el comando W escribe su propia marca: no se duplica
    char vbuf[24];
    snprintf(vbuf, sizeof(vbuf), "%u-%u fin auto", horaInicio, horaFin);
    escribirMarca("VENTANA", vbuf);
  }
  Serial.printf("[i] Fin de la jornada extendida (%s). Ventana: %02d:00-%02d:00\n",
                motivo, horaInicio, horaFin);
}

// Arma la extensión de 24 h para el día de hoy. Se llama solo al identificar
// un aula NUEVA con el comando E.
void armarExtension() {
  if (horaFin - horaInicio >= 24) {
    Serial.println(F("  [i] La ventana ya cubre 24 h: no hace falta extender la primera jornada."));
    return;
  }
  if (!ds3231_ok) {
    Serial.println(F("  [!] Sin reloj: NO se ha extendido la primera jornada a 24 h."));
    Serial.println(F("      Sin fecha no hay día contra el que caducar la extensión."));
    return;
  }
  DateTime n = rtc.now();
  if (n.year() < 2020) {
    Serial.println(F("  [!] Reloj sin ajustar: NO se ha extendido la primera jornada."));
    Serial.println(F("      Ajusta la hora con el comando T y repite el comando E."));
    return;
  }
  snprintf(ventanaExtDia, sizeof(ventanaExtDia), "%04d-%02d-%02d", n.year(), n.month(), n.day());
  guardarVentanaExt();
  escribirMarca("VENTANA", "0-24 auto");
  Serial.println(F("  [OK] Primera jornada en esta aula: se registra hasta las 24:00."));
  Serial.println(F("       Es la noche que necesita el cálculo de renovación de aire."));
  Serial.println(F("       Mañana vuelve sola al horario configurado. Para anularla,"));
  Serial.println(F("       fija la ventana a mano con W."));
}

// Devuelve true si la hora actual está dentro de la ventana de registro.
// Si el RTC no está disponible, registra siempre (para no perder datos).
// La extensión automática de la primera jornada tiene prioridad sobre la
// ventana configurada, y solo mientras la fecha coincida con la que se guardó.
bool enVentanaHoraria() {
  if (!ds3231_ok) return true;
  DateTime n = rtc.now();          // una sola lectura del RTC por llamada
  if (ventanaExtDia[0] != '\0' && n.year() >= 2020) {
    char hoy[11];
    snprintf(hoy, sizeof(hoy), "%04d-%02d-%02d", n.year(), n.month(), n.day());
    if (strcmp(hoy, ventanaExtDia) == 0) return true;
  }
  int hora = n.hour();
  return (hora >= horaInicio && hora < horaFin);
}


// ============================================================
// LED WS2812B (GPIO48, 5V, neopixelWrite)
// ============================================================
void setLED(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(PIN_LED, r, g, b); }
void ledOff()   { setLED(0,0,0); }
void ledGreen() { setLED(0,80,0); }
void ledRed()   { setLED(80,0,0); }
void ledBlue()  { setLED(0,0,80); }
void ledError(int t){ for(int i=0;i<t;i++){ledRed();delay(200);ledOff();delay(200);} }

// -- LED de estado a brillo mínimo --
// Durante el registro el LED queda encendido al mínimo perceptible, solo como
// testigo de que el sistema funciona. NO codifica condiciones ambientales ni
// la ventana horaria: si lo hiciera, delataría el estudio a los ocupantes.
// Se mantiene encendido porque el módulo DS3231 ya lleva un LED de
// alimentación permanente que no puede apagarse; el equipo se ve encendido
// igualmente, de modo que un testigo tenue no añade información nueva.
#define BRILLO_MIN     3       // 3/255: visible de cerca, apenas perceptible
#define BRILLO_ALERTA 90       // rojo bien visible para reclamar atención
#define BLINK_MS     600UL     // periodo de parpadeo de la alerta

// Estado del testigo. El fallo se señaliza con rojo INTERMITENTE y brillante:
// una avería debe verse desde lejos para que el operador intervenga. No
// contamina el estudio porque no informa de condiciones ambientales, sino de
// que el equipo necesita atención.
bool estadoFallo = false;
unsigned long lastBlink = 0;
bool blinkEncendido = false;

void ledEstadoOK() {
  estadoFallo = false;
  setLED(0, BRILLO_MIN, 0);            // verde tenue: todo correcto
}
void ledEstadoFallo() {
  estadoFallo = true;                  // el parpadeo lo gestiona el loop
  blinkEncendido = true;
  setLED(BRILLO_ALERTA, 0, 0);
  lastBlink = millis();
}

// Gestión no bloqueante del parpadeo de alerta. Se llama desde el loop en
// cada vuelta, también fuera de la ventana horaria: un fallo debe verse
// siempre, no solo en horario de registro.
void actualizarTestigo() {
  if (!estadoFallo) return;
  unsigned long ahora = millis();
  if (ahora - lastBlink >= BLINK_MS) {
    lastBlink = ahora;
    blinkEncendido = !blinkEncendido;
    if (blinkEncendido) setLED(BRILLO_ALERTA, 0, 0);
    else                setLED(0, 0, 0);
  }
}



// ============================================================
// DBMETER - lectura por registro I2C
// ============================================================
uint8_t readDBMeter() {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_DECIBEL);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom((uint8_t)ADDR_DBMETER, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}
// Configura el tiempo de promediado interno del sonómetro.
// El byte alto debe escribirse primero (así lo exige el módulo).
bool setDBMeterTavg(uint16_t ms) {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_TAVG_HIGH);
  Wire.write((uint8_t)(ms >> 8));
  Wire.write((uint8_t)(ms & 0xFF));   // el registro siguiente es TAVG_LOW
  return Wire.endTransmission() == 0;
}

uint16_t getDBMeterTavg() {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_TAVG_HIGH);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom((uint8_t)ADDR_DBMETER, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  uint16_t hi = Wire.read(), lo = Wire.read();
  return (hi << 8) | lo;
}

uint8_t readDBMeterVersion() {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_VERSION);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom((uint8_t)ADDR_DBMETER, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}


// ============================================================
// FASE 1 - Escaneo I2C
// ============================================================
void fase1_scanI2C() {
  Serial.println(F("\n== FASE 1 - Escaneo del bus I2C =="));
  int found = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  [OK] Dispositivo en 0x%02X", a);
      if (a == ADDR_SHT41)   Serial.print(" -> SHT41 T/H");
      if (a == ADDR_DBMETER) Serial.print(" -> DBMETER");
      if (a == ADDR_DS3231)  Serial.print(" -> DS3231 RTC");
      Serial.println();
      found++; delay(10);
    }
  }
  if (found == 0) {
    Serial.println(F("  [!!] NO se encontró ningún dispositivo I2C."));
    Serial.println(F("       Verifica SDA(53B), SCL(56B), 3V3, GND y pull-ups."));
  }
  auto chk = [](byte a)->bool{ Wire.beginTransmission(a); return Wire.endTransmission()==0; };
  sht41_ok = chk(ADDR_SHT41); dbmeter_ok = chk(ADDR_DBMETER); ds3231_ok = chk(ADDR_DS3231);
  Serial.println();
  Serial.printf("  SHT41   (0x44): %s\n", sht41_ok   ? "OK":"NO DETECTADO");
  Serial.printf("  DBMETER (0x48): %s\n", dbmeter_ok ? "OK":"NO DETECTADO");
  Serial.printf("  DS3231  (0x68): %s\n", ds3231_ok  ? "OK":"NO DETECTADO");
  if(!sht41_ok)   Serial.println(F("  -> SHT41: cols 14B(SCL) 15B(SDA) 13B(3V3) 12B(GND). Girado 180!"));
  if(!dbmeter_ok) Serial.println(F("  -> DBMETER: cols 22B(SCL) 23B(SDA) 20B(3V3) 24B(GND)."));

  // Configurar el promediado interno del sonómetro a 125 ms ("fast mode").
  // Por defecto viene a 1000 ms ("slow mode"), que suaviza los picos.
  // El valor debe coincidir con DB_SAMPLE_MS para que no queden huecos.
  if (dbmeter_ok) {
    if (setDBMeterTavg(DBM_TAVG_MS)) {
      delay(20);
      uint16_t leido = getDBMeterTavg();
      Serial.printf("  DBMETER: promediado interno ajustado a %u ms", leido);
      if (leido == DBM_TAVG_MS) Serial.println(F("  [OK]"));
      else Serial.printf("  [!] se esperaban %d ms\n", DBM_TAVG_MS);
    } else {
      Serial.println(F("  [!] No se pudo ajustar el promediado del DBMETER."));
    }
  }
  if(!ds3231_ok)  Serial.println(F("  -> DS3231: cols 32B(SCL) 31B(SDA) 30B(3V3) 29B(GND). Pila CR2032?"));
}


// ============================================================
// FASE 2 - Lectura de sensores
// ============================================================
void fase2_leerSensores() {
  Serial.println(F("\n== FASE 2 - Lectura de sensores =="));
  if (sht41_ok) {
    float t=0,h=0; uint16_t e=sht4x.measureHighPrecision(t,h);
    if(e==0) Serial.printf("  SHT41   -> Temp: %.2f C  |  HR: %.1f %%\n",t,h);
    else     Serial.printf("  SHT41   -> Error lectura: %d\n",e);
  } else Serial.println(F("  SHT41   -> [OMITIDO]"));

  if (ds3231_ok) {
    DateTime n=rtc.now();
    Serial.printf("  DS3231  -> %04d-%02d-%02d %02d:%02d:%02d\n",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
    Serial.printf("            Temp interna RTC: %.1f C\n", rtc.getTemperature());
  } else Serial.println(F("  DS3231  -> [OMITIDO]"));

  if (dbmeter_ok) {
    uint8_t v=readDBMeterVersion();
    Serial.printf("  DBMETER -> Versión fw: 0x%02X  |  Tavg: %u ms\n", v, getDBMeterTavg());
    delay(1100);
    uint8_t db=readDBMeter();
    if(db>0) Serial.printf("  DBMETER -> Nivel sonoro: %d dB SPL\n", db);
    else     Serial.println(F("  DBMETER -> Lectura = 0. Espera >1s."));
  } else Serial.println(F("  DBMETER -> [OMITIDO]"));

  if (s8_ok) {
    int16_t c=sensor_S8->get_co2();
    if(c>0) Serial.printf("  S8 LP   -> CO2: %d ppm\n", c);
    else    Serial.println(F("  S8 LP   -> Lectura = 0. Tarda ~30s en responder."));
  } else Serial.println(F("  S8 LP   -> [OMITIDO]"));
}


// ============================================================
// FASE 3 - LED
// ============================================================
void fase3_pruebaLED() {
  Serial.println(F("\n== FASE 3 - Prueba del testigo luminoso =="));
  Serial.println(F("  El LED solo indica el estado del sistema, no las"));
  Serial.println(F("  condiciones ambientales (ver nota de no interferencia)."));
  Serial.println(F(""));

  // Comprobación del hardware a brillo pleno: verifica que el WS2812B
  // responde en los tres canales de color.
  Serial.println(F("  Comprobación de canales a brillo pleno:"));
  setLED(80,0,0);  Serial.println(F("    rojo"));  delay(500);
  setLED(0,80,0);  Serial.println(F("    verde")); delay(500);
  setLED(0,0,80);  Serial.println(F("    azul"));  delay(500);
  ledOff();        delay(300);

  // Comprobación de los dos estados reales de trabajo, a brillo mínimo:
  // es como se vera durante el estudio.
  Serial.println(F("\n  Estados de trabajo:"));
  ledEstadoOK();
  Serial.printf("    verde tenue (%d/255)      -> sistema correcto\n", BRILLO_MIN);
  delay(1500);
  Serial.println(F("    rojo intermitente        -> requiere atención"));
  for (int i = 0; i < 4; i++) {
    setLED(BRILLO_ALERTA,0,0); delay(BLINK_MS);
    setLED(0,0,0);             delay(BLINK_MS);
  }
  estadoFallo = false;
  ledOff();

  Serial.println(F("\n  Si has visto los tres colores y los dos estados tenues, el LED funciona."));
}


// ============================================================
// FASE 4 - LittleFS
// ============================================================
void fase4_pruebaLittleFS() {
  Serial.println(F("\n== FASE 4 - Prueba LittleFS =="));
  if (!LittleFS.begin(true)) {
    Serial.println(F("  [!!] Error montando LittleFS. Verifica Flash Size = 16MB."));
    littlefs_ok=false; return;
  }
  littlefs_ok=true;
  Serial.println(F("  [OK] LittleFS montado."));
  size_t tot=LittleFS.totalBytes(), us=LittleFS.usedBytes();
  Serial.printf("  Total: %u KB | Usado: %u KB | Libre: %u KB\n", tot/1024, us/1024, (tot-us)/1024);

  const char* tf="/test_escritura.csv";
  File f=LittleFS.open(tf,"w");
  if(!f){ Serial.println(F("  [!!] No se puede crear fichero.")); littlefs_ok=false; return; }
  f.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,eventos,estado");
  f.println("2025-04-15T09:00:00,22.50,55.0,750,48,42,50,52,3,OK");
  f.println("2025-04-15T09:00:03,28.10,45.0,1350,58,44,66,71,12,ERR_CO2");
  f.close();
  Serial.printf("  [OK] Fichero de prueba escrito: %s\n", tf);

  f=LittleFS.open(tf,"r");
  if(!f){ Serial.println(F("  [!!] No se puede leer fichero.")); littlefs_ok=false; return; }
  Serial.println(F("  Contenido leído:"));
  while(f.available()){ Serial.print("    "); Serial.println(f.readStringUntil('\n')); }
  f.close();

  if(!LittleFS.exists(CSV_FILENAME)){
    File lg=LittleFS.open(CSV_FILENAME,"w");
    lg.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,eventos,estado");
    lg.close();
    Serial.printf("  [OK] Fichero de log creado: %s\n", CSV_FILENAME);
  } else {
    Serial.printf("  [OK] Fichero de log existente: %s\n", CSV_FILENAME);
  }
  LittleFS.remove(tf);
  Serial.println(F("  [OK] LittleFS: escritura/lectura/verificación superadas."));
}


// ============================================================
// FASE 5 - Registro de una fila
// ============================================================

// Escribe una línea de marca en el CSV. Las marcas empiezan por '#' y llevan
// su propio timestamp: sirven para segmentar el análisis por espacio y para
// dejar constancia de incidencias con la hora exacta en que ocurrieron.
bool escribirMarca(const char* tipo, const char* texto) {
  if (!littlefs_ok) { Serial.println(F("[!] LittleFS no disponible.")); return false; }
  char ts[25] = "0000-00-00T00:00:00";
  if (ds3231_ok) {
    DateTime n = rtc.now();
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d",
      n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
  }
  File f = LittleFS.open(CSV_FILENAME, "a");
  if (!f) { Serial.println(F("[!] No se pudo escribir la marca.")); return false; }
  f.printf("#%s,%s,%s\n", tipo, ts, texto);
  f.close();
  Serial.printf("[OK] #%s  %s  %s\n", tipo, ts, texto);
  return true;
}

// Identifica el espacio que se está midiendo y sus características acústicas.
// Se anota al instalar el equipo en cada aula: a partir de ese momento los
// registros pertenecen a ese espacio.
//
// Formato:  E Aula 3B / calle / RT 0.85 / STI 0.62
//
//   nombre       obligatorio
//   exposición   calle | patio | interior | mixta
//   RT           tiempo de reverberación en segundos (T30, promedio de las
//                bandas de 500, 1000 y 2000 Hz, aula desocupada)
//   STI          índice de inteligibilidad del habla (0 a 1)
//
// Todo salvo el nombre es opcional, y el orden de los campos es indiferente.
// El equipo NO mide la reverberación ni el STI: requieren excitación impulsiva
// y análisis del decaimiento. Se introducen aquí los valores obtenidos con
// otro instrumento para que queden asociados al espacio y el análisis pueda
// valorar su calidad estructural completa.
//
// Si al instalar el equipo aún no se conocen, basta con volver a ejecutar el
// comando más tarde con el mismo nombre: el análisis fusiona los datos.
//
// Referencias:
//   RT   CTE DB-HR (España), criterio principal: T <= 0,7 s con el aula
//        vacía y mobiliario móvil, V <= 350 m3 (supuesto adoptado para
//        aulas — ver documento de proyecto, apdo. 7.1). Solo es exigible
//        en obra nueva o rehabilitación con licencia posterior al
//        RD 1371/2007: en edificios anteriores no lo es, y el resultado
//        se expresa como que el aula "se aleja de los valores que
//        exigiría un edificio de nueva construcción", nunca como
//        incumplimiento. ANSI/ASA S12.60 (0,6 s hasta 283 m3, 0,7 s hasta
//        566 m3) se cita como referencia internacional convergente.
//   STI  España no fija un umbral de inteligibilidad para aulas. Se usa
//        DIN 18041 (>= 0,65, salas de comunicación) como referencia
//        internacional de buena práctica, no como normativa aplicable.
void fijarEspacio(const String& cmd) {
  String v = cmd.substring(1);
  v.trim();
  if (v.length() == 0) {
    Serial.printf("Espacio actual: %s\n", espacioActual);
    Serial.println(F(""));
    Serial.println(F("Formato:  E Aula 3B / calle / RT 0.85 / STI 0.62 / CEXT 420"));
    Serial.println(F("  exposición: calle | patio | interior | mixta"));
    Serial.println(F("    calle     da a vía pública: el exterior es sobre todo tráfico"));
    Serial.println(F("    patio     da al patio: actividad escolar al aire libre"));
    Serial.println(F("    interior  da a patio de luces o espacio cerrado"));
    Serial.println(F("    mixta     ventanas a más de una orientación"));
    Serial.println(F("  RT   reverberación en segundos (T30, media de 500/1k/2k Hz)"));
    Serial.println(F("  STI  índice de inteligibilidad (0 a 1)"));
    Serial.println(F("  CEXT CO2 exterior en ppm, medido al aire libre junto al aula"));
    Serial.println(F("       (p. ej. CEXT 420). Sin el, el análisis estima el exterior"));
    Serial.println(F("       con el mínimo interior del día, lo que SOBRESTIMA la"));
    Serial.println(F("       ventilación: el aula parece mejor ventilada de lo que esta."));
    Serial.println(F(""));
    Serial.println(F("Todo salvo el nombre es opcional. El equipo no mide RT ni STI:"));
    Serial.println(F("introduce los valores medidos con otro instrumento. Si aún no"));
    Serial.println(F("los conoces, repite el comando más tarde con el mismo nombre."));
    return;
  }

  // Avisos orientativos según lo que se haya indicado
  bool tieneExpo = false, tieneRT = false, tieneSTI = false;
  String low = v; low.toLowerCase();
  if (low.indexOf("calle")>=0 || low.indexOf("patio")>=0 ||
      low.indexOf("interior")>=0 || low.indexOf("mixta")>=0) tieneExpo = true;
  int pRT  = low.indexOf("rt");
  int pSTI = low.indexOf("sti");
  tieneRT  = (pRT  >= 0);
  tieneSTI = (pSTI >= 0);
  bool tieneCext = (low.indexOf("cext") >= 0);

  // ¿Es un aula NUEVA? Solo entonces se extiende la primera jornada a 24 h.
  // Se compara el NOMBRE (primer campo, antes de la primera barra), porque
  // repetir el comando con el mismo nombre para añadir RT, STI o CEXT más
  // tarde es la operativa documentada y no debe volver a extender.
  auto nombreDe = [](const String& s) {
    String r = s; int b = r.indexOf('/');
    if (b >= 0) r = r.substring(0, b);
    r.trim(); r.toLowerCase();
    return r;
  };
  bool aulaNueva = (nombreDe(v) != nombreDe(String(espacioActual)));

  v.toCharArray(espacioActual, ESPACIO_MAX);
  escribirMarca("ESPACIO", espacioActual);
  guardarEspacio();   // sobrevive a un reinicio

  // El umbral de eventos deja de ser valido al cambiar de aula: entre un aula
  // interior y otra a fachada el fondo difiere en 10-15 dB, y arrastrarlo
  // haría que el primer intervalo del espacio nuevo contase eventos que no lo
  // son, o ninguno. Con 0 el detector queda inhibido un intervalo (30 s) y
  // vuelve a arrancar con el fondo real (revision-rigor-2.md, hallazgo 20).
  dbFondoPrevio = 0;

  // Aula nueva: se registra esta jornada hasta las 24:00 sin que nadie tenga
  // que acordarse de ponerlo ni de quitarlo (revision-rigor-3.md, hallazgo 35).
  if (aulaNueva) armarExtension();

  if (!tieneExpo) {
    Serial.println(F("  [i] Sin exposición indicada. Añádela: E ... / calle"));
    Serial.println(F("      Sin ella no puede interpretarse el ruido exterior."));
  }
  if (!tieneRT) {
    Serial.println(F("  [i] Sin reverberación (RT). La valoración estructural queda"));
    Serial.println(F("      incompleta: mídela con otro instrumento y repite el comando."));
  }
  if (!tieneCext) {
    Serial.println(F("  [i] Sin CO2 exterior (CEXT). El análisis lo estimara con el"));
    Serial.println(F("      mínimo interior, lo que sobrestima la ventilación. Mide al"));
    Serial.println(F("      aire libre junto al aula y repite:  E ... / CEXT 420"));
  }
  if (tieneRT) {
    // Valorar el RT frente al CTE DB-HR (criterio principal en España).
    // Supuesto: aula vacía con mobiliario móvil, V <= 350 m3 (documento
    // de proyecto, 7.1). Si el edificio es anterior al RD 1371/2007 y no
    // ha tenido licencia de obra posterior, el CTE no es exigible: el
    // mensaje "por encima" debe leerse como que el aula se aleja de los
    // valores que exigiría un edificio de nueva construcción, no como
    // incumplimiento normativo.
    float rt = v.substring(pRT+2).toFloat();
    if (rt > 0) {
      if (rt <= 0.5)      Serial.println(F("  [OK] RT cumple incluso el criterio mas exigente del CTE DB-HR."));
      else if (rt <= 0.7) Serial.println(F("  [OK] RT cumple el CTE DB-HR (aula con mobiliario móvil, V<=350 m3)."));
      else if (rt <= 0.9) Serial.println(F("  [i]  RT por encima del CTE DB-HR (0,7 s): mejorable."));
      else                Serial.println(F("  [!]  RT muy por encima del CTE DB-HR: inteligibilidad comprometida."));
      Serial.println(F("       Ref. internacional convergente: ANSI/ASA S12.60 (0,6-0,7 s según volumen)."));
    }
  }
  if (tieneSTI) {
    float sti = v.substring(pSTI+3).toFloat();
    if (sti > 0) {
      if (sti >= 0.75)      Serial.println(F("  [OK] STI excelente."));
      else if (sti >= 0.65) Serial.println(F("  [OK] STI cumple DIN 18041 (salas de comunicación)."));
      else if (sti >= 0.60) Serial.println(F("  [i]  STI aceptable, por debajo del criterio DIN 18041."));
      else                  Serial.println(F("  [!]  STI insuficiente: la voz no se entiende bien."));
      if (sti < 0.75) Serial.println(F("       DIN 18041 es una referencia internacional: España no fija un umbral de STI para aulas."));
    }
  }
}

// Anota una incidencia con su hora: obras en el pasillo, ventana abierta,
// actividad en el aula contigua... Lo que el análisis posterior no puede saber.
void anotarNota(const String& cmd) {
  String v = cmd.substring(1);
  v.trim();
  if (v.length() == 0) {
    Serial.println(F("Escribe la nota tras la N. Ejemplo:  N obras en el pasillo"));
    return;
  }
  char buf[80];
  v.toCharArray(buf, sizeof(buf));
  escribirMarca("NOTA", buf);
}

// Construye el campo "estado": marcas de fiabilidad del dato.
// Devuelve "OK" cuando la lectura es fiable en todas las variables.
void construirEstado(char* buf, size_t n,
                     bool errTH, bool errCO2, bool errDB, bool errRTC,
                     bool co2Calentando, bool dbMuestraUnica, bool dbIncompleto) {
  buf[0] = '\0';
  bool primera = true;
  auto add = [&](const char* etiqueta) {
    if (!primera) strncat(buf, ";", n - strlen(buf) - 1);
    strncat(buf, etiqueta, n - strlen(buf) - 1);
    primera = false;
  };

  if (errRTC) add("ERR_RTC");   // marca de tiempo no fiable
  if (errTH)  add("ERR_TH");    // sensor de temperatura/humedad sin respuesta
  if (errCO2) add("ERR_CO2");   // sensor de CO2 sin respuesta valida
  // El S8 devuelve 0 mientras se calienta (~30 s tras el encendido). No es una
  // avería, pero TAMPOCO es una medida: sin esta marca la fila saldría con
  // co2_ppm = 0 y estado "OK", que es justo lo que el campo estado existe para
  // evitar (revision-rigor-2.md, hallazgo 17).
  if (co2Calentando) add("CO2_CALENTANDO");
  if (errDB)  add("ERR_DB");    // sonómetro sin respuesta valida
  // Intervalo observado solo en parte: el hilo estuvo bloqueado (lo habitual
  // es el timeout de 5 s del S8 cuando el sensor de CO2 no responde). El LAeq
  // sigue siendo razonable, pero el recuento de eventos esta infraestimado en
  // la misma proporción que el tiempo perdido (revision-rigor-3.md, 29).
  if (dbIncompleto) add("DB_INCOMPLETO");
  // Intervalo sin muestras acumuladas: los cuatro niveles acústicos salen de
  // UNA sola lectura, de modo que LAeq = fondo = pico y el rango dinámico es
  // cero por construcción, no porque el aula sea silenciosa. Sin esta marca la
  // fila entraba en los promedios y en el índice de fluctuación como si fuera
  // un intervalo medido (revision-rigor-2.md, hallazgo 21).
  if (dbMuestraUnica) add("DB_MUESTRA_UNICA");
  if (flashCasiLlena) add("FLASH_BAJA");

  if (primera) strncpy(buf, "OK", n);
}

void logRow() {
  float t=0,h=0; int co2=0;
  bool errTH=false, errCO2=false, errDB=false, errRTC=false, co2Calentando=false;
  bool dbMuestraUnica=false, dbIncompleto=false;

  // Lectura T/HR con detección de error
  if (sht41_ok) {
    uint16_t e = sht4x.measureHighPrecision(t,h);
    if (e != 0) { errTH = true; t = 0; h = 0; }
  }

  // Lectura CO2 con detección de error (get_co2 devuelve <0 si falla)
  if (s8_ok) {
    int16_t r = sensor_S8->get_co2();
    if (r > 0) co2 = r;
    else if (r < 0) errCO2 = true;
    else co2Calentando = true;   // r == 0: calentamiento, no es dato valido
  }

  // Ruido: nivel equivalente (LAeq) y pico del intervalo.
  // LAeq = 10 * log10( (1/n) * suma(10^(Li/10)) )
  // El LAeq se guarda con UN DECIMAL. Cada muestra del módulo es un entero de
  // dB, pero el promedio energético de ~240 muestras recupera resolución por
  // debajo del dB, y esa resolución importa: el reparto exógeno/endógeno resta
  // energías de dos niveles próximos y con enteros el resultado oscilaba
  // decenas de puntos porcentuales (revision-rigor-2.md, hallazgo 18).
  // El fondo y el pico siguen siendo enteros: el fondo sale de un histograma
  // de 1 dB y el pico es el máximo de muestras enteras.
  float   dbEq = 0.0f;
  uint8_t dbMaxF = dbMaxIntervalo, dbFondo = 0;
  uint16_t eventos = dbEventos;
  if (dbNumMuestras > 0) {
    dbEq = (float)(10.0 * log10(dbSumaEnergia / dbNumMuestras));
    dbFondo = calcularFondo();
    dbFondoPrevio = dbFondo;   // referencia para los eventos del siguiente
    // ¿Se ha observado el intervalo entero? (revision-rigor-3.md, hallazgo 29)
    if ((uint32_t)dbNumMuestras * 100UL <
        (uint32_t)DB_MUESTRAS_ESPERADAS * DB_MUESTRAS_MIN_PCT) dbIncompleto = true;
  } else if (dbmeter_ok) {
    // Ruta de respaldo: el intervalo no ha acumulado ninguna muestra (primer
    // intervalo tras el arranque o tras un cambio de ventana horaria). Se hace
    // una lectura suelta para no perder la fila, pero los cuatro niveles salen
    // del mismo valor: hay que marcarlo para que el análisis no lo lea como un
    // intervalo de fluctuación nula (revision-rigor-2.md, hallazgo 21).
    uint8_t v = readDBMeter();
    dbEq = (float)v; dbMaxF = v; dbFondo = v;
    if (v == 0) errDB = true;
    else        dbMuestraUnica = true;
  }

  // Marca de tiempo
  char ts[25]="0000-00-00T00:00:00";
  if(ds3231_ok){
    DateTime n=rtc.now();
    if (n.year() < 2020) errRTC = true;   // reloj sin ajustar o sin pila
    snprintf(ts,sizeof(ts),"%04d-%02d-%02dT%02d:%02d:%02d",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  } else errRTC = true;

  // Caducidad de la extensión automática de la primera jornada: se comprueba
  // aquí porque la fecha ya está calculada y el registro pasa por este punto
  // una vez por intervalo. No basta con el cambio de ventana horaria: con una
  // ventana de 24 h configurada no hay transición en la que engancharse.
  if (!errRTC && ventanaExtDia[0] != '\0' && strncmp(ts, ventanaExtDia, 10) != 0) {
    cancelarExtension("cambio de día", true);
  }

  // Control de espacio libre en flash
  if (littlefs_ok) {
    size_t libre = LittleFS.totalBytes() - LittleFS.usedBytes();
    bool bajaAhora = (libre / 1024) < FLASH_MIN_KB;
    if (bajaAhora && !flashCasiLlena) {
      Serial.printf("\n[!!] AVISO: quedan %u KB de flash. Vuelca y borra los datos.\n\n",
        libre/1024);
      ledEstadoFallo();       // rojo intermitente: requiere intervención
    } else if (!bajaAhora && flashCasiLlena && fallosEscritura == 0) {
      // Se ha liberado espacio y no hay otros fallos: volver a estado normal
      Serial.println(F("[OK] Espacio de almacenamiento restablecido.\n"));
      ledEstadoOK();
    }
    flashCasiLlena = bajaAhora;
  }

  // 80 B: las marcas no son excluyentes entre si y la cadena mas larga
  // realista (ERR_RTC;ERR_TH;ERR_CO2;DB_MUESTRA_UNICA;FLASH_BAJA) ocupa 51.
  char estado[80];
  construirEstado(estado, sizeof(estado), errTH, errCO2, errDB, errRTC,
                  co2Calentando, dbMuestraUnica, dbIncompleto);

  char row[200];
  snprintf(row,sizeof(row),"%s,%.2f,%.1f,%d,%.1f,%d,%d,%u,%s",
    ts, t, h, co2, dbEq, dbFondo, dbMaxF, eventos, estado);

  // Escritura con verificación: si falla, se contabiliza y se avisa
  bool guardado = false;
  if(littlefs_ok){
    File f=LittleFS.open(CSV_FILENAME,"a");
    if(f){
      size_t escritos = f.println(row);
      f.close();
      if (escritos > 0) { rowCount++; guardado = true; }
    }
  }
  if (!guardado) {
    fallosEscritura++;
    Serial.printf("[!!] FALLO DE ESCRITURA #%u — dato perdido: %s\n", fallosEscritura, row);
    ledEstadoFallo();
  } else {
    Serial.printf("[LOG #%lu] %s\n", rowCount, row);
  }

  if(littlefs_ok && rowCount%10==0 && guardado){
    size_t fb=LittleFS.totalBytes()-LittleFS.usedBytes();
    Serial.printf("  [Flash libre: %u KB]\n", fb/1024);
  }

  // Reiniciar acumuladores del intervalo
  reiniciarAcumuladoresDb();
}


// ============================================================
// COMANDOS por monitor serie
// ============================================================
void mostrarAyuda() {
  Serial.println(F("\n============ COMANDOS ============"));
  Serial.println(F(""));
  Serial.println(F("  T  AJUSTAR EL RELOJ"));
  Serial.println(F("     Formato exacto:  TAAAA-MM-DD hh:mm:ss"));
  Serial.println(F("     Ejemplo:         T2026-09-08 08:30:00"));
  Serial.println(F("     La T va pegada a la fecha, sin espacio."));
  Serial.println(F("     Para acertar al segundo: escribe una hora unos"));
  Serial.println(F("     segundos por delante y pulsa Enter justo cuando"));
  Serial.println(F("     tu reloj de referencia la marque."));
  Serial.println(F("     El DS3231 conserva la hora con su pila CR2032,"));
  Serial.println(F("     así que solo hay que ajustarlo una vez."));
  Serial.println(F(""));
  Serial.println(F("  E  IDENTIFICAR EL ESPACIO y su exposición acústica"));
  Serial.println(F("     Ejemplo:  E Aula 3B / calle / RT 0.85 / STI 0.62 / CEXT 420"));
  Serial.println(F("     Exposiciones: calle | patio | interior | mixta"));
  Serial.println(F("     RT y STI son opcionales: medidos con otro instrumento."));
  Serial.println(F("     CEXT es el CO2 exterior en ppm: la lectura estable que"));
  Serial.println(F("     da el propio equipo al aire libre antes de instalarlo"));
  Serial.println(F("     (comando C). Sin ella la ventilación sale sobrestimada."));
  Serial.println(F("     Escribe solo E para ver el formato completo."));
  Serial.println(F("     Hazlo SIEMPRE al instalar el equipo en un aula nueva."));
  Serial.println(F("     Sin esta marca, al volcar varios espacios los datos"));
  Serial.println(F("     quedan mezclados y no hay forma de separarlos."));
  Serial.println(F("     Al identificar un aula NUEVA, esa jornada se registra"));
  Serial.println(F("     hasta las 24:00 automáticamente: es la noche que"));
  Serial.println(F("     necesita el cálculo de renovación de aire. Vuelve sola"));
  Serial.println(F("     al horario configurado al día siguiente. Repetir E con"));
  Serial.println(F("     el mismo nombre (para añadir RT o CEXT) no la repite."));
  Serial.println(F("     Escribe solo E para ver el espacio actual."));
  Serial.println(F(""));
  Serial.println(F("  N  ANOTAR UNA INCIDENCIA con su hora"));
  Serial.println(F("     Ejemplo:  N obras en el pasillo"));
  Serial.println(F("     Útil para lo que el análisis no puede deducir:"));
  Serial.println(F("     ventana abierta, actividad en el aula contigua..."));
  Serial.println(F(""));
  Serial.println(F("  W  VENTANA HORARIA de registro (se guarda en la flash)"));
  Serial.println(F("     Ejemplo:  W 0-24   registro continuo las 24 h"));
  Serial.println(F("               W 7-19   horario del centro (por defecto)"));
  Serial.println(F("     La primera noche en cada aula NO hay que pedirla: el"));
  Serial.println(F("     comando E ya extiende esa jornada a 24 h y la deshace"));
  Serial.println(F("     solo al día siguiente. Usar W la cancela."));
  Serial.println(F("     Escribe solo W para ver la ventana actual."));
  Serial.println(F(""));
  Serial.println(F("  C  VERIFICAR EL SENSOR DE CO2 (hazlo antes de cada campaña)"));
  Serial.println(F("     C           diagnostico: lectura, estado del sensor y ABC."));
  Serial.println(F("                 No modifica nada. Al aire libre y tras 30 min,"));
  Serial.println(F("                 la lectura que da es el valor CEXT del comando E."));
  Serial.println(F("     C CALIBRAR  fuerza la calibración de fondo. SOLO al aire libre:"));
  Serial.println(F("                 no ajusta contra un patrón, le ordena al sensor"));
  Serial.println(F("                 asumir que lo que mide AHORA son 400 ppm. Dentro"));
  Serial.println(F("                 del aula estropea el sensor en vez de arreglarlo."));
  Serial.println(F(""));
  Serial.println(F("  H  Mostrar la hora actual del reloj"));
  Serial.println(F("  D  Volcar por pantalla todo el CSV guardado"));
  Serial.println(F("  I  Info: espacio, registros, estado y autonomía"));
  Serial.println(F("  L  Destello de comprobación del equipo"));
  Serial.println(F("  X  BORRAR todos los datos (pide confirmar con SI)"));
  Serial.println(F("  ?  Esta ayuda"));
  Serial.println(F(""));
  Serial.println(F("El registro continua mientras se usan los comandos."));
  Serial.println(F("==================================\n"));
}

void sincronizarReloj(const String& cmd) {
  if(!ds3231_ok){ Serial.println(F("[!] DS3231 no disponible.")); return; }
  if(cmd.length()<20){
    Serial.println(F("[!] Formato incorrecto. Debe ser: TAAAA-MM-DD hh:mm:ss"));
    Serial.println(F("    Ejemplo: T2026-09-08 08:30:00  (la T pegada a la fecha)"));
    return;
  }
  int yr=cmd.substring(1,5).toInt(), mon=cmd.substring(6,8).toInt(), day=cmd.substring(9,11).toInt();
  int hr=cmd.substring(12,14).toInt(), mi=cmd.substring(15,17).toInt(), se=cmd.substring(18,20).toInt();
  if(yr<2020||yr>2099||mon<1||mon>12||day<1||day>31||hr>23||mi>59||se>59){
    Serial.println(F("[!] Valores fuera de rango. Revisa mes (1-12), día (1-31) y hora (0-23)."));
    Serial.println(F("    Ejemplo valido: T2026-09-08 08:30:00"));
    return;
  }
  rtc.adjust(DateTime(yr,mon,day,hr,mi,se));
  DateTime n=rtc.now();
  Serial.printf("[OK] Reloj ajustado a: %04d-%02d-%02d %02d:%02d:%02d\n",
    n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
}

void mostrarHora() {
  if(!ds3231_ok){ Serial.println(F("[!] DS3231 no disponible.")); return; }
  DateTime n=rtc.now();
  Serial.printf("Hora del RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
    n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
}

void volcarCSV() {
  if(!littlefs_ok || !LittleFS.exists(CSV_FILENAME)){ Serial.println(F("[!] No hay CSV.")); return; }
  File f=LittleFS.open(CSV_FILENAME,"r");
  if(!f){ Serial.println(F("[!] No se puede abrir el CSV.")); return; }
  Serial.println(F("\n----- INICIO CSV -----"));
  int n=0; while(f.available()){ Serial.println(f.readStringUntil('\n')); n++; }
  f.close();
  Serial.printf("----- FIN CSV (%d lineas) -----\n\n", n);
}

void infoCSV() {
  size_t tot=LittleFS.totalBytes(), us=LittleFS.usedBytes(), libre=tot-us;
  Serial.printf("Flash: %u KB total | %u KB usado | %u KB libre\n", tot/1024, us/1024, libre/1024);
  Serial.printf("Espacio: %s\n", espacioActual);
  Serial.printf("Ventana de registro: %02d:00-%02d:00 (todos los días)\n", horaInicio, horaFin);
  if (ventanaExtDia[0] != '\0')
    Serial.printf("Primera jornada en el aula: 24 h el día %s (automático)\n", ventanaExtDia);
  Serial.printf("Intervalo: %lu s  |  CPU: %lu MHz\n", LOG_INTERVAL_MS/1000, getCpuFrequencyMhz());
  if(littlefs_ok && LittleFS.exists(CSV_FILENAME)){
    File f=LittleFS.open(CSV_FILENAME,"r");
    size_t by=f.size(); int ln=0;
    while(f.available()) if(f.read()=='\n') ln++;
    f.close();
    int regs = ln>0?ln-1:0;
    Serial.printf("CSV: %u bytes, %d registros\n", by, regs);
    // Autonomía con el tamaño REAL de fila, medido sobre el propio fichero.
    // Antes se usaba una constante de 86 B que se quedó de antes de que el
    // campo estado incorporara CO2_CALENTANDO y DB_MUESTRA_UNICA: el peor caso
    // real son 112 B y la autonomía informada salía un 30 % optimista
    // (revision-rigor-3.md, hallazgo 31). Se dan las dos cifras: al ritmo
    // actual y si todo empezara a fallar a la vez.
    uint32_t regDia = ((horaFin-horaInicio)*3600UL)/(LOG_INTERVAL_MS/1000);
    uint16_t bpf     = (regs > 0) ? (uint16_t)(by / regs) : 60;
    uint16_t bpfPeor = (bpf > 112) ? bpf : 112;
    Serial.printf("Tamaño medio de fila: %u B (peor caso 112 B)\n", bpf);
    Serial.printf("Autonomía al ritmo actual: %lu registros = %.1f días\n",
      libre / bpf, (float)(libre / bpf)/regDia);
    Serial.printf("Autonomía en el peor caso: %lu registros = %.1f días\n",
      libre / bpfPeor, (float)(libre / bpfPeor)/regDia);
    if (ventanaExtDia[0] != '\0')
      Serial.println(F("  [i] Hoy se registra 24 h: esta jornada consume el doble."));
  }
  Serial.printf("Estado: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  if (fallosEscritura > 0)
    Serial.printf("[!!] Fallos de escritura acumulados: %u (datos perdidos)\n", fallosEscritura);
  if (flashCasiLlena)
    Serial.println(F("[!!] Flash por debajo del umbral: vuelca y borra los datos."));
}

// Comprobación de funcionamiento bajo demanda: un único destello verde.
// Se usa solo cuando el operador está presente; el resto del tiempo el LED
// permanece apagado para no señalizar la medición a los ocupantes.
void comprobarVida() {
  Serial.println(F("Destello de comprobación (el LED vuelve al brillo mínimo)."));
  bool todoOk = sht41_ok && dbmeter_ok && ds3231_ok && s8_ok && littlefs_ok
                && fallosEscritura == 0 && !flashCasiLlena;
  ledGreen(); delay(400);
  if (todoOk) ledEstadoOK(); else ledEstadoFallo();
  if (ds3231_ok) {
    DateTime n = rtc.now();
    Serial.printf("  Hora: %02d:%02d:%02d | Estado: %s | Registros: %lu\n",
      n.hour(), n.minute(), n.second(),
      enVentanaHoraria() ? "REGISTRANDO" : "en espera", rowCount);
    if (fallosEscritura > 0)
      Serial.printf("  [!!] %u fallos de escritura\n", fallosEscritura);
  }
}

// Borrado del CSV. Pide confirmación explicita: es irreversible y elimina
// todos los datos registrados. Tras borrar, se recrea el fichero con su
// cabecera para que el registro continue sin necesidad de reiniciar.
void borrarCSV() {
  if (!littlefs_ok) { Serial.println(F("[!] LittleFS no disponible.")); return; }
  if (!LittleFS.exists(CSV_FILENAME)) {
    Serial.println(F("[!] No hay fichero que borrar."));
    return;
  }
  // Informar de lo que se va a perder
  File f = LittleFS.open(CSV_FILENAME, "r");
  int ln = 0;
  if (f) { while (f.available()) if (f.read() == '\n') ln++; f.close(); }
  Serial.printf("\n[!] Se van a BORRAR %d registros. Es IRREVERSIBLE.\n", ln > 0 ? ln-1 : 0);
  Serial.println(F("    Escribe SI (mayúsculas) para confirmar, o cualquier otra cosa para cancelar."));

  // Esperar confirmación con límite de tiempo (evita bloqueo indefinido)
  unsigned long limite = millis() + 30000UL;
  while (!Serial.available()) {
    if (millis() > limite) {
      Serial.println(F("Tiempo agotado. Cancelado: el fichero NO se ha tocado.\n"));
      return;
    }
    delay(50);
  }
  String resp = Serial.readStringUntil('\n');
  resp.trim();

  if (resp == "SI") {
    if (LittleFS.remove(CSV_FILENAME)) {
      // Recrear con cabecera para poder seguir registrando
      File nf = LittleFS.open(CSV_FILENAME, "w");
      if (nf) {
        nf.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,eventos,estado");
        nf.close();
      }
      rowCount = 0;
      fallosEscritura = 0;
      flashCasiLlena = false;
      // El espacio se conserva: borrar los datos no cambia dónde está el equipo.
      Serial.println(F("[OK] Datos borrados. Fichero recreado, listo para un nuevo estudio.\n"));
      ledEstadoOK();
    } else {
      Serial.println(F("[!] No se pudo borrar el fichero.\n"));
    }
  } else {
    Serial.println(F("Cancelado: el fichero NO se ha tocado.\n"));
  }
}

// ============================================================
// COMANDO C - Verificación del CO2 y calibración de fondo del S8
// ============================================================
// El Senseair S8 mantiene su exactitud con una autocalibración de línea base
// (ABC) que necesita ver aire exterior (~400-420 ppm) periódicamente. En un
// aula ocupada de forma continua ese contacto puede no darse y el cero deriva
// sin aviso (revision-rigor-2.md, hallazgo 8).
//
// El protocolo de campaña dice: exponer el equipo al aire libre 30 minutos y
// comprobar que la lectura converge a ~420 ppm. Este comando ejecuta esa
// comprobación y, si de verdad hace falta, la corrección.
//
//   C           diagnóstico: lectura actual, estado del sensor y periodo ABC.
//               No modifica nada. Es lo que hay que hacer siempre.
//   C CALIBRAR  fuerza una calibración de fondo. SOLO al aire libre.
//
// Por qué la calibración va en un comando aparte y no dentro de C: la
// calibración de fondo NO ajusta el sensor contra un patrón, sino que le
// ordena asumir que LO QUE ESTÁ MIDIENDO AHORA son 400 ppm. Ejecutada dentro
// del aula destruye la calibración en lugar de arreglarla, y el daño es
// silencioso. Un comando que se teclea entero no se dispara por descuido.
//
// Dos cautelas técnicas que el código aplica y conviene no perder:
//
//  1. get_meter_status(), get_ABC_period() y get_acknowledgement() devuelven 0
//     tanto si el registro vale 0 como si falla la comunicación con el sensor.
//     Un cable suelto se leería, por tanto, como "sin errores". Por eso lo
//     primero es una lectura de CO2 válida, que sí distingue ambos casos.
//  2. El sensor PUEDE NEGARSE a calibrar si la señal es inestable en ese
//     momento, y lo hace sin devolver error. La especificación Modbus define
//     el bit 5 del registro de reconocimiento (0x0020) para saberlo. Mandar el
//     comando sin comprobar ese bit es el error habitual: se da por hecha una
//     calibración que no se ha producido.
//
// Referencia: "Modbus on Senseair S8", doc. TDE2067 — HR1 registro de
// reconocimiento, HR2 comando especial 0x7C06 (calibración de fondo),
// HR32 periodo ABC.
void calibrarCO2(const String& cmd) {
  if (!s8_ok) {
    Serial.println(F("[!] Sensor de CO2 no disponible: no hay nada que verificar."));
    return;
  }

  String arg = cmd.substring(1);
  arg.trim();
  arg.toLowerCase();
  bool calibrar = (arg == "calibrar");

  Serial.println(F("\n===== VERIFICACIÓN DEL SENSOR DE CO2 ====="));

  // (1) Lectura válida: sirve de prueba de vida de la comunicación
  int16_t antes = sensor_S8->get_co2();
  if (antes < 0) {
    Serial.println(F("[!] El sensor no responde. Revisa el UART y la alimentación de 5 V."));
    Serial.println(F("    Sin comunicación, el resto de registros leería 0 y parecería correcto."));
    return;
  }
  if (antes == 0) {
    Serial.println(F("[i] El sensor devuelve 0: aún se está calentando (~30 s tras encender)."));
    Serial.println(F("    Espera y repite el comando."));
    return;
  }
  Serial.printf("  Lectura actual .......... %d ppm\n", antes);

  // (2) Estado interno del sensor
  int16_t st = sensor_S8->get_meter_status();
  if (st & S8_MASK_METER_ANY_ERROR) {
    Serial.println(F("  Estado .................. [!!] EL SENSOR DECLARA ERROR"));
    if (st & S8_MASK_METER_FATAL_ERROR)             Serial.println(F("      - error fatal"));
    if (st & S8_MASK_METER_OFFSET_REGULATION_ERROR) Serial.println(F("      - error de regulación de offset"));
    if (st & S8_MASK_METER_ALGORITHM_ERROR)         Serial.println(F("      - error de algoritmo"));
    if (st & S8_MASK_METER_OUTPUT_ERROR)            Serial.println(F("      - error de salida"));
    if (st & S8_MASK_METER_SELF_DIAG_ERROR)         Serial.println(F("      - error de autodiagnóstico"));
    if (st & S8_MASK_METER_OUT_OF_RANGE)            Serial.println(F("      - medida fuera de rango"));
    if (st & S8_MASK_METER_MEMORY_ERROR)            Serial.println(F("      - error de memoria"));
    Serial.println(F("\n  Un sensor que declara error NO se calibra: la calibración"));
    Serial.println(F("  enmascararía el fallo. Sustituye el sensor.\n"));
    return;
  }
  Serial.println(F("  Estado .................. sin errores declarados"));

  // (3) Autocalibración automática
  int16_t abc = sensor_S8->get_ABC_period();
  if (abc > 0) Serial.printf("  Autocalibración ABC ..... activa, periodo %d h\n", abc);
  else         Serial.println(F("  Autocalibración ABC ..... SUSPENDIDA (periodo 0)"));

  if (!calibrar) {
    // --- Modo diagnóstico: interpretar la lectura y decir qué hacer ---
    Serial.println(F("\n  --- Interpretación (válida SOLO al aire libre, tras 30 min) ---"));
    if (antes >= 380 && antes <= 470) {
      Serial.println(F("  [OK] Compatible con aire exterior. El sensor NO necesita calibrarse."));
      Serial.printf("       Anota este valor al identificar el espacio:  E <aula> / CEXT %d\n", antes);
    } else if (antes < 380) {
      Serial.println(F("  [!] Por debajo del aire exterior. Si el equipo lleva 30 min"));
      Serial.println(F("      al aire libre, hay deriva a la baja: procede calibrar."));
    } else if (antes <= 550) {
      Serial.println(F("  [!] Algo alto para aire exterior. Antes de calibrar, descarta"));
      Serial.println(F("      lo mas probable: gente cerca, tráfico, patio cerrado, una"));
      Serial.println(F("      salida de extracción, o menos de 30 min de estabilización."));
    } else {
      Serial.println(F("  [!!] Demasiado alto para ser aire exterior. Casi con seguridad"));
      Serial.println(F("       el equipo NO esta al aire libre, o hay alguien al lado."));
      Serial.println(F("       Calibrar ahora fijaría este valor como 400 ppm y arruinaría"));
      Serial.println(F("       todas las medidas posteriores."));
    }
    Serial.println(F("\n  Si procede calibrar, escribe:  C CALIBRAR"));
    Serial.println(F("  Requisito: equipo al aire libre, lejos de tráfico y de personas,"));
    Serial.println(F("  quieto y con 30 minutos de estabilización.\n"));
    return;
  }

  // --- Calibración de fondo forzada ---
  Serial.println(F("\n  --- CALIBRACIÓN DE FONDO ---"));
  Serial.printf("  Se le va a ordenar al sensor asumir que %d ppm son 400 ppm.\n", antes);
  if (antes > 550) {
    Serial.println(F("  [!!] CANCELADO: esa lectura no es aire exterior."));
    Serial.println(F("       Por encima de 550 ppm el comando no se ejecuta, porque el"));
    Serial.println(F("       resultado seria un sensor peor calibrado que antes."));
    Serial.println(F("       Saca el equipo al aire libre, espera 30 min y repite.\n"));
    return;
  }

  if (!sensor_S8->manual_calibration()) {
    Serial.println(F("  [!] No se pudo enviar el comando de calibración al sensor.\n"));
    escribirMarca("CALIBRACION", "fallo de comunicación");
    return;
  }

  // Esperar al ciclo de lámpara y COMPROBAR el bit de reconocimiento: el
  // sensor puede haber ignorado la orden si la señal era inestable.
  Serial.print(F("  Esperando confirmación del sensor"));
  bool hecha = false;
  for (int i = 0; i < 15 && !hecha; i++) {   // hasta 30 s
    delay(2000);
    Serial.print(F("."));
    int16_t ack = sensor_S8->get_acknowledgement();
    hecha = (ack & S8_MASK_CO2_BACKGROUND_CALIBRATION) != 0;
  }
  Serial.println();

  char cbuf[64];
  if (!hecha) {
    Serial.println(F("  [!] El sensor NO confirma la calibración (bit 5 sin activar)."));
    Serial.println(F("      Suele significar que la concentración estaba cambiando en ese"));
    Serial.println(F("      momento. Deja el equipo quieto unos minutos mas y repite."));
    Serial.println(F("      IMPORTANTE: la calibración NO se ha aplicado.\n"));
    snprintf(cbuf, sizeof(cbuf), "NO realizada / antes %d ppm", antes);
    escribirMarca("CALIBRACION", cbuf);
    return;
  }

  delay(3000);
  int16_t despues = sensor_S8->get_co2();
  Serial.println(F("  [OK] Calibración de fondo CONFIRMADA por el sensor."));
  Serial.printf("       Antes: %d ppm   ->   Ahora: %d ppm\n", antes, despues);
  Serial.printf("       Anota el exterior al identificar el espacio:  E <aula> / CEXT %d\n",
                despues > 0 ? despues : 400);
  Serial.println(F("       Queda anotada en el CSV: la serie de CO2 tiene un salto en"));
  Serial.println(F("       este instante y el análisis debe saberlo.\n"));

  snprintf(cbuf, sizeof(cbuf), "OK / antes %d ppm / después %d ppm", antes, despues);
  escribirMarca("CALIBRACION", cbuf);
}

void procesarComando() {
  String cmd=Serial.readStringUntil('\n');
  cmd.trim();
  if(cmd.length()==0) return;
  switch(cmd.charAt(0)){
    case 'T': case 't': sincronizarReloj(cmd); break;
    case 'H': case 'h': mostrarHora();          break;
    case 'D': case 'd': volcarCSV();            break;
    case 'I': case 'i': infoCSV();              break;
    case 'L': case 'l': comprobarVida();        break;
    case 'E': case 'e': fijarEspacio(cmd);      break;
    case 'N': case 'n': anotarNota(cmd);        break;
    case 'W': case 'w': fijarVentana(cmd);      break;
    case 'C': case 'c': calibrarCO2(cmd);       break;
    case 'X': case 'x': borrarCSV();            break;
    case '?':           mostrarAyuda();         break;
    default: Serial.printf("Comando '%c' no reconocido. ? para ayuda.\n", cmd.charAt(0));
  }
}


// ============================================================
// setup()
// ============================================================
void setup() {
  // Reducir la frecuencia antes de inicializar nada más.
  setCpuFrequencyMhz(CPU_FREQ_MHZ);

  Serial.begin(115200);
  delay(2000);
  Serial.println(F("\n== DATALOGGER AMBIENTAL - Test firmware v1.0 =="));
  Serial.printf("   YD-ESP32-S3 N16R8 | core 3.2.0 | CPU a %lu MHz\n", getCpuFrequencyMhz());

  ledBlue();
  Wire.begin(PIN_SDA, PIN_SCL, 100000UL);
  delay(50);

  // UART S8 - workaround bug core 3.x (begin cuelga si RX tiene tráfico)
  Serial.println(F("\n[UART] Iniciando Senseair S8 LP..."));
  S8_serial.begin(9600, SERIAL_8N1, -1, -1);
  S8_serial.setPins(PIN_S8_RX, PIN_S8_TX);
  delay(100);
  sensor_S8 = new S8_UART(S8_serial);

  S8_sensor s8;
  sensor_S8->get_firmware_version(s8.firm_version);
  s8.sensor_type_id = sensor_S8->get_sensor_type_ID();
  if (strlen(s8.firm_version)>0 || s8.sensor_type_id>0) {
    s8_ok=true;
    Serial.printf("  [OK] S8 LP detectado. Tipo ID: 0x%08X  FW: %s\n", s8.sensor_type_id, s8.firm_version);
  } else {
    delay(2000);
    int c=sensor_S8->get_co2();
    s8_ok=(c>=0);
    if(s8_ok) Serial.println(F("  [OK] S8 LP respondiendo."));
    else {
      Serial.println(F("  [!!] S8 LP no responde. Verifica:"));
      Serial.println(F("       - UART cruzado: GPIO1(45J)TX->S8 RxD(2J) y GPIO2(46J)RX<-S8 TxD(3J)"));
      Serial.println(F("       - 5V: ESP32 col62B -> S8 col1A(G+). Puente IN-OUT cerrado?"));
      Serial.println(F("       - GND: S8 col2A(G0) -> carril GND"));
    }
  }

  // SHT41
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);
  sht4x.softReset();
  delay(10);
  uint32_t sn;
  if(sht4x.serialNumber(sn)==0){ sht41_ok=true; Serial.printf("  [OK] SHT41 init. N serie: 0x%08X\n", sn); }

  // DS3231
  if(rtc.begin(&Wire)){
    ds3231_ok=true;
    if(rtc.lostPower()){
      Serial.println(F("  [!] DS3231 perdió alimentación. Ajusta la hora con el comando T."));
    }
    DateTime n=rtc.now();
    Serial.printf("  [OK] DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  }

  fase1_scanI2C();
  fase2_leerSensores();
  fase3_pruebaLED();
  fase4_pruebaLittleFS();
  cargarEspacio();   // recupera la identificación tras un reinicio
  cargarVentana();   // y la ventana horaria ajustada con W
  cargarVentanaExt();// y la extensión de la primera jornada, si sigue vigente

  Serial.println(F("\n== RESUMEN DE DIAGNÓSTICO =="));
  Serial.printf("  SHT41 (T/H)       : %s\n", sht41_ok    ? "OK":"FALLO");
  Serial.printf("  DBMETER (dB)      : %s\n", dbmeter_ok  ? "OK":"FALLO");
  Serial.printf("  DS3231 (RTC)      : %s\n", ds3231_ok   ? "OK":"FALLO");
  Serial.printf("  Senseair S8 (CO2) : %s\n", s8_ok       ? "OK":"FALLO");
  Serial.printf("  LittleFS (flash)  : %s\n", littlefs_ok ? "OK":"FALLO");
  int ok=sht41_ok+dbmeter_ok+ds3231_ok+s8_ok+littlefs_ok;
  Serial.printf("\n  %d/5 componentes OK.\n", ok);
  if(ok==5){ Serial.println(F("  Sistema completo. Iniciando logging...")); ledGreen(); }
  else     { Serial.println(F("  [!] Hay fallos. Revisa el diagnóstico.")); ledError(3); }

  // Al iniciar el registro el LED pasa a brillo mínimo como simple testigo de
  // funcionamiento. No cambia con las condiciones ambientales ni con la
  // ventana horaria, para no señalizar la medición a los ocupantes.
  delay(1500);   // margen para que el operador vea el resultado del diagnóstico
  if (ok==5) ledEstadoOK(); else ledEstadoFallo();

  lastLog = millis() - LOG_INTERVAL_MS;
  enVentanaAnterior = enVentanaHoraria();

  Serial.printf("\n>> Ventana de registro: %02d:00-%02d:00 (todos los días, incl. fines de semana)\n",
    horaInicio, horaFin);
  Serial.printf(">> Estado actual: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  Serial.println(F(">> LED a brillo mínimo: solo testigo de funcionamiento."));
  Serial.println(F(">> No indica condiciones ambientales, para no contaminar el estudio."));
  Serial.println(F(">> IMPORTANTE: identifica el espacio con E antes de empezar."));
  Serial.println(F(">>   Ejemplo:  E Aula 3B / calle / RT 0.85 / CEXT 420"));
  Serial.println(F(">>   CEXT es la lectura al aire libre (comando C) antes de instalar."));
  Serial.println(F(">> Comandos: T=reloj H=hora E=espacio N=nota W=ventana C=calibrar"));
  Serial.println(F(">>           D=volcar I=info L=test X=borrar ?=ayuda"));
  Serial.println(F(">> Ejemplo para ajustar hora: T2026-08-06 06:50:00\n"));
}


// ============================================================
// loop()
// ============================================================
void loop() {
  if (Serial.available()) procesarComando();

  unsigned long now = millis();
  actualizarTestigo();          // parpadeo de alerta, dentro y fuera de horario
  bool activo = enVentanaHoraria();

  // Avisar por serie al entrar o salir de la ventana horaria
  // (solo por consola: el LED permanece apagado para no señalizar a los
  //  ocupantes cuando el equipo está registrando)
  if (activo != enVentanaAnterior) {
    if (ds3231_ok) {
      DateTime n = rtc.now();
      Serial.printf("\n>> %s ventana de registro (%02d:%02d) — activa %02d:00-%02d:00\n\n",
        activo ? "ENTRANDO en" : "SALIENDO de", n.hour(), n.minute(),
        horaInicio, horaFin);
    }
    enVentanaAnterior = activo;
    // Descartar acumuladores al cambiar de estado
    reiniciarAcumuladoresDb();
    // Y también la referencia de fondo: al entrar en la ventana por la mañana,
    // el umbral de eventos del primer intervalo seria el fondo de las 18:59 del
    // día anterior (revision-rigor-2.md, hallazgo 20). Con 0 el detector queda
    // inhibido un intervalo y arranca con el fondo real de la mañana.
    dbFondoPrevio = 0;
    lastLog = now - LOG_INTERVAL_MS;   // registrar de inmediato al entrar
  }

  if (!activo) { delay(50); return; }    // fuera de horario: en espera

  // Muestreo del sonómetro para media y pico del intervalo
  if (dbmeter_ok && (now - lastDbSample >= DB_SAMPLE_MS)) {
    lastDbSample = now;
    uint8_t db = readDBMeter();
    if (db > 0) {
      if (db > dbMaxIntervalo) dbMaxIntervalo = db;
      // El nivel equivalente promedia ENERGÍA, no decibelios: la escala es
      // logarítmica y una media aritmética de dB subestima los picos.
      double energia = pow(10.0, db / 10.0);
      dbSumaEnergia += energia;
      if (db < DB_HIST_SIZE) dbHistograma[db]++;
      dbNumMuestras++;

      // Ya no hay filtro software para reconstruir "Fast": el promediado de
      // 125 ms lo hace el propio módulo (DBM_TAVG_MS), de modo que cada
      // lectura ya es el valor ponderado y el pico del intervalo es
      // directamente dbMaxIntervalo.

      // Detección de eventos: se cuenta cada vez que el nivel sube por encima
      // del fondo más un margen. La histéresis evita contar varias veces un
      // mismo evento que oscila alrededor del umbral.
      if (dbFondoPrevio > 0) {
        uint8_t umbral = dbFondoPrevio + EVENTO_MARGEN_DB;
        if (!dbEnEvento && db >= umbral) { dbEventos++; dbEnEvento = true; }
        else if (dbEnEvento && db < umbral - EVENTO_HISTERESIS) { dbEnEvento = false; }
      }
    }
  }

  // Registro periódico
  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    logRow();
  }
}
