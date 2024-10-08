ESP-NETIF Developer's manual
============================

:link_to_translation:`zh_CN:[中文]`

As shown in the :ref:`esp-netif structure` diagram, ESP-NETIF is in fact an intermediary between the I/O driver and the TCP/IP stack. This manual describes customization of these two sides, that is if you need to implement :ref:`esp_netif_custom_driver` or if you need to employ :ref:`esp_netif_tcpip_stack`.


.. _esp_netif_custom_driver:

ESP-NETIF Custom I/O Driver
---------------------------

This section outlines implementing a new I/O driver with ESP-NETIF connection capabilities.

By convention, the I/O driver has to register itself as an ESP-NETIF driver, and thus holds a dependency on ESP-NETIF component and is responsible for providing data path functions, post-attach callback and in most cases, also default event handlers to define network interface actions based on driver's lifecycle transitions.


Packet Input/Output
^^^^^^^^^^^^^^^^^^^

According to the diagram shown in the :ref:`esp-netif structure` part, the following three API functions for the packet data path must be defined for connecting with ESP-NETIF:

* :cpp:func:`esp_netif_transmit()`
* :cpp:func:`esp_netif_free_rx_buffer()`
* :cpp:func:`esp_netif_receive()`

The first two functions for transmitting and freeing the rx buffer are provided as callbacks, i.e., they get called from ESP-NETIF (and its underlying TCP/IP stack) and I/O driver provides their implementation.

The receiving function on the other hand gets called from the I/O driver, so that the driver's code simply calls :cpp:func:`esp_netif_receive()` on a new data received event.


Post Attach Callback
^^^^^^^^^^^^^^^^^^^^

A final part of the network interface initialization consists of attaching the ESP-NETIF instance to the I/O driver, by means of calling the following API:

.. code:: c

    esp_err_t esp_netif_attach(esp_netif_t *esp_netif, esp_netif_iodriver_handle driver_handle);

It is assumed that the ``esp_netif_iodriver_handle`` is a pointer to driver's object, a struct derived from ``struct esp_netif_driver_base_s``, so that the first member of I/O driver structure must be this base structure with pointers to:

* post-attach function callback
* related ESP-NETIF instance

As a result, the I/O driver has to create an instance of the struct per below:

.. code:: c

    typedef struct my_netif_driver_s {
            esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
            driver_impl             *h;             /*!< handle of driver implementation */
        } my_netif_driver_t;

with actual values of ``my_netif_driver_t::base.post_attach`` and the actual drivers handle ``my_netif_driver_t::h``.

So when the :cpp:func:`esp_netif_attach()` gets called from the initialization code, the post-attach callback from I/O driver's code gets executed to mutually register callbacks between ESP-NETIF and I/O driver instances. Typically the driver is started as well in the post-attach callback. An example of a simple post-attach callback is outlined below:

.. code:: c

    static esp_err_t my_post_attach_start(esp_netif_t * esp_netif, void * args)
    {
        my_netif_driver_t *driver = args;
        const esp_netif_driver_ifconfig_t driver_ifconfig = {
                .driver_free_rx_buffer = my_free_rx_buf,
                .transmit = my_transmit,
                .handle = driver->driver_impl
        };
        driver->base.netif = esp_netif;
        ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
        my_driver_start(driver->driver_impl);
        return ESP_OK;
    }


Default Handlers
^^^^^^^^^^^^^^^^

I/O drivers also typically provide default definitions of lifecycle behavior of related network interfaces based on state transitions of I/O drivers. For example *driver start* ``->`` *network start*, etc.

An example of such a default handler is provided below:

.. code:: c

    esp_err_t my_driver_netif_set_default_handlers(my_netif_driver_t *driver, esp_netif_t * esp_netif)
    {
        driver_set_event_handler(driver->driver_impl, esp_netif_action_start, MY_DRV_EVENT_START, esp_netif);
        driver_set_event_handler(driver->driver_impl, esp_netif_action_stop, MY_DRV_EVENT_STOP, esp_netif);
        return ESP_OK;
    }


Network Stack Connection
------------------------

The packet data path functions for transmitting and freeing the rx buffer (defined in the I/O driver) are called from the ESP-NETIF, specifically from its TCP/IP stack connecting layer.

Note that ESP-IDF provides several network stack configurations for the most common network interfaces, such as for the Wi-Fi station or Ethernet. These configurations are defined in :component_file:`esp_netif/include/esp_netif_defaults.h` and should be sufficient for most network drivers.

In some cases, you might want to define a custom lwIP based interface, for example if you need to update :component_file:`esp_netif/lwip/netif/wlanif.c` with a specific packet pool. In that case, you would have to define an explicit dependency to lwIP and include :component_file:`esp_netif/include/lwip/esp_netif_net_stack.h` for the relevant lwIP configuration structures.


.. _esp_netif_tcpip_stack:

ESP-NETIF Custom TCP/IP Stack
-----------------------------

It is possible to use a custom TCP/IP stack with ESP-IDF, provided it implements BSD API. You can add support for your own TCP/IP stack, while using the generic ESP-NETIF functionality, so the application code can stay the same as with the lwIP.

In this case, please choose ``ESP_NETIF_PROVIDE_CUSTOM_IMPLEMENTATION`` in the ESP-NETIF component configuration menu. This option will disable lwIP implementation of the ESP-NETIF functions and provide only header files with declarations of types and API. You will have to supply the necessary implementation in your custom component. You can refer to the :component_file:`esp_netif/loopback/esp_netif_loopback.c` for example of dummy implementations of these functions.

It is also possible to build ESP-IDF without lwIP, please refer to :idf_file:`components/esp_netif_stack/README.md`.

API Reference
-------------

The following API reference outlines these network stack interaction with the ESP-NETIF:

.. include-build-file:: inc/esp_netif_net_stack.inc
