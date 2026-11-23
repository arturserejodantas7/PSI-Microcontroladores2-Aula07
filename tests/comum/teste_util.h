/*
 * teste_util.h — utilidades comuns aos testes com HAL fake.
 */
#ifndef TESTE_UTIL_H_
#define TESTE_UTIL_H_

#include <zephyr/ztest.h>
#include "hal_fake.h"

/* Tolerância de tempo (ms). Cobre a granularidade do tick do sistema e o
 * atraso de agendamento das threads. */
#define TOL_MS        60
/* Latência máxima entre um fio mudar e a placa reagir:
 * debounce (2 amostras x 10 ms) + 1 período de amostragem + folga. */
#define LAT_LINK_MS   45
/* Latência máxima do botão: debounce (3 x 10 ms) + 1 período + folga. */
#define LAT_BOTAO_MS  55

#define MAX_PULSOS 12

/* Verifica uma duração com tolerância. */
#define CHECA_DUR(real, esperado, nome)                                            \
	zassert_within((int)(real), (int)(esperado), TOL_MS,                         \
		       "%s durou %d ms (esperado %d +/- %d)", (nome), (int)(real),    \
		       (int)(esperado), TOL_MS)

/* Espera uma borda e FALHA o teste se não ocorrer no prazo. */
static inline int64_t esperar_borda(sinal_t s, bool v, uint32_t timeout)
{
	int64_t t = hal_fake_esperar_borda(s, v, timeout);

	zassert_true(t >= 0, "timeout esperando %s -> %d", hal_nome(s), v);
	return t;
}

static inline int64_t esperar_nivel(sinal_t s, bool v, uint32_t timeout)
{
	int64_t t = hal_fake_esperar_nivel(s, v, timeout);

	zassert_true(t >= 0, "timeout esperando %s == %d", hal_nome(s), v);
	return t;
}

static inline int pulsos(sinal_t s, hal_pulso_t *p)
{
	return hal_fake_pulsos(s, p, MAX_PULSOS);
}

/* Mantém só os pulsos que começaram em t >= t0 (descarta ciclos anteriores). */
static inline int pulsos_desde(sinal_t s, hal_pulso_t *p, int64_t t0)
{
	hal_pulso_t tmp[MAX_PULSOS];
	int n = hal_fake_pulsos(s, tmp, MAX_PULSOS);
	int k = 0;

	for (int i = 0; i < n; i++) {
		if ((int64_t)tmp[i].inicio >= t0 - 20) { /* 20 ms: atraso de detecção */
			p[k++] = tmp[i];
		}
	}
	return k;
}

#endif /* TESTE_UTIL_H_ */
