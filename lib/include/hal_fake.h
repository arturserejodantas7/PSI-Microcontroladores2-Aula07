/*
 * hal_fake.h — API extra da HAL simulada, usada só pelos testes.
 */
#ifndef HAL_FAKE_H_
#define HAL_FAKE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "semaforo_hal.h"

#define HAL_FAKE_MAX_EVENTOS 200

typedef struct {
	uint32_t t;     /* ms desde o boot */
	uint8_t sinal;  /* sinal_t         */
	uint8_t valor;  /* 0 ou 1          */
} hal_evento_t;

typedef struct {
	uint32_t inicio;   /* ms */
	uint32_t duracao;  /* ms */
} hal_pulso_t;

/* Define o valor de uma entrada (botão, fio vindo da outra placa). */
void hal_fake_set(sinal_t s, bool valor);

/* Valor atual de qualquer sinal. */
bool hal_fake_valor(sinal_t s);

/* Aperta um botão por 'ms' milissegundos (bloqueante). */
void hal_fake_apertar(sinal_t botao, uint32_t ms);

void hal_fake_limpar_eventos(void);
size_t hal_fake_eventos(const hal_evento_t **buf);
bool hal_fake_estourou(void);

/* Extrai os pulsos em nível alto COMPLETOS (subida e descida) de um sinal. */
int hal_fake_pulsos(sinal_t s, hal_pulso_t *out, int max);

/* Espera a próxima borda (para 'valor'). Retorna o instante (ms) ou -1. */
int64_t hal_fake_esperar_borda(sinal_t s, bool valor, uint32_t timeout_ms);

/* Espera o sinal estar em 'valor'. Retorna o instante (ms) ou -1. */
int64_t hal_fake_esperar_nivel(sinal_t s, bool valor, uint32_t timeout_ms);

#endif /* HAL_FAKE_H_ */
