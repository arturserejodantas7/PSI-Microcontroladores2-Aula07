/*
 * semaforo_logica.c — lógica pura (ver semaforo_logica.h)
 */
#include "semaforo_logica.h"

veh_cor_t veh_proximo(veh_cor_t atual, bool noite)
{
	if (noite) {
		return VEH_AMARELO;
	}

	switch (atual) {
	case VEH_VERDE:
		return VEH_AMARELO;
	case VEH_AMARELO:
		return VEH_VERMELHO;
	case VEH_VERMELHO:
	default:
		return VEH_VERDE;
	}
}

uint32_t veh_duracao_ms(veh_cor_t cor)
{
	switch (cor) {
	case VEH_VERDE:
		return T_VEH_VERDE_MS;
	case VEH_AMARELO:
		return T_VEH_AMARELO_MS;
	case VEH_VERMELHO:
	default:
		return T_VEH_VERMELHO_MS;
	}
}

uint32_t veh_duracao_verde_ms(bool pedido)
{
	return pedido ? T_VEH_VERDE_MIN_MS : T_VEH_VERDE_MS;
}

bool veh_aceita_pedido(veh_cor_t vez, bool noite)
{
	/* No vermelho os pedestres já estão liberados; no noturno não há travessia. */
	return !noite && (vez != VEH_VERMELHO);
}

bool veh_libera_pedestre(veh_cor_t vez, bool noite)
{
	return !noite && (vez == VEH_VERMELHO);
}

bool ped_pode_verde(bool ped_go, bool noite, bool bloqueado)
{
	return ped_go && !noite && !bloqueado;
}

bool ped_aceita_botao(bool ped_go, bool noite)
{
	return !ped_go && !noite;
}

void debounce_init(debounce_t *d, uint8_t limiar, bool inicial)
{
	d->limiar = (limiar == 0U) ? 1U : limiar;
	d->contador = 0U;
	d->estavel = inicial;
}

bool debounce_atualizar(debounce_t *d, bool amostra)
{
	if (amostra == d->estavel) {
		d->contador = 0U;
		return false;
	}

	d->contador++;
	if (d->contador >= d->limiar) {
		d->estavel = amostra;
		d->contador = 0U;
		return true;
	}
	return false;
}
