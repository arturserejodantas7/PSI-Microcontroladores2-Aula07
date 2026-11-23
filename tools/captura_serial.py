#!/usr/bin/env python3
"""
captura_serial.py — grava a saída serial de uma placa em arquivo (e mostra na tela).

Requer:  pip install pyserial
Uso:     python tools/captura_serial.py COM5 docs/evidencias/log_veiculos.txt
         python tools/captura_serial.py COM6 docs/evidencias/log_pedestres.txt --segundos 180
(Windows: veja a porta COM em Gerenciador de Dispositivos > Portas;
 Linux/macOS: /dev/ttyACM0, /dev/tty.usbmodemXXXX)

Ctrl+C encerra a captura.
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("Instale o pyserial:  pip install pyserial")

ap = argparse.ArgumentParser()
ap.add_argument("porta")
ap.add_argument("arquivo")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--segundos", type=float, default=0, help="0 = até Ctrl+C")
a = ap.parse_args()

fim = time.time() + a.segundos if a.segundos else None
with serial.Serial(a.porta, a.baud, timeout=0.2) as s, \
        open(a.arquivo, "w", encoding="utf-8") as f:
    print(f"Gravando {a.porta} em {a.arquivo} (Ctrl+C para parar)...")
    try:
        while fim is None or time.time() < fim:
            linha = s.readline().decode("utf-8", errors="ignore")
            if linha:
                sys.stdout.write(linha)
                f.write(linha)
                f.flush()
    except KeyboardInterrupt:
        pass
print("\nCaptura encerrada.")
