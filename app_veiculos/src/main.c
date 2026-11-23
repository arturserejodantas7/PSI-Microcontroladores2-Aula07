/*
 * Semáforo de VEÍCULOS (FRDM-KL25Z)
 * Toda a lógica está em lib/src/veiculos.c — aqui só inicializamos.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "semaforo_hal.h"
#include "veiculos.h"

int main(void)
{
	printk("\n=== SEMAFORO DE VEICULOS (Aula07 PSI-Micro2) ===\n");
	printk("Formato do log: @<ms> <SINAL> <0|1>\n");

	if (veiculos_iniciar() != 0) {
		printk("ERRO: falha ao configurar GPIOs (veja o overlay)\n");
	}

	/* Heartbeat: a cada 10 s mostra a contagem de violações (deve ser 0). */
	for (;;) {
		k_sleep(K_SECONDS(10));
		printk("@%u STATUS violacoes=%u\n", k_uptime_get_32(), hal_violacoes());
	}
	return 0;
}
