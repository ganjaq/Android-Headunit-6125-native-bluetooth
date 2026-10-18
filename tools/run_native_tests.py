#!/usr/bin/env python3
"""Linux-only offline transaction/snapshot tests; no ADB or Android actions."""
from pathlib import Path
import subprocess,tempfile,sys
base=Path(__file__).resolve().parents[1];s=base/'src/session'
tests={
 'transaction':['ngd_native_transaction_test.cpp','ngd_native_transaction.cpp','ngd_native_session_client.cpp'],
 'snapshot':['ngd_btdata_snapshot_test.cpp','ngd_btdata_snapshot.cpp'],
 'heartbeat':['ngd14_host_heartbeat_test.cpp'],
}
with tempfile.TemporaryDirectory(prefix='ngd-native-tests-') as tmp:
    for name,files in tests.items():
        exe=Path(tmp)/name
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-pthread','-I'+str(s),*[str(s/x) for x in files],'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('OFFLINE_NATIVE_TESTS_PASS; NO_DEVICE_ACCESSED')
