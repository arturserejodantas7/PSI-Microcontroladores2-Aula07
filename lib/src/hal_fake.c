/*
 * hal_fake.c — HAL simulada para os testes (ztest).
 *
 * - Saídas: guarda o valor e registra um evento com timestamp.
 * - Entradas: o teste define o valor com hal_fake_set().
 * - Como todos os sinais ficam no mesmo vetor, quando os dois semáforos
 *   rodam na mesma imagem (teste de integração) o que a placa de veículos
 *   escreve em LINK_PED_GO é exatamente o que a de pedestres lê: é um
 *   "fio" em software.
 */
#include <zephyr/kernel.h>
#include <string.h>

#include "semaforo_hal.h"
#include "hal_fake.h"

static hal_evento_t eventos[HAL_FAKE_MAX_EVENTOS];
static size_t n_eventos;
static bool estourou;

static void guardar(sinal_t s, bool valor)
{
	k_sched_lock();
	if (n_eventos < HAL_FAKE_MAX_EVENTOS) {
		eventos[n_eventos].t = k_uptime_get_32();
		eventos[n_eventos].sinal = (uint8_t)s;
		eventos[n_eventos].valor = valor ? 1U : 0U;
		n_eventos++;
	} else {
		estourou = true;
	}
	hal_comum_registrar(s, valor);
	k_sched_unlock();
}

int hal_configurar(sinal_t s, bool saida)
{
	if ((unsigned)s >= SIG_COUNT) {
		return -EINVAL;
	}
	if (saida) {
		guardar(s, false);
	}
	return 0;
}

void hal_escrever(sinal_t s, bool valor)
{
	if ((unsigned)s < SIG_COUNT) {
		guardar(s, valor);
	}
}

bool hal_ler(sinal_t s)
{
	return hal_comum_valor(s);
}

/* ------------------- API exclusiva dos testes ------------------- */

void hal_fake_set(sinal_t s, bool valor)
{
	if ((unsigned)s < SIG_COUNT && hal_comum_valor(s) != valor) {
		guardar(s, valor);
	}
}

bool hal_fake_valor(sinal_t s)
{
	return hal_comum_valor(s);
}

void hal_fake_limpar_eventos(void)
{
	k_sched_lock();
	n_eventos = 0;
	estourou = false;
	k_sched_unlock();
}

size_t hal_fake_eventos(const hal_evento_t **buf)
{
	*buf = eventos;
	return n_eventos;
}

bool hal_fake_estourou(void)
{
	return estourou;
}

int hal_fake_pulsos(sinal_t s, hal_pulso_t *out, int max)
{
	int n = 0;
	bool alto = false;
	uint32_t inicio = 0;

	for (size_t i = 0; i < n_eventos && n < max; i++) {
		if (eventos[i].sinal != (uint8_t)s) {
			continue;
		}
		if (eventos[i].valor && !alto) {
			alto = true;
			inicio = eventos[i].t;
		} else if (!eventos[i].valor && alto) {
			alto = false;
			out[n].inicio = inicio;
			out[n].duracao = eventos[i].t - inicio;
			n++;
		}
	}
	return n;
}

int64_t hal_fake_esperar_borda(sinal_t s, bool valor, uint32_t timeout_ms)
{
	int64_t limite = k_uptime_get() + timeout_ms;

	/* primeiro espera estar no nível oposto, depois a borda */
	while (hal_comum_valor(s) == valor) {
		if (k_uptime_get() > limite) {
			return -1;
		}
		k_msleep(1);
	}
	while (hal_comum_valor(s) != valor) {
		if (k_uptime_get() > limite) {
			return -1;
		}
		k_msleep(1);
	}
	return k_uptime_get();
}

int64_t hal_fake_esperar_nivel(sinal_t s, bool valor, uint32_t timeout_ms)
{
	int64_t limite = k_uptime_get() + timeout_ms;

	while (hal_comum_valor(s) != valor) {
		if (k_uptime_get() > limite) {
			return -1;
		}
		k_msleep(1);
	}
	return k_uptime_get();
}

void hal_fake_apertar(sinal_t botao, uint32_t ms)
{
	hal_fake_set(botao, true);
	k_msleep(ms);
	hal_fake_set(botao, false);
}
