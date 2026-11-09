======================================
MCPWM Capture: Measure an Input Pulse
======================================

.. contents::
    :local:
    :depth: 2

Capture is an independent MCPWM path: a capture timer timestamps edges on capture-channel GPIOs. It does not require a PWM timer, operator, comparator, or generator. This makes it ideal for echo pulses, tachometers, Hall sensors, and RC receiver signals.

It is the MCPWM path for bringing external timing into the chip. Use it when the problem is pulse width, period, phase, or speed rather than PWM generation.

Measure a pulse width
=====================

Configure both edges, save the rising timestamp, and subtract it from the falling timestamp. With a 1 MHz capture resolution, the difference is directly in microseconds.

.. code-block:: c

    mcpwm_cap_timer_handle_t cap_timer = NULL;
    mcpwm_cap_channel_handle_t cap_channel = NULL;
    ESP_ERROR_CHECK(mcpwm_new_capture_timer(
        &(mcpwm_capture_timer_config_t) {
            .group_id = 0,
            .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
            .resolution_hz = 1000000,
        }, &cap_timer));
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(cap_timer,
        &(mcpwm_capture_channel_config_t) {
            .gpio_num = 6,
            .prescale = 1,
            .flags.pos_edge = true,
            .flags.neg_edge = true,
        }, &cap_channel));

Allocation alone does not start measurement. Arm the channel and run the capture timer:

.. code-block:: c

    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(cap_channel));
    ESP_ERROR_CHECK(mcpwm_capture_timer_enable(cap_timer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_start(cap_timer));

:cpp:func:`mcpwm_capture_channel_enable()` and :cpp:func:`mcpwm_capture_timer_enable()` set up the system services the capture needs; neither starts the measurement yet. :cpp:func:`mcpwm_capture_timer_start()` finally makes the counter run, so edges start being timestamped.

The captured edge values reach the application through a callback, described in the next section.

.. figure:: /../_static/mcpwm/capture_measurement.svg
    :align: center
    :alt: Capture timestamps the rising and falling edges; subtracting them yields the high-pulse width.

    Capture the rising and falling edge timestamps, then subtract to get the high-pulse width.

The two configuration structs are worth reading separately:

Capture timer configuration
---------------------------

.. list::

    - :cpp:member:`group_id <mcpwm_capture_timer_config_t::group_id>` — the MCPWM group the capture timer is allocated from.
    - :cpp:member:`clk_src <mcpwm_capture_timer_config_t::clk_src>` — the clock feeding the capture timer. :c:macro:`MCPWM_CAPTURE_CLK_SRC_DEFAULT` is right for most applications. Pick a specific source when the default one may be gated — for example, in low-power scenarios where a clock that can be switched off would stop the capture timer and corrupt your timestamps.
    - :cpp:member:`resolution_hz <mcpwm_capture_timer_config_t::resolution_hz>` — the tick rate of the capture timer. One tick lasts ``1 / resolution_hz`` seconds, so 1 MHz gives microsecond resolution. It directly sets the precision of every captured timestamp.
    - :cpp:member:`allow_pd <mcpwm_capture_timer_config_t::flags::allow_pd>` — lets the MCPWM power domain switch off during sleep, backing up and restoring the capture registers around the sleep transition at the cost of extra RAM.

Capture channel configuration
-----------------------------

.. list::

    - :cpp:member:`gpio_num <mcpwm_capture_channel_config_t::gpio_num>` — the GPIO carrying the input signal. The driver configures it as an input but does not enable any pull-up or pull-down; if the signal is not actively driven to both levels, call :cpp:func:`gpio_set_pull_mode()` so the pin idles at the level you expect.
    - :cpp:member:`prescale <mcpwm_capture_channel_config_t::prescale>` — input prescale ratio. Same-edge captures (same reported edge type) are spaced about ``prescale`` input periods apart (capture rate ≈ ``input_rate / prescale``). ``0`` or ``1`` means no prescaling (bypass); any other value must be even. Leaving the field at ``0`` (the C default) is therefore bypass. Raise it to extend the measurable period range, at the cost of time resolution. See :ref:`mcpwm-cap-input-prescale` for pipeline order and edge-mode behavior.
    - :cpp:member:`pos_edge <mcpwm_capture_channel_config_t::flags::pos_edge>` and :cpp:member:`neg_edge <mcpwm_capture_channel_config_t::flags::neg_edge>` — which edges are captured. The example captures both, which is what a pulse-width measurement needs.
    - :cpp:member:`invert_cap_signal <mcpwm_capture_channel_config_t::flags::invert_cap_signal>` — inverts the input signal before capture, so a logical ``1`` on the pin is seen as ``0`` by the capture peripheral and vice versa.
    - :cpp:member:`intr_priority <mcpwm_capture_channel_config_t::intr_priority>` — the interrupt priority used by the capture callbacks. Not setting it (``0``) lets the driver choose a low priority.

.. _mcpwm-cap-input-prescale:

Input prescale
==============

The capture channel processes the input in a **fixed, serial** order; the two stages cannot be swapped:

1. **First** divide the GPIO waveform by ``prescale`` (``0``/``1`` means bypass);
2. **Then** use ``pos_edge``/``neg_edge`` to decide which post-prescale events are reported to software.

In other words, hardware does **not** “pick GPIO edges by the configured polarity first, then divide those edges.” With ``prescale > 1``:

- The :cpp:member:`cap_edge <mcpwm_capture_event_data_t::cap_edge>` in the callback may not match the physical GPIO edge;
- Adjacent opposite-edge gaps are **not pulse width**, and **the true duty cycle cannot be recovered**.

Always compute period or frequency from timestamps of the same reported edge type. See the figures below for per-mode timing.

.. note::

    Keep ``prescale = 1`` (bypass) and capture both edges when measuring pulse width or duty cycle. Prefer raising ``prescale`` only for very fast inputs when you only need frequency or period, and capture a single edge type only.

The figures below show how ``prescale`` affects capture timing for rising-only, falling-only, and both-edge modes.

.. figure:: /../_static/mcpwm/capture_prescale_rising.svg
    :align: center
    :alt: Rising-edge only capture with prescale bypass and prescale 4.

Rising-edge only. With ``prescale = 1`` (bypass), every rising edge captures as ``R``. With ``prescale > 1``, the first capture is at cycle ``prescale / 2 - 1``, then every ``prescale``-th rising edge (the figure shows ``prescale = 4``: cycle 1, 5, …). Same-edge spacing is ``prescale`` input periods.

.. figure:: /../_static/mcpwm/capture_prescale_falling.svg
    :align: center
    :alt: Falling-edge only capture with prescale bypass and prescale 4.

Falling-edge only. With ``prescale = 1`` (bypass), every falling edge captures as ``F``. With ``prescale > 1``, GPIO falling edges do not produce captures; **events land on rising steps**. The first capture is at cycle ``prescale - 1``, then every ``prescale`` rising steps, while ``cap_edge`` still reports ``F`` (the figure shows ``prescale = 4``: cycle 3, 7, …).

.. figure:: /../_static/mcpwm/capture_prescale_both.svg
    :align: center
    :alt: Both-edge capture with prescale bypass and prescale 4.

Both edges. With ``prescale = 1`` (bypass), ``R``/``F`` match the physical pin edges. With ``prescale > 1``, captures fire on rising pin steps only (every ``prescale / 2`` periods). The first capture matches rising-only (cycle ``prescale / 2 - 1``) and reports ``R``, then ``R``/``F`` alternate; adjacent ``R``/``F`` spacing is not pulse width. Same-edge gaps remain about ``prescale`` periods. Always compute period or frequency from timestamps of the same reported edge type.

.. warning::

    With ``prescale > 1``, edge polarity can disappear: reported ``R``/``F`` describe the post-prescale event type, not the physical GPIO edge. Falling-edge-only capture in particular can fire on rising pin steps while still reporting ``F``. Do not infer the pin transition from ``cap_edge``, and do not treat adjacent opposite-edge gaps as pulse width.

Capture event callbacks
=======================

The event data tells you the edge and latched count. The calculation below leaves heavy work to a task in a real application.

.. code-block:: c

    static uint32_t rise_tick;
    static bool IRAM_ATTR on_capture(mcpwm_cap_channel_handle_t channel,
                                     const mcpwm_capture_event_data_t *edata,
                                     void *user_data)
    {
        if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
            rise_tick = edata->cap_value;
        } else {
            uint32_t width_ticks = edata->cap_value - rise_tick;
            // Notify a task with width_ticks; do not printf here.
        }
        return false;
    }

    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(cap_channel,
        &(mcpwm_capture_event_callbacks_t) { .on_cap = on_capture }, NULL));

Obtain the actual resolution with :cpp:func:`mcpwm_capture_timer_get_resolution()` before converting ticks to time. On targets where capture shares the MCPWM group clock, create capture and PWM timers in a consistent requested-resolution order.

To measure speed or period, record two timestamps of the same edge type, subtract them to get period ticks, then convert that value to frequency or RPM with the actual capture resolution.

Useful controls
===============

:cpp:func:`mcpwm_capture_channel_trigger_soft_catch()` generates a software capture event, commonly used for testing but also handy to land the timing of important software events on the same capture timebase as hardware edges; it invokes the callback as well. :cpp:func:`mcpwm_capture_get_latched_value()` reads the latest timestamp without registering any callback.

:cpp:func:`mcpwm_capture_timer_stop()` halts the counter, :cpp:func:`mcpwm_capture_channel_disable()` gates an individual input, and stopping the timer gates the whole measurement engine. Call :cpp:func:`mcpwm_capture_timer_disable()` to undo the setup done by :cpp:func:`mcpwm_capture_timer_enable()` before deleting the objects.

Capture timer synchronization
=============================

The capture timer free-runs by default, so the zero point of its count is arbitrary and timestamps can only be compared with each other. Synchronization makes the running capture timer load a given count value when a sync edge arrives, anchoring the timestamps to a meaningful reference.

The most common use is aligning the capture timer with a PWM timer: use the sync emitted by the PWM timer at each period zero (TEZ) as the source and set the count value to 0, so the capture timer restarts from zero every period. A captured timestamp then directly represents the phase within the PWM period. This matters in motor control and power conversion, where feedback edges from a Hall sensor, encoder, or current sense are only meaningful at a specific phase of the PWM cycle.

Sync sources are shared with the PWM timers (GPIO, software, or timer — all in the same MCPWM group). Configure the receiving side with :cpp:func:`mcpwm_capture_timer_set_phase_on_sync()`:

.. code-block:: c

    ESP_ERROR_CHECK(mcpwm_capture_timer_set_phase_on_sync(cap_timer,
        &(mcpwm_capture_timer_sync_phase_config_t) {
            .sync_src = timer_a_sync,  // created with mcpwm_new_timer_sync_src()
            .count_value = 0,
            .direction = MCPWM_TIMER_DIRECTION_UP,
        }));

.. list::

    - :cpp:member:`sync_src <mcpwm_capture_timer_sync_phase_config_t::sync_src>` — the sync source; pass ``NULL`` to detach synchronization.
    - :cpp:member:`count_value <mcpwm_capture_timer_sync_phase_config_t::count_value>` — the count loaded when the sync edge arrives.
    - :cpp:member:`direction <mcpwm_capture_timer_sync_phase_config_t::direction>` — the counting direction after loading; the capture timer only counts up, so it is always :cpp:enumerator:`MCPWM_TIMER_DIRECTION_UP`.

Software and GPIO sync sources can also give the capture timer a known origin or align it to an external reference. See :doc:`synchronization <mcpwm_sync>` for how to create the sync sources and other details.

API Reference
=============

MCPWM Capture Driver Functions
------------------------------

.. include-build-file:: inc/mcpwm_cap.inc
