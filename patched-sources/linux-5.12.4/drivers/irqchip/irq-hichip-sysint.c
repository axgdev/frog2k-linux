// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hichip HC15xx/HC16xx cascaded system interrupt controller.
 *
 * The register layout and interrupt numbering match the vendor HC Linux
 * driver.  This irqchip form keeps the SF2000 target on the generic MIPS
 * platform while still allowing peripherals to use their SoC IRQ numbers.
 */

#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>

#define REG_POLARITY_INDEX 0
#define REG_STATUS_INDEX 1
#define REG_ENABLE_INDEX 2
#define REG_MBOX_STATUS_INDEX 3
#define HC_SYSINT_BANK_BITS 32

struct hichip_sysint {
	u32 max_irqs;
	void __iomem *polarity_regs;
	void __iomem *status_regs;
	void __iomem *enable_regs;
	void __iomem *mbox_status_regs;
	struct irq_domain *domain;
};

static void hichip_sysint_enable(struct irq_data *data)
{
	struct hichip_sysint *intc = irq_data_get_irq_chip_data(data);
	irq_hw_number_t hwirq = data->hwirq;
	u32 bit = hwirq & 0x1f;
	u32 bank = hwirq >> 5;
	u32 value;

	value = readl(intc->enable_regs + bank * sizeof(u32));
	value |= BIT(bit);
	writel(value, intc->enable_regs + bank * sizeof(u32));
}

static void hichip_sysint_disable(struct irq_data *data)
{
	struct hichip_sysint *intc = irq_data_get_irq_chip_data(data);
	irq_hw_number_t hwirq = data->hwirq;
	u32 bit = hwirq & 0x1f;
	u32 bank = hwirq >> 5;
	u32 value;

	value = readl(intc->enable_regs + bank * sizeof(u32));
	value &= ~BIT(bit);
	writel(value, intc->enable_regs + bank * sizeof(u32));
}

static struct irq_chip hichip_sysint_chip = {
	.name = "hichip-sysint",
	.irq_mask = hichip_sysint_disable,
	.irq_unmask = hichip_sysint_enable,
	.irq_enable = hichip_sysint_enable,
	.irq_disable = hichip_sysint_disable,
	.irq_shutdown = hichip_sysint_disable,
};

static void hichip_sysint_handle(struct irq_desc *desc)
{
	struct hichip_sysint *intc = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	u32 banks = DIV_ROUND_UP(intc->max_irqs, HC_SYSINT_BANK_BITS);
	bool handled = false;
	u32 bank;

	chained_irq_enter(chip, desc);

	for (bank = 0; bank < banks; bank++) {
		u32 status;
		u32 enable;
		u32 pending;

		if (bank < 2) {
			status = readl(intc->status_regs + bank * sizeof(u32));
		} else if (intc->mbox_status_regs &&
			   (readl(intc->mbox_status_regs) & 0xff)) {
			status = BIT(2);
		} else {
			status = 0;
		}

		enable = readl(intc->enable_regs + bank * sizeof(u32));
		pending = status & enable;

		while (pending) {
			u32 bit = __ffs(pending);
			unsigned int virq = irq_find_mapping(intc->domain,
				bank * HC_SYSINT_BANK_BITS + bit);

			if (virq)
				generic_handle_irq(virq);
			pending &= ~BIT(bit);
			handled = true;
		}
	}

	if (!handled)
		handle_bad_irq(desc);

	chained_irq_exit(chip, desc);
}

static int hichip_sysint_map(struct irq_domain *domain, unsigned int irq,
		irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &hichip_sysint_chip, handle_level_irq);
	irq_set_chip_data(irq, domain->host_data);
	irq_set_noprobe(irq);
	return 0;
}

static const struct irq_domain_ops hichip_sysint_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = hichip_sysint_map,
};

static int __init hichip_sysint_init(struct device_node *node,
		struct device_node *parent)
{
	struct hichip_sysint *intc;
	unsigned int parent_irq;
	int ret;
	int i;

	if (!parent)
		return -EINVAL;

	intc = kzalloc(sizeof(*intc), GFP_KERNEL);
	if (!intc)
		return -ENOMEM;

	intc->polarity_regs = of_iomap(node, REG_POLARITY_INDEX);
	intc->status_regs = of_iomap(node, REG_STATUS_INDEX);
	intc->enable_regs = of_iomap(node, REG_ENABLE_INDEX);
	intc->mbox_status_regs = of_iomap(node, REG_MBOX_STATUS_INDEX);
	if (!intc->polarity_regs || !intc->status_regs || !intc->enable_regs) {
		ret = -ENXIO;
		goto err_unmap;
	}

	ret = of_property_read_u32(node, "max-irqs", &intc->max_irqs);
	if (ret || !intc->max_irqs) {
		ret = -EINVAL;
		goto err_unmap;
	}

	intc->domain = irq_domain_add_linear(node, intc->max_irqs,
		&hichip_sysint_domain_ops, intc);
	if (!intc->domain) {
		ret = -ENOMEM;
		goto err_unmap;
	}

	for (i = 0; i < of_irq_count(node); i++) {
		parent_irq = irq_of_parse_and_map(node, i);
		if (parent_irq)
			irq_set_chained_handler_and_data(parent_irq,
				hichip_sysint_handle, intc);
	}

	pr_info("hichip-sysint: %u SoC IRQs\n", intc->max_irqs);
	return 0;

err_unmap:
	if (intc->polarity_regs)
		iounmap(intc->polarity_regs);
	if (intc->status_regs)
		iounmap(intc->status_regs);
	if (intc->enable_regs)
		iounmap(intc->enable_regs);
	if (intc->mbox_status_regs)
		iounmap(intc->mbox_status_regs);
	kfree(intc);
	return ret;
}

IRQCHIP_DECLARE(hichip_sysint, "hichip,generic-intc", hichip_sysint_init);
