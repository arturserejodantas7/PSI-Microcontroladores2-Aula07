/*
 * TESTES UNITÁRIOS — semáforo de PEDESTRES isolado (threads reais + HAL fake)
 *
 * O teste faz o papel da placa de veículos: controla os fios PED_GO e NOITE
 * com hal_fake_set() e observa os LEDs e o fio PED_REQ.
 */
#include <zephyr/ztest.h>

#include "teste_util.h"
#include "pedestres.h"

static void *setup(void)
{
	zassert_equal(pedestres_iniciar(), 0);
	k_msleep(50);
	return NULL;
}

ZTEST_SUITE(pedestres, NULL, setup, NULL, NULL, NULL);

/* TU-P01: partida — pedestre VERMELHO, sem pedido */
ZTEST(pedestres, test_TU_P01_partida_vermelho)
{
	zassert_true(hal_fake_valor(SIG_PED_VERMELHO));
	zassert_false(hal_fake_valor(SIG_PED_VERDE));
	zassert_false(hal_fake_valor(SIG_LINK_PED_REQ));
}

/* TU-P02: sem PED_GO (fio solto = 0) o pedestre NUNCA abre */
ZTEST(pedestres, test_TU_P02_sem_ped_go_fica_vermelho)
{
	hal_fake_limpar_eventos();
	k_msleep(3000);

	hal_pulso_t p[MAX_PULSOS];

	zassert_equal(pulsos(SIG_PED_VERDE, p), 0);
	zassert_true(hal_fake_valor(SIG_PED_VERMELHO));
}

/* TU-P03: verde segue PED_GO, com latência pequena nas duas bordas */
ZTEST(pedestres, test_TU_P03_segue_ped_go)
{
	int64_t t0 = k_uptime_get();

	hal_fake_set(SIG_LINK_PED_GO, true);
	int64_t t1 = esperar_nivel(SIG_PED_VERDE, true, 500);

	zassert_true(t1 - t0 <= LAT_LINK_MS, "verde demorou %d ms", (int)(t1 - t0));
	zassert_false(hal_fake_valor(SIG_PED_VERMELHO), "vermelho e verde juntos");

	k_msleep(T_VEH_VERMELHO_MS - T_GUARDA_MS - (t1 - t0));

	int64_t t2 = k_uptime_get();

	hal_fake_set(SIG_LINK_PED_GO, false);
	int64_t t3 = esperar_nivel(SIG_PED_VERDE, false, 500);

	zassert_true(t3 - t2 <= LAT_LINK_MS, "verde demorou %d ms p/ apagar", (int)(t3 - t2));
	esperar_nivel(SIG_PED_VERMELHO, true, 50);
}

/* TU-P04: 2 ciclos simulados (PED_GO 3,9 s em 1 / 4,1 s em 0):
 * verde ~3,9 s, vermelho ~4,1 s, nunca os dois juntos */
ZTEST(pedestres, test_TU_P04_ciclos_e_exclusao)
{
	hal_pulso_t g[MAX_PULSOS], r[MAX_PULSOS];

	hal_fake_limpar_eventos();
	for (int i = 0; i < 2; i++) {
		hal_fake_set(SIG_LINK_PED_GO, true);
		k_msleep(T_VEH_VERMELHO_MS - T_GUARDA_MS);
		hal_fake_set(SIG_LINK_PED_GO, false);
		k_msleep(T_VEH_VERDE_MS + T_VEH_AMARELO_MS + T_GUARDA_MS);
	}

	int ng = pulsos(SIG_PED_VERDE, g);
	int nr = pulsos(SIG_PED_VERMELHO, r);

	zassert_equal(ng, 2, "verde abriu %d vezes", ng);
	zassert_true(nr >= 1);
	for (int i = 0; i < ng; i++) {
		CHECA_DUR(g[i].duracao, T_VEH_VERMELHO_MS - T_GUARDA_MS, "pedestre verde");
	}
	CHECA_DUR(r[nr - 1].duracao, T_VEH_VERDE_MS + T_VEH_AMARELO_MS + T_GUARDA_MS,
		  "pedestre vermelho");
	zassert_equal(hal_violacoes(), 0);
}

/* TU-P05: botão com pedestre fechado gera PED_REQ; PED_GO=1 zera o pedido */
ZTEST(pedestres, test_TU_P05_botao_gera_pedido)
{
	int64_t t0 = k_uptime_get();

	hal_fake_set(SIG_BTN_TRAVESSIA, true);
	int64_t t1 = esperar_nivel(SIG_LINK_PED_REQ, true, 500);

	hal_fake_set(SIG_BTN_TRAVESSIA, false);
	zassert_true(t1 - t0 <= LAT_BOTAO_MS, "pedido demorou %d ms", (int)(t1 - t0));

	/* pedido fica "travado" (latch) mesmo depois de soltar o botão */
	k_msleep(500);
	zassert_true(hal_fake_valor(SIG_LINK_PED_REQ), "pedido sumiu sozinho");

	/* placa de veículos atende */
	hal_fake_set(SIG_LINK_PED_GO, true);
	esperar_nivel(SIG_LINK_PED_REQ, false, 200);
	esperar_nivel(SIG_PED_VERDE, true, 200);
	k_msleep(1000);
	hal_fake_set(SIG_LINK_PED_GO, false);
	esperar_nivel(SIG_PED_VERMELHO, true, 200);
}

/* TU-P06: botão com pedestre já verde é ignorado */
ZTEST(pedestres, test_TU_P06_botao_no_verde_ignorado)
{
	hal_fake_set(SIG_LINK_PED_GO, true);
	esperar_nivel(SIG_PED_VERDE, true, 200);
	hal_fake_apertar(SIG_BTN_TRAVESSIA, 100);
	k_msleep(100);
	zassert_false(hal_fake_valor(SIG_LINK_PED_REQ));
	hal_fake_set(SIG_LINK_PED_GO, false);
	esperar_nivel(SIG_PED_VERMELHO, true, 200);
}

/* TU-P07: FAIL-SAFE — PED_GO preso em 1 -> verde apaga em 4,3 s e só
 * reabre depois que PED_GO voltar a 0 */
ZTEST(pedestres, test_TU_P07_failsafe_ped_go_preso)
{
	hal_fake_set(SIG_LINK_PED_GO, true);
	int64_t t0 = esperar_nivel(SIG_PED_VERDE, true, 200);
	int64_t t1 = esperar_nivel(SIG_PED_VERDE, false, 6000);

	CHECA_DUR(t1 - t0, T_PED_VERDE_MAX_MS, "verde (fail-safe)");
	zassert_true(pedestres_bloqueado());

	/* continua preso em 1: NÃO pode reabrir */
	k_msleep(2000);
	zassert_false(hal_fake_valor(SIG_PED_VERDE), "reabriu com fio preso");
	zassert_true(hal_fake_valor(SIG_PED_VERMELHO));

	/* fio volta ao normal: rearma */
	hal_fake_set(SIG_LINK_PED_GO, false);
	k_msleep(100);
	zassert_false(pedestres_bloqueado());
	hal_fake_set(SIG_LINK_PED_GO, true);
	esperar_nivel(SIG_PED_VERDE, true, 200);
	hal_fake_set(SIG_LINK_PED_GO, false);
	esperar_nivel(SIG_PED_VERMELHO, true, 200);
}

/* TU-P08: modo noturno — vermelho pisca 1 s/1 s; verde nunca acende */
ZTEST(pedestres, test_TU_P08_noturno)
{
	hal_pulso_t r[MAX_PULSOS], g[MAX_PULSOS];

	hal_fake_set(SIG_LINK_NOITE, true);
	k_msleep(300);
	hal_fake_limpar_eventos();

	/* mesmo que PED_GO suba por erro, o verde não pode acender */
	hal_fake_set(SIG_LINK_PED_GO, true);
	hal_fake_apertar(SIG_BTN_TRAVESSIA, 100);
	k_msleep(6000);
	hal_fake_set(SIG_LINK_PED_GO, false);

	int nr = pulsos(SIG_PED_VERMELHO, r);

	zassert_true(nr >= 2, "vermelho piscou %d vezes", nr);
	for (int i = 0; i < nr; i++) {
		CHECA_DUR(r[i].duracao, T_PISCA_MS, "vermelho aceso (noturno)");
		if (i > 0) {
			uint32_t apagado = r[i].inicio - (r[i - 1].inicio + r[i - 1].duracao);

			CHECA_DUR(apagado, T_PISCA_MS, "vermelho apagado (noturno)");
		}
	}
	zassert_equal(pulsos(SIG_PED_VERDE, g), 0, "verde acendeu no noturno");
	zassert_false(hal_fake_valor(SIG_PED_VERDE));
	zassert_false(hal_fake_valor(SIG_LINK_PED_REQ), "botão gerou pedido no noturno");
}

/* TU-P09: sai do noturno -> vermelho fixo */
ZTEST(pedestres, test_TU_P09_sai_do_noturno)
{
	hal_fake_set(SIG_LINK_NOITE, false);
	k_msleep(1200);
	zassert_true(hal_fake_valor(SIG_PED_VERMELHO));
	k_msleep(1500);
	zassert_true(hal_fake_valor(SIG_PED_VERMELHO), "continua piscando");
	zassert_false(hal_fake_valor(SIG_PED_VERDE));
}

/* TU-P10: nenhuma violação de exclusão mútua em toda a execução */
ZTEST(pedestres, test_TU_P10_sem_violacoes)
{
	zassert_equal(hal_violacoes(), 0, "violações = %u", hal_violacoes());
	zassert_false(hal_fake_estourou());
}
