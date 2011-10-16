#include <common.h>
#include <usbdevice.h>
#include <asm/arch/s3c24x0_cpu.h>

void udc_ctrl(enum usbd_event event, int param)
{
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();

	switch (event) {
	case UDC_CTRL_PULLUP_ENABLE:
#if defined(CONFIG_SMDK2440)
		if (param)
			gpio->GPCDAT |= (1 << 5);	/* GPC5 */
		else
			gpio->GPCDAT &= ~(1 << 5);	/* GPC5 */
#endif
		break;
	default:
		break;
	}
}

