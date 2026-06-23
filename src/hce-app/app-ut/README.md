# hce-ut Test Execution Guide

The `hce-ut` application now supports selective test execution by category via command-line switches.

## Building

```powershell
# Windows (MSYS2)
$msys2 = "D:\develop\msys64"
$env:PATH = "$msys2\ucrt64\bin;$msys2\usr\bin;" + $env:PATH
cmake --build cmake-build-debug --target hce-ut
```

## Running Tests

### Default: All Tests

Run all test categories:
```powershell
$msys2 = "D:\develop\msys64"
$env:PATH = "$msys2\ucrt64\bin;$msys2\usr\bin;" + $env:PATH
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe
```

Equivalent to:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-all
```

### Test by Category

Run a specific test category using direct flags (recommended):

#### Security Commands
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-security
```
Tests: authenticateLegacy, authenticateISO, authenticateAES, getKeySettings, getKeyVersion, changeKey, changeKeySettings, authenticationErrors, unknownCommand, sessionKey2K3DES.

#### PICC Level Commands
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-picc
```
Tests: getVersionInfo, getFreeMemory, getCardUID, createApplication, selectApplication, listApplications, deleteApplicationAutoSelect, accessControl, listDFNames, formatCard.

#### Application Level Commands
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-application
```
Tests: standard files, backup files, value files, value file limits, record files (linear/cyclic), file settings, limited credit operations.

#### ISO7816-4 Commands
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-iso
```
Tests: isoSelect, isoSelectById, isoAbsoluteOffset, getIsoFileIDs, isoReadBinary, isoUpdateBinary, isoSFI.

#### Transaction Control
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-transaction
```
Tests: commitTransaction, abortTransaction, clearRecordBehavior, noChangesTransaction, transactionChaining, macingMode, aesEncipheredMode, cmacPlain.


### Alternative Syntax (with space)

For backward compatibility, you can also use:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test security
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test picc
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test application
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test iso
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test transaction
```

### Options

| Option | Description |
|--------|-------------|
| `--test-security` | Run Security Commands Tests |
| `--test-picc` | Run PICC Level Commands Tests |
| `--test-application` | Run Application Level Commands Tests |
| `--test-iso` | Run ISO7816-4 Commands Tests |
| `--test-transaction` | Run Transaction Control Tests |
| `--test-all` | Run all test categories (default) |
| `--pcsc` | Use physical PCSC reader instead of emulated loopback |
| `--verbose` | Enable TRACE_LEVEL logging for DESFire client API |
| `--help`, `-h` | Show help message |

### Examples

Run security tests with verbose logging:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-security --verbose
```

Run ISO tests against a physical card via PCSC:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-iso --pcsc
```

Run all tests with trace-level logging:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-all --verbose
```

Run application tests (data files and records):
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-application
```

Run PICC-level tests (cards, applications):
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-picc
```

Run transaction tests:
```powershell
.\cmake-build-debug\src\hce-app\app-ut\hce-ut.exe --test-transaction
```

## Output

Each test run displays:
- Test category header (e.g., `=== Security Commands Tests ===`)
- Individual test results (PASS/FAIL with details)
- Summary line (e.g., `=== Security Tests Results: 24 passed, 0 failed ===`)
- Exit code: 0 if all passed, 1 if any failed

## Test Categories Structure

### Separated Functions
- `runSecurityTests()` — 10 security-related tests
- `runPiccTests()` — 10 PICC-level tests
- `runApplicationTests()` — 14 application-level tests
- `runIsoTests()` — 6 ISO7816-4 tests
- `runTransactionTests()` — 8 transaction/macing tests
- `runAllTests()` — all tests in sequence

Each function maintains independent `TestContext`, runs selected tests, and returns exit code.

## Notes

- Tests are sequential within each category; state from one test carries to the next
- Teardown (cleanup) occurs at the end of each test via lambda.
- Default emulation mode requires no hardware; `--pcsc` requires a connected NFC reader and virgin DESFire card
- Verbose mode (`--verbose`) shows every APDU exchanged; useful for debugging protocol issues

