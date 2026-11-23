/*
 * veiculos.c — Semáforo de VEÍCULOS
 *
 * Arquitetura (padrão "monitor" = mutex + variável de condição):
 *
 *   - 1 mutex (mtx) protege TODO o estado compartilhado (st) e as escritas
 *     nos LEDs.
 *   - "st.vez" é o "token": só a thread cuja cor == st.vez pode acender o
 *     seu LED. A vez só é passada DEPOIS que o LED foi apagado, dentro do
 *     mutex -> nunca há dois LEDs acesos (exclusão mútua).
 *   - Todas as esperas usam k_condvar_wait(), que LIBERA o mutex enquanto
 *     espera; assim a thread de entradas consegue atualizar pedido/noite a
 *     qualquer momento e acordar a thread que está com o LED aceso.
 *
 *   thread_verde    : verde 3 s (ou 1 s mínimo se houver pedido de pedestre)
 *   thread_amarelo  : amarelo 1 s; no modo noturno pisca 1 s/1 s
 *   thread_vermelho : vermelho 4 s; liga a linha PED_GO (libera pedestres)
 *                     e a desliga T_GUARDA ms antes de sair do vermelho
 *   thread_entradas : lê botão do modo noturno e o fio PED_REQ (10 ms)
 */
#include <zephyr/kernel.h>

#include "semaforo_hal.h"
#include "semaforo_logica.h"
#include "veiculos.h"

#define STACK_LED      768
#define STACK_ENTRADAS 768
#define PRIO_LED       5
#define PRIO_ENTRADAS  4   /* maior prioridade que os LEDs */

static struct k_mutex mtx;
static struct k_condvar cv;

static struct {
	veh_cor_t vez;
	bool noite;
	bool pedido;
} st;

K_THREAD_STACK_DEFINE(stk_veh_verde, STACK_LED);
K_THREAD_STACK_DEFINE(stk_veh_amarelo, STACK_LED);
K_THREAD_STACK_DEFINE(stk_veh_vermelho, STACK_LED);
K_THREAD_STACK_DEFINE(stk_veh_entradas, STACK_ENTRADAS);
static struct k_thread th_verde, th_amarelo, th_vermelho, th_entradas;

/* ----------------- auxiliares (chamar com mtx travado) ----------------- */

static void aguardar_vez(veh_cor_t cor)
{
	while (st.vez != cor) {
		(void)k_condvar_wait(&cv, &mtx, K_FOREVER);
	}
}

static void passar_vez(veh_cor_t prox)
{
	st.vez = prox;
	k_condvar_broadcast(&cv);
}

/* Espera até o instante 'prazo' (uptime em ms) OU até interromper()
 * ficar verdadeiro. Retorna true se foi interrompida. */
static bool esperar_ate(int64_t prazo, bool (*interromper)(void))
{
	for (;;) {
		if (interromper != NULL && interromper()) {
			return true;
		}
		int64_t resta = prazo - k_uptime_get();

		if (resta <= 0) {
			return false;
		}
		(void)k_condvar_wait(&cv, &mtx, K_MSEC(resta));
	}
}

static bool noite_ligada(void)
{
	return st.noite;
}

static bool noite_desligada(void)
{
	return !st.noite;
}

/* ------------------------------ threads ------------------------------ */

static void thread_verde(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	k_mutex_lock(&mtx, K_FOREVER);
	for (;;) {
		aguardar_vez(VEH_VERDE);

		hal_escrever(SIG_VEH_VERDE, true);
		int64_t inicio = k_uptime_get();

		/* O prazo é recalculado a cada despertar: se chegar um pedido de
		 * travessia, o verde encurta para T_VEH_VERDE_MIN_MS. */
		for (;;) {
			if (st.noite) {
				break;
			}
			int64_t prazo = inicio + veh_duracao_verde_ms(st.pedido);
			int64_t resta = prazo - k_uptime_get();

			if (resta <= 0) {
				break;
			}
			(void)k_condvar_wait(&cv, &mtx, K_MSEC(resta));
		}

		hal_escrever(SIG_VEH_VERDE, false);
		passar_vez(veh_proximo(VEH_VERDE, st.noite));
	}
}

static void thread_amarelo(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	k_mutex_lock(&mtx, K_FOREVER);
	for (;;) {
		aguardar_vez(VEH_AMARELO);

		if (st.noite) {
			/* Modo noturno: 1 s aceso, 1 s apagado. Sai na hora se o
			 * modo noturno for desligado. */
			hal_escrever(SIG_VEH_AMARELO, true);
			bool saiu = esperar_ate(k_uptime_get() + T_PISCA_MS, noite_desligada);

			hal_escrever(SIG_VEH_AMARELO, false);
			if (!saiu) {
				(void)esperar_ate(k_uptime_get() + T_PISCA_MS, noite_desligada);
			}
			if (!st.noite) {
				/* Saída do noturno: vai para o vermelho (parada segura). */
				passar_vez(VEH_VERMELHO);
			}
			continue;
		}

		hal_escrever(SIG_VEH_AMARELO, true);
		(void)esperar_ate(k_uptime_get() + T_VEH_AMARELO_MS, NULL);
		hal_escrever(SIG_VEH_AMARELO, false);

		passar_vez(veh_proximo(VEH_AMARELO, st.noite));
	}
}

static void thread_vermelho(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	k_mutex_lock(&mtx, K_FOREVER);
	for (;;) {
		aguardar_vez(VEH_VERMELHO);

		hal_escrever(SIG_VEH_VERMELHO, true);
		int64_t inicio = k_uptime_get();

		st.pedido = false; /* pedido atendido: os pedestres vão passar */

		/* Libera os pedestres SÓ depois que o vermelho já está aceso. */
		if (veh_libera_pedestre(VEH_VERMELHO, st.noite)) {
			hal_escrever(SIG_LINK_PED_GO, true);
		}

		/* Vermelho até (4 s - guarda), ou até entrar no modo noturno. */
		(void)esperar_ate(inicio + T_VEH_VERMELHO_MS - T_GUARDA_MS, noite_ligada);

		/* Retira a permissão dos pedestres e mantém "todos vermelhos"
		 * por pelo menos T_GUARDA_MS antes de qualquer outra cor. */
		hal_escrever(SIG_LINK_PED_GO, false);
		int64_t fim = st.noite ? (k_uptime_get() + T_GUARDA_MS)
				       : (inicio + T_VEH_VERMELHO_MS);

		(void)esperar_ate(fim, NULL);

		hal_escrever(SIG_VEH_VERMELHO, false);
		passar_vez(veh_proximo(VEH_VERMELHO, st.noite));
	}
}

static void thread_entradas(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	debounce_t db_btn, db_req;

	debounce_init(&db_btn, DEBOUNCE_BOTAO_N, false);
	debounce_init(&db_req, DEBOUNCE_LINK_N, false);

	for (;;) {
		bool mudou_btn = debounce_atualizar(&db_btn, hal_ler(SIG_BTN_NOITE));
		bool mudou_req = debounce_atualizar(&db_req, hal_ler(SIG_LINK_PED_REQ));

		if (mudou_btn || mudou_req) {
			k_mutex_lock(&mtx, K_FOREVER);

			/* Borda de descida->subida do botão: alterna modo noturno */
			if (mudou_btn && db_btn.estavel) {
				st.noite = !st.noite;
				hal_evento("NOITE", st.noite);
				hal_escrever(SIG_LINK_NOITE, st.noite);
				if (st.noite) {
					st.pedido = false;
				}
			}

			/* Borda de subida do pedido de travessia */
			if (mudou_req && db_req.estavel) {
				if (veh_aceita_pedido(st.vez, st.noite)) {
					st.pedido = true;
					hal_evento("PEDIDO", 1);
				} else {
					hal_evento("PEDIDO_IGNORADO", 1);
				}
			}

			k_condvar_broadcast(&cv);
			k_mutex_unlock(&mtx);
		}

		k_msleep(T_AMOSTRAGEM_MS);
	}
}

/* ------------------------------ API ------------------------------ */

int veiculos_iniciar(void)
{
	int ret = 0;

	k_mutex_init(&mtx);
	k_condvar_init(&cv);

	ret |= hal_configurar(SIG_VEH_VERDE, true);
	ret |= hal_configurar(SIG_VEH_AMARELO, true);
	ret |= hal_configurar(SIG_VEH_VERMELHO, true);
	ret |= hal_configurar(SIG_LINK_PED_GO, true);
	ret |= hal_configurar(SIG_LINK_NOITE, true);
	ret |= hal_configurar(SIG_LINK_PED_REQ, false);
	ret |= hal_configurar(SIG_BTN_NOITE, false);

	/* Partida segura: carros começam no VERMELHO. */
	st.vez = VEH_VERMELHO;
	st.noite = false;
	st.pedido = false;

	k_thread_create(&th_entradas, stk_veh_entradas, K_THREAD_STACK_SIZEOF(stk_veh_entradas),
			thread_entradas, NULL, NULL, NULL, PRIO_ENTRADAS, 0, K_NO_WAIT);
	k_thread_create(&th_verde, stk_veh_verde, K_THREAD_STACK_SIZEOF(stk_veh_verde),
			thread_verde, NULL, NULL, NULL, PRIO_LED, 0, K_NO_WAIT);
	k_thread_create(&th_amarelo, stk_veh_amarelo, K_THREAD_STACK_SIZEOF(stk_veh_amarelo),
			thread_amarelo, NULL, NULL, NULL, PRIO_LED, 0, K_NO_WAIT);
	k_thread_create(&th_vermelho, stk_veh_vermelho, K_THREAD_STACK_SIZEOF(stk_veh_vermelho),
			thread_vermelho, NULL, NULL, NULL, PRIO_LED, 0, K_NO_WAIT);

	k_thread_name_set(&th_entradas, "veh_entradas");
	k_thread_name_set(&th_verde, "veh_verde");
	k_thread_name_set(&th_amarelo, "veh_amarelo");
	k_thread_name_set(&th_vermelho, "veh_vermelho");

	return ret;
}

veh_cor_t veiculos_vez(void)
{
	k_mutex_lock(&mtx, K_FOREVER);
	veh_cor_t v = st.vez;
	k_mutex_unlock(&mtx);
	return v;
}

bool veiculos_noite(void)
{
	k_mutex_lock(&mtx, K_FOREVER);
	bool n = st.noite;
	k_mutex_unlock(&mtx);
	return n;
}

bool veiculos_pedido(void)
{
	k_mutex_lock(&mtx, K_FOREVER);
	bool p = st.pedido;
	k_mutex_unlock(&mtx);
	return p;
}
