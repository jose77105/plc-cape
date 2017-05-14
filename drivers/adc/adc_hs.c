/*
 * ADC MFD HighSpeed driver
 *
 * Copyright (C) 2016-2017
 *	Based on the standard driver "TI ADC MFD driver" provided by Texas Instruments Incorporated
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// One second contention between warnings to avoid CPU overflow and hanging
#define WARNING_CONTENTION_US 1000000

// [PLC] Enable debug messages with 'dev_dbg'
// #define DEBUG 1

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iio/iio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/mfd/ti_am335x_tscadc.h>
#include <linux/platform_data/ti_am335x_adc.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/wait.h>
#include <linux/sched.h>

// [PLC] Includes required to add chard-device access
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/compat.h>

// asm/uaccess.h: copy-to-user
#include <asm/uaccess.h>

// [PLC] Globals
static int dma_mode = 0;
static const int continuous_transfer = 1;
static int continuous_transfer_started = 0;
static int abort_continuous_transfer = 0;
static const int iio_std_exchange = 0;
static u64 next_underflow_warning_stamp = 0;
static u64 next_overrun_warning_stamp = 0;

#define DMA_SAMPLES_COUNT (2048+FIFO1_THRESHOLD)

// [PLC] Redirection to cloned and customized versions of the exported functions
void am335x_tsc_se_update_plc(struct ti_tscadc_dev *tsadc);
void am335x_tsc_se_set_plc(struct ti_tscadc_dev *tsadc, u32 val);
void am335x_tsc_se_clr_plc(struct ti_tscadc_dev *tsadc, u32 val);

struct tiadc_device {
	struct ti_tscadc_dev *mfd_tscadc;
	int channels;
	struct iio_map *map;
	u8 channel_line[8];
	u8 channel_step[8];
	struct work_struct poll_work;
	wait_queue_head_t wq_data_avail;
	bool data_avail;
	u32 *inputbuffer;
	int sample_count;
	int irq;
	// [PLC] Buffer for fast DMA capturing
	u16 *dma_samples;
	u16 *dma_samples_end;
	u16 *dma_samples_cur_write;
	u16 *dma_samples_cur_read;
	u64 start_adc_buffering_jiffies;
};

struct iio_dev		*indio_dev_singleton;

static unsigned int tiadc_readl(struct tiadc_device *adc, unsigned int reg)
{
	return readl(adc->mfd_tscadc->tscadc_base + reg);
}

static void tiadc_writel(struct tiadc_device *adc, unsigned int reg,
					unsigned int val)
{
	writel(val, adc->mfd_tscadc->tscadc_base + reg);
}

static u32 get_adc_step_mask(struct tiadc_device *adc_dev)
{
	u32 step_en;

	step_en = ((1 << adc_dev->channels) - 1);
	step_en <<= TOTAL_STEPS - adc_dev->channels + 1;
	// [PLC] Activate only 1 step (AIN0) to maximize speed (0x200)
	step_en = 0x200;
	return step_en;
}

static void tiadc_step_config(struct iio_dev *indio_dev)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	unsigned int stepconfig;
	int i, steps, chan;

	/*
	 * There are 16 configurable steps and 8 analog input
	 * lines available which are shared between Touchscreen and ADC.
	 * Steps backwards i.e. from 16 towards 0 are used by ADC
	 * depending on number of input lines needed.
	 * Channel would represent which analog input
	 * needs to be given to ADC to digitalize data.
	 */
	steps = TOTAL_STEPS - adc_dev->channels;
	// [PLC] The standard version comes with a default value of 'STEPCONFIG_AVG_16' for better
	//	averaging. Changing to 'STEPCONFIG_AVG(0)' to maximize capture speed
	if (iio_buffer_enabled(indio_dev))
		stepconfig = STEPCONFIG_AVG(0) | STEPCONFIG_FIFO1
					| STEPCONFIG_MODE_SWCNT;
	else
		stepconfig = STEPCONFIG_AVG(0) | STEPCONFIG_FIFO1;

	for (i = 0; i < adc_dev->channels; i++) {
		chan = adc_dev->channel_line[i];
		tiadc_writel(adc_dev, REG_STEPCONFIG(steps),
				stepconfig | STEPCONFIG_INP(chan));
		// [PLC] The standard version came with a default value of 'STEPCONFIG_OPENDLY' = 
		//	'STEPDELAY_OPEN(0x098)'. Set to 'STEPDELAY_OPEN(0x0)' to maximize capturing speed
		//	tiadc_writel(adc_dev, REG_STEPDELAY(steps), STEPCONFIG_OPENDLY);
		tiadc_writel(adc_dev, REG_STEPDELAY(steps), STEPDELAY_OPEN(0));
		adc_dev->channel_step[i] = steps;
		steps++;
	}
}

static irqreturn_t tiadc_irq(int irq, void *private)
{
	struct iio_dev *idev = private;
	struct tiadc_device *adc_dev = iio_priv(idev);
	unsigned int status, config;
	status = tiadc_readl(adc_dev, REG_IRQSTATUS);

	/* FIFO Overrun. Clear flag. Disable/Enable ADC to recover */
	if (status & IRQENB_FIFO1OVRRUN) {
		u64 stamp = jiffies;
		if (stamp > next_overrun_warning_stamp) {
			next_overrun_warning_stamp = stamp+usecs_to_jiffies(WARNING_CONTENTION_US);
			printk(KERN_INFO "adc_hs_plc: FIFO1 Overrun!\n");
		}
		config = tiadc_readl(adc_dev, REG_CTRL);
		config &= ~(CNTRLREG_TSCSSENB);
		tiadc_writel(adc_dev, REG_CTRL, config);
		tiadc_writel(adc_dev, REG_IRQSTATUS, IRQENB_FIFO1OVRRUN |
				IRQENB_FIFO1UNDRFLW | IRQENB_FIFO1THRES);
		tiadc_writel(adc_dev, REG_CTRL, (config | CNTRLREG_TSCSSENB));
		return IRQ_HANDLED;
	} else if (status & IRQENB_FIFO1THRES) {
		int fifo1count;
		/* Wake adc_work that pushes FIFO data to iio buffer */
		// tiadc_writel(adc_dev, REG_IRQCLR, IRQENB_FIFO1THRES);
		fifo1count = tiadc_readl(adc_dev, REG_FIFO1CNT);
		for (; fifo1count > 0; fifo1count--) {
			*adc_dev->dma_samples_cur_write++ = tiadc_readl(adc_dev, REG_FIFO1)
					& FIFOREAD_DATA_MASK;
			// Circular buffer overflow
			if (adc_dev->dma_samples_cur_write == adc_dev->dma_samples_end)
				adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
			if (adc_dev->dma_samples_cur_write == adc_dev->dma_samples_cur_read)
			{
				u64 stamp = jiffies;
				if (stamp > next_underflow_warning_stamp) {
					next_underflow_warning_stamp = stamp+usecs_to_jiffies(WARNING_CONTENTION_US);
					printk(KERN_INFO "adc_hs_plc: Read underflow!\n");
				}
			}
		}
		tiadc_writel(adc_dev, REG_IRQSTATUS, IRQENB_FIFO1THRES);
		// tiadc_writel(adc_dev, REG_IRQENABLE, IRQENB_FIFO1THRES);
		adc_dev->data_avail = 1;
		wake_up_interruptible(&adc_dev->wq_data_avail);
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

static irqreturn_t tiadc_trigger_h(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	unsigned int config;

	adc_dev->start_adc_buffering_jiffies = jiffies;
	dev_dbg(&indio_dev->dev, "adc_hs_plc: tiadc_trigger_h: %u\n",
			(u32)adc_dev->start_adc_buffering_jiffies);
	if (continuous_transfer) abort_continuous_transfer = 0;
	adc_dev->data_avail = 0;
	continuous_transfer_started = 1;
	adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
	adc_dev->dma_samples_cur_read = adc_dev->dma_samples;

	if (iio_std_exchange) {
		schedule_work(&adc_dev->poll_work);
		dev_dbg(&indio_dev->dev, "adc_hs_plc: work_scheduled\n");
	}

	config = tiadc_readl(adc_dev, REG_CTRL);
	tiadc_writel(adc_dev, REG_CTRL,	config & ~CNTRLREG_TSCSSENB);
	tiadc_writel(adc_dev, REG_CTRL,	config |  CNTRLREG_TSCSSENB);

	tiadc_writel(adc_dev,  REG_IRQSTATUS, IRQENB_FIFO1THRES |
			 IRQENB_FIFO1OVRRUN | IRQENB_FIFO1UNDRFLW);
	tiadc_writel(adc_dev,  REG_IRQENABLE, IRQENB_FIFO1THRES
				| IRQENB_FIFO1OVRRUN);

	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int tiadc_buffer_preenable(struct iio_dev *indio_dev)
{
	return iio_sw_buffer_preenable(indio_dev);
}

static int tiadc_buffer_postenable(struct iio_dev *indio_dev)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	struct iio_buffer *buffer = indio_dev->buffer;
	unsigned int enb, stepnum;
	u8 bit;

	tiadc_step_config(indio_dev);
	tiadc_writel(adc_dev, REG_SE, 0x00);
	for_each_set_bit(bit, buffer->scan_mask,
			adc_dev->channels) {
		struct iio_chan_spec const *chan = indio_dev->channels + bit;
		/*
		 * There are a total of 16 steps available
		 * that are shared between ADC and touchscreen.
		 * We start configuring from step 16 to 0 incase of
		 * ADC. Hence the relation between input channel
		 * and step for ADC would be as below.
		 */
		stepnum = chan->channel + 9;
		enb = tiadc_readl(adc_dev, REG_SE);
		enb |= (1 << stepnum);
		tiadc_writel(adc_dev, REG_SE, enb);
	}

	return iio_triggered_buffer_postenable(indio_dev);
}

static int tiadc_buffer_predisable(struct iio_dev *indio_dev)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	int fifo1count, i, read, config;

	config = tiadc_readl(adc_dev, REG_CTRL);
	config &= ~(CNTRLREG_TSCSSENB);
	tiadc_writel(adc_dev, REG_CTRL, config);
	tiadc_writel(adc_dev, REG_IRQCLR, (IRQENB_FIFO1THRES |
				IRQENB_FIFO1OVRRUN | IRQENB_FIFO1UNDRFLW));
	tiadc_writel(adc_dev, REG_SE, STPENB_STEPENB_TC);

	/* Flush FIFO of any leftover data */
	fifo1count = tiadc_readl(adc_dev, REG_FIFO1CNT);
	for (i = 0; i < fifo1count; i++)
		read = tiadc_readl(adc_dev, REG_FIFO1);

	return iio_triggered_buffer_predisable(indio_dev);
}

static int tiadc_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	int config;

	tiadc_step_config(indio_dev);
	config = tiadc_readl(adc_dev, REG_CTRL);
	tiadc_writel(adc_dev, REG_CTRL, (config | CNTRLREG_TSCSSENB));

	return 0;
}

static const struct iio_buffer_setup_ops tiadc_buffer_setup_ops = {
	.preenable = &tiadc_buffer_preenable,
	.postenable = &tiadc_buffer_postenable,
	.predisable = &tiadc_buffer_predisable,
	.postdisable = &tiadc_buffer_postdisable,
};

static void tiadc_adc_work(struct work_struct *work_s)
{
	struct tiadc_device *adc_dev =
		container_of(work_s, struct tiadc_device, poll_work);
	struct iio_dev *indio_dev = iio_priv_to_dev(adc_dev);
	struct iio_buffer *buffer = indio_dev->buffer;
	int i, samples_to_read;
	unsigned int config;
	int size_to_acquire = buffer->access->get_length(buffer);
	int sample_count = 0;
	// TODO:
	//	u64 init_adc_work_jiffies = jiffies;
	//	printk(KERN_INFO "adc_hs_plc: tiadc_adc_work: %u\n", (u32)init_adc_work_jiffies);
	// For simplicity only consider the typical case 'indio_dev->scan_bytes == 4'
	if (indio_dev->scan_bytes != 4) {
		printk(KERN_INFO "adc_hs_plc: Unexpected 'scan_bytes'\n");
		goto out;
	}
	adc_dev->data_avail = 0;
	// 'continuous_transfer == 0' needs a
	//	'plc_file_write_string(iio_dir, "devices/trigger0/trigger_now", "1")' per buffer
	if (!continuous_transfer) {
		while (sample_count < size_to_acquire) {
			int ret = wait_event_interruptible(adc_dev->wq_data_avail,
						(adc_dev->data_avail == 1));
			// Check for a signal interruption
			if (adc_dev->data_avail == 0) continue;
			if (ret == 0) {
				adc_dev->data_avail = 0;
				samples_to_read = adc_dev->dma_samples_cur_write-adc_dev->dma_samples_cur_read;
				if (samples_to_read * sizeof(u32) < buffer->access->get_bytes_per_datum(buffer)) {
					printk(KERN_INFO "adc_hs_plc: Continue? %u\n",
							buffer->access->get_bytes_per_datum(buffer));
					goto out;
				}
				sample_count += samples_to_read;
				for (i = samples_to_read; i > 0; i--) {
					u32 data = *adc_dev->dma_samples_cur_read++;
					iio_push_to_buffers(indio_dev, (u8 *) &data);
				}
			} else if (ret != -ERESTARTSYS) {
				printk(KERN_INFO "adc_hs_plc: Unexpected error on 'wait_event_interruptible'!\n");
				goto out;
			}
		}
	} else {
		// 'continuous_transfer == 1' needs an initial
		//	'plc_file_write_string(iio_dir, "devices/trigger0/trigger_now", "1")'
		// and a IOCTL ADCHS_IOC_WR_STOP_CAPTURE to abort
		while (!abort_continuous_transfer) {
			int ret = wait_event_interruptible(adc_dev->wq_data_avail,
						(adc_dev->data_avail == 1) || (abort_continuous_transfer));
			if (abort_continuous_transfer) break;
			// Check for a signal interruption
			if (adc_dev->data_avail == 0) continue;
			// To avoid unexpected overwritting we should check for the buffer not to be full
			// NOTE: 'data_available' doesn't exist on v3.8
			//	if (buffer->access->data_available(buffer) == size_to_acquire)
			//		printk(KERN_INFO "adc_hs_plc: Buffer full!\n");
			if (ret == 0) {
				// Circular buffer management
				int samples_to_read_after_overflow = 0;
				adc_dev->data_avail = 0;
				if (adc_dev->dma_samples_cur_write >= adc_dev->dma_samples_cur_read) {
					samples_to_read = adc_dev->dma_samples_cur_write-adc_dev->dma_samples_cur_read;
				} else {
					samples_to_read = adc_dev->dma_samples_end-adc_dev->dma_samples_cur_read;
					samples_to_read_after_overflow =
							adc_dev->dma_samples_cur_write-adc_dev->dma_samples;
				}
				sample_count += samples_to_read + samples_to_read_after_overflow;
				for (i = samples_to_read; i > 0; i--) {
					u32 data = *adc_dev->dma_samples_cur_read++;
					iio_push_to_buffers(indio_dev, (u8 *) &data);
				}
				if (samples_to_read_after_overflow) {
					adc_dev->dma_samples_cur_read = adc_dev->dma_samples;
					for (i = samples_to_read_after_overflow; i > 0; i--) {
						u32 data = *adc_dev->dma_samples_cur_read++;
						iio_push_to_buffers(indio_dev, (u8 *) &data);
					}
				}
			} else if (ret != -ERESTARTSYS) {
				printk(KERN_INFO "adc_hs_plc: Unexpected error on 'wait_event_interruptible'!\n");
				goto out;
			}
		}
	}
	// [PLC] jiffies
	{
		u32 lapse_adc_buffering_jiffies = jiffies-adc_dev->start_adc_buffering_jiffies;
		dev_dbg(&indio_dev->dev, "adc_hs_plc: ADC %u samples (in blocks of %u bytes) "
				"captured in %u us (%u jiffies)\n",
				size_to_acquire, indio_dev->scan_bytes,
				jiffies_to_usecs(lapse_adc_buffering_jiffies), lapse_adc_buffering_jiffies);
	}
out:
	tiadc_writel(adc_dev, REG_IRQCLR, (IRQENB_FIFO1THRES |
				IRQENB_FIFO1OVRRUN | IRQENB_FIFO1UNDRFLW));
	config = tiadc_readl(adc_dev, REG_CTRL);
	tiadc_writel(adc_dev, REG_CTRL,	config & ~CNTRLREG_TSCSSENB);
}

irqreturn_t tiadc_iio_pollfunc(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	int i, fifo1count, read;

	tiadc_writel(adc_dev, REG_IRQCLR, (IRQENB_FIFO1THRES |
				IRQENB_FIFO1OVRRUN |
				IRQENB_FIFO1UNDRFLW));

	/* Flush FIFO before trigger */
	fifo1count = tiadc_readl(adc_dev, REG_FIFO1CNT);
	for (i = 0; i < fifo1count; i++)
		read = tiadc_readl(adc_dev, REG_FIFO1);

	return IRQ_WAKE_THREAD;
}

static const char * const chan_name_ain[] = {
	"AIN0",
	"AIN1",
	"AIN2",
	"AIN3",
	"AIN4",
	"AIN5",
	"AIN6",
	"AIN7",
};

static int tiadc_channel_init(struct iio_dev *indio_dev, int channels)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	struct iio_chan_spec *chan_array;
	struct iio_chan_spec *chan;
	struct iio_map *map;
	int i, ret;

	indio_dev->num_channels = channels;
	chan_array = kcalloc(channels,
			sizeof(struct iio_chan_spec), GFP_KERNEL);
	if (chan_array == NULL) {
		ret = -ENOMEM;
		goto err_no_chan_array;
	}

	chan = chan_array;
	for (i = 0; i < channels; i++, chan++) {

		chan->type = IIO_VOLTAGE;
		chan->indexed = 1;
		chan->channel = adc_dev->channel_line[i];
		chan->info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT;
		chan->datasheet_name = chan_name_ain[chan->channel];
		chan->scan_index = i;
		chan->scan_type.sign = 'u';
		chan->scan_type.realbits = 12;
		chan->scan_type.storagebits = 32;
	}

	indio_dev->channels = chan_array;

	map = kcalloc(channels + 1, sizeof(struct iio_map), GFP_KERNEL);
	if (map == NULL) {
		ret = -ENOMEM;
		goto err_no_iio_map;
	}
	adc_dev->map = map;

	for (i = 0, chan = chan_array; i < channels; i++, chan++, map++) {
		map->adc_channel_label = chan->datasheet_name;
		map->consumer_dev_name = "any";
		map->consumer_channel = chan->datasheet_name;
	}
	map->adc_channel_label = NULL;
	map->consumer_dev_name = NULL;
	map->consumer_channel = NULL;

	ret = iio_map_array_register(indio_dev, adc_dev->map);
	if (ret != 0)
		goto err_iio_map_register_fail;

	return 0;

err_iio_map_register_fail:
	kfree(adc_dev->map);
	adc_dev->map = NULL;
err_no_iio_map:
	kfree(chan_array);
	indio_dev->channels = NULL;
err_no_chan_array:
	return ret;
}

static void tiadc_channels_remove(struct iio_dev *indio_dev)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	kfree(indio_dev->channels);
	kfree(adc_dev->map);
}

static int tiadc_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val, int *val2, long mask)
{
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	int i, map_val;
	unsigned int fifo1count, read, stepid, step_en;

	// [PLC]
	if (dma_mode)
	{
		static int third_time = 0;
		if (++third_time == 3)
		{
			// TODO: Enable dma capturing
			printk(KERN_INFO "adc_hs_plc: Third read raw\n");
		}
		if (adc_dev->dma_samples_cur_write == adc_dev->dma_samples_end)
			adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
		*val = *adc_dev->dma_samples_cur_write++;
		return IIO_VAL_INT;
	}

	if (iio_buffer_enabled(indio_dev))
		return -EBUSY;
	else {
		unsigned long timeout = jiffies + usecs_to_jiffies
					(IDLE_TIMEOUT * adc_dev->channels);
		step_en = get_adc_step_mask(adc_dev);
		// printk(KERN_INFO "adc_hs:tiadc_read_raw:step_enable_set(%x)\n", step_en);
		am335x_tsc_se_set_plc(adc_dev->mfd_tscadc, step_en);

		/* Wait for ADC sequencer to complete sampling */
		while (tiadc_readl(adc_dev, REG_ADCFSM) & SEQ_STATUS) {
			if (time_after(jiffies, timeout))
				return -EAGAIN;
			}
		map_val = chan->channel + TOTAL_CHANNELS;

		/*
		 * When the sub-system is first enabled,
		 * the sequencer will always start with the
		 * lowest step (1) and continue until step (16).
		 * For ex: If we have enabled 4 ADC channels and
		 * currently use only 1 out of them, the
		 * sequencer still configures all the 4 steps,
		 * leading to 3 unwanted data.
		 * Hence we need to flush out this data.
		 */

		fifo1count = tiadc_readl(adc_dev, REG_FIFO1CNT);
		for (i = 0; i < fifo1count; i++) {
			read = tiadc_readl(adc_dev, REG_FIFO1);
			stepid = read & FIFOREAD_CHNLID_MASK;
			stepid = stepid >> 0x10;

			if (stepid == map_val) {
				read = read & FIFOREAD_DATA_MASK;
				*val = read;
				return IIO_VAL_INT;
			}
		}
		return -EAGAIN;
	}
}

static const struct iio_info tiadc_info = {
	.read_raw = &tiadc_read_raw,
	.driver_module = THIS_MODULE,
};

static int adchs_init(void);
static void adchs_exit(void);
static int tiadc_probe(struct platform_device *pdev)
{
	struct iio_dev		*indio_dev;
	struct tiadc_device	*adc_dev;
	struct ti_tscadc_dev	*tscadc_dev = ti_tscadc_dev_get(pdev);
	struct mfd_tscadc_board	*pdata = tscadc_dev->dev->platform_data;
	struct device_node	*node = tscadc_dev->dev->of_node;
	struct property		*prop;
	const __be32		*cur;
	int			err;
	u32			val;
	int			channels = 0;

	// [PLC] Debugging info
	// NOTE: struct ti_tscadc_dev * ti_tscadc_dev_get() {
	//	return *(struct ti_tscadc_dev**)(pdev->dev.platform_data);}
	dev_dbg(&pdev->dev, "adc_hs:probe:dev_name(&pdev->dev)=%s\n", dev_name(&pdev->dev));
	dev_dbg(&pdev->dev, "adc_hs:probe:pdev->dev.platform_data=%x\n",
			(unsigned int)(pdev->dev.platform_data));

	if (!pdata && !node) {
		dev_err(&pdev->dev, "Could not find platform data\n");
		return -EINVAL;
	}

	indio_dev = iio_device_alloc(sizeof(struct tiadc_device));
	if (indio_dev == NULL) {
		dev_err(&pdev->dev, "failed to allocate iio device\n");
		err = -ENOMEM;
		goto err_ret;
	}
	indio_dev_singleton = indio_dev;

	adc_dev = iio_priv(indio_dev);

	adc_dev->mfd_tscadc = ti_tscadc_dev_get(pdev);

	if (pdata)
		adc_dev->channels = pdata->adc_init->adc_channels;
	else {
		node = of_get_child_by_name(node, "adc");
		of_property_for_each_u32(node, "ti,adc-channels", prop, cur, val) {
				adc_dev->channel_line[channels] = val;
			channels++;
		}
		adc_dev->channels = channels;
	}
	adc_dev->channels = channels;
	adc_dev->irq = adc_dev->mfd_tscadc->irq;

	// [PLC]
	// if (dma_mode)
	{
		int n = 0;
		adc_dev->dma_samples = kzalloc(DMA_SAMPLES_COUNT*sizeof(*adc_dev->dma_samples), GFP_KERNEL);
		if (!adc_dev->dma_samples) {
			dev_err(&pdev->dev, "Can't allocate memory\n");
			return -ENOMEM;
		}
		adc_dev->dma_samples_end = adc_dev->dma_samples+DMA_SAMPLES_COUNT;
		for (adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
				adc_dev->dma_samples_cur_write < adc_dev->dma_samples_end;
				adc_dev->dma_samples_cur_write++)
					*adc_dev->dma_samples_cur_write = (u16)(n++);
		adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
		adc_dev->dma_samples_cur_read = adc_dev->dma_samples;
	}



	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &tiadc_info;

	tiadc_step_config(indio_dev);
	tiadc_writel(adc_dev, REG_FIFO1THR, FIFO1_THRESHOLD);

	err = tiadc_channel_init(indio_dev, adc_dev->channels);
	if (err < 0) {
		dev_err(&pdev->dev, "tiadc_channel_init() failed\n");
		goto err_free_device;
	}

	INIT_WORK(&adc_dev->poll_work, &tiadc_adc_work);
	init_waitqueue_head(&adc_dev->wq_data_avail);

	err = request_irq(adc_dev->irq, tiadc_irq, IRQF_SHARED,
		indio_dev->name, indio_dev);
	if (err)
		goto err_free_irq;

	err = iio_triggered_buffer_setup(indio_dev, &tiadc_iio_pollfunc,
			&tiadc_trigger_h, &tiadc_buffer_setup_ops);
	if (err)
		goto err_unregister;

	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(&pdev->dev, "iio_device_register() failed\n");
		goto err_free_channels;
	}

	platform_set_drvdata(pdev, indio_dev);
	dev_dbg(&pdev->dev, "adc_hs:probe:end\n");

	{
		int ret, val, val2;
		ret = tiadc_read_raw(indio_dev, indio_dev->channels, &val, &val2, 0);
		printk(KERN_INFO "adc_hs:probe:tiadc_read_raw = %d\n", val);
	}

	adchs_init();

	return 0;

err_unregister:
	iio_buffer_unregister(indio_dev);
err_free_irq:
	free_irq(adc_dev->irq, indio_dev);
err_free_channels:
	tiadc_channels_remove(indio_dev);
err_free_device:
	iio_device_free(indio_dev);
err_ret:
	return err;
}

static int tiadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	u32 step_en;

	adchs_exit();

	free_irq(adc_dev->irq, indio_dev);
	iio_device_unregister(indio_dev);
	iio_buffer_unregister(indio_dev);
	tiadc_channels_remove(indio_dev);

	step_en = get_adc_step_mask(adc_dev);
	am335x_tsc_se_clr_plc(adc_dev->mfd_tscadc, step_en);

	iio_device_free(indio_dev);

	return 0;
}

#ifdef CONFIG_PM
static int tiadc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	struct ti_tscadc_dev *tscadc_dev;
	unsigned int idle;

	tscadc_dev = ti_tscadc_dev_get(to_platform_device(dev));
	if (!device_may_wakeup(tscadc_dev->dev)) {
		idle = tiadc_readl(adc_dev, REG_CTRL);
		idle &= ~(CNTRLREG_TSCSSENB);
		tiadc_writel(adc_dev, REG_CTRL, (idle |
				CNTRLREG_POWERDOWN));
	}

	return 0;
}

static int tiadc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tiadc_device *adc_dev = iio_priv(indio_dev);
	unsigned int restore;

	/* Make sure ADC is powered up */
	restore = tiadc_readl(adc_dev, REG_CTRL);
	restore &= ~(CNTRLREG_TSCSSENB);
	tiadc_writel(adc_dev, REG_CTRL, restore);

	tiadc_writel(adc_dev, REG_FIFO1THR, FIFO1_THRESHOLD);
	tiadc_step_config(indio_dev);

	/* Make sure ADC is powered up */
	restore &= ~(CNTRLREG_POWERDOWN);
	restore |= CNTRLREG_TSCSSENB;
	tiadc_writel(adc_dev, REG_CTRL, restore);

	return 0;
}

static const struct dev_pm_ops tiadc_pm_ops = {
	.suspend = tiadc_suspend,
	.resume = tiadc_resume,
};
#define TIADC_PM_OPS (&tiadc_pm_ops)
#else
#define TIADC_PM_OPS NULL
#endif

static const struct of_device_id ti_adc_dt_ids[] = {
	{ .compatible = "ti,ti-tscadc_plc", },
	{ }
};
MODULE_DEVICE_TABLE(of, ti_adc_dt_ids);

static struct platform_driver tiadc_driver = {
	.driver = {
		.name   = "tiadc_plc",
		.owner	= THIS_MODULE,
		.pm	= TIADC_PM_OPS,
		.of_match_table = of_match_ptr(ti_adc_dt_ids),
	},
	.probe	= tiadc_probe,
	.remove	= tiadc_remove,
};

module_platform_driver(tiadc_driver);


/*-------------------------------------------------------------------------*/

static unsigned bufsiz = 4096;
struct adchsdev_data {
	spinlock_t		adchs_lock;
	struct iio_dev	*indio_dev;
	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
	int eof_reached;
};

// PRECONDITION: if 'continuous_transfer_started' then 'count%sizeof(u16) == 0'
// Remark: 'adchs_read' present two different behaviors
//	- sends the current value of one sample of the ADC in ASCII (use 'cat /dev/adchs_plc')
//	- sends WORD samples of a continuous and concurrent ADC capturing
static ssize_t
adchs_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned long missing;
	struct adchsdev_data * adchsdev = filp->private_data;
	struct tiadc_device *adc_dev = iio_priv(adchsdev->indio_dev);
	// dev_dbg(&adchsdev->indio_dev->dev, "adchs:read %d bytes to read\n", count);
	if (continuous_transfer_started) {
		u32 samples_count = count/sizeof(u16);
		// 'continuous_transfer == 1' needs an initial
		//	'plc_file_write_string(iio_dir, "devices/trigger0/trigger_now", "1")'
		// and a IOCTL ADCHS_IOC_WR_STOP_CAPTURE to abort
		int ret = wait_event_interruptible(adc_dev->wq_data_avail,
					(adc_dev->data_avail == 1) || (abort_continuous_transfer));
		if (abort_continuous_transfer)
			return -EINVAL;
		// Check for a signal interruption
		if (adc_dev->data_avail == 0)
			return -EAGAIN;
		// To avoid unexpected overwritting we should check for the buffer not to be full
		// NOTE: 'data_available' doesn't exist on v3.8
		//	if (buffer->access->data_available(buffer) == size_to_acquire)
		//		printk(KERN_INFO "adc_hs_plc: Buffer full!\n");
		if (ret == 0) {
			// Circular buffer management
			int samples_to_read;
			int samples_to_read_after_overflow;
			adc_dev->data_avail = 0;
			if (adc_dev->dma_samples_cur_write >= adc_dev->dma_samples_cur_read) {
				samples_to_read = adc_dev->dma_samples_cur_write-adc_dev->dma_samples_cur_read;
				samples_to_read_after_overflow = 0;
			} else {
				samples_to_read = adc_dev->dma_samples_end-adc_dev->dma_samples_cur_read;
				samples_to_read_after_overflow =
						adc_dev->dma_samples_cur_write-adc_dev->dma_samples;
			}
			if (samples_to_read > samples_count) {
				samples_to_read = samples_count;
				samples_to_read_after_overflow = 0;
			}
			missing = copy_to_user(buf, adc_dev->dma_samples_cur_read, samples_to_read*sizeof(u16));
			if (missing != 0)
				return -EINVAL;
			samples_count -= samples_to_read;
			if (samples_to_read_after_overflow == 0) {
				adc_dev->dma_samples_cur_read += samples_to_read;
			} else {
				buf += samples_to_read*sizeof(u16);
				if (samples_to_read_after_overflow > samples_count)
					samples_to_read_after_overflow = samples_count;
				missing = copy_to_user(buf, adc_dev->dma_samples,
						samples_to_read_after_overflow*sizeof(u16));
				if (missing != 0)
					return -EINVAL;
				samples_count -= samples_to_read_after_overflow;
				adc_dev->dma_samples_cur_read = adc_dev->dma_samples+samples_to_read_after_overflow;
			}
		} else if (ret != -ERESTARTSYS) {
			printk(KERN_INFO "adc_hs_plc: Unexpected error on 'wait_event_interruptible'!\n");
			return -EINVAL;
		}
		// dev_dbg(&adchsdev->indio_dev->dev, "adchs:read %d bytes read\n",
		//		count-samples_count*sizeof(u16));
		return count-samples_count*sizeof(u16);
	} else {
		/* Read-only message with current device setup */
		unsigned long	missing;
		char			msg[6];
		ssize_t			bytes = 8;
		int ret, val, val2;
		// printk(KERN_INFO "adchs:read %d bytes\n", count);
		// printk(KERN_INFO "adchs:private_data = %x\n", (int)filp->private_data);
		if (adchsdev->eof_reached) {
			adchsdev->eof_reached = 0;
			return 0;
		}
		// printk(KERN_INFO "adchs_read...\n");
		ret = tiadc_read_raw(adchsdev->indio_dev, adchsdev->indio_dev->channels, &val, &val2, 0);
		bytes = sprintf(msg, "%04d\n", val);
		// printk(KERN_INFO "adchs_read=%d (%d bytes)\n", val, bytes);
		// [PLC] Trick:
		//	- If count > bytes (case 'cat /dev/adchs_plc') send just a capture. For that send a 0
		//		in the next cycle
		//	- If count < bytes (case read_hs desde lab-app) send truncated data
		if (bytes > count)
			bytes = count;
		else
			adchsdev->eof_reached = 1;
		missing = copy_to_user(buf, msg, bytes);
		if (missing != 0)
			return -EINVAL;
		return bytes;
		/*
		// Example of how to send just an 'A' at each 'cat'
		ssize_t	bytes_to_copy;
		int dummy = 65;
		bytes_to_copy = 1;
		if (adchsdev->eof_reached) { adchsdev->eof_reached = 0; return 0; }
		if (bytes_to_copy > count) bytes_to_copy = count;
		else adchsdev->eof_reached = 1;
		missing = copy_to_user(buf, &dummy, bytes_to_copy);
		if (missing != 0) return -EINVAL;
		return bytes_to_copy;
		*/
	}
}

// TODO: Antes de salir liberar memoria ya reservada
// TODO: La estrategia de reserva y liberación del 'adchsdev' así como la gestión de
//	'adchsdev->users' no es correcta
static int adchs_open(struct inode *inode, struct file *filp)
{
	struct adchsdev_data	*adchsdev;
	int			status = 0;

	/* Allocate driver data */
	adchsdev = kzalloc(sizeof(*adchsdev), GFP_KERNEL);
	if (!adchsdev)
		return -ENOMEM;

	adchsdev->indio_dev = indio_dev_singleton;
	dev_dbg(&adchsdev->indio_dev->dev, "adchs:open\n");

	/* Initialize the driver data */
	spin_lock_init(&adchsdev->adchs_lock);
	mutex_init(&adchsdev->buf_lock);
	adchsdev->eof_reached = 0;

	if (!adchsdev->buffer) {
		adchsdev->buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!adchsdev->buffer) {
			status = -ENOMEM;
		}
	}
	if (status == 0) {
		adchsdev->users++;
		filp->private_data = adchsdev;
		nonseekable_open(inode, filp);
	}
	return status;
}

static int adchs_release(struct inode *inode, struct file *filp)
{
	struct adchsdev_data	*adchsdev;
	printk(KERN_INFO "adchs:release\n");
	adchsdev = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	adchsdev->users--;
	if (!adchsdev->users) {
		kfree(adchsdev->buffer);
		adchsdev->buffer = NULL;
		kfree(adchsdev);
	}
	return 0;
}

// TODO: Refactor. Based on 'debugfs_ioctl' from 'spi-omap2-mcspi_plc.c'
#define ADCHS_IOC_MAGIC 'a'
#define ADCHS_IOC_WR_START_CAPTURE _IOW(ADCHS_IOC_MAGIC, 1, __u8)
#define ADCHS_IOC_WR_STOP_CAPTURE _IOW(ADCHS_IOC_MAGIC, 2, __u8)
static long
adchs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct adchsdev_data *adchsdev = filp->private_data;
	struct tiadc_device *adc_dev = iio_priv(adchsdev->indio_dev);

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != ADCHS_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	/*
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;
	*/

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	// mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case ADCHS_IOC_WR_START_CAPTURE:
		dev_dbg(&adchsdev->indio_dev->dev, "IOCTL: Start capture mode\n");
		if (!continuous_transfer)
			return -EFAULT;
		abort_continuous_transfer = 0;
		adc_dev->data_avail = 0;
		continuous_transfer_started = 1;
		adc_dev->dma_samples_cur_write = adc_dev->dma_samples;
		adc_dev->dma_samples_cur_read = adc_dev->dma_samples;
		{
			unsigned int config;
			config = tiadc_readl(adc_dev, REG_CTRL);
			tiadc_writel(adc_dev, REG_CTRL,	config & ~CNTRLREG_TSCSSENB);
			tiadc_writel(adc_dev, REG_CTRL,	config |  CNTRLREG_TSCSSENB);

			tiadc_writel(adc_dev,  REG_IRQSTATUS, IRQENB_FIFO1THRES |
					 IRQENB_FIFO1OVRRUN | IRQENB_FIFO1UNDRFLW);
			tiadc_writel(adc_dev,  REG_IRQENABLE, IRQENB_FIFO1THRES
						| IRQENB_FIFO1OVRRUN);
		}
		break;
	case ADCHS_IOC_WR_STOP_CAPTURE:
		dev_dbg(&adchsdev->indio_dev->dev, "IOCTL: Stop capture mode\n");
		abort_continuous_transfer = 1;
		wake_up_interruptible(&adc_dev->wq_data_avail);
		if (!iio_std_exchange)
		{
			unsigned int config;
			tiadc_writel(adc_dev, REG_IRQCLR, (IRQENB_FIFO1THRES |
						IRQENB_FIFO1OVRRUN | IRQENB_FIFO1UNDRFLW));
			config = tiadc_readl(adc_dev, REG_CTRL);
			tiadc_writel(adc_dev, REG_CTRL,	config & ~CNTRLREG_TSCSSENB);
		}
		continuous_transfer_started = 0;
		// TODO: Improve to notify an idle or aborted DMA transfer instead of
		//	just sending a hard-coded zero or a partially transmitted buffer
		/*
		if (!mcspi_dma->dma_tx_in_progress) {
			retval = __put_user(0, (__u8 __user *)arg);
			break;
		}
		mcspi_dma->ping_pong.dma_buffer_waiting = 1;
		init_completion(&mcspi_dma->dma_tx_completion);
		wait_for_completion(&mcspi_dma->dma_tx_completion);
		mcspi_dma->ping_pong.dma_buffer_waiting = 0;
		retval = __put_user(mcspi_dma->ping_pong.cur_buffer_in_dma_index,
					(__u8 __user *)arg);
		*/
		break;
	}
	// mutex_unlock(&spidev->buf_lock);
	// spi_dev_put(spi);
	return retval;
}

static const struct file_operations adchs_fops = {
	.owner =	THIS_MODULE,
	.write =	NULL,
	.read =		adchs_read,
	.unlocked_ioctl = adchs_ioctl,
	.compat_ioctl = NULL,
	.open =		adchs_open,
	.release =	adchs_release,
	.llseek =	NULL,
};

static struct class *adchs_class;

// static int __init adchs_init(void)
static int adchs_init(void)
{
	int status;
	static const int DEV_MAJOR = 0;
	printk(KERN_INFO "adchs:init\n");

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	// BUILD_BUG_ON(N_SPI_MINORS > 256);

	// [PLC]
	//	status = register_chrdev(0, "spi", &adchs_fops);
	//	status = register_chrdev(SPIDEV_MAJOR, "spi", &adchs_fops);
	//	if (status < 0) return status;
	
	// status = register_chrdev(SPIDEV_MAJOR, "spi", &adchs_fops);
	status = register_chrdev(DEV_MAJOR, "adchs", &adchs_fops);
	if (status < 0) {
		unregister_chrdev(DEV_MAJOR, "adchs");
		status = register_chrdev(DEV_MAJOR, "adchs", &adchs_fops);
		if (status < 0)
			return status;
	}
	printk(KERN_INFO "adchs:end, 'adchs' registered as #%d\n", status);

	adchs_class = class_create(THIS_MODULE, "adchs_class");
	if (IS_ERR(adchs_class)) {
		unregister_chrdev(DEV_MAJOR, tiadc_driver.driver.name);
		return PTR_ERR(adchs_class);
	}

	return 0;
}
// module_init(adchs_init);

// static void __exit adchs_exit(void)
static void adchs_exit(void)
{
	static const int DEV_MAJOR = 0;
	printk(KERN_INFO "adchs:exit\n");
	class_destroy(adchs_class);
	unregister_chrdev(DEV_MAJOR, tiadc_driver.driver.name);
}
// module_exit(adchs_exit);


MODULE_DESCRIPTION("ADC MFD HighSpeed driver");
MODULE_AUTHOR("Jose Maria Ortega <jose77105@gmail.com>");
MODULE_LICENSE("GPL");
