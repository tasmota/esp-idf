# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import logging
import sys

import pytest
from test_build_system_helpers import IdfPyFunc


@pytest.mark.skipif(sys.platform == 'win32', reason='Unix test')
@pytest.mark.usefixtures('test_app_copy')
def test_linux_target_build(idf_py: IdfPyFunc) -> None:
    logging.info('Can build for Linux target')
    idf_py('--preview', '-DIDF_TARGET=linux', 'build')
