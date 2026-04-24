# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
from pytest_embedded_idf.utils import soc_filtered_targets


@pytest.mark.generic
@idf_parametrize('target', soc_filtered_targets('SOC_LP_CORE_SUPPORTED == 1'), indirect=['target'])
@pytest.mark.temp_skip_ci(
    targets=['esp32s31'], reason='s31 bringup on this module is not done, TODO: [ESP32S31] IDF-14637'
)
def test_lp_mailbox(dut: Dut) -> None:
    # Wait for LP core to be loaded and running
    dut.expect_exact('LP Mailbox initialized successfully')
    dut.expect_exact('Final sum: 2957')
