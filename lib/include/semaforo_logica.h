/*
 * semaforo_logica.h
 *
 * Lógica PURA do sistema de semáforos (sem threads, sem GPIO).
 * Tudo aqui é determinístico e testado pelos testes unitários
 * (tests/unit_logica). As threads em veiculos.c / pedestres.c
 * apenas consultam estas funções.
 */
#ifndef SEMAFORO_LOGICA_H_
#define SEMAFORO_LOGICA_H_

#include <stdbool.h>
#include <stdint.h>

/* ---------------- Tempos do enunciado (ms) ---------------- */
#define T_VEH_VERDE_MS        3000U  /* carro verde                          */
#define T_VEH_AMARELO_MS      1000U  /* carro amarelo                        */
#define T_VEH_VERMELHO_MS     4000U  /* carro vermelho (= pedestre verde)    */
#define T_PISCA_MS            1000U  /* modo noturno: 1 s aceso / 1 s apagado */

/* ---------------- Tempos de projeto (segurança) ------------ */
#define T_VEH_VERDE_MIN_MS    1000U  /* verde mínimo quando há pedido de travessia */
#define T_GUARDA_MS            100U  /* "todos vermelhos": PED_GO cai 100 ms antes
                                        do fim do vermelho dos carros            */
#define T_PED_VERDE_MAX_MS    4300U  /* fail-safe: pedestre verde nunca passa disso */
#define T_AMOSTRAGEM_MS         10U  /* período da thread de entradas            */
#define DEBOUNCE_BOTAO_N         3U  /* 3 amostras iguais = 30 ms                */
#define DEBOUNCE_LINK_N          2U  /* 2 amostras iguais = 20 ms                */

/* ---------------- Estados ---------------- */
typedef enum {
	VEH_VERDE = 0,
	VEH_AMARELO,
	VEH_VERMELHO,
} veh_cor_t;

typedef enum {
	PED_VERDE = 0,
	PED_VERMELHO,
} ped_cor_t;

/* ---------------- Semáforo de veículos ---------------- */

/* Próxima cor após 'atual'. No modo noturno sempre vai para AMARELO
 * (a thread do amarelo é quem pisca). */
veh_cor_t veh_proximo(veh_cor_t atual, bool noite);

/* Duração nominal de cada cor no modo normal. */
uint32_t veh_duracao_ms(veh_cor_t cor);

/* Duração do verde: 3 s normalmente, 1 s (mínimo) se houver pedido. */
uint32_t veh_duracao_verde_ms(bool pedido);

/* O semáforo de carros aceita um pedido de travessia neste estado? */
bool veh_aceita_pedido(veh_cor_t vez, bool noite);

/* Os pedestres podem ser liberados (linha PED_GO = 1)? */
bool veh_libera_pedestre(veh_cor_t vez, bool noite);

/* ---------------- Semáforo de pedestres ---------------- */

/* Pedestre pode ficar verde? Só com PED_GO=1, fora do modo noturno
 * e sem bloqueio de falha (fail-safe). */
bool ped_pode_verde(bool ped_go, bool noite, bool bloqueado);

/* O botão de travessia deve gerar um pedido agora? */
bool ped_aceita_botao(bool ped_go, bool noite);

/* ---------------- Debounce genérico ---------------- */
typedef struct {
	uint8_t limiar;   /* nº de amostras iguais para aceitar mudança */
	uint8_t contador;
	bool estavel;     /* estado estável atual                       */
} debounce_t;

void debounce_init(debounce_t *d, uint8_t limiar, bool inicial);

/* Alimenta uma amostra; retorna true se o estado estável MUDOU. */
bool debounce_atualizar(debounce_t *d, bool amostra);

#endif /* SEMAFORO_LOGICA_H_ */
