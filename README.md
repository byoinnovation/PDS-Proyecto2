# Proyecto 2 — Mejora de la SNR de una IMU mediante filtros FIR e IIR

Procesamiento Digital de Señales (Teoría) · Ingeniería Mecatrónica · UMNG · 2026-2
Juan Andrés Bello Ordoñez — 7004103

## Descripción

Se adquieren tres registros de 20 s de un acelerómetro en reposo, bajo tres niveles de
vibración, y se compara cuánto mejora la relación señal a ruido al filtrar con un FIR de
orden 20 diseñado por ventana y con un IIR Butterworth de orden 2, ambos con frecuencia
de corte de 5 Hz.

Con la IMU quieta la media representa la gravedad proyectada sobre el eje y la variación
alrededor de la media representa el ruido, de modo que

    P_s = media(x)^2        P_n = var(x)        SNR = 10*log10(P_s/P_n)  [dB]

## Montaje

| Elemento | Detalle |
| --- | --- |
| IMU | GY-521 con MPU6500 (WHO_AM_I = 0x70), rango ±2 g, 16384 LSB/g |
| Microcontrolador | Heltec WiFi LoRa 32 V3 (ESP32-S3) |
| Conexión | I²C a 100 kHz — SDA en GPIO5, SCL en GPIO6 |
| Perturbación | motorreductor DC sobre chasis impreso en 3D, sin masa añadida |
| Driver | L298N con fuente independiente, PWM de 1 kHz desde GPIO7 |
| Muestreo | 100 Hz por espera activa, 2000 muestras por registro |
| Antialias | filtro interno del acelerómetro, ACCEL_CONFIG2 = 3 (~44,8 Hz) |

## Registros

| Archivo | Condición | PWM (8 bits) |
| --- | --- | --- |
| `datos/R1.csv` | motor apagado | 0 |
| `datos/R2.csv` | perturbación baja | 150 |
| `datos/R3.csv` | perturbación alta | 230 |

Cada archivo conserva la marca de tiempo en microsegundos y las cuentas crudas de los tres
ejes, con una cabecera que registra la frecuencia de muestreo, la escala, el rango y el
nivel de PWM. Los tres se capturaron con 2000 muestras, fs medida de 100,000 Hz y cero
errores de I²C.

## Organización

    firmware/imu_captura/imu_captura.ino   programa de la Heltec: muestreo y control del motor
    captura/captura_imu.py                 recibe los datos por USB y escribe los CSV
    datos/                                 registros crudos R1, R2 y R3
    analisis/analisis_snr.ipynb            análisis completo, ejecutado
    analisis/analisis_snr.py               el mismo análisis como script
    analisis/py_a_notebook.py              genera el notebook a partir del script
    resultados/                            figuras y tablas, todas generadas por el análisis

## Reproducir el análisis

    pip install -r requirements.txt
    python analisis/analisis_snr.py

Lee los CSV de `datos/`, imprime las tablas en consola y regenera en `resultados/` las nueve
figuras y las tablas de resultados. El mismo contenido está en el notebook, ya ejecutado.

## Resultados

| Registro | SNR entrada | ΔSNR FIR | ΔSNR IIR |
| --- | --- | --- | --- |
| R1 | 54,54 dB | +10,19 dB | +9,14 dB |
| R2 | 29,67 dB | +20,56 dB | +19,27 dB |
| R3 | 31,46 dB | +14,56 dB | +13,46 dB |

Ninguno de los dos filtros altera el valor medio más de 0,039 mg. El FIR mejora alrededor
de 1 dB más, a costa de 21 multiplicaciones por muestra y 100 ms de retardo, frente a 5
multiplicaciones y 45 ms del IIR.
