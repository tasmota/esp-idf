# SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# pylint: disable=W0621  # redefined-outer-name
#
# IDF is using [pytest](https://github.com/pytest-dev/pytest) and
# [pytest-embedded plugin](https://github.com/espressif/pytest-embedded) as its test framework.
#
# if you found any bug or have any question,
# please report to https://github.com/espressif/pytest-embedded/issues
# or discuss at https://github.com/espressif/pytest-embedded/discussions
import os
import sys

if os.path.join(os.path.dirname(__file__), 'tools', 'ci') not in sys.path:
    sys.path.append(os.path.join(os.path.dirname(__file__), 'tools', 'ci'))

if os.path.join(os.path.dirname(__file__), 'tools', 'ci', 'python_packages') not in sys.path:
    sys.path.append(os.path.join(os.path.dirname(__file__), 'tools', 'ci', 'python_packages'))

import glob
import io
import json
import logging
import os
import re
import signal
import time
import typing as t
import zipfile
from copy import deepcopy
from urllib.parse import quote

import common_test_methods  # noqa: F401
import gitlab_api
import pexpect
import pytest
import requests
import yaml
from _pytest.config import Config
from _pytest.fixtures import FixtureRequest
from artifacts_handler import ArtifactType
from dynamic_pipelines.constants import TEST_RELATED_APPS_DOWNLOAD_URLS_FILENAME
from idf_ci_local.app import import_apps_from_txt
from idf_ci_local.uploader import AppDownloader
from idf_ci_local.uploader import AppUploader
from idf_ci_utils import IDF_PATH
from idf_ci_utils import idf_relpath
from idf_pytest.constants import DEFAULT_LOGDIR
from idf_pytest.constants import DEFAULT_SDKCONFIG
from idf_pytest.constants import ENV_MARKERS
from idf_pytest.constants import SPECIAL_MARKERS
from idf_pytest.constants import TARGET_MARKERS
from idf_pytest.constants import PytestCase
from idf_pytest.plugin import IDF_PYTEST_EMBEDDED_KEY
from idf_pytest.plugin import ITEM_PYTEST_CASE_KEY
from idf_pytest.plugin import IdfPytestEmbedded
from idf_pytest.utils import format_case_id
from pytest_embedded.plugin import multi_dut_argument
from pytest_embedded.plugin import multi_dut_fixture
from pytest_embedded.utils import to_bytes
from pytest_embedded.utils import to_str
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.unity_tester import CaseTester
from pytest_embedded_jtag._telnetlib.telnetlib import Telnet  # python 3.13 removed telnetlib, use this instead


############
# Fixtures #
############
@pytest.fixture(scope='session')
def idf_path() -> str:
    return os.path.dirname(__file__)


@pytest.fixture(scope='session')
def session_root_logdir(idf_path: str) -> str:
    """Session scoped log dir for pytest-embedded"""
    return idf_path


@pytest.fixture
def case_tester(unity_tester: CaseTester) -> CaseTester:
    return unity_tester


@pytest.fixture
@multi_dut_argument
def config(request: FixtureRequest) -> str:
    return getattr(request, 'param', None) or DEFAULT_SDKCONFIG  # type: ignore


@pytest.fixture
@multi_dut_fixture
def target(request: FixtureRequest, dut_total: int, dut_index: int) -> str:
    plugin = request.config.stash[IDF_PYTEST_EMBEDDED_KEY]

    if dut_total == 1:
        return plugin.target[0]  # type: ignore

    return plugin.target[dut_index]  # type: ignore


@pytest.fixture
def test_func_name(request: FixtureRequest) -> str:
    return request.node.function.__name__  # type: ignore


@pytest.fixture
def test_case_name(request: FixtureRequest, target: str, config: str) -> str:
    is_qemu = request._pyfuncitem.get_closest_marker('qemu') is not None
    if hasattr(request._pyfuncitem, 'callspec'):
        params = deepcopy(request._pyfuncitem.callspec.params)  # type: ignore
    else:
        params = {}

    filtered_params = {}
    for k, v in params.items():
        if k not in request.session._fixturemanager._arg2fixturedefs:  # type: ignore
            filtered_params[k] = v  # not fixture ones

    return format_case_id(target, config, request.node.originalname, is_qemu=is_qemu, params=filtered_params)  # type: ignore


@pytest.fixture(scope='session')
def pipeline_id(request: FixtureRequest) -> t.Optional[str]:
    return request.config.getoption('pipeline_id', None) or os.getenv('PARENT_PIPELINE_ID', None)  # type: ignore


class BuildReportDownloader(AppDownloader):
    def __init__(self, presigned_url_yaml: str) -> None:
        self.app_presigned_urls_dict: t.Dict[str, t.Dict[str, str]] = yaml.safe_load(presigned_url_yaml)

    def _download_app(self, app_build_path: str, artifact_type: ArtifactType) -> None:
        url = self.app_presigned_urls_dict[app_build_path][artifact_type.value]

        logging.info('Downloading app from %s', url)
        with io.BytesIO() as f:
            for chunk in requests.get(url).iter_content(chunk_size=1024 * 1024):
                if chunk:
                    f.write(chunk)

            f.seek(0)

            with zipfile.ZipFile(f) as zip_ref:
                zip_ref.extractall(IDF_PATH)

    def download_app(self, app_build_path: str, artifact_type: t.Optional[ArtifactType] = None) -> None:
        if app_build_path not in self.app_presigned_urls_dict:
            raise ValueError(
                f'No presigned url found for {app_build_path}. '
                f'Usually this should not happen, please re-trigger a pipeline.'
                f'If this happens again, please report this bug to the CI channel.'
            )

        super().download_app(app_build_path, artifact_type)


class OpenOCD:
    def __init__(self, dut: 'IdfDut'):
        self.MAX_RETRIES = 3
        self.RETRY_DELAY = 1
        self.TELNET_PORT = 4444
        self.dut = dut
        self.telnet: t.Optional[Telnet] = None
        self.log_file = os.path.join(self.dut.logdir, 'ocd.txt')
        self.proc: t.Optional[pexpect.spawn] = None

    def __enter__(self) -> 'OpenOCD':
        return self

    def __exit__(self, exception_type: t.Any, exception_value: t.Any, exception_traceback: t.Any) -> None:
        self.kill()

    def run(self) -> t.Optional['OpenOCD']:
        desc_path = os.path.join(self.dut.app.binary_path, 'project_description.json')

        try:
            with open(desc_path, 'r') as f:
                project_desc = json.load(f)
        except FileNotFoundError:
            logging.error('Project description file not found at %s', desc_path)
            raise

        openocd_scripts = os.getenv('OPENOCD_SCRIPTS')
        if not openocd_scripts:
            raise RuntimeError('OPENOCD_SCRIPTS environment variable is not set.')

        debug_args = project_desc.get('debug_arguments_openocd')
        if not debug_args:
            raise KeyError("'debug_arguments_openocd' key is missing in project_description.json")

        # For debug purposes, make the value '4'
        ocd_env = os.environ.copy()
        ocd_env['LIBUSB_DEBUG'] = '1'

        for _ in range(1, self.MAX_RETRIES + 1):
            try:
                self.proc = pexpect.spawn(
                    command='openocd',
                    args=['-s', openocd_scripts] + debug_args.split(),
                    timeout=5,
                    encoding='utf-8',
                    codec_errors='ignore',
                    env=ocd_env,
                )
                if self.proc and self.proc.isalive():
                    self.proc.expect_exact('Info : Listening on port 3333 for gdb connections', timeout=5)
                    self.connect_telnet()
                    self.write('log_output {}'.format(self.log_file))
                    return self
            except (pexpect.exceptions.EOF, pexpect.exceptions.TIMEOUT, ConnectionRefusedError) as e:
                logging.error('Error running OpenOCD: %s', str(e))
                self.kill()
            time.sleep(self.RETRY_DELAY)

        raise RuntimeError('Failed to run OpenOCD after %d attempts.', self.MAX_RETRIES)

    def connect_telnet(self) -> None:
        for attempt in range(1, self.MAX_RETRIES + 1):
            try:
                self.telnet = Telnet('127.0.0.1', self.TELNET_PORT, 5)
                break
            except ConnectionRefusedError as e:
                logging.error('Error telnet connection: %s in attempt:%d', e, attempt)
                time.sleep(1)
        else:
            raise ConnectionRefusedError

    def write(self, s: str) -> t.Any:
        if self.telnet is None:
            logging.error('Telnet connection is not established.')
            return ''
        resp = self.telnet.read_very_eager()
        self.telnet.write(to_bytes(s, '\n'))
        resp += self.telnet.read_until(b'>')
        return to_str(resp)

    def apptrace_wait_stop(self, timeout: int = 30) -> None:
        stopped = False
        end_before = time.time() + timeout
        while not stopped:
            cmd_out = self.write('esp apptrace status')
            for line in cmd_out.splitlines():
                if line.startswith('Tracing is STOPPED.'):
                    stopped = True
                    break
            if not stopped and time.time() > end_before:
                raise pexpect.TIMEOUT('Failed to wait for apptrace stop!')
            time.sleep(1)

    def kill(self) -> None:
        # Check if the process is still running
        if self.proc and self.proc.isalive():
            self.proc.terminate()
            self.proc.kill(signal.SIGKILL)


@pytest.fixture
def openocd_dut(dut: IdfDut) -> OpenOCD:
    if isinstance(dut, tuple):
        raise ValueError('Multi-DUT support is not implemented yet')
    return OpenOCD(dut)


@pytest.fixture(scope='session')
def app_downloader(pipeline_id: t.Optional[str]) -> t.Optional[AppDownloader]:
    if not pipeline_id:
        return None

    if (
        'IDF_S3_BUCKET' in os.environ
        and 'IDF_S3_ACCESS_KEY' in os.environ
        and 'IDF_S3_SECRET_KEY' in os.environ
        and 'IDF_S3_SERVER' in os.environ
        and 'IDF_S3_BUCKET' in os.environ
    ):
        return AppUploader(pipeline_id)

    logging.info('Downloading build report from the build pipeline %s', pipeline_id)
    test_app_presigned_urls_file = None

    gl = gitlab_api.Gitlab(os.getenv('CI_PROJECT_ID', 'espressif/esp-idf'))

    for child_pipeline in gl.project.pipelines.get(pipeline_id, lazy=True).bridges.list(iterator=True):
        if child_pipeline.name == 'build_child_pipeline':
            for job in gl.project.pipelines.get(child_pipeline.downstream_pipeline['id'], lazy=True).jobs.list(
                iterator=True
            ):
                if job.name == 'generate_pytest_build_report':
                    test_app_presigned_urls_file = gl.download_artifact(
                        job.id, [TEST_RELATED_APPS_DOWNLOAD_URLS_FILENAME]
                    )[0]
                    break

    if test_app_presigned_urls_file:
        return BuildReportDownloader(test_app_presigned_urls_file)

    return None


@pytest.fixture
@multi_dut_fixture
def build_dir(
    request: FixtureRequest,
    app_path: str,
    target: t.Optional[str],
    config: t.Optional[str],
    app_downloader: t.Optional[AppDownloader],
) -> str:
    """
    Check local build dir with the following priority:

    1. build_<target>_<config>
    2. build_<target>
    3. build_<config>
    4. build

    Returns:
        valid build directory
    """
    # download from minio on CI
    case: PytestCase = request._pyfuncitem.stash[ITEM_PYTEST_CASE_KEY]
    if app_downloader:
        # somehow hardcoded...
        app_build_path = os.path.join(idf_relpath(app_path), f'build_{target}_{config}')
        if case.requires_elf_or_map:
            app_downloader.download_app(app_build_path)
        else:
            app_downloader.download_app(app_build_path, ArtifactType.BUILD_DIR_WITHOUT_MAP_AND_ELF_FILES)
        check_dirs = [f'build_{target}_{config}']
    else:
        check_dirs = []
        build_dir_arg = request.config.getoption('build_dir', None)
        if build_dir_arg:
            check_dirs.append(build_dir_arg)
        if target is not None and config is not None:
            check_dirs.append(f'build_{target}_{config}')
        if target is not None:
            check_dirs.append(f'build_{target}')
        if config is not None:
            check_dirs.append(f'build_{config}')
        check_dirs.append('build')

    for check_dir in check_dirs:
        binary_path = os.path.join(app_path, check_dir)
        if os.path.isdir(binary_path):
            logging.info(f'found valid binary path: {binary_path}')
            return check_dir

        logging.warning('checking binary path: %s... missing... try another place', binary_path)

    raise ValueError(
        f'no build dir valid. Please build the binary via "idf.py -B {check_dirs[0]} build" and run pytest again'
    )


@pytest.fixture(autouse=True)
@multi_dut_fixture
def junit_properties(test_case_name: str, record_xml_attribute: t.Callable[[str, object], None]) -> None:
    """
    This fixture is autoused and will modify the junit report test case name to <target>.<config>.<case_name>
    """
    record_xml_attribute('name', test_case_name)


@pytest.fixture(autouse=True)
@multi_dut_fixture
def ci_job_url(record_xml_attribute: t.Callable[[str, object], None]) -> None:
    if ci_job_url := os.getenv('CI_JOB_URL'):
        record_xml_attribute('ci_job_url', ci_job_url)


@pytest.fixture(autouse=True)
def set_test_case_name(request: FixtureRequest, test_case_name: str) -> None:
    request.node.funcargs['test_case_name'] = test_case_name


@pytest.fixture(autouse=True)
def set_dut_log_url(record_xml_attribute: t.Callable[[str, object], None], _pexpect_logfile: str) -> t.Generator:
    # Record the "dut_log_url" attribute in the XML report once test execution finished
    yield

    if not isinstance(_pexpect_logfile, str):
        record_xml_attribute('dut_log_url', 'No log URL found')
        return

    ci_pages_url = os.getenv('CI_PAGES_URL')
    logdir_pattern = re.compile(rf'({DEFAULT_LOGDIR}/.*)')
    match = logdir_pattern.search(_pexpect_logfile)

    if not match:
        record_xml_attribute('dut_log_url', 'No log URL found')
        return

    if not ci_pages_url:
        record_xml_attribute('dut_log_url', _pexpect_logfile)
        return

    job_id = os.getenv('CI_JOB_ID', '0')
    modified_ci_pages_url = ci_pages_url.replace('esp-idf', '-/esp-idf')
    log_url = f'{modified_ci_pages_url}/-/jobs/{job_id}/artifacts/{match.group(1)}'

    record_xml_attribute('dut_log_url', log_url)


######################
# Log Util Functions #
######################
@pytest.fixture
def log_performance(record_property: t.Callable[[str, object], None]) -> t.Callable[[str, str], None]:
    """
    log performance item with pre-defined format to the console
    and record it under the ``properties`` tag in the junit report if available.
    """

    def real_func(item: str, value: str) -> None:
        """
        :param item: performance item name
        :param value: performance value
        """
        logging.info('[Performance][%s]: %s', item, value)
        record_property(item, value)

    return real_func


@pytest.fixture
def check_performance(idf_path: str) -> t.Callable[[str, float, str], None]:
    """
    check if the given performance item meets the passing standard or not
    """

    def real_func(item: str, value: float, target: str) -> None:
        """
        :param item: performance item name
        :param value: performance item value
        :param target: target chip
        :raise: AssertionError: if check fails
        """

        def _find_perf_item(operator: str, path: str) -> float:
            with open(path, encoding='utf-8') as f:
                data = f.read()
            match = re.search(rf'#define\s+IDF_PERFORMANCE_{operator}_{item.upper()}\s+([\d.]+)', data)
            return float(match.group(1))  # type: ignore

        def _check_perf(operator: str, standard_value: float) -> None:
            if operator == 'MAX':
                ret = value <= standard_value
            else:
                ret = value >= standard_value
            if not ret:
                raise AssertionError(
                    f"[Performance] {item} value is {value}, doesn't meet pass standard {standard_value}"
                )

        path_prefix = os.path.join(idf_path, 'components', 'idf_test', 'include')
        performance_files = (
            os.path.join(path_prefix, target, 'idf_performance_target.h'),
            os.path.join(path_prefix, 'idf_performance.h'),
        )

        found_item = False
        for op in ['MIN', 'MAX']:
            for performance_file in performance_files:
                try:
                    standard = _find_perf_item(op, performance_file)
                except (OSError, AttributeError):
                    # performance file doesn't exist or match is not found in it
                    continue

                _check_perf(op, standard)
                found_item = True
                break

        if not found_item:
            raise AssertionError(f'Failed to get performance standard for {item}')

    return real_func


@pytest.fixture
def log_minimum_free_heap_size(dut: IdfDut, config: str, idf_path: str) -> t.Callable[..., None]:
    def real_func() -> None:
        res = dut.expect(r'Minimum free heap size: (\d+) bytes')
        logging.info(
            '\n------ heap size info ------\n'
            '[app_path] {}\n'
            '[app_name] {}\n'
            '[config_name] {}\n'
            '[target] {}\n'
            '[minimum_free_heap_size] {} Bytes\n'
            '------ heap size end ------'.format(
                dut.app.app_path.replace(idf_path, '').lstrip('/\\'),
                os.path.basename(dut.app.app_path),
                config,
                dut.target,
                res.group(1).decode('utf8'),
            )
        )

    return real_func


@pytest.fixture(scope='session')
def dev_password(request: FixtureRequest) -> str:
    return request.config.getoption('dev_passwd') or ''


@pytest.fixture(scope='session')
def dev_user(request: FixtureRequest) -> str:
    return request.config.getoption('dev_user') or ''


##################
# Hook functions #
##################
def pytest_addoption(parser: pytest.Parser) -> None:
    idf_group = parser.getgroup('idf')
    idf_group.addoption(
        '--sdkconfig',
        help='sdkconfig postfix, like sdkconfig.ci.<config>. (Default: None, which would build all found apps)',
    )
    idf_group.addoption(
        '--dev-user',
        help='user name associated with some specific device/service used during the test execution',
    )
    idf_group.addoption(
        '--dev-passwd',
        help='password associated with some specific device/service used during the test execution',
    )
    idf_group.addoption(
        '--app-info-filepattern',
        help='glob pattern to specify the files that include built app info generated by '
        '`idf-build-apps --collect-app-info ...`. will not raise ValueError when binary '
        'paths not exist in local file system if not listed recorded in the app info.',
    )
    idf_group.addoption(
        '--pipeline-id',
        help='main pipeline id, not the child pipeline id. Specify this option to download the artifacts '
        'from the minio server for debugging purpose.',
    )


def pytest_configure(config: Config) -> None:
    from idf_pytest.constants import PREVIEW_TARGETS
    from idf_pytest.constants import SUPPORTED_TARGETS
    from pytest_embedded_idf.utils import preview_targets
    from pytest_embedded_idf.utils import supported_targets

    supported_targets.set(SUPPORTED_TARGETS)
    preview_targets.set(PREVIEW_TARGETS)

    # cli option "--target"
    target = [_t.strip().lower() for _t in (config.getoption('target', '') or '').split(',') if _t.strip()]

    # add markers based on idf_pytest/constants.py
    for name, description in {
        **TARGET_MARKERS,
        **ENV_MARKERS,
        **SPECIAL_MARKERS,
    }.items():
        config.addinivalue_line('markers', f'{name}: {description}')

    help_commands = ['--help', '--fixtures', '--markers', '--version']
    for cmd in help_commands:
        if cmd in config.invocation_params.args:
            target = ['unneeded']
            break

    markexpr = config.getoption('markexpr') or ''
    # check marker expr set via "pytest -m"
    if not target and markexpr:
        # we use `-m "esp32 and generic"` in our CI to filter the test cases
        # this doesn't cover all use cases, but fit what we do in CI.
        for marker in markexpr.split('and'):
            marker = marker.strip()
            if marker in TARGET_MARKERS:
                target.append(marker)

    # "--target" must be set
    if not target:
        raise SystemExit(
            """Pass `--target TARGET[,TARGET...]` to specify all targets the test cases are using.
    - for single DUT, we run with `pytest --target esp32`
    - for multi DUT, we run with `pytest --target esp32,esp32,esp32s2` to indicate all DUTs
"""
        )

    apps = None
    app_info_filepattern = config.getoption('app_info_filepattern')
    if app_info_filepattern:
        apps = []
        for f in glob.glob(os.path.join(IDF_PATH, app_info_filepattern)):
            apps.extend(import_apps_from_txt(f))

    if '--collect-only' not in config.invocation_params.args:
        config.stash[IDF_PYTEST_EMBEDDED_KEY] = IdfPytestEmbedded(
            config_name=config.getoption('sdkconfig'),
            target=target,
            apps=apps,
        )
        config.pluginmanager.register(config.stash[IDF_PYTEST_EMBEDDED_KEY])


def pytest_unconfigure(config: Config) -> None:
    _pytest_embedded = config.stash.get(IDF_PYTEST_EMBEDDED_KEY, None)
    if _pytest_embedded:
        del config.stash[IDF_PYTEST_EMBEDDED_KEY]
        config.pluginmanager.unregister(_pytest_embedded)


dut_artifacts_url = []


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):  # type: ignore
    outcome = yield
    report = outcome.get_result()
    report.sections = []
    if report.failed:
        _dut = item.funcargs.get('dut')
        if not _dut:
            return

        job_id = os.getenv('CI_JOB_ID', 0)
        url = os.getenv('CI_PAGES_URL', '').replace('esp-idf', '-/esp-idf')
        template = f'{url}/-/jobs/{job_id}/artifacts/{DEFAULT_LOGDIR}/{{}}'
        logs_files = []

        def get_path(x: str) -> str:
            return x.split(f'{DEFAULT_LOGDIR}/', 1)[1]

        if isinstance(_dut, list):
            logs_files.extend([template.format(get_path(d.logfile)) for d in _dut])
            dut_artifacts_url.append('{}:'.format(_dut[0].test_case_name))
        else:
            logs_files.append(template.format(get_path(_dut.logfile)))
            dut_artifacts_url.append('{}:'.format(_dut.test_case_name))

        for file in logs_files:
            dut_artifacts_url.append('    - {}'.format(quote(file, safe=':/')))


def pytest_terminal_summary(terminalreporter, exitstatus, config):  # type: ignore
    if dut_artifacts_url:
        terminalreporter.ensure_newline()
        terminalreporter.section('Failed Test Artifacts URL', sep='-', red=True, bold=True)
        terminalreporter.line('\n'.join(dut_artifacts_url))
