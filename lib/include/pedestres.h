/*
 * pedestres.h — semáforo de pedestres (2 LEDs, 2 threads + thread de entradas)
 */
#ifndef PEDESTRES_H_
#define PEDESTRES_H_

#include <stdbool.h>
#include "semaforo_logica.h"

/* Configura os sinais e cria as threads. Chamar uma única vez. */
int pedestres_iniciar(void);

/* Consultas (thread-safe) — usadas pelos testes. */
ped_cor_t pedestres_vez(void);
bool pedestres_bloqueado(void);

#endif /* PEDESTRES_H_ */
