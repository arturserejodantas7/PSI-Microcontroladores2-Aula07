/*
 * hal_comum.c — partes comuns às HALs real e fake:
 *   - nomes dos sinais
 *   - "sombra" do valor de cada sinal
 *   - verificação contínua das invariantes de segurança
 *   - log serial padronizado "@<ms> <SINAL> <valor>"
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "semaforo_hal.h"

static const char *const nomes[SIG_COUNT] = {
	[SIG_VEH_VERDE] = "VEH_VERDE",
	[SIG_VEH_AMARELO] = "VEH_AMARELO",
	[SIG_VEH_VERMELHO] = "VEH_VERMELHO",
	[SIG_PED_VERDE] = "PED_VERDE",
	[SIG_PED_VERMELHO] = "PED_VERMELHO",
	[SIG_LINK_PED_GO] = "LINK_PED_GO",
	[SIG_LINK_NOITE] = "LINK_NOITE",
	[SIG_LINK_PED_REQ] = "LINK_PED_REQ",
	[SIG_BTN_NOITE] = "BTN_NOITE",
	[SIG_BTN_TRAVESSIA] = "BTN_TRAVESSIA",
};

static bool sombra[SIG_COUNT];
static uint32_t violacoes;
static uint32_t conflitos;

const char *hal_nome(sinal_t s)
{
	return ((unsigned)s < SIG_COUNT) ? nomes[s] : "?";
}

void hal_comum_registrar(sinal_t s, bool valor)
{
	if ((unsigned)s >= SIG_COUNT) {
		return;
	}

	/* Impede que outra thread intercale no meio do registro/log. */
	k_sched_lock();

	sombra[s] = valor;

	int veh = sombra[SIG_VEH_VERDE] + sombra[SIG_VEH_AMARELO] + sombra[SIG_VEH_VERMELHO];
	int ped = sombra[SIG_PED_VERDE] + sombra[SIG_PED_VERMELHO];
	bool viol = (veh > 1) || (ped > 1);
	bool conf = sombra[SIG_PED_VERDE] && (sombra[SIG_VEH_VERDE] || sombra[SIG_VEH_AMARELO]);
	uint32_t t = k_uptime_get_32();

	printk("@%u %s %d\n", t, nomes[s], valor ? 1 : 0);
	if (viol) {
		violacoes++;
		printk("@%u VIOLACAO %u\n", t, violacoes);
	}
	if (conf) {
		conflitos++;
		printk("@%u CONFLITO %u\n", t, conflitos);
	}

	k_sched_unlock();
}

bool hal_comum_valor(sinal_t s)
{
	return ((unsigned)s < SIG_COUNT) ? sombra[s] : false;
}

void hal_evento(const char *nome, int valor)
{
	k_sched_lock();
	printk("@%u EVT %s %d\n", k_uptime_get_32(), nome, valor);
	k_sched_unlock();
}

uint32_t hal_violacoes(void)
{
	return violacoes;
}

uint32_t hal_conflitos(void)
{
	return conflitos;
}
