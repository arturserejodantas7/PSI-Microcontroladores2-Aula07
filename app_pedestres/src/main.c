/*
 * Semáforo de PEDESTRES (FRDM-KL25Z)
 * Toda a lógica está em lib/src/pedestres.c — aqui só inicializamos.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "semaforo_hal.h"
#include "pedestres.h"

int main(void)
{
	printk("\n=== SEMAFORO DE PEDESTRES (Aula07 PSI-Micro2) ===\n");
	printk("Formato do log: @<ms> <SINAL> <0|1>\n");

	if (pedestres_iniciar() != 0) {
		printk("ERRO: falha ao configurar GPIOs (veja o overlay)\n");
	}

	for (;;) {
		k_sleep(K_SECONDS(10));
		printk("@%u STATUS violacoes=%u\n", k_uptime_get_32(), hal_violacoes());
	}
	return 0;
}
