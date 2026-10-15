import argparse
import datetime as dt
import pathlib
import statistics
import sys
import time

import serial

COMANDO = {"R1": "S0", "R2": "S1", "R3": "S2"}
DESCRIPCION = {"R1": "motor/servo apagado", "R2": "perturbacion baja", "R3": "perturbacion alta"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--puerto", required=True)
    ap.add_argument("--registro", required=True, choices=["R1", "R2", "R3"])
    ap.add_argument("--equipo", default="EquipoXX")
    ap.add_argument("--salida", default=str(pathlib.Path(__file__).resolve().parent.parent / "datos"))
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    salida = pathlib.Path(args.salida)
    salida.mkdir(parents=True, exist_ok=True)
    archivo = salida / f"{args.registro}.csv"
    if archivo.exists():
        resp = input(f"{archivo.name} ya existe. ¿Sobrescribir? [s/N] ")
        if resp.strip().lower() != "s":
            sys.exit("Cancelado.")

    with serial.Serial(args.puerto, args.baud, timeout=30) as ser:
        time.sleep(2.5)
        ser.reset_input_buffer()
        print(f"Capturando {args.registro} ({DESCRIPCION[args.registro]}). NO toque la mesa ni la IMU...")
        ser.write((COMANDO[args.registro] + "\n").encode())

        lineas, dentro = [], False
        while True:
            raw = ser.readline()
            if not raw:
                sys.exit("Tiempo de espera agotado: no llegó #END. Revise el puerto y el firmware.")
            linea = raw.decode(errors="replace").strip()
            if linea == "#BEGIN":
                dentro = True
                continue
            if linea == "#END":
                break
            if dentro:
                lineas.append(linea)
            elif linea:
                print("  ESP32:", linea)

    meta = [l for l in lineas if l.startswith("#")]
    datos = [l for l in lineas if l and not l.startswith("#")]
    cabecera, filas = datos[0], datos[1:]
    if cabecera != "t_us,ax_raw,ay_raw,az_raw":
        sys.exit(f"Cabecera inesperada: {cabecera}")

    t_us = [int(f.split(",")[0]) for f in filas]
    dts = [(b - a) for a, b in zip(t_us, t_us[1:])]
    fs_real = 1e6 / statistics.mean(dts)

    with open(archivo, "w", newline="") as f:
        f.write(f"# registro={args.registro} ({DESCRIPCION[args.registro]})\n")
        f.write(f"# equipo={args.equipo}\n")
        f.write(f"# fecha_captura={dt.datetime.now().isoformat(timespec='seconds')}\n")
        for m in meta:
            f.write(m + "\n")
        f.write(f"# fs_medida_hz={fs_real:.4f}\n")
        f.write("t_us,t_s,ax_raw,ay_raw,az_raw\n")
        for fila, t in zip(filas, t_us):
            _, ax, ay, az = fila.split(",")
            f.write(f"{t},{t / 1e6:.6f},{ax},{ay},{az}\n")

    print(f"\nGuardado: {archivo}")
    print("Verificación:")
    print(f"  muestras        = {len(filas)}  (esperadas 2000 a 100 Hz x 20 s)")
    print(f"  fs medida       = {fs_real:.3f} Hz")
    print(f"  dt min/max [us] = {min(dts)} / {max(dts)}  (ideal 10000)")
    print(f"  duración        = {(t_us[-1] - t_us[0]) / 1e6:.3f} s")
    for m in meta:
        if "errores_i2c" in m:
            print("  " + m.lstrip("# "))
    ok = len(filas) >= 1990 and abs(fs_real - 100) < 1 and max(dts) < 20000
    print("  RESULTADO:", "OK" if ok else "REVISAR (repita la captura)")


if __name__ == "__main__":
    main()
