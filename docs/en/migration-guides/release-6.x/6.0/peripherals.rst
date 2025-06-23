Peripherals
===========

:link_to_translation:`zh_CN:[中文]`

Common Changes
--------------

All drivers' ``io_loop_back`` configuration have been removed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Different driver objects can share the same GPIO number, enabling more complex functionalities. For example, you can bind the TX and RX channels of the RMT peripheral to the same GPIO to simulate 1-Wire bus read and write timing. In previous versions, you needed to configure the ``io_loop_back`` setting in the driver to achieve this "loopback" functionality. Now, this configuration has been removed. Simply configuring the same GPIO number in different drivers will achieve the same functionality.

ADC
---

The legacy ADC driver ``driver/adc.h`` is deprecated since version 5.0 (see :ref:`deprecate_adc_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_adc`, and the header file path is ``esp_adc/adc_oneshot.h``, ``esp_adc/adc_continuous.h``, ``esp_adc/adc_cali.h`` and ``esp_adc/adc_cali_scheme.h``.


RMT
---

The ``io_od_mode`` member in the :cpp:type:`rmt_tx_channel_config_t` configuration structure has been removed. If you want to use open-drain mode, you need to manually call the :func:`gpio_od_enable` function.

MCPWM
-----

The ``io_od_mode`` member in the :cpp:type:`mcpwm_generator_config_t` configuration structure has been removed. If you want to use open-drain mode, you need to manually call the :func:`gpio_od_enable` function.

The ``pull_up`` and ``pull_down`` members have been removed from the following configuration structures. You need to manually call the :func:`gpio_set_pull_mode` function to configure the pull-up and pull-down resistors for the IO:

.. list::

    - :cpp:type:`mcpwm_generator_config_t`
    - :cpp:type:`mcpwm_gpio_fault_config_t`
    - :cpp:type:`mcpwm_gpio_sync_src_config_t`
    - :cpp:type:`mcpwm_capture_channel_config_t`

GPIO
----

:func:`gpio_iomux_in` and :func:`gpio_iomux_out` have been replaced by :func:`gpio_iomux_input` and :func:`gpio_iomux_output`, and have been moved to ``esp_private/gpio.h`` header file as private APIs for internal use only.

I2C
---

I2C slave has been redesigned in v5.4. In the current version, the old I2C slave driver has been removed. For details, please refer to the I2C slave section in the programming guide.

The major breaking changes in concept and usage are listed as follows:

Major Changes in Concepts
~~~~~~~~~~~~~~~~~~~~~~~~~

- Previously, the I2C slave driver performed active read and write operations. In the new version, these operations are handled passively via callbacks triggered by master events, aligning with standard I2C slave behavior.

Major Changes in Usage
~~~~~~~~~~~~~~~~~~~~~~

- ``i2c_slave_receive`` has been removed. In the new driver, data reception is handled via callbacks.
- ``i2c_slave_transmit`` has been replaced by ``i2c_slave_write``.
- ``i2c_slave_write_ram`` has been removed。
- ``i2c_slave_read_ram`` has been removed。

Legacy Timer Group Driver is Removed
------------------------------------

The legacy timer group driver ``driver/timer.h`` is deprecated since version 5.0 (see :ref:`deprecate_gptimer_legacy_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_driver_gptimer`, and the header file path is ``driver/gptimer.h``.

.. only:: SOC_I2S_SUPPORTED

    Legacy I2S Driver is Removed
    ------------------------------------

    - The legacy i2s driver ``driver/i2s.h`` is deprecated since version 5.0 (see :ref:`deprecate_i2s_legacy_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_driver_i2s`, and the header file path is ``driver/i2s_std.h``, ``driver/i2s_pdm.h`` and ``driver/i2s_tdm.h``.
    - API ``i2s_set_adc_mode``,  ``i2s_adc_enable`` and ``i2s_adc_disable`` are deprecated since version 5.0. Starting from version 6.0, these APIs are completely removed.

.. only:: SOC_PCNT_SUPPORTED

    Legacy PCNT Driver is Removed
    ------------------------------------

    The legacy PCNT driver ``driver/pcnt.h`` is deprecated since version 5.0 (see :ref:`deprecate_pcnt_legacy_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_driver_pcnt`, and the header file path is ``driver/pulse_cnt.h``.

GDMA
----

- The ``GDMA_ISR_IRAM_SAFE`` Kconfig option has been removed due to potential risks. Now, the interrupt behavior of different DMA channels during Cache disabled periods are independent of each other.
- ``gdma_new_channel`` is removed. When requesting a GDMA channel, use either ``gdma_new_ahb_channel`` or ``gdma_new_axi_channel`` according to the bus type.
- The ``sram_trans_align`` and ``psram_trans_align`` members have been removed from :cpp:type:`async_memcpy_config_t`. Use :cpp:member:`async_memcpy_config_t::dma_burst_size` to set the DMA burst transfer size.
- The ``esp_dma_capable_malloc`` and ``esp_dma_capable_calloc`` functions have been removed. Use :cpp:func:`heap_caps_malloc` and :cpp:func:`heap_caps_calloc` from :component_file:`heap/include/esp_heap_caps.h` with ``MALLOC_CAP_DMA|MALLOC_CAP_CACHE_ALIGNED`` to allocate memory suitable for DMA and cache alignment.

SDMMC
-----

- The ``get_dma_info`` member in the :cpp:type:`sdmmc_host_t` structure, as well as the ``sdspi_host_get_dma_info`` and ``sdmmc_host_get_dma_info`` functions, have been removed. DMA configuration is now handled internally by the driver.

.. only:: SOC_DAC_SUPPORTED

    Legacy DAC Driver is Removed
    ------------------------------------

    The legacy DAC driver ``driver/dac.h`` is deprecated since version 5.1 (see :ref:`deprecate_dac_legacy_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_driver_dac`, and the header file path is ``driver/dac_oneshot.h``, ``driver/dac_continuous.h`` and ``driver/dac_cosine.h``.

.. only:: SOC_TEMP_SENSOR_SUPPORTED

    Legacy Temperature Sensor Driver is Removed
    -------------------------------------------

    The legacy temperature sensor driver ``driver/temp_sensor.h`` is deprecated since version 5.0 (see :ref:`deprecate_tsens_legacy_driver`). Starting from version 6.0, the legacy driver is completely removed. The new driver is placed in the :component:`esp_driver_tsens`, and the header file path is ``driver/temperature_sensor.h``.
