/*
 * TESTES UNITÁRIOS — lógica pura (lib/src/semaforo_logica.c)
 * Modelo V: nível de "projeto detalhado / módulo".
 *
 * IDs correspondem ao plano de testes (docs/PLANO_DE_TESTES.md).
 */
#include <zephyr/ztest.h>

#include "semaforo_logica.h"

ZTEST_SUITE(logica, NULL, NULL, NULL, NULL, NULL);

/* TU-L01: sequência normal dos carros V -> A -> Vm -> V */
ZTEST(logica, test_TU_L01_sequencia_veiculos)
{
	zassert_equal(veh_proximo(VEH_VERDE, false), VEH_AMARELO);
	zassert_equal(veh_proximo(VEH_AMARELO, false), VEH_VERMELHO);
	zassert_equal(veh_proximo(VEH_VERMELHO, false), VEH_VERDE);
}

/* TU-L02: no modo noturno qualquer estado vai para o AMARELO (piscante);
 * nunca verde -> vermelho direto. */
ZTEST(logica, test_TU_L02_noturno_vai_para_amarelo)
{
	zassert_equal(veh_proximo(VEH_VERDE, true), VEH_AMARELO);
	zassert_equal(veh_proximo(VEH_AMARELO, true), VEH_AMARELO);
	zassert_equal(veh_proximo(VEH_VERMELHO, true), VEH_AMARELO);
}

/* TU-L03: tempos do enunciado */
ZTEST(logica, test_TU_L03_duracoes)
{
	zassert_equal(veh_duracao_ms(VEH_VERDE), 3000U);
	zassert_equal(veh_duracao_ms(VEH_AMARELO), 1000U);
	zassert_equal(veh_duracao_ms(VEH_VERMELHO), 4000U);
	zassert_equal(T_PISCA_MS, 1000U, "pisca deve ser 1 s aceso / 1 s apagado");
	/* ciclo de pedestre: verde 4 s (= vermelho carro) e vermelho 4 s (= V + A) */
	zassert_equal(T_VEH_VERDE_MS + T_VEH_AMARELO_MS, 4000U);
}

/* TU-L04: verde encurta para o mínimo quando há pedido */
ZTEST(logica, test_TU_L04_verde_com_pedido)
{
	zassert_equal(veh_duracao_verde_ms(false), T_VEH_VERDE_MS);
	zassert_equal(veh_duracao_verde_ms(true), T_VEH_VERDE_MIN_MS);
	zassert_true(T_VEH_VERDE_MIN_MS < T_VEH_VERDE_MS);
}

/* TU-L05: quando o semáforo de carros aceita pedido de travessia */
ZTEST(logica, test_TU_L05_aceita_pedido)
{
	zassert_true(veh_aceita_pedido(VEH_VERDE, false));
	zassert_true(veh_aceita_pedido(VEH_AMARELO, false));
	zassert_false(veh_aceita_pedido(VEH_VERMELHO, false), "já liberado");
	zassert_false(veh_aceita_pedido(VEH_VERDE, true), "noturno ignora botão");
	zassert_false(veh_aceita_pedido(VEH_AMARELO, true));
}

/* TU-L06: pedestres só são liberados com carro VERMELHO e fora do noturno */
ZTEST(logica, test_TU_L06_libera_pedestre)
{
	zassert_false(veh_libera_pedestre(VEH_VERDE, false));
	zassert_false(veh_libera_pedestre(VEH_AMARELO, false));
	zassert_true(veh_libera_pedestre(VEH_VERMELHO, false));
	zassert_false(veh_libera_pedestre(VEH_VERMELHO, true));
}

/* TU-L07: tabela-verdade completa de ped_pode_verde (8 combinações) */
ZTEST(logica, test_TU_L07_ped_pode_verde)
{
	for (int i = 0; i < 8; i++) {
		bool go = i & 1, noite = i & 2, bloq = i & 4;
		bool esperado = go && !noite && !bloq;

		zassert_equal(ped_pode_verde(go, noite, bloq), esperado,
			      "go=%d noite=%d bloq=%d", go, noite, bloq);
	}
}

/* TU-L08: botão de travessia só gera pedido com pedestre fechado e de dia */
ZTEST(logica, test_TU_L08_ped_aceita_botao)
{
	zassert_true(ped_aceita_botao(false, false));
	zassert_false(ped_aceita_botao(true, false));
	zassert_false(ped_aceita_botao(false, true));
	zassert_false(ped_aceita_botao(true, true));
}

/* TU-L09: debounce rejeita ruído menor que o limiar */
ZTEST(logica, test_TU_L09_debounce_rejeita_ruido)
{
	debounce_t d;

	debounce_init(&d, 3, false);
	zassert_false(debounce_atualizar(&d, true));
	zassert_false(debounce_atualizar(&d, true));
	zassert_false(debounce_atualizar(&d, false)); /* glitch: zera contagem */
	zassert_false(debounce_atualizar(&d, true));
	zassert_false(debounce_atualizar(&d, true));
	zassert_false(d.estavel, "2 amostras não bastam");
}

/* TU-L10: debounce aceita mudança após N amostras e informa só UMA borda */
ZTEST(logica, test_TU_L10_debounce_aceita)
{
	debounce_t d;

	debounce_init(&d, 3, false);
	zassert_false(debounce_atualizar(&d, true));
	zassert_false(debounce_atualizar(&d, true));
	zassert_true(debounce_atualizar(&d, true), "3a amostra muda o estado");
	zassert_true(d.estavel);
	zassert_false(debounce_atualizar(&d, true), "sem nova borda");
	/* volta para 0 */
	zassert_false(debounce_atualizar(&d, false));
	zassert_false(debounce_atualizar(&d, false));
	zassert_true(debounce_atualizar(&d, false));
	zassert_false(d.estavel);
}

/* TU-L11: limiar 0 é tratado como 1 (robustez) */
ZTEST(logica, test_TU_L11_debounce_limiar_zero)
{
	debounce_t d;

	debounce_init(&d, 0, false);
	zassert_true(debounce_atualizar(&d, true));
}
