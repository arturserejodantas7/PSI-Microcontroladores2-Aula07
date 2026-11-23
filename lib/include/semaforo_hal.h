/*
 * semaforo_hal.h — camada de abstração de hardware (HAL).
 *
 * Duas implementações:
 *   - hal_gpio.c : placa real (FRDM-KL25Z), pinos vindos do devicetree
 *   - hal_fake.c : testes (ztest). Registra cada escrita com timestamp e
 *                  verifica as invariantes de segurança automaticamente.
 *
 * Toda escrita é registrada na serial no formato:
 *     @<tempo_ms> <SINAL> <0|1>
 * que é lido pelo tools/analisa_log.py.
 */
#ifndef SEMAFORO_HAL_H_
#define SEMAFORO_HAL_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	/* LEDs da placa de veículos */
	SIG_VEH_VERDE = 0,
	SIG_VEH_AMARELO,
	SIG_VEH_VERMELHO,
	/* LEDs da placa de pedestres */
	SIG_PED_VERDE,
	SIG_PED_VERMELHO,
	/* Fios de sincronismo entre as placas */
	SIG_LINK_PED_GO,   /* veículos -> pedestres: 1 = carros parados, pode atravessar */
	SIG_LINK_NOITE,    /* veículos -> pedestres: 1 = modo noturno                    */
	SIG_LINK_PED_REQ,  /* pedestres -> veículos: 1 = pedido de travessia pendente    */
	/* Botões */
	SIG_BTN_NOITE,     /* placa de veículos: alterna modo noturno                    */
	SIG_BTN_TRAVESSIA, /* placa de pedestres: botão de travessia                     */
	SIG_COUNT
} sinal_t;

/* Configura um sinal como saída (inicia em 0) ou entrada. */
int hal_configurar(sinal_t s, bool saida);

/* Escreve valor lógico em uma saída (e registra no log). */
void hal_escrever(sinal_t s, bool valor);

/* Lê valor lógico de uma entrada (botão pressionado = true). */
bool hal_ler(sinal_t s);

/* Registra um evento textual no log (ex.: "EVT NOITE 1"). */
void hal_evento(const char *nome, int valor);

/* Nome do sinal (para logs). */
const char *hal_nome(sinal_t s);

/* Quantas vezes a invariante "no máximo 1 LED aceso por semáforo"
 * foi violada desde o boot (deve ser sempre 0). */
uint32_t hal_violacoes(void);

/* Quantas vezes houve CONFLITO entre semáforos: pedestre verde aceso
 * junto com carro verde ou amarelo. Só é mensurável quando os dois
 * semáforos rodam na mesma imagem (teste de integração com HAL fake). */
uint32_t hal_conflitos(void);

/* ---- uso interno das implementações (hal_comum.c) ---- */
void hal_comum_registrar(sinal_t s, bool valor);
bool hal_comum_valor(sinal_t s);

#endif /* SEMAFORO_HAL_H_ */
