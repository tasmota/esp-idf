=========================
MCPWM 捕获：测量输入脉冲
=========================

.. contents::
    :local:
    :depth: 2

捕获是独立的 MCPWM 路径：捕获定时器为捕获通道 GPIO 的边沿打时间戳，不需要 PWM 定时器、操作器、比较器或生成器，适用于回波脉冲、转速计、霍尔传感器和 RC 接收机。

它适合"把外部世界发生的时刻带进芯片里"。当你关心的是脉宽、周期、相位差或转速，而不是输出 PWM，本页就是 MCPWM 的另一条主线入口。

测量脉宽
========

同时捕获两个边沿，保存上升沿时间戳，再用下降沿时间戳减去它。1 MHz 分辨率下，差值的单位直接是微秒。

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

仅分配对象并不会开始测量。还需要使能通道并启动捕获定时器：

.. code-block:: c

    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(cap_channel));
    ESP_ERROR_CHECK(mcpwm_capture_timer_enable(cap_timer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_start(cap_timer));

:cpp:func:`mcpwm_capture_channel_enable()` 和 :cpp:func:`mcpwm_capture_timer_enable()` 负责捕获所需的系统服务准备，二者都尚未开始测量。:cpp:func:`mcpwm_capture_timer_start()` 才真正启动计数器，边沿开始被打上时间戳。

捕获到的边沿值通过回调送达应用，详见下一节。

.. figure:: /../_static/mcpwm/capture_measurement.svg
    :align: center
    :alt: 捕获上升沿与下降沿时间戳，相减得到高电平脉宽。

    捕获上升沿和下降沿时间戳，相减得到高电平脉宽。

两个配置结构体分开讲解：

捕获定时器配置
--------------

.. list::

    - :cpp:member:`group_id <mcpwm_capture_timer_config_t::group_id>` — 捕获定时器从哪个 MCPWM 组分配。
    - :cpp:member:`clk_src <mcpwm_capture_timer_config_t::clk_src>` — 捕获定时器的时钟源。:c:macro:`MCPWM_CAPTURE_CLK_SRC_DEFAULT` 适合绝大多数应用。当默认时钟源可能被关闭时需要显式选择其他源——例如低功耗场景下，若所选时钟被关断，捕获定时器会停止，时间戳就会失真。
    - :cpp:member:`resolution_hz <mcpwm_capture_timer_config_t::resolution_hz>` — 捕获定时器的 Tick 频率。一个 Tick 持续 ``1 / resolution_hz`` 秒，1 MHz 即微秒级分辨率，直接决定每次捕获时间戳的精度。
    - :cpp:member:`allow_pd <mcpwm_capture_timer_config_t::flags::allow_pd>` — 允许睡眠时关闭 MCPWM 电源域，在睡眠前后备份并恢复捕获寄存器，代价是额外占用 RAM。

捕获通道配置
------------

.. list::

    - :cpp:member:`gpio_num <mcpwm_capture_channel_config_t::gpio_num>` — 承载输入信号的 GPIO。驱动会把它配成输入，但不会设置上拉或下拉；若信号并非主动驱动到两个电平，请调用 :cpp:func:`gpio_set_pull_mode()`，让引脚空闲时保持在期望电平。
    - :cpp:member:`prescale <mcpwm_capture_channel_config_t::prescale>` — 输入预分频比。同沿（相同上报边沿类型）两次捕获的间隔约为 ``prescale`` 个输入周期（捕获速率 ≈ ``输入速率 / prescale``）。``0`` 或 ``1`` 表示不分频（bypass）；其它值必须为偶数。结构体里不写该字段时默认为 ``0``，即 bypass。提高它可扩展可测周期范围，但会降低时间分辨率。流水线顺序与各边沿模式下的行为见 :ref:`mcpwm-cap-input-prescale`。
    - :cpp:member:`pos_edge <mcpwm_capture_channel_config_t::flags::pos_edge>` 和 :cpp:member:`neg_edge <mcpwm_capture_channel_config_t::flags::neg_edge>` — 捕获哪些边沿。示例同时捕获两个边沿，这正是脉宽测量所需的。
    - :cpp:member:`invert_cap_signal <mcpwm_capture_channel_config_t::flags::invert_cap_signal>` — 捕获前反相输入信号，让引脚上的逻辑 ``1`` 在捕获外设看来是 ``0``，反之亦然。
    - :cpp:member:`intr_priority <mcpwm_capture_channel_config_t::intr_priority>` — 捕获回调使用的中断优先级。不设置（``0``）时由驱动选择较低优先级。

.. _mcpwm-cap-input-prescale:

输入预分频
==========

捕获通道内部按**固定顺序**串行处理输入，两级不能对调：

1. 先用 ``prescale`` 对 GPIO 波形做预分频（``0``/``1`` 为 bypass）；
2. 再根据 ``pos_edge``/``neg_edge`` 决定哪些分频后的事件上报给软件。

也就是说，硬件不是“先按设定的边沿挑选 GPIO，再对选中的边沿分频”。因此 ``prescale > 1`` 时：

- 回调里的 :cpp:member:`cap_edge <mcpwm_capture_event_data_t::cap_edge>` 不一定等于 GPIO 的物理边沿；
- 相邻异沿间隔**不是真实脉宽**，**无法还原真实占空比**。

请用相同上报类型（同沿）的时间戳计算周期或频率。各边沿模式下的具体时序见下方示意图。

.. note::

    测脉宽或占空比时保持 ``prescale = 1`` （bypass）并捕获双边沿。建议仅在输入过快、只需测量频率/周期时再提高 ``prescale``，且只捕获单边沿。

下图说明 ``prescale`` 在仅上升沿、仅下降沿和双边沿模式下如何影响捕获时机。

.. figure:: /../_static/mcpwm/capture_prescale_rising.svg
    :align: center
    :alt: 仅上升沿捕获：prescale bypass 与 prescale 4。

仅上升沿。``prescale = 1`` （bypass）时每个上升沿捕获为 ``R``。``prescale > 1`` 时，首次捕获在 cycle ``prescale / 2 - 1``，之后每隔 ``prescale`` 个上升沿一次（图中 ``prescale = 4``：cycle 1、5、…）。同沿间隔为 ``prescale`` 个输入周期。

.. figure:: /../_static/mcpwm/capture_prescale_falling.svg
    :align: center
    :alt: 仅下降沿捕获：prescale bypass 与 prescale 4。

仅下降沿。``prescale = 1`` （bypass）时每个下降沿捕获为 ``F``。``prescale > 1`` 时 GPIO 下降沿不产生捕获；**事件落在上升沿步骤上**。首次捕获在 cycle ``prescale - 1``，之后每隔 ``prescale`` 个上升沿一次，``cap_edge`` 仍报 ``F`` （图中 ``prescale = 4``：cycle 3、7、…）。

.. figure:: /../_static/mcpwm/capture_prescale_both.svg
    :align: center
    :alt: 双边沿捕获：prescale bypass 与 prescale 4。

双边沿。``prescale = 1`` （bypass）时 ``R``/``F`` 对应真实引脚边沿。``prescale > 1`` 时只在上升沿步骤触发（每隔 ``prescale / 2`` 个周期）。首次与仅上升沿相同（cycle ``prescale / 2 - 1``）且报 ``R``，随后 ``R``/``F`` 交替；相邻 ``R``/``F`` 间隔不是脉宽。同沿间隔仍约为 ``prescale`` 个周期。请用相同上报类型（同沿）的时间戳计算周期或频率。

.. warning::

    ``prescale > 1`` 时边沿极性可能“消失”：上报的 ``R``/``F`` 描述的是分频之后的事件类型，不是 GPIO 物理边沿。尤其是仅下降沿路径会出现“上升沿步骤却报 ``F``”——不要用它推断引脚真实跳变，也不要用相邻异沿去算脉宽。

捕获事件回调
============

事件数据告知边沿类型和锁存计数值。真实应用中应将耗时工作留给任务。

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
            // 使用 ISR 安全方式将 width_ticks 通知任务
        }
        return false;
    }

    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(cap_channel,
        &(mcpwm_capture_event_callbacks_t) { .on_cap = on_capture }, NULL));

通过 :cpp:func:`mcpwm_capture_timer_get_resolution()` 获取实际分辨率后，再将 Tick 转换为时间。在捕获时钟与 MCPWM 组时钟共享的芯片上，应保持一致的请求分辨率顺序创建捕获和 PWM 定时器。

若要测转速或周期，可记录两次同类边沿（例如两次上升沿）的时间戳，相减得到周期 Tick 数，再结合实际分辨率换算为频率或转速。

实用控制
========

:cpp:func:`mcpwm_capture_channel_trigger_soft_catch()` 生成软件捕获事件，常用于测试，也可借此把重要软件事件的发生时刻落到捕获时间轴上，与硬件边沿时间戳对齐；该调用同样会触发回调。:cpp:func:`mcpwm_capture_get_latched_value()` 在不注册回调的情况下也能读取最新时间戳。

:cpp:func:`mcpwm_capture_timer_stop()` 使计数器停止，:cpp:func:`mcpwm_capture_channel_disable()` 可关闭单个输入，停止定时器则关闭整个测量引擎。删除对象前，先调用 :cpp:func:`mcpwm_capture_timer_disable()` 撤销 :cpp:func:`mcpwm_capture_timer_enable()` 做的准备工作。

捕获定时器同步
==============

捕获定时器默认自由运行，计数值的“零点”是任意的，时间戳只能互相比较。同步让运行中的捕获定时器在同步边沿到来时加载一个指定计数值，从而把时间戳对应到有意义的参考系。

最常见的场景是让捕获定时器与 PWM 定时器对齐相位：以 PWM 定时器每周期零（TEZ）发出的同步为源、计数值设为 0，捕获定时器每个周期归零，捕获时间戳就直接表示周期内的相位。这在电机控制或功率变换中尤为重要——霍尔、编码器或电流检测的反馈边沿只有落在 PWM 周期的具体相位才有意义。

同步源与 PWM 定时器共用（GPIO、软件、定时器三种，须与捕获定时器同组），接收端用 :cpp:func:`mcpwm_capture_timer_set_phase_on_sync()` 配置：

.. code-block:: c

    ESP_ERROR_CHECK(mcpwm_capture_timer_set_phase_on_sync(cap_timer,
        &(mcpwm_capture_timer_sync_phase_config_t) {
            .sync_src = timer_a_sync,  // 由 mcpwm_new_timer_sync_src() 创建的同步源
            .count_value = 0,
            .direction = MCPWM_TIMER_DIRECTION_UP,
        }));

.. list::

    - :cpp:member:`sync_src <mcpwm_capture_timer_sync_phase_config_t::sync_src>` — 同步源，设为 ``NULL`` 可解除同步。
    - :cpp:member:`count_value <mcpwm_capture_timer_sync_phase_config_t::count_value>` — 同步边沿到达时加载的计数值。
    - :cpp:member:`direction <mcpwm_capture_timer_sync_phase_config_t::direction>` — 加载后的计数方向，捕获定时器只向上计数，固定为 :cpp:enumerator:`MCPWM_TIMER_DIRECTION_UP`。

软件同步与 GPIO 同步源也可让捕获定时器建立已知起点或对齐外部参考，同步源的创建与其余用法详见 :doc:`同步 <mcpwm_sync>`。

API 参考
========

MCPWM 捕获驱动函数
------------------

.. include-build-file:: inc/mcpwm_cap.inc
