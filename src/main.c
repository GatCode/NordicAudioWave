#include <zephyr.h>
#include <string.h>
#include <nrf.h>
#include <device.h>
#include <drivers/i2s.h>

/* Prepare for 32 sample values */
#define NUM_SAMPLES 32

/* Sine Wave Sample */
static int16_t data_frame[NUM_SAMPLES] = {
	  6392,  12539,  18204,  23169,  27244,  30272,  32137,  32767,  32137,
	 30272,  27244,  23169,  18204,  12539,   6392,      0,  -6393, -12540,
	-18205, -23170, -27245, -30273, -32138, -32767, -32138, -30273, -27245,
	-23170, -18205, -12540,  -6393,     -1,
};

/* The size of the memory block should be a multiple of data_frame size */
#define BLOCK_SIZE (2 * sizeof(data_frame))

/* The number of memory blocks in a slab has to be at least 2 per queue */
#define NUM_BLOCKS 5

/* Define a new Memory Slab which consistes of NUM_BLOCKS blocks
   __________________________________________________________________________
  |    Block 0   |    Block 1   |    Block 2   |    Block 3   |    Block 4   |
  |    0...31    |    0...31    |    0...31    |    0...31    |    0...31    |
  |______________|______________|______________|______________|______________|
*/
static K_MEM_SLAB_DEFINE(mem_slab, BLOCK_SIZE, NUM_BLOCKS, NUM_SAMPLES);

void main(void) {
	/* Get I2S device from the devicetree */
	const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s_rxtx));
	if (!device_is_ready(i2s_dev)) {
		printk("%s is not ready\n", i2s_dev->name);
		return;
	}

	/* Configure the I2S device */
	struct i2s_config i2s_cfg;
	i2s_cfg.word_size = 16; // due to int16_t in data_frame declaration
	i2s_cfg.channels = 2; // L + R channel
	i2s_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	i2s_cfg.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
	i2s_cfg.frame_clk_freq = 44100;
	i2s_cfg.mem_slab = &mem_slab;
	i2s_cfg.block_size = BLOCK_SIZE;
	i2s_cfg.timeout = 1000;
	int ret = i2s_configure(i2s_dev, I2S_DIR_TX, &i2s_cfg);
	if (ret < 0) {
		printk("Failed to configure the I2S stream: (%d)\n", ret);
		return;
	}

	/* Allocate the memory blocks (tx buffer) from the slab and
	   set everything to 0 */
	void *mem_blocks;
	ret = k_mem_slab_alloc(&mem_slab, &mem_blocks, K_NO_WAIT);
	if (ret < 0) {
		printk("Failed to allocate the memory blocks: %d\n", ret);
		return;
	}
	memset((uint16_t*)mem_blocks, 0, NUM_SAMPLES * NUM_BLOCKS);

	/* Start the transmission of data */
	ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
	if (ret < 0) {
		printk("Failed to start the transmission: %d\n", ret);
		return;
	}

	for(;;) {
		/* Put data into the tx buffer */
		for (int i = 0; i < NUM_SAMPLES * NUM_BLOCKS; i++) {
			((uint16_t*)mem_blocks)[i] = data_frame[i % NUM_SAMPLES];
		}

		/* Write Data */
		int ret = i2s_buf_write(i2s_dev, mem_blocks, BLOCK_SIZE);
		if (ret < 0) {
			printk("Error: i2s_write failed with %d\n", ret);
			return;
		}
	}
}