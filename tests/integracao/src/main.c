/*
 * TESTES DE INTEGRAÇÃO (software) — os DOIS semáforos na mesma imagem.
 *
 * veiculos.c e pedestres.c rodam juntos (7 threads). A HAL fake liga os
 * "fios" em software: o que veículos escreve em LINK_PED_GO / LINK_NOITE é
 * lido por pedestres, e o LINK_PED_REQ de pedestres é lido por veículos.
 *
 * A HAL conta CONFLITOS em toda escrita:
 *     pedestre VERDE aceso junto com carro VERDE ou AMARELO.
 * O teste de integração em hardware (com os fios reais) está descrito no
 * plano de testes (TI-HW-xx) e é validado com tools/analisa_log.py.
 */
#include <zephyr/ztest.h>

#include "teste_util.h"
#include "veiculos.h"
#include "pedestres.h"

static void *setup(void)
{
	zassert_equal(pedestres_iniciar(), 0);
	zassert_equal(veiculos_iniciar(), 0);
	k_msleep(50);
	return NULL;
}

ZTEST_SUITE(integracao, NULL, setup, NULL, NULL, NULL);

/* TI-01: sincronismo em 2 ciclos — todo verde de pedestre está contido
 * num vermelho de carro, com folga de segurança no fim */
ZTEST(integracao, test_TI_01_sincronismo)
{
	hal_pulso_t pv[MAX_PULSOS], vr[MAX_PULSOS], pr[MAX_PULSOS];

	hal_fake_limpar_eventos();
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 9000);

	k_msleep(16200);

	int npv = pulsos_desde(SIG_PED_VERDE, pv, t0);
	int nvr = pulsos_desde(SIG_VEH_VERMELHO, vr, t0);
	int npr = pulsos_desde(SIG_PED_VERMELHO, pr, t0);

	zassert_true(npv >= 2 && nvr >= 2, "npv=%d nvr=%d", npv, nvr);

	for (int i = 0; i < 2; i++) {
		uint32_t pv_fim = pv[i].inicio + pv[i].duracao;
		uint32_t vr_fim = vr[i].inicio + vr[i].duracao;

		zassert_true(pv[i].inicio > vr[i].inicio,
			     "pedestre abriu antes do vermelho dos carros");
		zassert_true(vr_fim - pv_fim >= 50,
			     "folga no fim só %d ms", (int)(vr_fim - pv_fim));
		/* ~4 s de verde para o pedestre (3,9 s nominal por causa da guarda) */
		CHECA_DUR(pv[i].duracao, T_VEH_VERMELHO_MS - T_GUARDA_MS, "pedestre verde");
	}
	/* vermelho do pedestre ~4 s (= verde + amarelo dos carros + guarda) */
	zassert_true(npr >= 1);
	CHECA_DUR(pr[0].duracao, T_VEH_VERDE_MS + T_VEH_AMARELO_MS + T_GUARDA_MS,
		  "pedestre vermelho");

	zassert_equal(hal_conflitos(), 0, "CONFLITO entre semáforos");
	zassert_equal(hal_violacoes(), 0);
}

/* TI-02: botão de travessia fim a fim — botão na placa de pedestres
 * encurta o verde dos carros e abre o pedestre de forma segura */
ZTEST(integracao, test_TI_02_travessia_fim_a_fim)
{
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 9000);

	k_msleep(300);
	int64_t tb = k_uptime_get();

	hal_fake_apertar(SIG_BTN_TRAVESSIA, 150);
	zassert_true(hal_fake_valor(SIG_LINK_PED_REQ), "pedido não chegou ao fio");

	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 4000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MIN_MS, "verde dos carros com pedido");

	int64_t ta = esperar_borda(SIG_VEH_AMARELO, false, 2000);

	CHECA_DUR(ta - t1, T_VEH_AMARELO_MS, "amarelo");

	int64_t tp = esperar_nivel(SIG_PED_VERDE, true, 1000);

	zassert_true(hal_fake_valor(SIG_VEH_VERMELHO), "pedestre abriu sem carro vermelho");
	zassert_true(tp - tb <= 2200, "pedestre esperou %d ms", (int)(tp - tb));
	esperar_nivel(SIG_LINK_PED_REQ, false, 200);
	zassert_equal(hal_conflitos(), 0);
}

/* TI-03: botão com pedestre já verde não altera o próximo ciclo dos carros */
ZTEST(integracao, test_TI_03_botao_no_verde_do_pedestre)
{
	esperar_nivel(SIG_PED_VERDE, true, 9000);
	hal_fake_apertar(SIG_BTN_TRAVESSIA, 150);
	zassert_false(hal_fake_valor(SIG_LINK_PED_REQ));

	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 6000);
	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 6000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MS, "verde dos carros");
}

/* TI-04: modo noturno fim a fim — carros amarelo piscante, pedestres
 * vermelho piscante, nenhum verde */
ZTEST(integracao, test_TI_04_noturno_fim_a_fim)
{
	hal_pulso_t a[MAX_PULSOS], r[MAX_PULSOS], x[MAX_PULSOS];

	/* entra durante o verde do pedestre (pior caso: tem gente atravessando) */
	esperar_nivel(SIG_PED_VERDE, true, 9000);
	k_msleep(1000);
	hal_fake_apertar(SIG_BTN_NOITE, 100);

	esperar_nivel(SIG_PED_VERDE, false, 300);
	k_msleep(1500);
	hal_fake_limpar_eventos();
	k_msleep(6200);

	int na = pulsos(SIG_VEH_AMARELO, a);
	int nr = pulsos(SIG_PED_VERMELHO, r);

	zassert_true(na >= 2, "amarelo piscou %d", na);
	zassert_true(nr >= 2, "vermelho pedestre piscou %d", nr);
	for (int i = 0; i < na; i++) {
		CHECA_DUR(a[i].duracao, T_PISCA_MS, "amarelo noturno");
	}
	for (int i = 0; i < nr; i++) {
		CHECA_DUR(r[i].duracao, T_PISCA_MS, "vermelho pedestre noturno");
	}
	zassert_equal(pulsos(SIG_VEH_VERDE, x), 0);
	zassert_equal(pulsos(SIG_PED_VERDE, x), 0);
	zassert_equal(hal_conflitos(), 0);
}

/* TI-05: saída do noturno -> carros vermelho, pedestres verde, ciclo normal */
ZTEST(integracao, test_TI_05_sai_do_noturno)
{
	hal_fake_apertar(SIG_BTN_NOITE, 100);
	esperar_nivel(SIG_VEH_VERMELHO, true, 500);
	int64_t tp = esperar_nivel(SIG_PED_VERDE, true, 500);

	zassert_true(tp > 0);
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 6000);
	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 6000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MS, "verde após noturno");
	zassert_false(hal_fake_valor(SIG_PED_VERDE));
}

/* TI-06: estresse — botão apertado repetidamente por 20 s */
ZTEST(integracao, test_TI_06_estresse_botao)
{
	hal_fake_limpar_eventos();
	int64_t fim = k_uptime_get() + 20000;

	while (k_uptime_get() < fim) {
		hal_fake_apertar(SIG_BTN_TRAVESSIA, 80);
		k_msleep(370);
	}
	zassert_equal(hal_conflitos(), 0, "conflitos = %u", hal_conflitos());
	zassert_equal(hal_violacoes(), 0, "violações = %u", hal_violacoes());
}

/* TI-07: resumo — zero conflitos e zero violações na execução inteira */
ZTEST(integracao, test_TI_07_sem_conflitos)
{
	zassert_equal(hal_conflitos(), 0);
	zassert_equal(hal_violacoes(), 0);
	zassert_false(hal_fake_estourou());
}
