/*
 * TESTES UNITÁRIOS — semáforo de VEÍCULOS isolado (threads reais + HAL fake)
 *
 * O módulo veiculos.c roda com suas 4 threads. As entradas (botão noturno e
 * fio PED_REQ) são simuladas pelo teste com hal_fake_set(); as saídas (LEDs
 * e fios) são registradas com timestamp pela HAL fake.
 *
 * Os testes rodam EM ORDEM (nomes TU_V01, TU_V02...) sobre a mesma execução.
 */
#include <zephyr/ztest.h>

#include "teste_util.h"
#include "veiculos.h"

static void *setup(void)
{
	zassert_equal(veiculos_iniciar(), 0);
	k_msleep(50);
	return NULL;
}

ZTEST_SUITE(veiculos, NULL, setup, NULL, NULL, NULL);

/* TU-V01: partida segura — carros começam no VERMELHO, pedestres liberados */
ZTEST(veiculos, test_TU_V01_partida_segura)
{
	zassert_true(hal_fake_valor(SIG_VEH_VERMELHO));
	zassert_false(hal_fake_valor(SIG_VEH_VERDE));
	zassert_false(hal_fake_valor(SIG_VEH_AMARELO));
	zassert_true(hal_fake_valor(SIG_LINK_PED_GO));
	zassert_false(hal_fake_valor(SIG_LINK_NOITE));
}

/* TU-V02: ciclo normal — V 3 s -> A 1 s -> Vm 4 s, em ordem, 2 ciclos */
ZTEST(veiculos, test_TU_V02_ciclo_normal)
{
	hal_pulso_t v[MAX_PULSOS], a[MAX_PULSOS], r[MAX_PULSOS];

	hal_fake_limpar_eventos();
	/* referência: início de um verde */
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 9000);

	k_msleep(16200); /* 2 ciclos completos */

	int nv = pulsos_desde(SIG_VEH_VERDE, v, t0);
	int na = pulsos_desde(SIG_VEH_AMARELO, a, t0);
	int nr = pulsos_desde(SIG_VEH_VERMELHO, r, t0);

	zassert_true(nv >= 2 && na >= 2 && nr >= 2, "pulsos v=%d a=%d r=%d", nv, na, nr);

	for (int i = 0; i < 2; i++) {
		CHECA_DUR(v[i].duracao, T_VEH_VERDE_MS, "verde");
		CHECA_DUR(a[i].duracao, T_VEH_AMARELO_MS, "amarelo");
		CHECA_DUR(r[i].duracao, T_VEH_VERMELHO_MS, "vermelho");
		/* ordem: amarelo começa quando verde termina, vermelho quando amarelo termina */
		zassert_within((int)a[i].inicio, (int)(v[i].inicio + v[i].duracao), 15,
			       "amarelo não seguiu o verde");
		zassert_within((int)r[i].inicio, (int)(a[i].inicio + a[i].duracao), 15,
			       "vermelho não seguiu o amarelo");
	}
	zassert_equal(hal_violacoes(), 0, "dois LEDs acesos ao mesmo tempo");
}

/* TU-V03: exclusão mútua por amostragem — a cada 1 ms, no máx. 1 LED aceso */
ZTEST(veiculos, test_TU_V03_exclusao_mutua_amostrada)
{
	int max_acesos = 0;
	int amostras_com_1 = 0;
	int64_t fim = k_uptime_get() + 8500;

	while (k_uptime_get() < fim) {
		int n = hal_fake_valor(SIG_VEH_VERDE) + hal_fake_valor(SIG_VEH_AMARELO) +
			hal_fake_valor(SIG_VEH_VERMELHO);
		if (n > max_acesos) {
			max_acesos = n;
		}
		amostras_com_1 += (n == 1);
		k_msleep(1);
	}
	zassert_true(max_acesos <= 1, "%d LEDs acesos ao mesmo tempo", max_acesos);
	zassert_true(amostras_com_1 > 0, "nenhum LED aceso?");
	zassert_equal(hal_violacoes(), 0);
}

/* TU-V04: PED_GO só fica em 1 DENTRO do vermelho, com guarda de 100 ms no fim */
ZTEST(veiculos, test_TU_V04_ped_go_dentro_do_vermelho)
{
	hal_pulso_t r[MAX_PULSOS], g[MAX_PULSOS];

	esperar_borda(SIG_VEH_VERDE, true, 9000);
	hal_fake_limpar_eventos();
	k_msleep(8200);

	int nr = pulsos(SIG_VEH_VERMELHO, r);
	int ng = pulsos(SIG_LINK_PED_GO, g);

	zassert_true(nr >= 1 && ng >= 1, "nr=%d ng=%d", nr, ng);
	zassert_true(g[0].inicio >= r[0].inicio, "PED_GO subiu antes do vermelho");
	uint32_t fim_r = r[0].inicio + r[0].duracao;
	uint32_t fim_g = g[0].inicio + g[0].duracao;

	zassert_true(fim_r - fim_g >= T_GUARDA_MS - 15,
		     "guarda de segurança só %d ms", (int)(fim_r - fim_g));
	CHECA_DUR(g[0].duracao, T_VEH_VERMELHO_MS - T_GUARDA_MS, "PED_GO");
}

/* TU-V05: pedido de travessia no início do verde -> verde cai p/ 1 s (mínimo) */
ZTEST(veiculos, test_TU_V05_pedido_encurta_verde)
{
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 9000);

	k_msleep(200);
	hal_fake_set(SIG_LINK_PED_REQ, true);

	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 4000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MIN_MS, "verde com pedido");

	/* depois do verde vem amarelo 1 s e vermelho (transição segura) */
	int64_t t2 = esperar_borda(SIG_VEH_AMARELO, false, 2000);

	CHECA_DUR(t2 - t1, T_VEH_AMARELO_MS, "amarelo após pedido");
	esperar_nivel(SIG_VEH_VERMELHO, true, 100);
	esperar_nivel(SIG_LINK_PED_GO, true, 100);

	/* a placa de pedestres baixaria o pedido ao ver PED_GO = 1 */
	hal_fake_set(SIG_LINK_PED_REQ, false);
	k_msleep(50);
	zassert_false(veiculos_pedido(), "pedido deveria ter sido atendido/zerado");
}

/* TU-V06: pedido tardio (verde já passou de 1 s) -> verde corta na hora */
ZTEST(veiculos, test_TU_V06_pedido_tardio_corta_na_hora)
{
	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 9000);

	k_msleep(2000);
	int64_t tp = k_uptime_get();

	hal_fake_set(SIG_LINK_PED_REQ, true);
	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 2000);

	zassert_true(t1 - tp <= LAT_LINK_MS, "reação ao pedido levou %d ms", (int)(t1 - tp));
	zassert_true(t1 - t0 < T_VEH_VERDE_MS, "verde não foi encurtado");

	esperar_nivel(SIG_LINK_PED_GO, true, 2000);
	hal_fake_set(SIG_LINK_PED_REQ, false);
}

/* TU-V07: pedido durante o vermelho é ignorado -> próximo verde dura 3 s */
ZTEST(veiculos, test_TU_V07_pedido_no_vermelho_ignorado)
{
	esperar_borda(SIG_VEH_VERMELHO, true, 9000);
	k_msleep(500);
	hal_fake_set(SIG_LINK_PED_REQ, true);
	k_msleep(100);
	hal_fake_set(SIG_LINK_PED_REQ, false);
	zassert_false(veiculos_pedido(), "pedido no vermelho não deve ficar pendente");

	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 5000);
	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 5000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MS, "verde após pedido ignorado");
}

/* TU-V08: entrada no modo noturno (durante o verde) -> amarelo pisca 1 s/1 s */
ZTEST(veiculos, test_TU_V08_modo_noturno)
{
	hal_pulso_t a[MAX_PULSOS];

	esperar_borda(SIG_VEH_VERDE, true, 9000);
	k_msleep(500);
	int64_t tb = k_uptime_get();

	hal_fake_set(SIG_BTN_NOITE, true);           /* aperta...  */
	int64_t tn = esperar_nivel(SIG_LINK_NOITE, true, 500);

	k_msleep(60);
	hal_fake_set(SIG_BTN_NOITE, false);          /* ...e solta */
	zassert_true(tn - tb <= LAT_BOTAO_MS, "noturno demorou %d ms", (int)(tn - tb));
	zassert_true(veiculos_noite());

	/* o verde é interrompido e vai para o amarelo (nunca direto ao vermelho) */
	esperar_nivel(SIG_VEH_VERDE, false, 200);

	k_msleep(300);
	hal_fake_limpar_eventos();
	k_msleep(6200);

	int na = pulsos(SIG_VEH_AMARELO, a);

	zassert_true(na >= 2, "amarelo piscou %d vezes", na);
	for (int i = 0; i < na; i++) {
		CHECA_DUR(a[i].duracao, T_PISCA_MS, "amarelo aceso (noturno)");
		if (i > 0) {
			uint32_t apagado = a[i].inicio - (a[i - 1].inicio + a[i - 1].duracao);

			CHECA_DUR(apagado, T_PISCA_MS, "amarelo apagado (noturno)");
		}
	}
	hal_pulso_t x[MAX_PULSOS];

	zassert_equal(pulsos(SIG_VEH_VERDE, x), 0, "verde acendeu no noturno");
	zassert_equal(pulsos(SIG_VEH_VERMELHO, x), 0, "vermelho acendeu no noturno");
	zassert_false(hal_fake_valor(SIG_LINK_PED_GO), "pedestres liberados no noturno");
}

/* TU-V09: no noturno o pedido de travessia é ignorado */
ZTEST(veiculos, test_TU_V09_noturno_ignora_pedido)
{
	hal_fake_set(SIG_LINK_PED_REQ, true);
	k_msleep(100);
	zassert_false(veiculos_pedido());
	hal_fake_set(SIG_LINK_PED_REQ, false);
	zassert_false(hal_fake_valor(SIG_VEH_VERDE));
}

/* TU-V10: saída do noturno -> vermelho (parada segura) e volta ao ciclo normal */
ZTEST(veiculos, test_TU_V10_sai_do_noturno)
{
	hal_fake_apertar(SIG_BTN_NOITE, 100);
	esperar_nivel(SIG_LINK_NOITE, false, 300);
	zassert_false(veiculos_noite());

	esperar_nivel(SIG_VEH_VERMELHO, true, 300);
	zassert_false(hal_fake_valor(SIG_VEH_AMARELO));
	esperar_nivel(SIG_LINK_PED_GO, true, 100);

	int64_t t0 = esperar_borda(SIG_VEH_VERDE, true, 5000);
	int64_t t1 = esperar_borda(SIG_VEH_VERDE, false, 5000);

	CHECA_DUR(t1 - t0, T_VEH_VERDE_MS, "verde após noturno");
}

/* TU-V11: em toda a execução, nenhuma violação de exclusão mútua */
ZTEST(veiculos, test_TU_V11_sem_violacoes)
{
	zassert_equal(hal_violacoes(), 0, "violações = %u", hal_violacoes());
	zassert_false(hal_fake_estourou(), "buffer de eventos estourou");
}
