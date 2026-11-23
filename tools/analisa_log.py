#!/usr/bin/env python3
"""
analisa_log.py — valida automaticamente os logs seriais das placas.

Cada placa imprime linhas no formato:
    @<ms> <SINAL> <0|1>          (ex.: @12345 VEH_VERDE 1)
    @<ms> EVT <NOME> <valor>     (ex.: @12400 EVT PEDIDO 1)

Uso (no terminal do VS Code, na raiz do repositório):
    python tools/analisa_log.py --veiculos docs/evidencias/log_veiculos.txt
    python tools/analisa_log.py --pedestres docs/evidencias/log_pedestres.txt
    python tools/analisa_log.py --veiculos LOG_V --pedestres LOG_P --grafico docs/evidencias/linha_do_tempo.png

Saída: tabela PASS/FAIL por verificação e código de retorno 0 (tudo ok) ou 1.
"""
import argparse
import re
import sys

TOL = 80            # ms — tolerância de tempo no hardware
T_VERDE = 3000
T_VERDE_MIN = 1000
T_AMARELO = 1000
T_VERMELHO = 4000
T_GUARDA = 100
T_PISCA = 1000
T_PED_VERDE = T_VERMELHO - T_GUARDA
LAT_MAX = 50        # ms — latência máxima de leitura dos fios na placa de pedestres

LINHA = re.compile(r"^@(\d+)\s+(EVT\s+)?(\S+)\s+(-?\d+)")


def ler_log(caminho):
    eventos = []
    with open(caminho, encoding="utf-8", errors="ignore") as f:
        for linha in f:
            m = LINHA.match(linha.strip())
            if m:
                t, evt, nome, v = m.groups()
                eventos.append((int(t), ("EVT:" if evt else "") + nome, int(v)))
    return eventos


def pulsos(eventos, sinal):
    """Lista de (inicio, fim) dos pulsos em 1 completos de um sinal."""
    out, ini = [], None
    for t, s, v in eventos:
        if s != sinal:
            continue
        if v == 1 and ini is None:
            ini = t
        elif v == 0 and ini is not None:
            out.append((ini, t))
            ini = None
    return out


def intervalos_de(eventos, sinal):
    """Intervalos em que um sinal/EVT estava em 1 (fecha no fim do log)."""
    p = pulsos(eventos, sinal)
    aberto = [t for t, s, v in eventos if s == sinal]
    if aberto:
        ult = [(t, v) for t, s, v in eventos if s == sinal][-1]
        if ult[1] == 1:
            p.append((ult[0], eventos[-1][0]))
    return p


def dentro(t, intervalos, folga=0):
    return any(a - folga <= t <= b + folga for a, b in intervalos)


def sobrepoe(a, b, intervalos):
    return any(a < y and x < b for x, y in intervalos)


class Relatorio:
    def __init__(self):
        self.linhas = []
        self.ok = True

    def item(self, id_, desc, passou, detalhe=""):
        self.ok &= passou
        self.linhas.append((id_, "PASS" if passou else "FAIL", desc, detalhe))

    def imprimir(self):
        print(f"{'ID':<8} {'RES':<5} DESCRIÇÃO")
        print("-" * 78)
        for id_, r, d, det in self.linhas:
            print(f"{id_:<8} {r:<5} {d}")
            if det:
                print(f"{'':<14} {det}")
        print("-" * 78)
        print("RESULTADO GERAL:", "PASS" if self.ok else "FAIL")


def exclusao(ev, sinais):
    estado = {s: 0 for s in sinais}
    pior = 0
    for _, s, v in ev:
        if s in estado:
            estado[s] = v
            pior = max(pior, sum(estado.values()))
    return pior


def analisa_veiculos(ev, rel):
    noite = intervalos_de(ev, "LINK_NOITE")
    pedidos = [t for t, s, v in ev if s == "EVT:PEDIDO" and v == 1]
    verdes = pulsos(ev, "VEH_VERDE")
    amarelos = pulsos(ev, "VEH_AMARELO")
    vermelhos = pulsos(ev, "VEH_VERMELHO")
    go = pulsos(ev, "LINK_PED_GO")

    pior = exclusao(ev, ["VEH_VERDE", "VEH_AMARELO", "VEH_VERMELHO"])
    viol = [t for t, s, v in ev if s == "VIOLACAO"]
    rel.item("LOG-V01", "Exclusão mútua: no máx. 1 LED de carro aceso",
             pior <= 1 and not viol, f"máx. simultâneos = {pior}")

    # verdes normais (sem pedido e fora do noturno)
    normais = [(a, b) for a, b in verdes
               if not sobrepoe(a, b, noite) and not any(a <= p <= b for p in pedidos)]
    ruins = [b - a for a, b in normais if abs((b - a) - T_VERDE) > TOL]
    rel.item("LOG-V02", f"Verde = {T_VERDE} ms (+/-{TOL}) em {len(normais)} ciclos",
             bool(normais) and not ruins, f"fora: {ruins}" if ruins else "")

    am_norm = [(a, b) for a, b in amarelos if not sobrepoe(a - 50, b + 50, noite)]
    ruins = [b - a for a, b in am_norm if abs((b - a) - T_AMARELO) > TOL]
    rel.item("LOG-V03", f"Amarelo = {T_AMARELO} ms em {len(am_norm)} ciclos",
             bool(am_norm) and not ruins, f"fora: {ruins}" if ruins else "")

    vm_norm = [(a, b) for a, b in vermelhos if not sobrepoe(a, b, noite)]
    # o 1º vermelho após sair do noturno/boot pode ser mais curto? não: é 4 s.
    ruins = [b - a for a, b in vm_norm if abs((b - a) - T_VERMELHO) > TOL]
    rel.item("LOG-V04", f"Vermelho = {T_VERMELHO} ms em {len(vm_norm)} ciclos",
             bool(vm_norm) and not ruins, f"fora: {ruins}" if ruins else "")

    # ordem: depois de todo verde vem amarelo
    # (intervalos_de inclui o último pulso, ainda aberto no fim do log)
    am_todos = intervalos_de(ev, "VEH_AMARELO")
    vm_todos = intervalos_de(ev, "VEH_VERMELHO")
    ordem_ok = all(any(abs(a2 - b) <= 30 for a2, _ in am_todos) for _, b in verdes)
    ordem_ok &= all(any(abs(a2 - b) <= 30 for a2, _ in vm_todos)
                    for a, b in am_norm)
    rel.item("LOG-V05", "Ordem verde -> amarelo -> vermelho (nunca verde -> vermelho)",
             ordem_ok)

    # PED_GO contido no vermelho, com guarda
    go_ok, guarda_min = True, None
    for a, b in go:
        cont = [(x, y) for x, y in vermelhos if x <= a and b <= y]
        if not cont:
            go_ok = False
            continue
        g = cont[0][1] - b
        guarda_min = g if guarda_min is None else min(guarda_min, g)
    go_ok &= guarda_min is None or guarda_min >= T_GUARDA - 20
    rel.item("LOG-V06", "PED_GO=1 só durante o vermelho, com guarda >= 80 ms",
             bool(go) and go_ok, f"guarda mínima = {guarda_min} ms")

    # pedido de travessia encurta o verde
    if pedidos:
        enc = []
        for p in pedidos:
            v = [(a, b) for a, b in verdes if a <= p <= b]
            if v:
                a, b = v[0]
                esperado = max(a + T_VERDE_MIN, p)
                enc.append(b - esperado)
        ok = enc and all(-TOL <= d <= TOL for d in enc)
        rel.item("LOG-V07", "Botão de travessia encerra o verde (mín. 1 s) com segurança",
                 bool(ok), f"atraso vs. esperado (ms): {enc}")
    else:
        rel.item("LOG-V07", "Botão de travessia (nenhum pedido no log)", False,
                 "aperte o botão de travessia durante a captura")

    # noturno
    if noite:
        # ignora o último pulso, interrompido pela saída do noturno (por projeto)
        am_noite = [(a, b) for a, b in amarelos
                    if any(x <= a and b <= y - 60 for x, y in noite)]
        dur = [b - a for a, b in am_noite]
        gaps = [am_noite[i + 1][0] - am_noite[i][1] for i in range(len(am_noite) - 1)]
        ok = len(am_noite) >= 2 and all(abs(d - T_PISCA) <= TOL for d in dur) \
            and all(abs(g - T_PISCA) <= TOL for g in gaps)
        outros = [p for p in verdes + go if any(x + 300 < p[0] < y for x, y in noite)]
        rel.item("LOG-V08", "Noturno: amarelo pisca 1 s/1 s, sem verde e sem PED_GO",
                 ok and not outros, f"aceso={dur[:6]} apagado={gaps[:6]}")
    else:
        rel.item("LOG-V08", "Noturno (não testado no log)", False,
                 "aperte o botão do modo noturno durante a captura")


def analisa_pedestres(ev, rel):
    lido = intervalos_de(ev, "EVT:PED_GO_LIDO")
    noite = intervalos_de(ev, "EVT:NOITE_LIDO")
    verdes = pulsos(ev, "PED_VERDE")
    vermelhos = pulsos(ev, "PED_VERMELHO")

    pior = exclusao(ev, ["PED_VERDE", "PED_VERMELHO"])
    rel.item("LOG-P01", "Exclusão mútua: verde e vermelho do pedestre nunca juntos",
             pior <= 1, f"máx. simultâneos = {pior}")

    fora = [(a, b) for a, b in verdes if not any(x <= a and b <= y + LAT_MAX for x, y in lido)]
    rel.item("LOG-P02", "Pedestre verde só com PED_GO=1 (sincronismo)",
             bool(verdes) and not fora, f"{len(verdes)} verdes, fora: {fora}")

    lat = []
    for x, y in lido:
        v = [(a, b) for a, b in verdes if x <= a <= y]
        if v:
            lat.append(v[0][1] - y if v[0][1] >= y else 0)
    rel.item("LOG-P03", f"Verde apaga até {LAT_MAX} ms após PED_GO cair",
             bool(lat) and max(lat) <= LAT_MAX, f"latências: {lat[:8]}")

    # exclui verdes interrompidos pela entrada do noturno (por projeto)
    normais = [(a, b) for a, b in verdes
               if not sobrepoe(a, b + 200, noite)]
    ruins = [b - a for a, b in normais if abs((b - a) - T_PED_VERDE) > TOL + 40]
    rel.item("LOG-P04", f"Pedestre verde ~{T_PED_VERDE} ms (~4 s)",
             bool(normais) and not ruins, f"fora: {ruins}" if ruins else "")

    botoes = [t for t, s, v in ev if s == "EVT:BOTAO"]
    req = pulsos(ev, "LINK_PED_REQ")
    if botoes:
        ok = all(any(abs(a - t) <= 20 for a, _ in req) for t in botoes)
        rel.item("LOG-P05", "Botão gera PED_REQ e o pedido é zerado ao abrir",
                 ok and len(req) >= len(botoes), f"{len(botoes)} botões, {len(req)} pedidos")
    else:
        rel.item("LOG-P05", "Botão de travessia (não testado no log)", False,
                 "aperte o botão durante a captura")

    if noite:
        vn = [(a, b) for a, b in vermelhos if any(x <= a and b <= y - 60 for x, y in noite)]
        dur = [b - a for a, b in vn]
        ok = len(vn) >= 2 and all(abs(d - T_PISCA) <= TOL for d in dur)
        sem_verde = not [p for p in verdes if any(x < p[0] < y for x, y in noite)]
        rel.item("LOG-P06", "Noturno: vermelho do pedestre pisca 1 s/1 s, sem verde",
                 ok and sem_verde, f"aceso={dur[:6]}")
    else:
        rel.item("LOG-P06", "Noturno (não testado no log)", False, "")


def estimar_offset(ev_v, ev_p):
    """Deslocamento entre os relógios (ms): o candidato (p - v) que casa o
    maior número de bordas de PED_GO das duas placas (tolerância 60 ms)."""
    go_v = [a for a, _ in pulsos(ev_v, "LINK_PED_GO")]
    go_p = [a for a, _ in intervalos_de(ev_p, "EVT:PED_GO_LIDO")]
    melhor, base = -1, 0
    for v0 in go_v:
        for p0 in go_p:
            off = p0 - v0
            casadas = sum(1 for p in go_p if any(abs((p - off) - v) <= 60 for v in go_v))
            if casadas > melhor:
                melhor, base = casadas, off
    return base, melhor


def analisa_cruzado(ev_v, ev_p, rel):
    """Alinha os relógios das duas placas pelas bordas de PED_GO e verifica
    que nenhum verde de pedestre coincide com verde/amarelo dos carros."""
    go_v = [a for a, _ in pulsos(ev_v, "LINK_PED_GO")]
    go_p = [a for a, _ in intervalos_de(ev_p, "EVT:PED_GO_LIDO")]
    if not go_v or not go_p:
        rel.item("LOG-X01", "Sincronismo entre placas (sem dados)", False, "")
        return
    base, melhor = estimar_offset(ev_v, ev_p)
    carro = pulsos(ev_v, "VEH_VERDE") + pulsos(ev_v, "VEH_AMARELO")
    perigo, verificados = [], 0
    for a, b in pulsos(ev_p, "PED_VERDE"):
        lido = max([p for p in go_p if p <= a], default=None)
        if lido is None:
            continue
        v = min(go_v, key=lambda x: abs((lido - x) - base))
        off = lido - v                         # relógio + latência de leitura
        if abs(off - base) > 60:
            continue                           # verde fora do trecho comum dos logs
        verificados += 1
        a_v, b_v = a - off, b - off + LAT_MAX  # pessimista no fim
        if sobrepoe(a_v, b_v, carro):
            perigo.append((a, b))
    rel.item("LOG-X01", "Placas reais: pedestre verde nunca junto com carro verde/amarelo",
             verificados > 0 and not perigo,
             f"{verificados} verdes verificados, {len(perigo)} conflitos, "
             f"bordas casadas={melhor}")


def grafico(ev_v, ev_p, saida):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib não instalado: pip install matplotlib")
        return
    linhas = []
    titulo = "Linha do tempo dos semáforos"
    if ev_v and ev_p:
        off, _ = estimar_offset(ev_v, ev_p)
        ev_p = [(t - off, s, v) for t, s, v in ev_p]
        titulo += " (relógio da placa de pedestres alinhado ao de veículos)"
    if ev_v:
        linhas += [("VEH_VERDE", ev_v, "tab:green"), ("VEH_AMARELO", ev_v, "gold"),
                   ("VEH_VERMELHO", ev_v, "tab:red"), ("LINK_PED_GO", ev_v, "tab:blue"),
                   ("LINK_NOITE", ev_v, "gray")]
    if ev_p:
        linhas += [("PED_VERDE", ev_p, "tab:green"), ("PED_VERMELHO", ev_p, "tab:red"),
                   ("LINK_PED_REQ", ev_p, "tab:purple")]
    fig, ax = plt.subplots(figsize=(14, 0.5 * len(linhas) + 1.2))
    for i, (s, ev, cor) in enumerate(linhas):
        for a, b in intervalos_de(ev, s):
            ax.broken_barh([(a / 1000, (b - a) / 1000)], (i - 0.4, 0.8), color=cor)
    ax.set_yticks(range(len(linhas)))
    ax.set_yticklabels([s for s, _, _ in linhas])
    ax.set_xlabel("tempo (s)")
    ax.set_title(titulo)
    ax.grid(axis="x", alpha=0.3)
    fig.tight_layout()
    fig.savefig(saida, dpi=130)
    print("gráfico salvo em", saida)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--veiculos", help="log serial da placa de veículos")
    ap.add_argument("--pedestres", help="log serial da placa de pedestres")
    ap.add_argument("--grafico", help="salva PNG com a linha do tempo")
    a = ap.parse_args()
    if not a.veiculos and not a.pedestres:
        ap.error("informe --veiculos e/ou --pedestres")

    rel = Relatorio()
    ev_v = ler_log(a.veiculos) if a.veiculos else []
    ev_p = ler_log(a.pedestres) if a.pedestres else []
    if a.veiculos:
        analisa_veiculos(ev_v, rel)
    if a.pedestres:
        analisa_pedestres(ev_p, rel)
    if a.veiculos and a.pedestres:
        analisa_cruzado(ev_v, ev_p, rel)
    rel.imprimir()
    if a.grafico:
        grafico(ev_v, ev_p, a.grafico)
    sys.exit(0 if rel.ok else 1)


if __name__ == "__main__":
    main()
