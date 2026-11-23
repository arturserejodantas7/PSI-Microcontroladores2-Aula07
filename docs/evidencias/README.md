# Evidências

Coloque aqui os arquivos gerados na validação (todos referenciados no relatório):

| Arquivo | Teste | Como gerar |
|---|---|---|
| `ztest_unit_logica.txt` | TU-L01..11 | saída serial do `tests/unit_logica` na placa |
| `ztest_unit_veiculos.txt` | TU-V01..11 | saída serial do `tests/unit_veiculos` |
| `ztest_unit_pedestres.txt` | TU-P01..10 | saída serial do `tests/unit_pedestres` |
| `ztest_integracao.txt` | TI-01..07 | saída serial do `tests/integracao` |
| `log_veiculos.txt` | TS-01..04, TI-HW-03 | `tools/captura_serial.py` na placa de veículos |
| `log_pedestres.txt` | TS-01..04, TI-HW-03 | `tools/captura_serial.py` na placa de pedestres |
| `analise.txt` | TS / TI-HW | `python tools/analisa_log.py ... > docs/evidencias/analise.txt` |
| `linha_do_tempo.png` | TS-01..04 | `analisa_log.py --grafico` |
| `log_longa_duracao_*.txt` | TS-08 | captura de 10 min |
| `montagem.jpg` | — | foto das duas placas ligadas |
| vídeo (link no relatório) | TS-02..07 | celular; mostre as duas placas |
