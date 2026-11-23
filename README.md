# PSI-Microcontroladores2-Aula07 — Semáforos de Pedestres e Veículos

Sistema de semáforos em **Zephyr RTOS** com duas **FRDM-KL25Z**:
uma placa controla o semáforo de **pedestres** (2 LEDs), a outra o de **veículos** (3 LEDs).
Cada LED tem **sua própria thread**; a exclusão mútua é feita com **mutex + variável de condição**.
Modos: **dia**, **noturno** (piscante) e **travessia** (botão).
Validação no **modelo V**: testes unitários, de integração e de sistema → relatório em [`docs/RELATORIO.md`](docs/RELATORIO.md).

## Estrutura

```
lib/                      código comum (usado pelas apps e pelos testes)
  include/ src/
    semaforo_logica.*     lógica pura (estados, tempos, debounce)  -> testável
    semaforo_hal.h        interface de hardware
    hal_gpio.c            HAL real (GPIO pelo devicetree)
    hal_fake.c            HAL simulada para testes (registra eventos, conta violações)
    hal_comum.c           log serial "@ms SINAL valor" + verificação de invariantes
    veiculos.c            3 threads de LED + thread de entradas
    pedestres.c           2 threads de LED + thread de entradas
app_veiculos/             aplicação da placa de VEÍCULOS  (overlay com os pinos)
app_pedestres/            aplicação da placa de PEDESTRES (overlay com os pinos)
tests/
  unit_logica/            TU-L01..11  (ztest)
  unit_veiculos/          TU-V01..11  (ztest, threads reais + HAL fake)
  unit_pedestres/         TU-P01..10  (ztest, threads reais + HAL fake)
  integracao/             TI-01..07   (ztest, as duas máquinas juntas)
tools/
  captura_serial.py       grava a serial de uma placa em arquivo
  analisa_log.py          valida automaticamente os logs (PASS/FAIL) e gera gráfico
docs/
  RELATORIO.md            relatório + plano de testes (exportar para PDF)
  evidencias/             logs, fotos, prints, gráfico
```

## Montagem

| Sinal | Placa VEÍCULOS | Placa PEDESTRES |
|---|---|---|
| LED verde | PTD4 (D2) | PTD4 (D2) |
| LED amarelo | PTA12 (D3) | — |
| LED vermelho | PTA5 (D5) | PTA12 (D3) |
| Botão | PTC8 (D6) noturno | PTC8 (D6) travessia |
| PED_GO | PTC9 (D7) → | → PTC9 (D7) |
| NOITE | PTA13 (D8) → | → PTA13 (D8) |
| PED_REQ | PTD5 (D9) ← | ← PTD5 (D9) |
| GND | GND | GND |

- LED: `pino → 330 Ω → LED (anodo) → catodo → GND`.
- Botão: `pino → botão → GND` (pull-up interno já configurado).
- Entre as placas: **D7–D7, D8–D8, D9–D9 e GND–GND**, de preferência com **1 kΩ em série** em cada fio.
- Confira a posição dos pinos D2…D9 na serigrafia da sua placa (headers J1/J2).

## Compilar e gravar

No terminal do VS Code, com o ambiente do Zephyr ativo (o mesmo das aulas anteriores):

```bash
# placa de VEÍCULOS
west build -p always -b frdm_kl25z app_veiculos  -d build/veiculos
west flash -d build/veiculos

# placa de PEDESTRES (na outra placa)
west build -p always -b frdm_kl25z app_pedestres -d build/pedestres
west flash -d build/pedestres
```

Se o `west flash` não funcionar, copie `build/<nome>/zephyr/zephyr.bin` para o drive USB da placa (bootloader OpenSDA).
Serial: 115200 baud. Cada mudança aparece como `@<ms> <SINAL> <0|1>`.

## Rodar os testes (na própria placa)

```bash
west build -p always -b frdm_kl25z tests/unit_logica    -d build/t_logica
west flash -d build/t_logica
# abra o monitor serial e espere "PROJECT EXECUTION SUCCESSFUL"
```
Repita com `tests/unit_veiculos`, `tests/unit_pedestres` e `tests/integracao`
(cada um leva de 1 a 2 minutos, pois os tempos são reais). Salve a saída em `docs/evidencias/ztest_<suite>.txt`.

Em Linux/WSL também dá para rodar no PC: `west twister -T tests -p native_sim`.

## Teste de sistema com análise automática

```bash
pip install pyserial matplotlib
python tools/captura_serial.py COM5 docs/evidencias/log_veiculos.txt  --segundos 180
python tools/captura_serial.py COM6 docs/evidencias/log_pedestres.txt --segundos 180
python tools/analisa_log.py --veiculos docs/evidencias/log_veiculos.txt \
                            --pedestres docs/evidencias/log_pedestres.txt \
                            --grafico docs/evidencias/linha_do_tempo.png
```
Rode as duas capturas **ao mesmo tempo** (dois terminais). Durante os 3 min: aperte o botão de
travessia algumas vezes (com carro verde, amarelo e vermelho) e ligue/desligue o modo noturno uma vez.
