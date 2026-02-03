#ifndef EMP_VOLE_PORTABLE_H__
#define EMP_VOLE_PORTABLE_H__

/*
 * Portable VOLE - links original emp-zk code with minimal shim
 * - No AES-NI/SSE dependency (uses BLAKE3)
 * - No OpenSSL dependency (uses mock base VOLE)
 * - Includes malicious security checks from original code
 */

// Step 1: Include the shim (provides block, PRG, Hash, ThreadPool, NetIO, OTPre)
#include "emp_tool_shim.h"

// Step 2: Include utility.h (uses EMP_PORTABLE branch automatically)
#include "utility.h"

// Step 3: Include original VOLE components (they use the shim types)
#include "base_cot_mock.h"
#include "base_svole_direct_mock.h"
#include "spfss_sender_blake3.h"
#include "spfss_recver_blake3.h"
#include "mpfss_reg_blake3.h"
#include "lpn_blake3.h"
#include "vole_triple_blake3.h"

#endif // EMP_VOLE_PORTABLE_H__
