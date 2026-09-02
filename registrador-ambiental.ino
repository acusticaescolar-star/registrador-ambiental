// ============================================================
// datalogger_test_v1.ino
// Monitor Ambiental - firmware de prueba, verificacion y logging
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
// Librerias (Library Manager):
//   - Sensirion I2C SHT4x  (Sensirion)
//   - S8_UART              (jcomas)  -> el fichero es s8_uart.h (minusculas)
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
//   pasa casi todo el tiempo esperando, de modo que la reduccion no afecta al
//   funcionamiento y baja el consumo de ~84 a ~64 mA: unos 7,2 dias de
//   autonomia con un banco de 20.000 mAh, frente a 5,5 dias a 240 MHz.
//
// VENTANA HORARIA:
//   Solo se registra entre las 07:00 y las 19:00, todos los dias (incluidos
//   sabados y domingos, para permitir estudios de ruido exterior en fin de
//   semana). Fuera de ese horario el sistema queda en espera sin escribir.
//   Autonomia a 30 s de intervalo: >11 dias incluso en el peor caso.
//
// FORMATO CSV:
//   timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,dB_max,eventos,estado

//   El campo "estado" marca la FIABILIDAD del dato, no la valoracion de las
//   condiciones (esa se hace al analizar, con la tabla del documento de
//   proyecto). Vale "OK" o una o mas de estas marcas separadas por ';':
//     ERR_TH ERR_CO2 ERR_DB   sensor sin respuesta valida en esa lectura
//     ERR_RTC                 marca de tiempo no fiable
//     FLASH_BAJA              queda poco espacio de almacenamiento
//   Si aparece una marca ERR_, esa variable debe excluirse del analisis en
//   las filas afectadas.
//   Umbrales segun RD 486/1997 Anexo III, RITE (IDA2) y CTE DB-HS 3.
//   dB_LAeq  nivel continuo equivalente del intervalo. Promedia ENERGIA (no
//            decibelios), que es la magnitud correcta en acustica y la que
//            exige la normativa.
//   dB_fondo nivel de fondo (LA90 aprox.): el nivel que se supera el 90 % del
//            tiempo. Describe el ruido sostenido del aula.
//   dB_maxF  pico con ponderacion "Fast" (125 ms) reconstruida por software.
//            Es el comparable con un sonometro comercial y con los umbrales
//            de la normativa; sobre el se evaluan las alertas acusticas.
//   dB_max   pico con las muestras crudas de 31 ms, sensible a impulsos
//            breves (portazos, sillas). Se acerca a la ponderacion "Impulse".
//            La diferencia dB_max - dB_maxF indica cuan impulsivo es el ruido:
//            un impulso de 31 ms se atenua ~6,6 dB al aplicar Fast; uno
//            sostenido de mas de 500 ms, nada.
//   eventos  numero de veces que el nivel sube por encima del fondo mas 10 dB.
//
//   Los tres ultimos capturan la FLUCTUACION del ruido. El estudio BREATHE
//   (Foraster et al., 2022, PLOS Medicine) hallo que dentro del aula la
//   fluctuacion se asocia de forma consistente con el desarrollo cognitivo,
//   mientras que el nivel medio apenas lo hace. Registrar solo el LAeq
//   dejaria fuera la dimension que la investigacion senala como decisiva.
//   El rango dinamico (dB_max - dB_fondo) es el indicador de fluctuacion mas
//   directo que puede derivarse de estos datos.
//
//   El sonometro se muestrea cada 125 ms, al mismo ritmo que su promediado
//   interno ("Fast"), para no perder picos.
//
// NO INTERFERENCIA CON EL ESTUDIO:
//   Durante el registro el LED queda a brillo minimo (3/255) como simple
//   testigo de funcionamiento: verde tenue = todo correcto. Si algo falla
//   (sensor sin respuesta, error de escritura, flash casi llena) pasa a
//   ROJO INTERMITENTE y brillante: una averia debe verse desde lejos para
//   que el operador intervenga, y no informa de condiciones ambientales.
//   NO codifica condiciones ambientales ni la ventana horaria. Si lo hiciera,
//   los ocupantes sabrian cuando se mide o que el aire esta cargado, y
//   podrian ventilar o bajar la voz: el dato dejaria de reflejar el aula
//   real (reactividad). Se mantiene encendido porque el modulo DS3231 ya
//   lleva un LED de alimentacion permanente e inevitable; el equipo se ve
//   encendido igualmente. El comando L da un destello puntual mas visible
//   para que el operador compruebe el estado cuando lo necesite.
//
// COMANDOS por el monitor serie (escribir y pulsar Enter):
//   T2026-08-06 06:50:00  ajustar reloj (formato exacto TAAAA-MM-DD hh:mm:ss)
//   H   mostrar hora del RTC
//   D   volcar el CSV guardado
//   I   info del fichero y flash
//   L   destello de comprobacion del estado
//   X   BORRAR todos los datos (pide confirmacion)
//   ?   ayuda
// El registro de datos sigue activo mientras se aceptan comandos.
// ============================================================
// Autoria: www.acusticaescolar.com - Licencia MIT
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

// Tiempo de promediado interno del sonometro.
// 125 ms equivale a la ponderacion temporal "Fast" de un sonometro normalizado.
// Por defecto el modulo viene a 1000 ms ("Slow"), que suaviza los picos y
// hace que el maximo del intervalo pierda significado acustico.
#define DBM_TAVG_MS    31   // muestreo rapido, sensible a impulsos breves

// Reconstruccion de la ponderacion "Fast" (125 ms) por software.
// El sonometro promedia de forma exponencial. Muestreando a 31 ms se puede
// reconstruir lo que marcaria a 125 ms aplicando un filtro exponencial de
// primer orden sobre la ENERGIA: alpha = 1 - exp(-dt/tau).
// Asi se obtienen a la vez el pico sensible a impulsos (dB_max) y el pico
// normalizado comparable con un sonometro comercial (dB_maxF).
#define DBM_TAU_FAST_MS  125
#define ALPHA_FAST  0.2196f   // 1 - exp(-31/125)

// -- Sobre la valoracion de las condiciones --
// El firmware NO clasifica las condiciones ambientales: registra los valores
// medidos y deja la valoracion para el analisis posterior. Asi los datos no
// caducan si cambian los umbrales normativos (RITE y CTE estan en revision),
// y las etiquetas de condicion serian ademas redundantes: T_ALTA se deduce de
// temp_C, CO2_MALA de co2_ppm, etc.
// La tabla de interpretacion (RD 486/1997 Anexo III, RITE IDA2, CTE DB-HS 3,
// NTP 742) figura en el documento de proyecto, que es donde puede mantenerse
// actualizada.
// El campo "estado" del CSV solo informa de la FIABILIDAD del dato, que NO es
// derivable de los valores: si una lectura de CO2 vale 0, sin la marca no
// habria forma de saber si fue un fallo del sensor o una medida real.

// -- Indicadores de fluctuacion (marco BREATHE / Foraster et al. 2022) --
// El estudio BREATHE (ISGlobal, 2.680 escolares de Barcelona) encontro que
// dentro del aula la FLUCTUACION del ruido se asocia de forma consistente con
// el desarrollo cognitivo, mientras que el nivel medio apenas lo hace. Por eso
// no basta con registrar el LAeq: se anaden el nivel de fondo y un contador de
// eventos, que capturan esa dimension.
#define DB_HIST_SIZE       128   // histograma de niveles (1 dB por casilla)
#define DB_PERCENTIL_FONDO  10   // LA90: nivel superado el 90 % del tiempo
#define EVENTO_MARGEN_DB    10   // un evento supera el fondo en esta cantidad
#define EVENTO_HISTERESIS    3   // margen de salida, evita contar rebotes

// -- Muestreo del sonometro --
// Se muestrea al mismo ritmo que el promediado interno (125 ms) para no
// perder picos entre lecturas: a 1 s de intervalo quedaban 875 ms sin observar.
#define DB_SAMPLE_MS  125UL

// -- Logging --
#define LOG_INTERVAL_MS  30000UL
#define CSV_FILENAME     "/datalog.csv"

// -- Frecuencia de la CPU --
// A 80 MHz el consumo baja de ~45 a ~25 mA sin afectar al funcionamiento: el
// firmware pasa casi todo el tiempo esperando, no calculando. Supone un 38 %
// mas de autonomia con la misma bateria. Subir a 240 solo si se anadieran
// tareas exigentes (WiFi, procesado de senal).
#define CPU_FREQ_MHZ  80

// -- Ventana horaria de registro --
// Solo se registra entre HORA_INICIO y HORA_FIN (todos los dias, incluidos
// fines de semana: permite estudios de ruido exterior en sabado y domingo).
// Fuera de la ventana el sistema permanece en espera sin escribir en flash.
#define HORA_INICIO   7    // 07:00 h
#define HORA_FIN     19    // 19:00 h (se registra hasta las 18:59:59)
// Nota: la ventana no admite cruce de medianoche (HORA_FIN debe ser mayor
// que HORA_INICIO). Para registro continuo 24 h, poner 0 y 24.

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
uint8_t  dbMaxIntervalo = 0;    // pico observado en el intervalo actual
double   dbSumaEnergia = 0.0;   // suma de energias para el LAeq
uint16_t dbNumMuestras = 0;     // numero de muestras acumuladas
uint16_t dbHistograma[DB_HIST_SIZE];  // distribucion de niveles del intervalo
uint8_t  dbFondoPrevio = 0;     // fondo del intervalo anterior (umbral eventos)
double   dbEnergiaFast = 0.0;   // estado del filtro que reconstruye "Fast"
uint8_t  dbMaxFast = 0;         // pico segun ponderacion Fast reconstruida
bool     dbFastIniciado = false;
uint16_t dbEventos = 0;         // eventos detectados en el intervalo
bool     dbEnEvento = false;    // estado del detector de eventos

// Vacia el histograma y los acumuladores del intervalo
void reiniciarAcumuladoresDb() {
  reiniciarAcumuladoresDb();
  dbEventos = 0; dbEnEvento = false;
  dbMaxFast = 0; dbFastIniciado = false;
  memset(dbHistograma, 0, sizeof(dbHistograma));
}

// Nivel de fondo del intervalo: percentil bajo de la distribucion (LA90),
// es decir, el nivel que se supera el 90 % del tiempo. Describe el ruido
// sostenido sobre el que destacan los eventos.
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

// -- Identificacion del espacio medido --
// Imprescindible cuando el equipo rota entre aulas: sin esta marca, al volcar
// varios dias de varios espacios los datos quedarian mezclados sin remedio.
// Se guarda como linea de marca en el CSV, no como columna, para no repetir
// el nombre en cada registro.
#define ESPACIO_MAX 48
char espacioActual[ESPACIO_MAX] = "sin identificar";

// -- Ventana horaria --
bool enVentanaAnterior = true;  // para avisar solo en los cambios de estado

// Devuelve true si la hora actual esta dentro de la ventana de registro.
// Si el RTC no esta disponible, registra siempre (para no perder datos).
bool enVentanaHoraria() {
  if (!ds3231_ok) return true;
  int hora = rtc.now().hour();
  return (hora >= HORA_INICIO && hora < HORA_FIN);
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

// -- LED de estado a brillo minimo --
// Durante el registro el LED queda encendido al minimo perceptible, solo como
// testigo de que el sistema funciona. NO codifica condiciones ambientales ni
// la ventana horaria: si lo hiciera, delataria el estudio a los ocupantes.
// Se mantiene encendido porque el modulo DS3231 ya lleva un LED de
// alimentacion permanente que no puede apagarse; el equipo se ve encendido
// igualmente, de modo que un testigo tenue no anade informacion nueva.
#define BRILLO_MIN     3       // 3/255: visible de cerca, apenas perceptible
#define BRILLO_ALERTA 90       // rojo bien visible para reclamar atencion
#define BLINK_MS     600UL     // periodo de parpadeo de la alerta

// Estado del testigo. El fallo se senaliza con rojo INTERMITENTE y brillante:
// una averia debe verse desde lejos para que el operador intervenga. No
// contamina el estudio porque no informa de condiciones ambientales, sino de
// que el equipo necesita atencion.
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

// Gestion no bloqueante del parpadeo de alerta. Se llama desde el loop en
// cada vuelta, tambien fuera de la ventana horaria: un fallo debe verse
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
// Configura el tiempo de promediado interno del sonometro.
// El byte alto debe escribirse primero (asi lo exige el modulo).
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
    Serial.println(F("  [!!] NO se encontro ningun dispositivo I2C."));
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

  // Configurar el promediado interno del sonometro a 125 ms ("Fast").
  // Por defecto viene a 1000 ms, que suaviza los picos.
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
    Serial.printf("  DBMETER -> Version fw: 0x%02X  |  Tavg: %u ms\n", v, getDBMeterTavg());
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

  // Comprobacion del hardware a brillo pleno: verifica que el WS2812B
  // responde en los tres canales de color.
  Serial.println(F("  Comprobacion de canales a brillo pleno:"));
  setLED(80,0,0);  Serial.println(F("    rojo"));  delay(500);
  setLED(0,80,0);  Serial.println(F("    verde")); delay(500);
  setLED(0,0,80);  Serial.println(F("    azul"));  delay(500);
  ledOff();        delay(300);

  // Comprobacion de los dos estados reales de trabajo, a brillo minimo:
  // es como se vera durante el estudio.
  Serial.println(F("\n  Estados de trabajo:"));
  ledEstadoOK();
  Serial.printf("    verde tenue (%d/255)      -> sistema correcto\n", BRILLO_MIN);
  delay(1500);
  Serial.println(F("    rojo intermitente        -> requiere atencion"));
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
  f.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,dB_max,eventos,estado");
  f.println("2025-04-15T09:00:00,22.50,55.0,750,48,42,50,52,3,OK");
  f.println("2025-04-15T09:00:03,28.10,45.0,1350,58,44,66,71,12,ERR_CO2");
  f.close();
  Serial.printf("  [OK] Fichero de prueba escrito: %s\n", tf);

  f=LittleFS.open(tf,"r");
  if(!f){ Serial.println(F("  [!!] No se puede leer fichero.")); littlefs_ok=false; return; }
  Serial.println(F("  Contenido leido:"));
  while(f.available()){ Serial.print("    "); Serial.println(f.readStringUntil('\n')); }
  f.close();

  if(!LittleFS.exists(CSV_FILENAME)){
    File lg=LittleFS.open(CSV_FILENAME,"w");
    lg.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,dB_max,eventos,estado");
    lg.close();
    Serial.printf("  [OK] Fichero de log creado: %s\n", CSV_FILENAME);
  } else {
    Serial.printf("  [OK] Fichero de log existente: %s\n", CSV_FILENAME);
  }
  LittleFS.remove(tf);
  Serial.println(F("  [OK] LittleFS: escritura/lectura/verificacion superadas."));
}


// ============================================================
// FASE 5 - Registro de una fila
// ============================================================

// Escribe una linea de marca en el CSV. Las marcas empiezan por '#' y llevan
// su propio timestamp: sirven para segmentar el analisis por espacio y para
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

// Identifica el espacio que se esta midiendo y su exposicion acustica.
// Se anota al instalar el equipo en cada aula: a partir de ese momento los
// registros pertenecen a ese espacio.
//
// Formato:  E Aula 3B / calle
//           E Aula 2A / patio
//
// La exposicion condiciona la interpretacion. En un aula orientada al patio,
// las franjas sin ocupacion NO reflejan el ruido exterior estructural: hay
// alumnos esperando al comedor, educacion fisica, recreos escalonados. El
// analisis lo tiene en cuenta y omite ahi la estimacion de fuentes, en lugar
// de dar un numero enganoso.
void fijarEspacio(const String& cmd) {
  String v = cmd.substring(1);
  v.trim();
  if (v.length() == 0) {
    Serial.printf("Espacio actual: %s\n", espacioActual);
    Serial.println(F("Para cambiarlo:  E Aula 3B / calle"));
    Serial.println(F("Exposiciones:    calle | patio | interior | mixta"));
    Serial.println(F("  calle    da a via publica: el exterior es sobre todo trafico"));
    Serial.println(F("  patio    da al patio: actividad escolar al aire libre"));
    Serial.println(F("  interior da a un patio de luces o espacio cerrado"));
    Serial.println(F("  mixta    ventanas a mas de una orientacion"));
    return;
  }
  // Avisar si no se indica exposicion, pero aceptarlo igualmente
  if (v.indexOf('/') < 0) {
    Serial.println(F("[i] No has indicado la exposicion. Ejemplo: E Aula 3B / calle"));
    Serial.println(F("    Sin ella, el analisis no puede interpretar el ruido exterior."));
  }
  v.toCharArray(espacioActual, ESPACIO_MAX);
  escribirMarca("ESPACIO", espacioActual);
}

// Anota una incidencia con su hora: obras en el pasillo, ventana abierta,
// actividad en el aula contigua... Lo que el analisis posterior no puede saber.
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
                     bool errTH, bool errCO2, bool errDB, bool errRTC) {
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
  if (errDB)  add("ERR_DB");    // sonometro sin respuesta valida
  if (flashCasiLlena) add("FLASH_BAJA");

  if (primera) strncpy(buf, "OK", n);
}

void logRow() {
  float t=0,h=0; int co2=0;
  bool errTH=false, errCO2=false, errDB=false, errRTC=false;

  // Lectura T/HR con deteccion de error
  if (sht41_ok) {
    uint16_t e = sht4x.measureHighPrecision(t,h);
    if (e != 0) { errTH = true; t = 0; h = 0; }
  }

  // Lectura CO2 con deteccion de error (get_co2 devuelve <0 si falla)
  if (s8_ok) {
    int16_t r = sensor_S8->get_co2();
    if (r > 0) co2 = r;
    else if (r < 0) errCO2 = true;
    // r == 0 durante el calentamiento inicial: no se marca como error
  }

  // Ruido: nivel equivalente (LAeq) y pico del intervalo.
  // LAeq = 10 * log10( (1/n) * suma(10^(Li/10)) )
  uint8_t dbEq = 0, dbMax = dbMaxIntervalo, dbMaxF = dbMaxFast, dbFondo = 0;
  uint16_t eventos = dbEventos;
  if (dbNumMuestras > 0) {
    dbEq = (uint8_t)lround(10.0 * log10(dbSumaEnergia / dbNumMuestras));
    dbFondo = calcularFondo();
    dbFondoPrevio = dbFondo;   // referencia para los eventos del siguiente
  } else if (dbmeter_ok) {
    dbEq = readDBMeter(); dbMax = dbEq; dbMaxF = dbEq; dbFondo = dbEq;
    if (dbEq == 0) errDB = true;
  }

  // Marca de tiempo
  char ts[25]="0000-00-00T00:00:00";
  if(ds3231_ok){
    DateTime n=rtc.now();
    if (n.year() < 2020) errRTC = true;   // reloj sin ajustar o sin pila
    snprintf(ts,sizeof(ts),"%04d-%02d-%02dT%02d:%02d:%02d",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  } else errRTC = true;

  // Control de espacio libre en flash
  if (littlefs_ok) {
    size_t libre = LittleFS.totalBytes() - LittleFS.usedBytes();
    bool bajaAhora = (libre / 1024) < FLASH_MIN_KB;
    if (bajaAhora && !flashCasiLlena) {
      Serial.printf("\n[!!] AVISO: quedan %u KB de flash. Vuelca y borra los datos.\n\n",
        libre/1024);
      ledEstadoFallo();       // rojo intermitente: requiere intervencion
    } else if (!bajaAhora && flashCasiLlena && fallosEscritura == 0) {
      // Se ha liberado espacio y no hay otros fallos: volver a estado normal
      Serial.println(F("[OK] Espacio de almacenamiento restablecido.\n"));
      ledEstadoOK();
    }
    flashCasiLlena = bajaAhora;
  }

  char estado[48];
  construirEstado(estado, sizeof(estado), errTH, errCO2, errDB, errRTC);

  char row[160];
  snprintf(row,sizeof(row),"%s,%.2f,%.1f,%d,%d,%d,%d,%d,%u,%s",
    ts, t, h, co2, dbEq, dbFondo, dbMaxF, dbMax, eventos, estado);

  // Escritura con verificacion: si falla, se contabiliza y se avisa
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
  Serial.println(F("     asi que solo hay que ajustarlo una vez."));
  Serial.println(F(""));
  Serial.println(F("  E  IDENTIFICAR EL ESPACIO y su exposicion acustica"));
  Serial.println(F("     Ejemplo:  E Aula 3B / calle"));
  Serial.println(F("     Exposiciones: calle | patio | interior | mixta"));
  Serial.println(F("     Hazlo SIEMPRE al instalar el equipo en un aula nueva."));
  Serial.println(F("     Sin esta marca, al volcar varios espacios los datos"));
  Serial.println(F("     quedan mezclados y no hay forma de separarlos."));
  Serial.println(F("     Escribe solo E para ver el espacio actual."));
  Serial.println(F(""));
  Serial.println(F("  N  ANOTAR UNA INCIDENCIA con su hora"));
  Serial.println(F("     Ejemplo:  N obras en el pasillo"));
  Serial.println(F("     Util para lo que el analisis no puede deducir:"));
  Serial.println(F("     ventana abierta, actividad en el aula contigua..."));
  Serial.println(F(""));
  Serial.println(F("  H  Mostrar la hora actual del reloj"));
  Serial.println(F("  D  Volcar por pantalla todo el CSV guardado"));
  Serial.println(F("  I  Info: espacio, registros, estado y autonomia"));
  Serial.println(F("  L  Destello de comprobacion del equipo"));
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
    Serial.println(F("[!] Valores fuera de rango. Revisa mes (1-12), dia (1-31) y hora (0-23)."));
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
  Serial.printf("Ventana de registro: %02d:00-%02d:00 (todos los dias)\n", HORA_INICIO, HORA_FIN);
  Serial.printf("Intervalo: %lu s  |  CPU: %lu MHz\n", LOG_INTERVAL_MS/1000, getCpuFrequencyMhz());
  if(littlefs_ok && LittleFS.exists(CSV_FILENAME)){
    File f=LittleFS.open(CSV_FILENAME,"r");
    size_t by=f.size(); int ln=0;
    while(f.available()) if(f.read()=='\n') ln++;
    f.close();
    int regs = ln>0?ln-1:0;
    Serial.printf("CSV: %u bytes, %d registros\n", by, regs);
    // Autonomia estimada en el peor caso (86 B/fila, todas las alertas)
    uint32_t regDia = ((HORA_FIN-HORA_INICIO)*3600UL)/(LOG_INTERVAL_MS/1000);
    uint32_t capacidad = libre / 86;
    Serial.printf("Autonomia restante (peor caso): %lu registros = %.1f dias\n",
      capacidad, (float)capacidad/regDia);
  }
  Serial.printf("Estado: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  if (fallosEscritura > 0)
    Serial.printf("[!!] Fallos de escritura acumulados: %u (datos perdidos)\n", fallosEscritura);
  if (flashCasiLlena)
    Serial.println(F("[!!] Flash por debajo del umbral: vuelca y borra los datos."));
}

// Comprobacion de funcionamiento bajo demanda: un unico destello verde.
// Se usa solo cuando el operador esta presente; el resto del tiempo el LED
// permanece apagado para no senalizar la medicion a los ocupantes.
void comprobarVida() {
  Serial.println(F("Destello de comprobacion (el LED vuelve al brillo minimo)."));
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

// Borrado del CSV. Pide confirmacion explicita: es irreversible y elimina
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
  Serial.println(F("    Escribe SI (mayusculas) para confirmar, o cualquier otra cosa para cancelar."));

  // Esperar confirmacion con limite de tiempo (evita bloqueo indefinido)
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
        nf.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_LAeq,dB_fondo,dB_maxF,dB_max,eventos,estado");
        nf.close();
      }
      rowCount = 0;
      fallosEscritura = 0;
      flashCasiLlena = false;
      Serial.println(F("[OK] Datos borrados. Fichero recreado, listo para un nuevo estudio.\n"));
      ledEstadoOK();
    } else {
      Serial.println(F("[!] No se pudo borrar el fichero.\n"));
    }
  } else {
    Serial.println(F("Cancelado: el fichero NO se ha tocado.\n"));
  }
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
    case 'X': case 'x': borrarCSV();            break;
    case '?':           mostrarAyuda();         break;
    default: Serial.printf("Comando '%c' no reconocido. ? para ayuda.\n", cmd.charAt(0));
  }
}


// ============================================================
// setup()
// ============================================================
void setup() {
  // Reducir la frecuencia antes de inicializar nada mas.
  setCpuFrequencyMhz(CPU_FREQ_MHZ);

  Serial.begin(115200);
  delay(2000);
  Serial.println(F("\n== DATALOGGER AMBIENTAL - Test firmware v1.0 =="));
  Serial.printf("   YD-ESP32-S3 N16R8 | core 3.2.0 | CPU a %lu MHz\n", getCpuFrequencyMhz());

  ledBlue();
  Wire.begin(PIN_SDA, PIN_SCL, 100000UL);
  delay(50);

  // UART S8 - workaround bug core 3.x (begin cuelga si RX tiene trafico)
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
      Serial.println(F("  [!] DS3231 perdio alimentacion. Ajusta la hora con el comando T."));
    }
    DateTime n=rtc.now();
    Serial.printf("  [OK] DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  }

  fase1_scanI2C();
  fase2_leerSensores();
  fase3_pruebaLED();
  fase4_pruebaLittleFS();

  Serial.println(F("\n== RESUMEN DE DIAGNOSTICO =="));
  Serial.printf("  SHT41 (T/H)       : %s\n", sht41_ok    ? "OK":"FALLO");
  Serial.printf("  DBMETER (dB)      : %s\n", dbmeter_ok  ? "OK":"FALLO");
  Serial.printf("  DS3231 (RTC)      : %s\n", ds3231_ok   ? "OK":"FALLO");
  Serial.printf("  Senseair S8 (CO2) : %s\n", s8_ok       ? "OK":"FALLO");
  Serial.printf("  LittleFS (flash)  : %s\n", littlefs_ok ? "OK":"FALLO");
  int ok=sht41_ok+dbmeter_ok+ds3231_ok+s8_ok+littlefs_ok;
  Serial.printf("\n  %d/5 componentes OK.\n", ok);
  if(ok==5){ Serial.println(F("  Sistema completo. Iniciando logging...")); ledGreen(); }
  else     { Serial.println(F("  [!] Hay fallos. Revisa el diagnostico.")); ledError(3); }

  // Al iniciar el registro el LED pasa a brillo minimo como simple testigo de
  // funcionamiento. No cambia con las condiciones ambientales ni con la
  // ventana horaria, para no senalizar la medicion a los ocupantes.
  delay(1500);   // margen para que el operador vea el resultado del diagnostico
  if (ok==5) ledEstadoOK(); else ledEstadoFallo();

  lastLog = millis() - LOG_INTERVAL_MS;
  enVentanaAnterior = enVentanaHoraria();

  Serial.printf("\n>> Ventana de registro: %02d:00-%02d:00 (todos los dias, incl. fines de semana)\n",
    HORA_INICIO, HORA_FIN);
  Serial.printf(">> Estado actual: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  Serial.println(F(">> LED a brillo minimo: solo testigo de funcionamiento."));
  Serial.println(F(">> No indica condiciones ambientales, para no contaminar el estudio."));
  Serial.println(F(">> IMPORTANTE: identifica el espacio con E antes de empezar."));
  Serial.println(F(">>   Ejemplo:  E Aula 3B / calle"));
  Serial.println(F(">> Comandos: T=reloj H=hora E=espacio N=nota D=volcar I=info L=test X=borrar ?=ayuda"));
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
  //  ocupantes cuando el equipo esta registrando)
  if (activo != enVentanaAnterior) {
    if (ds3231_ok) {
      DateTime n = rtc.now();
      Serial.printf("\n>> %s ventana de registro (%02d:%02d) — activa %02d:00-%02d:00\n\n",
        activo ? "ENTRANDO en" : "SALIENDO de", n.hour(), n.minute(),
        HORA_INICIO, HORA_FIN);
    }
    enVentanaAnterior = activo;
    // Descartar acumuladores al cambiar de estado
    reiniciarAcumuladoresDb();
    lastLog = now - LOG_INTERVAL_MS;   // registrar de inmediato al entrar
  }

  if (!activo) { delay(50); return; }    // fuera de horario: en espera

  // Muestreo del sonometro para media y pico del intervalo
  if (dbmeter_ok && (now - lastDbSample >= DB_SAMPLE_MS)) {
    lastDbSample = now;
    uint8_t db = readDBMeter();
    if (db > 0) {
      if (db > dbMaxIntervalo) dbMaxIntervalo = db;
      // El nivel equivalente promedia ENERGIA, no decibelios: la escala es
      // logaritmica y una media aritmetica de dB subestima los picos.
      double energia = pow(10.0, db / 10.0);
      dbSumaEnergia += energia;
      if (db < DB_HIST_SIZE) dbHistograma[db]++;
      dbNumMuestras++;

      // Filtro exponencial sobre la energia: reconstruye la ponderacion
      // temporal "Fast" (125 ms) a partir de las muestras rapidas de 31 ms.
      if (!dbFastIniciado) { dbEnergiaFast = energia; dbFastIniciado = true; }
      else dbEnergiaFast += ALPHA_FAST * (energia - dbEnergiaFast);
      uint8_t dbF = (uint8_t)lround(10.0 * log10(dbEnergiaFast));
      if (dbF > dbMaxFast) dbMaxFast = dbF;

      // Deteccion de eventos: se cuenta cada vez que el nivel sube por encima
      // del fondo mas un margen. La histeresis evita contar varias veces un
      // mismo evento que oscila alrededor del umbral.
      if (dbFondoPrevio > 0) {
        uint8_t umbral = dbFondoPrevio + EVENTO_MARGEN_DB;
        if (!dbEnEvento && db >= umbral) { dbEventos++; dbEnEvento = true; }
        else if (dbEnEvento && db < umbral - EVENTO_HISTERESIS) { dbEnEvento = false; }
      }
    }
  }

  // Registro periodico
  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    logRow();
  }
}
