/*
 * pedestres.c — Semáforo de PEDESTRES
 *
 * Mesma arquitetura do semáforo de veículos (mutex + variável de condição +
 * "token" st.vez). Esta placa é ESCRAVA da placa de veículos para garantir o
 * sincronismo: o verde do pedestre só acende enquanto o fio PED_GO = 1, ou
 * seja, enquanto o vermelho dos carros está aceso.
 *
 *   thread_verde    : verde enquanto PED_GO = 1 (~4 s). Fail-safe: se PED_GO
 *                     ficar preso em 1, apaga após T_PED_VERDE_MAX_MS e
 *                     bloqueia novo verde até PED_GO voltar a 0.
 *   thread_vermelho : vermelho fixo; no modo noturno pisca 1 s / 1 s.
 *   thread_entradas : lê PED_GO, NOITE (fios) e o botão de travessia;
 *                     controla o fio PED_REQ (pedido de travessia).
 *
 * Falha no cabo (fio solto): PED_GO tem pull-down -> lê 0 -> pedestre fica
 * no vermelho (estado seguro).
 */
#include <zephyr/kernel.h>

#include "semaforo_hal.h"
#include "semaforo_logica.h"
#include "pedestres.h"

#define STACK_LED      768
#define STACK_ENTRADAS 768
#define PRIO_LED       5
#define PRIO_ENTRADAS  4

static struct k_mutex mtx;
static struct k_condvar cv;

static struct {
	ped_cor_t vez;
	bool ped_go;     /* fio vindo da placa de veículos */
	bool noite;      /* fio vindo da placa de veículos */
	bool bloqueado;  /* fail-safe acionado              */
	bool req;        /* pedido de travessia pendente    */
} st;

K_THREAD_STACK_DEFINE(stk_ped_verde, STACK_LED);
K_THREAD_STACK_DEFINE(stk_ped_vermelho, STACK_LED);
K_THREAD_STACK_DEFINE(stk_ped_entradas, STACK_ENTRADAS);
static struct k_thread th_verde, th_vermelho, th_entradas;

/* ----------------- auxiliares (chamar com mtx travado) ----------------- */

static void aguardar_vez(ped_cor_t cor)
{
	while (st.vez != cor) {
		(void)k_condvar_wait(&cv, &mtx, K_FOREVER);
	}
}

static void passar_vez(ped_cor_t prox)
{
	st.vez = prox;
	k_condvar_broadcast(&cv);
}

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

static bool pode_verde(void)
{
	return ped_pode_verde(st.ped_go, st.noite, st.bloqueado);
}

static bool deve_sair_verde(void)
{
	return !pode_verde();
}

static bool pode_verde_ou_noite(void)
{
	return pode_verde() || st.noite;
}

static bool noite_desligada(void)
{
	return !st.noite;
}

static void atualizar_req(bool valor)
{
	if (st.req != valor) {
		st.req = valor;
		hal_escrever(SIG_LINK_PED_REQ, valor);
	}
}

/* ------------------------------ threads ------------------------------ */

static void thread_verde(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	k_mutex_lock(&mtx, K_FOREVER);
	for (;;) {
		aguardar_vez(PED_VERDE);

		hal_escrever(SIG_PED_VERDE, true);
		int64_t inicio = k_uptime_get();

		bool saiu = esperar_ate(inicio + T_PED_VERDE_MAX_MS, deve_sair_verde);

		hal_escrever(SIG_PED_VERDE, false);

		if (!saiu) {
			/* PED_GO ficou alto tempo demais: fio em curto/placa travada. */
			st.bloqueado = true;
			hal_evento("FAILSAFE", 1);
		}
		passar_vez(PED_VERMELHO);
	}
}

static void thread_vermelho(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	k_mutex_lock(&mtx, K_FOREVER);
	for (;;) {
		aguardar_vez(PED_VERMELHO);

		if (st.noite) {
			/* Modo noturno: vermelho pisca 1 s aceso / 1 s apagado. */
			hal_escrever(SIG_PED_VERMELHO, true);
			bool saiu = esperar_ate(k_uptime_get() + T_PISCA_MS, noite_desligada);

			hal_escrever(SIG_PED_VERMELHO, false);
			if (!saiu) {
				(void)esperar_ate(k_uptime_get() + T_PISCA_MS, noite_desligada);
			}
			continue;
		}

		hal_escrever(SIG_PED_VERMELHO, true);

		/* Fica vermelho até a placa de veículos liberar ou entrar o noturno. */
		while (!pode_verde_ou_noite()) {
			(void)k_condvar_wait(&cv, &mtx, K_FOREVER);
		}

		hal_escrever(SIG_PED_VERMELHO, false);

		if (pode_verde()) {
			passar_vez(PED_VERDE);
		}
		/* se for noite, o laço volta e começa a piscar */
	}
}

static void thread_entradas(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	debounce_t db_go, db_noite, db_btn;

	debounce_init(&db_go, DEBOUNCE_LINK_N, false);
	debounce_init(&db_noite, DEBOUNCE_LINK_N, false);
	debounce_init(&db_btn, DEBOUNCE_BOTAO_N, false);

	for (;;) {
		bool m_go = debounce_atualizar(&db_go, hal_ler(SIG_LINK_PED_GO));
		bool m_noite = debounce_atualizar(&db_noite, hal_ler(SIG_LINK_NOITE));
		bool m_btn = debounce_atualizar(&db_btn, hal_ler(SIG_BTN_TRAVESSIA));

		if (m_go || m_noite || m_btn) {
			k_mutex_lock(&mtx, K_FOREVER);

			if (m_go) {
				st.ped_go = db_go.estavel;
				hal_evento("PED_GO_LIDO", st.ped_go);
				if (st.ped_go) {
					atualizar_req(false); /* pedido atendido */
				} else {
					st.bloqueado = false; /* PED_GO voltou a 0: rearma */
				}
			}

			if (m_noite) {
				st.noite = db_noite.estavel;
				hal_evento("NOITE_LIDO", st.noite);
				if (st.noite) {
					atualizar_req(false);
				}
			}

			if (m_btn && db_btn.estavel) {
				if (ped_aceita_botao(st.ped_go, st.noite)) {
					atualizar_req(true);
					hal_evento("BOTAO", 1);
				} else {
					hal_evento("BOTAO_IGNORADO", 1);
				}
			}

			k_condvar_broadcast(&cv);
			k_mutex_unlock(&mtx);
		}

		k_msleep(T_AMOSTRAGEM_MS);
	}
}

/* ------------------------------ API ------------------------------ */

int pedestres_iniciar(void)
{
	int ret = 0;

	k_mutex_init(&mtx);
	k_condvar_init(&cv);

	ret |= hal_configurar(SIG_PED_VERDE, true);
	ret |= hal_configurar(SIG_PED_VERMELHO, true);
	ret |= hal_configurar(SIG_LINK_PED_REQ, true);
	ret |= hal_configurar(SIG_LINK_PED_GO, false);
	ret |= hal_configurar(SIG_LINK_NOITE, false);
	ret |= hal_configurar(SIG_BTN_TRAVESSIA, false);

	st.vez = PED_VERMELHO;
	st.ped_go = false;
	st.noite = false;
	st.bloqueado = false;
	st.req = false;

	k_thread_create(&th_entradas, stk_ped_entradas, K_THREAD_STACK_SIZEOF(stk_ped_entradas),
			thread_entradas, NULL, NULL, NULL, PRIO_ENTRADAS, 0, K_NO_WAIT);
	k_thread_create(&th_verde, stk_ped_verde, K_THREAD_STACK_SIZEOF(stk_ped_verde),
			thread_verde, NULL, NULL, NULL, PRIO_LED, 0, K_NO_WAIT);
	k_thread_create(&th_vermelho, stk_ped_vermelho, K_THREAD_STACK_SIZEOF(stk_ped_vermelho),
			thread_vermelho, NULL, NULL, NULL, PRIO_LED, 0, K_NO_WAIT);

	k_thread_name_set(&th_entradas, "ped_entradas");
	k_thread_name_set(&th_verde, "ped_verde");
	k_thread_name_set(&th_vermelho, "ped_vermelho");

	return ret;
}

ped_cor_t pedestres_vez(void)
{
	k_mutex_lock(&mtx, K_FOREVER);
	ped_cor_t v = st.vez;
	k_mutex_unlock(&mtx);
	return v;
}

bool pedestres_bloqueado(void)
{
	k_mutex_lock(&mtx, K_FOREVER);
	bool b = st.bloqueado;
	k_mutex_unlock(&mtx);
	return b;
}
