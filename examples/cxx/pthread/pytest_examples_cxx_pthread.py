# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['supported_targets'], indirect=['target'])
def test_examples_cpp_pthread(dut: IdfDut) -> None:
    dut.expect(
        [
            r'pthread: This thread \(with the default name\) may run on any core.'
            r'Core id: [01], prio: 5, minimum free stack: \d+ bytes\.',
            r'Thread [12]: Core id: [01], prio: 5, minimum free stack: \d+ bytes\.',
            r'Thread [12]: This is the INHERITING thread with the same parameters as our parent, '
            r'including name. Core id: [01], prio: 5, minimum free stack: \d+ bytes\.',
            r'Thread [12]: Core id: [01], prio: 5, minimum free stack: \d+ bytes\.',
        ],
        expect_all=True,
    )
