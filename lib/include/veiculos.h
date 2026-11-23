/*
 * veiculos.h — semáforo de veículos (3 LEDs, 3 threads + thread de entradas)
 */
#ifndef VEICULOS_H_
#define VEICULOS_H_

#include <stdbool.h>
#include "semaforo_logica.h"

/* Configura os sinais e cria as threads. Chamar uma única vez. */
int veiculos_iniciar(void);

/* Consultas (thread-safe) — usadas pelos testes. */
veh_cor_t veiculos_vez(void);
bool veiculos_noite(void);
bool veiculos_pedido(void);

#endif /* VEICULOS_H_ */
