#include <Wire.h>
#include "esp_system.h"

const uint8_t  PIN_SDA      = 5;
const uint8_t  PIN_SCL      = 6;
const uint8_t  PIN_MOTOR    = 7;
const uint32_t PWM_FREQ_HZ  = 1000;
const uint8_t  PWM_NIVEL[3] = {0, 90, 230};

const uint32_t FS_HZ        = 100;
const uint32_t DURACION_S   = 20;
const uint8_t  DLPF_CFG     = 3;
const float    LSB_POR_G    = 16384.0f;
const uint32_t I2C_HZ       = 100000;

const uint32_t TS_US = 1000000UL / FS_HZ;
uint8_t mpuAddr = 0x68;
bool    mpuOk   = false;

bool existe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void escribirReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int leerReg(uint8_t reg) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(mpuAddr, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

bool leerAcelUnaVez(int16_t &ax, int16_t &ay, int16_t &az);

bool leerAcel(int16_t &ax, int16_t &ay, int16_t &az) {
  for (int i = 0; i < 3; i++) if (leerAcelUnaVez(ax, ay, az)) return true;
  return false;
}

bool leerAcelUnaVez(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, (uint8_t)6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  ax = (int16_t)((b[0] << 8) | b[1]);
  ay = (int16_t)((b[2] << 8) | b[3]);
  az = (int16_t)((b[4] << 8) | b[5]);
  return true;
}

void escanearI2C() {
  Serial.printf("# Escaneo I2C (SDA=GPIO%u, SCL=GPIO%u): ", PIN_SDA, PIN_SCL);
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (existe(a)) { Serial.printf("0x%02X ", a); n++; }
  }
  Serial.println(n ? "" : "ningun dispositivo");
}

void recuperarBus() {
  Wire.end();
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_SCL, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9 && digitalRead(PIN_SDA) == LOW; i++) {
    digitalWrite(PIN_SCL, LOW);  delayMicroseconds(10);
    digitalWrite(PIN_SCL, HIGH); delayMicroseconds(10);
  }
  pinMode(PIN_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_SDA, LOW);  delayMicroseconds(10);
  digitalWrite(PIN_SCL, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_SDA, HIGH); delayMicroseconds(10);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);
  Wire.setTimeOut(20);
}

bool iniciarMPUunaVez();

bool iniciarMPU() {
  for (int i = 0; i < 5; i++) {
    if (!existe(0x68) && !existe(0x69)) recuperarBus();
    if (iniciarMPUunaVez()) return true;
    delay(200);
  }
  return false;
}

bool iniciarMPUunaVez() {
  if (existe(0x68))      mpuAddr = 0x68;
  else if (existe(0x69)) mpuAddr = 0x69;
  else return false;

  escribirReg(0x6B, 0x80);
  delay(100);
  escribirReg(0x6B, 0x01);
  escribirReg(0x1A, DLPF_CFG);
  escribirReg(0x19, 0x00);
  escribirReg(0x1C, 0x00);
  delay(50);

  int w = leerReg(0x75);
  if (w <= 0 || w == 0xFF) {
    Serial.printf("#   intento fallido: who_am_i=%d\n", w);
    return false;
  }
  if (w != 0x68) {

    escribirReg(0x1D, DLPF_CFG);
  }
  return true;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  void pwmInit()           { ledcAttach(PIN_MOTOR, PWM_FREQ_HZ, 8); }
  void pwmMotor(uint8_t d) { ledcWrite(PIN_MOTOR, d); }
#else
  void pwmInit()           { ledcSetup(0, PWM_FREQ_HZ, 8); ledcAttachPin(PIN_MOTOR, 0); }
  void pwmMotor(uint8_t d) { ledcWrite(0, d); }
#endif

void motor(uint8_t nivel) { pwmMotor(PWM_NIVEL[nivel]); }

void info() {
  Serial.printf("# fs_hz=%lu\n", (unsigned long)FS_HZ);
  Serial.printf("# duracion_s=%lu\n", (unsigned long)DURACION_S);
  Serial.println("# sensor=MPU6500 (modulo compatible MPU6050)");
  Serial.println("# plataforma=Heltec_WiFi_LoRa_32_V3_ESP32-S3");
  Serial.println("# rango=+-2g");
  Serial.println("# unidades=cuentas_ADC_crudas(LSB); g = raw/16384");
  Serial.printf("# lsb_por_g=%.1f\n", LSB_POR_G);
  Serial.printf("# dlpf_cfg=%u (filtro interno ~44 Hz)\n", DLPF_CFG);
  Serial.printf("# i2c_addr=0x%02X\n", mpuAddr);
  int w = leerReg(0x75);
  Serial.printf("# who_am_i=0x%02X\n", (uint8_t)w);
  if (w != 0x68) Serial.printf("# accel_dlpf_reg0x1D=%u (clon MPU6500)\n", (uint8_t)leerReg(0x1D) & 0x0F);
  Serial.println("# perturbacion=motor_DC_L298N");
  Serial.printf("# pwm_freq_hz=%lu\n", (unsigned long)PWM_FREQ_HZ);
  Serial.printf("# i2c_hz=%lu\n", (unsigned long)I2C_HZ);
  Serial.println("# firmware=v5");
}

bool verificarMPU() {
  if (mpuOk) return true;
  Serial.println("# Sensor no listo: reintentando...");
  mpuOk = iniciarMPU();
  if (!mpuOk) {
    Serial.println("# ERROR: sensor no detectado. Revise SDA->GPIO5, SCL->GPIO6, VCC->3V3, GND y soldadura.");
    escanearI2C();
  }
  if (mpuOk) Serial.println("# Sensor detectado al reintentar.");
  return mpuOk;
}

void diagnostico() {
  if (!verificarMPU()) return;
  int32_t sx = 0, sy = 0, sz = 0;
  int16_t ax, ay, az;
  uint32_t errores = 0;
  for (uint32_t i = 0; i < FS_HZ; i++) {
    if (leerAcel(ax, ay, az)) { sx += ax; sy += ay; sz += az; }
    else errores++;
    delayMicroseconds(TS_US);
  }
  uint32_t ok = FS_HZ - errores;
  if (ok == 0) { Serial.println("# ERROR: no se pudo leer el sensor."); return; }
  Serial.printf("Media 1 s [g]  X=%+.3f  Y=%+.3f  Z=%+.3f   (errores=%lu)\n",
                sx / (float)ok / LSB_POR_G, sy / (float)ok / LSB_POR_G,
                sz / (float)ok / LSB_POR_G, (unsigned long)errores);
  Serial.println("Un eje debe marcar cerca de +/-1.000 g y los otros cerca de 0.");
}

void capturar(uint8_t nivel) {
  if (!verificarMPU()) return;
  motor(nivel);
  Serial.println("# Estabilizando motor 3 s...");
  delay(3000);

  const uint32_t N = FS_HZ * DURACION_S;
  Serial.println("#BEGIN");
  info();
  Serial.printf("# nivel_perturbacion=%u\n", nivel);
  Serial.printf("# duty_pwm=%u\n", PWM_NIVEL[nivel]);
  Serial.println("t_us,ax_raw,ay_raw,az_raw");

  uint32_t errores = 0, atrasos = 0;
  int16_t ax = 0, ay = 0, az = 0;
  uint32_t tInicio = micros();
  uint32_t tProx = tInicio;

  for (uint32_t n = 0; n < N; n++) {
    while ((int32_t)(micros() - tProx) < 0) { }
    uint32_t tMuestra = micros();
    if ((int32_t)(tMuestra - tProx) > (int32_t)(TS_US / 2)) atrasos++;
    if (!leerAcel(ax, ay, az)) errores++;
    Serial.printf("%lu,%d,%d,%d\n", (unsigned long)(tMuestra - tInicio), ax, ay, az);
    tProx += TS_US;
  }

  Serial.printf("# muestras=%lu errores_i2c=%lu atrasos=%lu\n",
                (unsigned long)N, (unsigned long)errores, (unsigned long)atrasos);
  Serial.println("#END");
  motor(0);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pwmInit();
  motor(0);

  delay(300);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);
  Wire.setTimeOut(20);

  Serial.println();
  Serial.println("# PDS Proyecto 2 - Heltec V3 + MPU6500/6050 + driver motor");
  esp_reset_reason_t r = esp_reset_reason();
  const char *motivo = r == ESP_RST_POWERON ? "encendido normal" :
                       r == ESP_RST_BROWNOUT ? "CAIDA DE VOLTAJE (brownout) - revise alimentacion/ruido del motor" :
                       r == ESP_RST_SW ? "reinicio por software" :
                       r == ESP_RST_PANIC ? "error del programa" :
                       r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ? "watchdog" :
                       r == ESP_RST_EXT ? "boton RST" : "otro";
  Serial.printf("# Motivo del ultimo reinicio: %s\n", motivo);
  mpuOk = iniciarMPU();
  if (mpuOk) {
    Serial.printf("# Sensor OK en 0x%02X (who_am_i=0x%02X)\n", mpuAddr, (uint8_t)leerReg(0x75));
  } else {
    Serial.println("# ERROR: sensor no detectado. Revise SDA->GPIO5, SCL->GPIO6, VCC->3V3, GND y soldadura.");
    escanearI2C();
  }
  Serial.println("# Listo. Comandos: S0 S1 S2 (capturar), M0 M1 M2 (motor), D (diagnostico), I (info), X (escaneo I2C)");
}

void loop() {
  if (!Serial.available()) return;
  String c = Serial.readStringUntil('\n');
  c.trim();
  c.toUpperCase();
  if (c.length() == 0) return;

  if      (c == "S0" || c == "S1" || c == "S2") capturar(c[1] - '0');
  else if (c == "M0" || c == "M1" || c == "M2") { motor(c[1] - '0'); Serial.printf("# motor nivel %c (duty %u)\n", c[1], PWM_NIVEL[c[1] - '0']); }
  else if (c == "D") diagnostico();
  else if (c == "I") info();
  else if (c == "X") escanearI2C();
  else Serial.println("# Comando no reconocido. Use S0 S1 S2 M0 M1 M2 D I X");
}
