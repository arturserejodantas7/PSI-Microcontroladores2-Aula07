/*
 * hal_gpio.c — HAL real para a FRDM-KL25Z.
 *
 * Os pinos vêm do nó "zephyr,user" do overlay de cada aplicação
 * (app_veiculos/boards/frdm_kl25z.overlay e
 *  app_pedestres/boards/frdm_kl25z.overlay).
 * Sinais que não existem no overlay daquela placa ficam sem pino
 * (port == NULL) e são ignorados.
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "semaforo_hal.h"

#define ZU DT_PATH(zephyr_user)

static const struct gpio_dt_spec pinos[SIG_COUNT] = {
	[SIG_VEH_VERDE] = GPIO_DT_SPEC_GET_OR(ZU, veh_verde_gpios, {0}),
	[SIG_VEH_AMARELO] = GPIO_DT_SPEC_GET_OR(ZU, veh_amarelo_gpios, {0}),
	[SIG_VEH_VERMELHO] = GPIO_DT_SPEC_GET_OR(ZU, veh_vermelho_gpios, {0}),
	[SIG_PED_VERDE] = GPIO_DT_SPEC_GET_OR(ZU, ped_verde_gpios, {0}),
	[SIG_PED_VERMELHO] = GPIO_DT_SPEC_GET_OR(ZU, ped_vermelho_gpios, {0}),
	[SIG_LINK_PED_GO] = GPIO_DT_SPEC_GET_OR(ZU, link_ped_go_gpios, {0}),
	[SIG_LINK_NOITE] = GPIO_DT_SPEC_GET_OR(ZU, link_noite_gpios, {0}),
	[SIG_LINK_PED_REQ] = GPIO_DT_SPEC_GET_OR(ZU, link_ped_req_gpios, {0}),
	[SIG_BTN_NOITE] = GPIO_DT_SPEC_GET_OR(ZU, btn_noite_gpios, {0}),
	[SIG_BTN_TRAVESSIA] = GPIO_DT_SPEC_GET_OR(ZU, btn_travessia_gpios, {0}),
};

int hal_configurar(sinal_t s, bool saida)
{
	if ((unsigned)s >= SIG_COUNT || pinos[s].port == NULL) {
		printk("HAL: sinal %s sem pino no devicetree\n", hal_nome(s));
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&pinos[s])) {
		printk("HAL: GPIO de %s nao esta pronto\n", hal_nome(s));
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&pinos[s], saida ? GPIO_OUTPUT_INACTIVE : GPIO_INPUT);

	if (ret == 0 && saida) {
		hal_comum_registrar(s, false);
	}
	return ret;
}

void hal_escrever(sinal_t s, bool valor)
{
	if ((unsigned)s >= SIG_COUNT || pinos[s].port == NULL) {
		return;
	}
	(void)gpio_pin_set_dt(&pinos[s], valor ? 1 : 0);
	hal_comum_registrar(s, valor);
}

bool hal_ler(sinal_t s)
{
	if ((unsigned)s >= SIG_COUNT || pinos[s].port == NULL) {
		return false;
	}
	return gpio_pin_get_dt(&pinos[s]) > 0;
}
