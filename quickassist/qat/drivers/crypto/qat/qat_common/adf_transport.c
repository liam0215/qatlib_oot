// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0-only)
/* Copyright(c) 2014 - 2021 Intel Corporation */
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_transport_internal.h"
#include "adf_transport_access_macros.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"

static inline u32 adf_modulo(u32 data, u32 shift)
{
	u32 div = data >> shift;
	u32 mult = div << shift;

	return data - mult;
}

static inline int adf_check_ring_alignment(u64 addr, u64 size)
{
	if (((size - 1) & addr) != 0)
		return -EFAULT;
	return 0;
}

static int adf_verify_ring_size(u32 msg_size, u32 msg_num)
{
	int i = ADF_MIN_RING_SIZE;

	for (; i <= ADF_MAX_RING_SIZE; i++)
		if ((msg_size * msg_num) == ADF_SIZE_TO_RING_SIZE_IN_BYTES(i))
			return i;

	return ADF_DEFAULT_RING_SIZE;
}

static int adf_reserve_ring(struct adf_etr_bank_data *bank, u32 ring)
{
	spin_lock(&bank->lock);
	if (bank->ring_mask & (1 << ring)) {
		spin_unlock(&bank->lock);
		return -EFAULT;
	}
	bank->ring_mask |= (1 << ring);
	spin_unlock(&bank->lock);
	return 0;
}

static void adf_unreserve_ring(struct adf_etr_bank_data *bank, u32 ring)
{
	spin_lock(&bank->lock);
	bank->ring_mask &= ~(1 << ring);
	spin_unlock(&bank->lock);
}

static void adf_enable_ring_irq(struct adf_etr_bank_data *bank, u32 ring)
{
	struct adf_hw_csr_ops *csr_ops =
			&bank->accel_dev->hw_device->csr_info.csr_ops;
	u32 enable_int_col_mask = csr_ops->get_int_col_ctl_enable_mask();

	spin_lock_bh(&bank->lock);
	bank->irq_mask |= (1 << ring);
	spin_unlock_bh(&bank->lock);
	csr_ops->write_csr_int_col_en(bank->csr_addr, bank->bank_number,
			     bank->irq_mask);
	csr_ops->write_csr_int_col_ctl(bank->csr_addr, bank->bank_number,
				       bank->irq_coalesc_timer |
					       enable_int_col_mask);
}

static void adf_disable_ring_irq(struct adf_etr_bank_data *bank, u32 ring)
{
	struct adf_hw_csr_ops *csr_ops =
			&bank->accel_dev->hw_device->csr_info.csr_ops;

	spin_lock_bh(&bank->lock);
	bank->irq_mask &= ~(1 << ring);
	spin_unlock_bh(&bank->lock);
	csr_ops->write_csr_int_col_en(bank->csr_addr, bank->bank_number,
				     bank->irq_mask);
}

int adf_send_message(struct adf_etr_ring_data *ring, u32 *msg)
{
	u32 msg_size;

	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
			&accel_dev->hw_device->csr_info.csr_ops;

	if (atomic_add_return(1, ring->inflights) > ring->max_inflights) {
		atomic_dec(ring->inflights);
		return -EAGAIN;
	}

	msg_size = ADF_MSG_SIZE_TO_BYTES(ring->msg_size);
	spin_lock_bh(&ring->lock);
	memcpy((void *)((uintptr_t)ring->base_addr + ring->tail), msg,
	       msg_size);

	ring->tail = adf_modulo(ring->tail +
				msg_size,
				ADF_RING_SIZE_MODULO(ring->ring_size));

	csr_ops->write_csr_ring_tail(ring->bank->csr_addr,
				     ring->bank->bank_number,
				     ring->ring_number,
				     ring->tail);
	ring->csr_tail_offset = ring->tail;
	spin_unlock_bh(&ring->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(adf_send_message);

int adf_handle_response(struct adf_etr_ring_data *ring, u32 quota)
{
	u32 msg_counter = 0;
	u32 *msg = (u32 *)((uintptr_t)ring->base_addr + ring->head);
	struct adf_accel_dev *accel_dev = ring->bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
				&accel_dev->hw_device->csr_info.csr_ops;

	quota = (quota == 0) ? ADF_NO_RESPONSE_QUOTA : quota;

	while ((*msg != ADF_RING_EMPTY_SIG) && (msg_counter < quota)) {
		ring->callback((u32 *)msg);
		atomic_dec(ring->inflights);
		*msg = ADF_RING_EMPTY_SIG;
		ring->head = adf_modulo(ring->head +
					ADF_MSG_SIZE_TO_BYTES(ring->msg_size),
					ADF_RING_SIZE_MODULO(ring->ring_size));
		msg_counter++;
		msg = (u32 *)((uintptr_t)ring->base_addr + ring->head);
	}
	if (msg_counter > 0) {
		csr_ops->write_csr_ring_head(ring->bank->csr_addr,
					     ring->bank->bank_number,
					     ring->ring_number,
					     ring->head);
	}
	return msg_counter;
}
EXPORT_SYMBOL_GPL(adf_handle_response);

bool adf_check_resp_ring(struct adf_etr_ring_data *ring)
{
	u32 *msg;
	u32 num_checked_msg = 0;
	u32 cur_head = ring->head;

	while (num_checked_msg < atomic_read(ring->inflights)) {
		msg = (uint32_t *)(((uintptr_t)ring->base_addr) + cur_head);

		if (*msg != ADF_RING_EMPTY_SIG)
			return false;

		cur_head = adf_modulo(ring->head +
				ADF_MSG_SIZE_TO_BYTES(ring->msg_size),
				ADF_RING_SIZE_MODULO(ring->ring_size));
		num_checked_msg++;
	}
	return true;
}
EXPORT_SYMBOL_GPL(adf_check_resp_ring);

int adf_poll_bank(u32 accel_id, u32 bank_num, u32 quota)
{
	struct adf_accel_dev *accel_dev;
	struct adf_etr_bank_data *bank;
	struct adf_etr_data *etr_data;
	struct adf_etr_ring_data *ring;
	struct adf_hw_device_data *hw_data;
	struct adf_hw_csr_ops *csr_ops;
	int num_resp;
	u32 rings_not_empty;
	u32 ring_num;
	u32 num_rings_per_bank;
	u32 resp_total = 0;

	/* Find the accel device associated with the accelId
	 * passed in.
	 */
	accel_dev = adf_devmgr_get_dev_by_id(accel_id);
	if (!accel_dev) {
		dev_err(&GET_DEV(accel_dev), "There is no accel device"
				"associated with this accel id\n");
		return -EINVAL;
	}

	etr_data = accel_dev->transport;
	bank = &etr_data->banks[bank_num];
	hw_data = accel_dev->hw_device;
	csr_ops = &hw_data->csr_info.csr_ops;
	spin_lock(&bank->lock);

	/* Read the ring status CSR to determine which rings are empty. */
	rings_not_empty = csr_ops->read_csr_e_stat(bank->csr_addr, bank->bank_number);
	/* Complement to find which rings have data to be processed. */
	rings_not_empty = (~rings_not_empty) & bank->ring_mask;

	/* Return RETRY if the bank polling rings
	 * are all empty.
	 */
	if (!(rings_not_empty & bank->ring_mask)) {
		goto err;
	}

	/*
	 * Loop over all rings within this bank.
	 * The ring structure is global to all
	 * rings hence while we loop over all rings in the
	 * bank we use ring_number to get the global ring.
	 */
	num_rings_per_bank = accel_dev->hw_device->num_rings_per_bank;
	for (ring_num = 0; ring_num < num_rings_per_bank; ring_num++) {
		ring = &bank->rings[ring_num];
		/* If this ring has not initialized move to next ring. */
		if (!ring->base_addr)
			continue;

		/* And with polling ring mask.
		 * If the there is no data on this ring
		 * move to the next one.
		 */
		if (!(rings_not_empty & (1 << ring->ring_number)))
			continue;

		/* Poll the ring. */
		num_resp = adf_handle_response(ring, quota);
		resp_total += num_resp;
	}

	spin_unlock(&bank->lock);
	/* Return SUCCESS if there's any response message
	 * returned.
	 */
	return resp_total ? 0 : -EAGAIN;
err:
	spin_unlock(&bank->lock);
	return -EAGAIN;
}
EXPORT_SYMBOL_GPL(adf_poll_bank);

int adf_poll_all_banks(u32 accel_id, u32 quota)
{
	int status = -EAGAIN;
	struct adf_accel_dev *accel_dev;
	struct adf_etr_bank_data *bank;
	struct adf_etr_data *etr_data;
	u32 bank_num;
	u32 stat_total = 0;

	/* Find the accel device associated with the accelId
	 * passed in.
	 */
	accel_dev = adf_devmgr_get_dev_by_id(accel_id);
	if (!accel_dev) {
		dev_err(&GET_DEV(accel_dev), "There is no accel device"
				"associated with this accel id\n");
		return -EINVAL;
	}

	/* Loop over banks and call adf_poll_bank */
	etr_data = accel_dev->transport;
	for (bank_num = 0; bank_num < GET_MAX_BANKS(accel_dev); bank_num++) {
		bank = &etr_data->banks[bank_num];
		/* if there are no polling rings on this bank
		 * continue to the next bank number.
		 */
		if (!bank->ring_mask)
			continue;
		status = adf_poll_bank(accel_id, bank_num, quota);
		/* The successful status should be AGAIN or 0 */
		if (!status)
			stat_total++;
		else if (status != -EAGAIN)
			return status;
	}

	/* Return SUCCESS if adf_poll_bank returned SUCCESS
	 * at any stage. adf_poll_bank cannot
	 * return fail in the above case.
	 */
	if (stat_total)
		return 0;

	return -EAGAIN;
}
EXPORT_SYMBOL_GPL(adf_poll_all_banks);

static void adf_configure_tx_ring(struct adf_etr_ring_data *ring)
{
	struct adf_accel_dev *accel_dev = ring->bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
				&accel_dev->hw_device->csr_info.csr_ops;
	u32 ring_config = csr_ops->build_ring_config(ring->ring_size);

	csr_ops->write_csr_ring_config(ring->bank->csr_addr,
				       ring->bank->bank_number,
				       ring->ring_number,
				       ring_config);

}

static void adf_configure_rx_ring(struct adf_etr_ring_data *ring)
{
	struct adf_accel_dev *accel_dev = ring->bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
				&accel_dev->hw_device->csr_info.csr_ops;

	u32 ring_config =
		csr_ops->build_resp_ring_config(ring->ring_size,
						ADF_RING_NEAR_WATERMARK_512,
						ADF_RING_NEAR_WATERMARK_0);

	csr_ops->write_csr_ring_config(ring->bank->csr_addr,
				      ring->bank->bank_number,
				      ring->ring_number, ring_config);
}

static void adf_configure_ring(struct adf_etr_ring_data *ring)
{
	u64 ring_base;
	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_hw_csr_ops *csr_ops =
				&accel_dev->hw_device->csr_info.csr_ops;

	if (hw_data->tx_rings_mask & (1 << ring->ring_number))
		adf_configure_tx_ring(ring);
	else
		adf_configure_rx_ring(ring);

	ring_base = csr_ops->build_ring_base_addr(ring->dma_addr,
						  ring->ring_size);
	csr_ops->write_csr_ring_base(ring->bank->csr_addr,
				     ring->bank->bank_number,
				     ring->ring_number, ring_base);
}

static int adf_reinit_ring(struct adf_etr_ring_data *ring)
{
	u32 ring_size_bytes =
		ADF_RING_SIZE_BYTES_MIN(ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size));

	if (!ring->base_addr)
		return -ENOMEM;

	memset(ring->base_addr, ADF_RING_EMPTY_SIG & 0xFF, ring_size_bytes);

	spin_lock_init(&ring->lock);

	adf_configure_ring(ring);

	return 0;
}

static int adf_init_ring(struct adf_etr_ring_data *ring)
{
	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	u32 ring_size_bytes =
			ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size);
	ring_size_bytes = ADF_RING_SIZE_BYTES_MIN(ring_size_bytes);
	ring->base_addr = dma_alloc_coherent(&GET_DEV(accel_dev),
					     ring_size_bytes, &ring->dma_addr,
					     GFP_KERNEL);
	if (!ring->base_addr)
		return -ENOMEM;

	memset(ring->base_addr, ADF_RING_EMPTY_SIG & 0xFF, ring_size_bytes);
	/* The base_addr has to be aligned to the size of the buffer */
	if (adf_check_ring_alignment(ring->dma_addr, ring_size_bytes)) {
		dev_err(&GET_DEV(accel_dev), "Ring address not aligned\n");
		dma_free_coherent(&GET_DEV(accel_dev), ring_size_bytes,
				  ring->base_addr, ring->dma_addr);
		return -EFAULT;
	}

	spin_lock_init(&ring->lock);

	adf_configure_ring(ring);

	return 0;
}

static void adf_cleanup_ring(struct adf_etr_ring_data *ring)
{
	u32 ring_size_bytes =
			ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size);
	ring_size_bytes = ADF_RING_SIZE_BYTES_MIN(ring_size_bytes);

	if (ring->base_addr) {
		memset(ring->base_addr, 0x7F, ring_size_bytes);
		dma_free_coherent(&GET_DEV(ring->bank->accel_dev),
				  ring_size_bytes, ring->base_addr,
				  ring->dma_addr);
	}
}

static int adf_populate_ring(struct adf_accel_dev *accel_dev,
			     const char *section,
			     u32 bank_num, u32 num_msgs,
			     u32 msg_size, const char *ring_name,
			     adf_callback_fn callback,
			     struct adf_etr_ring_data **ring_ptr)

{
	struct adf_etr_bank_data *bank;
	struct adf_etr_ring_data *ring;
	struct adf_etr_data *transport_data = accel_dev->transport;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u32 ring_num;
	u8 num_rings_per_bank = accel_dev->hw_device->num_rings_per_bank;

	if (bank_num >= GET_MAX_BANKS(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Invalid bank number\n");
		return -EFAULT;
	}
	if (msg_size > ADF_MSG_SIZE_TO_BYTES(ADF_MAX_MSG_SIZE)) {
		dev_err(&GET_DEV(accel_dev), "Invalid msg size\n");
		return -EFAULT;
	}
	if (ADF_MAX_INFLIGHTS(adf_verify_ring_size(msg_size, num_msgs),
			      ADF_BYTES_TO_MSG_SIZE(msg_size)) < 2) {
		dev_err(&GET_DEV(accel_dev),
			"Invalid ring size for given msg size\n");
		return -EFAULT;
	}
	if (adf_cfg_get_param_value(accel_dev, section, ring_name, val)) {
		dev_err(&GET_DEV(accel_dev), "Section %s, no such entry : %s\n",
			section, ring_name);
		return -EFAULT;
	}
	if (kstrtouint(val, 10, &ring_num)) {
		dev_err(&GET_DEV(accel_dev), "Can't get ring number\n");
		return -EFAULT;
	}
	if (ring_num >= num_rings_per_bank) {
		dev_err(&GET_DEV(accel_dev), "Invalid ring number\n");
		return -EFAULT;
	}

	bank = &transport_data->banks[bank_num];
	if (adf_reserve_ring(bank, ring_num)) {
		dev_err(&GET_DEV(accel_dev), "Ring %d, %s already exists.\n",
			ring_num, ring_name);
		return -EFAULT;
	}
	ring = &bank->rings[ring_num];
	ring->ring_number = ring_num;
	ring->bank = bank;
	ring->callback = callback;
	ring->msg_size = (u8)ADF_BYTES_TO_MSG_SIZE(msg_size);
	ring->ring_size = (u8)adf_verify_ring_size(msg_size, num_msgs);
	ring->max_inflights = ADF_MAX_INFLIGHTS(ring->ring_size, ring->msg_size);
	ring->head = 0;
	ring->tail = 0;
	ring->csr_tail_offset = 0;
	atomic_set(ring->inflights, 0);

	*ring_ptr = ring;
	return 0;
}

int adf_create_ring(struct adf_accel_dev *accel_dev, const char *section,
		    u32 bank_num, u32 num_msgs,
		    u32 msg_size, const char *ring_name,
		    adf_callback_fn callback, int poll_mode,
		    struct adf_etr_ring_data **ring_ptr)
{
	struct adf_etr_ring_data *ring;
	int ret;

	ret = adf_populate_ring(accel_dev, section, bank_num, num_msgs,
				msg_size, ring_name, callback, &ring);
	if (ret)
		return ret;

	ret = adf_init_ring(ring);
	if (ret)
		goto err;

	/* Enable HW arbitration for the given ring */
	adf_update_ring_arb(ring);

	if (adf_ring_debugfs_add(ring, ring_name)) {
		dev_err(&GET_DEV(accel_dev),
			"Couldn't add ring debugfs entry\n");
		ret = -EFAULT;
		goto err;
	}

	/* Enable interrupts if needed */
	if (callback && !poll_mode)
		adf_enable_ring_irq(ring->bank, ring->ring_number);
	*ring_ptr = ring;
	return 0;
err:
	adf_cleanup_ring(ring);
	adf_unreserve_ring(ring->bank, ring->ring_number);
	adf_update_ring_arb(ring);
	return ret;
}
EXPORT_SYMBOL_GPL(adf_create_ring);

int adf_recreate_ring(struct adf_accel_dev *accel_dev, const char *section,
		      u32 bank_num, u32 num_msgs,
		      u32 msg_size, const char *ring_name,
		      adf_callback_fn callback, int poll_mode,
		      struct adf_etr_ring_data **ring_ptr)
{
	struct adf_etr_ring_data *ring;
	int ret;

	ret = adf_populate_ring(accel_dev, section, bank_num, num_msgs,
				msg_size, ring_name, callback, &ring);
	if (ret)
		return ret;

	ret = adf_reinit_ring(ring);
	if (ret)
		goto err;

	/* Enable HW arbitration for the given ring */
	adf_update_ring_arb(ring);

	if (adf_ring_debugfs_add(ring, ring_name)) {
		dev_err(&GET_DEV(accel_dev),
			"Couldn't add ring debugfs entry\n");
		ret = -EFAULT;
		goto err;
	}

	/* Enable interrupts if needed */
	if (callback && !poll_mode)
		adf_enable_ring_irq(ring->bank, ring->ring_number);
	*ring_ptr = ring;
	return 0;
err:
	adf_cleanup_ring(ring);
	adf_unreserve_ring(ring->bank, ring->ring_number);
	adf_update_ring_arb(ring);
	return ret;
}
EXPORT_SYMBOL_GPL(adf_recreate_ring);

void adf_reset_ring(struct adf_etr_ring_data *ring)
{
	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
		&accel_dev->hw_device->csr_info.csr_ops;

	/* Disable interrupts for the given ring */
	adf_disable_ring_irq(bank, ring->ring_number);

	/* Clear the ring's configuration */
	csr_ops->write_csr_ring_config(bank->csr_addr, bank->bank_number,
			ring->ring_number, 0);
	csr_ops->write_csr_ring_base(bank->csr_addr, bank->bank_number,
			ring->ring_number, 0);

	adf_ring_debugfs_rm(ring);
	adf_unreserve_ring(bank, ring->ring_number);
	/* Disable HW arbitration for the given ring */
	adf_update_ring_arb(ring);
}
EXPORT_SYMBOL_GPL(adf_reset_ring);

void adf_remove_ring(struct adf_etr_ring_data *ring)
{
	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_csr_ops *csr_ops =
				&accel_dev->hw_device->csr_info.csr_ops;

	/* Disable interrupts for the given ring */
	adf_disable_ring_irq(bank, ring->ring_number);

	/* Clear PCI config space */

	csr_ops->write_csr_ring_config(bank->csr_addr, bank->bank_number,
				       ring->ring_number, 0);
	csr_ops->write_csr_ring_base(bank->csr_addr, bank->bank_number,
				     ring->ring_number, 0);
	adf_ring_debugfs_rm(ring);
	adf_unreserve_ring(bank, ring->ring_number);
	/* Disable HW arbitration for the given ring */
	adf_update_ring_arb(ring);
	adf_cleanup_ring(ring);
}
EXPORT_SYMBOL_GPL(adf_remove_ring);

void adf_ring_response_handler(struct adf_etr_bank_data *bank)
{
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_hw_csr_ops *csr_ops = &hw_data->csr_info.csr_ops;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	unsigned long empty_rings;
	int i;

	empty_rings = csr_ops->read_csr_e_stat(bank->csr_addr,
					       bank->bank_number);
	empty_rings = ~empty_rings & bank->irq_mask;

	for_each_set_bit(i, &empty_rings, num_rings_per_bank)
		adf_handle_response(&bank->rings[i], ADF_RESPONSE_IRQ_HANDLE);
}

void adf_response_handler_wq(struct work_struct *data)
{
	struct adf_etr_bank_data *bank =
		container_of(data, struct adf_etr_bank_data, resp_handler_wq);
	struct adf_hw_csr_info *csr_info =
				&bank->accel_dev->hw_device->csr_info;
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;
	unsigned long flags;

	/* Handle all the responses and reenable IRQs */
	adf_ring_response_handler(bank);

	if (bank->accel_dev->is_vf) {
		spin_lock_irqsave(&bank->accel_dev->vf2pf_csr_lock, flags);
		csr_ops->write_csr_int_flag_and_col(bank->csr_addr,
						    bank->bank_number,
						    bank->irq_mask);
		spin_unlock_irqrestore(&bank->accel_dev->vf2pf_csr_lock, flags);
	} else {
		csr_ops->write_csr_int_flag_and_col(bank->csr_addr,
						    bank->bank_number,
						    bank->irq_mask);
	}
}

static inline int adf_get_cfg_int(struct adf_accel_dev *accel_dev,
				  const char *section, const char *format,
				  u32 key, u32 *value)
{
	char key_buf[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val_buf[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	snprintf(key_buf, ADF_CFG_MAX_KEY_LEN_IN_BYTES, format, key);

	if (adf_cfg_get_param_value(accel_dev, section, key_buf, val_buf))
		return -EFAULT;

	if (kstrtouint(val_buf, 10, value))
		return -EFAULT;
	return 0;
}

static void adf_get_coalesc_timer(struct adf_etr_bank_data *bank,
				  const char *section,
				  u32 bank_num_in_accel)
{
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 coalesc_timer = hw_data->coalescing_def_time;
	u32 clock_speed = 0;
	u32 coalesc_factor = 0;

	adf_get_cfg_int(accel_dev, section,
			ADF_ETRMGR_COALESCE_TIMER_FORMAT,
			bank_num_in_accel, &coalesc_timer);

	if (hw_data->get_clock_speed) {
		clock_speed = hw_data->get_clock_speed(hw_data);
		coalesc_factor = (coalesc_timer * (clock_speed / USEC_PER_SEC)) / NSEC_PER_USEC;
		if (hw_data->coalescing_timer_div)
			bank->irq_coalesc_timer = coalesc_factor / hw_data->coalescing_timer_div;
		else
			bank->irq_coalesc_timer = coalesc_factor;
	} else {
		bank->irq_coalesc_timer = coalesc_timer;
	}

	if (bank->irq_coalesc_timer > hw_data->coalescing_max_time)
		bank->irq_coalesc_timer = hw_data->coalescing_max_time;
	else if (bank->irq_coalesc_timer < hw_data->coalescing_min_time)
		bank->irq_coalesc_timer = hw_data->coalescing_min_time;
}

static int adf_init_bank(struct adf_accel_dev *accel_dev,
			 struct adf_etr_bank_data *bank,
			 u32 bank_num, void __iomem *csr_addr)
{
	struct adf_etr_ring_data *ring;
	struct adf_etr_ring_data *tx_ring;
	u32 i, bank_irq_mask;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_hw_csr_info *csr_info = &hw_data->csr_info;
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;
	u32 coalesc_enabled = 0;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	unsigned long ring_mask;
	u32 size;
	u32 num_rings_per_int_srcsel = csr_info->num_rings_per_int_srcsel;
#ifdef QAT_UIO
	char val[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
#endif

	memset(bank, 0, sizeof(*bank));
	bank->bank_number = bank_num;
	bank->csr_addr = csr_addr;
	bank->accel_dev = accel_dev;
	spin_lock_init(&bank->lock);
#ifdef QAT_UIO
	if (accel_dev->cfg->dev &&
	    (!adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
	    ADF_SERVICES_ENABLED, val)))
		bank->type = accel_dev->cfg->dev->bundles[bank_num]->type;
	else
		bank->type = KERNEL;
#else
	bank->type = KERNEL;
#endif

	/* Allocate the rings in the bank */
	size = num_rings_per_bank * sizeof(struct adf_etr_ring_data);
	bank->rings = kzalloc_node(size, GFP_KERNEL,
				   dev_to_node(&GET_DEV(accel_dev)));
	if (!bank->rings)
		return -ENOMEM;

	/* Enable IRQ coalescing always. This will allow to use
	 * the optimised flag and coalesc register.
	 * If it is disabled in the config file just use default time value
	 */
	if ((adf_get_cfg_int(accel_dev, "Accelerator0",
			     ADF_ETRMGR_COALESCING_ENABLED_FORMAT, bank_num,
			     &coalesc_enabled) == 0) && coalesc_enabled)
		adf_get_coalesc_timer(bank, "Accelerator0", bank_num);
	else
		bank->irq_coalesc_timer = hw_data->coalescing_def_time;

	for (i = 0; i < num_rings_per_bank; i++) {
		csr_ops->write_csr_ring_config(csr_addr, bank_num, i, 0);
		csr_ops->write_csr_ring_base(csr_addr, bank_num, i, 0);

		ring = &bank->rings[i];
		if (hw_data->tx_rings_mask & (1 << i)) {
			ring->inflights =
				kzalloc_node(sizeof(atomic_t),
					     GFP_KERNEL,
					     dev_to_node(&GET_DEV(accel_dev)));
			if (!ring->inflights)
				goto err;
		} else {
			if (i < hw_data->tx_rx_gap) {
				dev_err(&GET_DEV(accel_dev),
					"Invalid tx rings mask config\n");
				goto err;
			}
			tx_ring = &bank->rings[i - hw_data->tx_rx_gap];
			ring->inflights = tx_ring->inflights;
		}
	}
	if (adf_bank_debugfs_add(bank)) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to add bank debugfs entry\n");
		goto err;
	}

	bank_irq_mask = csr_ops->get_bank_irq_mask(bank->irq_mask);
	csr_ops->write_csr_int_flag(csr_addr, bank_num, bank_irq_mask);

	for (i = 0; i < num_rings_per_bank / num_rings_per_int_srcsel; i++)
		csr_ops->write_csr_int_srcsel(csr_addr, bank_num, i,
					      csr_ops->get_src_sel_mask());

	return 0;
err:
	ring_mask = hw_data->tx_rings_mask;
	for_each_set_bit(i, &ring_mask, num_rings_per_bank) {
		ring = &bank->rings[i];
		kfree(ring->inflights);
		ring->inflights = NULL;
	}
	kfree(bank->rings);
	return -ENOMEM;
}

static int adf_create_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data;
	u32 size;
	u32 num_banks = 0;
	int ret = 0;

	etr_data = kzalloc_node(sizeof(*etr_data), GFP_KERNEL,
				dev_to_node(&GET_DEV(accel_dev)));
	if (!etr_data)
		return -ENOMEM;

	num_banks = GET_MAX_BANKS(accel_dev);
	size = num_banks * sizeof(struct adf_etr_bank_data);
	etr_data->banks = kzalloc_node(size, GFP_KERNEL,
				       dev_to_node(&GET_DEV(accel_dev)));
	if (!etr_data->banks) {
		ret = -ENOMEM;
		goto err_bank;
	}

	accel_dev->transport = etr_data;

	/* accel_dev->debugfs_dir should always be non-NULL here */
	etr_data->debug = debugfs_create_dir("transport",
					     accel_dev->debugfs_dir);
	if (!etr_data->debug) {
		dev_err(&GET_DEV(accel_dev),
			"Unable to create transport debugfs entry\n");
		ret = -ENOENT;
		goto err_bank_debug;
	}
	return 0;

err_bank_debug:
	kfree(etr_data->banks);
err_bank:
	kfree(etr_data);
	accel_dev->transport = NULL;
	return ret;
}

static int adf_init_banks(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	void __iomem *csr_addr;
	u32 num_banks = 0;
	int i, ret = 0;

	i = hw_data->get_etr_bar_id(hw_data);
	csr_addr = accel_dev->accel_pci_dev.pci_bars[i].virt_addr;
	etr_data = accel_dev->transport;
	num_banks = GET_MAX_BANKS(accel_dev);

	for (i = 0; i < num_banks; i++) {
		ret = adf_init_bank(accel_dev, &etr_data->banks[i], i,
				    csr_addr);
		if (ret)
			return ret;
	}
	return ret;
}

/**
 * adf_init_etr_data() - Initialize transport rings for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function is the initializes the communications channels (rings) to the
 * acceleration device accel_dev.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_init_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;
	int ret = 0;

	if (!adf_devmgr_in_reset(accel_dev) || !(accel_dev->is_vf))
		ret = adf_create_etr_data(accel_dev);
	if (ret)
		goto err_create;

	ret = adf_init_banks(accel_dev);
	if (ret)
		goto err_bank_all;

	return 0;

err_bank_all:
	debugfs_remove(etr_data->debug);
	kfree(etr_data->banks);
	kfree(etr_data);
	accel_dev->transport = NULL;
err_create:
	return ret;
}
EXPORT_SYMBOL_GPL(adf_init_etr_data);

static void cleanup_bank(struct adf_etr_bank_data *bank)
{
	u32 i;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;

	for (i = 0; i < num_rings_per_bank; i++) {
		struct adf_etr_ring_data *ring = &bank->rings[i];

		if (bank->ring_mask & (1 << i))
			adf_cleanup_ring(ring);

		if (hw_data->tx_rings_mask & (1 << i)) {
			kfree(ring->inflights);
			ring->inflights = NULL;
		}
	}
	kfree(bank->rings);
	adf_bank_debugfs_rm(bank);
	memset(bank, 0, sizeof(*bank));
}

static void adf_cleanup_etr_handles(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;
	u32 i, num_banks = GET_MAX_BANKS(accel_dev);

	if (!etr_data->banks->rings)
		return;

	for (i = 0; i < num_banks; i++)
		cleanup_bank(&etr_data->banks[i]);
}

static void adf_destroy_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;

	debugfs_remove(etr_data->debug);
	kfree(etr_data->banks->rings);
	kfree(etr_data->banks);
	kfree(etr_data);
	accel_dev->transport = NULL;
}

/**
 * adf_cleanup_etr_data() - Clear transport rings for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function is the clears the communications channels (rings) of the
 * acceleration device accel_dev.
 * To be used by QAT device specific drivers.
 *
 * Return: void
 */
void adf_cleanup_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;

	if (!etr_data)
		return;

	adf_cleanup_etr_handles(accel_dev);

	if (!adf_devmgr_in_reset(accel_dev) || !accel_dev->is_vf)
		adf_destroy_etr_data(accel_dev);
}
EXPORT_SYMBOL_GPL(adf_cleanup_etr_data);
