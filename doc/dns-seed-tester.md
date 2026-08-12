# DNS Seed Testing Tool

A standalone utility to test DNS seed servers for the Rincoin network.

## Overview

This tool queries DNS seed servers using the same methods as the Rincoin Core client, making it ideal for:
- Testing your own DNS seeder implementation
- Verifying DNS seed configuration
- Debugging connectivity issues
- Monitoring DNS seed health

## Building

### Build

`test-dns-seeds` is a `noinst_PROGRAMS` target of the normal build, so an
ordinary build produces it at `src/test-dns-seeds`:

```bash
./autogen.sh && ./configure && make
```

### Building with Rincoin Core

If you're building the full Rincoin Core:

```bash
./autogen.sh
./configure
make
# The tool will be built as: src/test-dns-seeds
```

### Manual Build

If you need to build manually:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I. -Isrc \
    src/test-dns-seeds.cpp \
    src/netbase.cpp \
    src/netaddress.cpp \
    src/chainparams.cpp \
    [... other required source files ...] \
    -lpthread -lboost_system -lboost_filesystem -lboost_thread \
    -o test-dns-seeds
```

## Usage

### Basic Usage

Test all configured mainnet DNS seeds:
```bash
./test-dns-seeds
```

Test testnet seeds:
```bash
./test-dns-seeds -testnet
```

### Advanced Options

```
Usage: test-dns-seeds [options]

Options:
  -testnet              Test testnet seeds instead of mainnet
  -regtest              Test regtest seeds instead of mainnet
  -maxips=<n>           Maximum number of IPs to request from each seed (default: 256)
  -seed=<hostname>      Test only a specific seed (can be used multiple times)
  -servicebits=<hex>    Query for specific service bits (default: 0x0)
  -timeout=<n>          Connection timeout in seconds (default: 5)
  -verbose              Show detailed output
  -help                 Display help message
```

### Examples

Test a specific DNS seed:
```bash
./test-dns-seeds -seed=seed.rincoin.tech
```

Test with specific service bits (e.g., NODE_NETWORK = 0x1):
```bash
./test-dns-seeds -servicebits=1
```

Get verbose output showing all returned IP addresses:
```bash
./test-dns-seeds -verbose
```

Test multiple specific seeds:
```bash
./test-dns-seeds -seed=seed1.rincoin.tech -seed=seed2.rincoin.tech
```

Limit the number of IPs requested:
```bash
./test-dns-seeds -maxips=50
```

## Output

The tool provides:
- Status of each DNS seed query (success/failure)
- Number of addresses returned by each seed
- Sample of returned IP addresses (first and last 5)
- Summary statistics:
  - Total seeds tested
  - Success rate
  - Total addresses returned
  - Average addresses per seed

### Exit Codes

- `0`: All tests passed successfully
- `1`: All DNS seed queries failed
- `2`: Some DNS seed queries failed

### Example Output

```
======================================================================
 Rincoin DNS Seed Tester
======================================================================

Start Time: 2026-02-05 10:30:45
Network: main
Max IPs per seed: 256
Service Bits: 0x0
Timeout: 5 seconds

Found 1 DNS seed(s) to test:
  1. seed.rincoin.tech

======================================================================
 Testing DNS Seeds
======================================================================

[1/1] Testing: seed.rincoin.tech
----------------------------------------------------------------------
Query hostname: seed.rincoin.tech
Resolving... OK
Addresses returned: 125

First 5 addresses:
  31.220.93.115
  46.250.248.103
  113.150.233.75
  203.45.67.89
  192.168.1.100
  ... (115 more) ...
Last 5 addresses:
  10.0.0.5
  172.16.0.10
  192.168.100.50
  8.8.8.8
  1.2.3.4

======================================================================
 Summary
======================================================================
Total seeds tested: 1
Successful queries: 1 (100%)
Failed queries: 0
Total addresses returned: 125
Average addresses per seed: 125

End Time: 2026-02-05 10:30:47

✓ All DNS seeds are working correctly!
```

## How It Works

The tool mimics the DNS seed resolution logic from Rincoin Core's `ThreadDNSAddressSeed()` function:

1. Reads DNS seed hostnames from chainparams (just like the Core client)
2. Optionally prepends service bit filter (e.g., `x1.seed.rincoin.tech`)
3. Performs DNS lookup using `LookupHost()` (same function as Core)
4. Displays all returned IP addresses
5. Provides success/failure statistics

## Testing Your DNS Seeder

When developing a DNS seeder, use this tool to verify:

1. **Basic Connectivity**: Can the seed hostname be resolved?
   ```bash
   ./test-dns-seeds -seed=your-seed.example.com
   ```

2. **Service Bit Filtering**: Does your seeder support service bit queries?
   ```bash
   ./test-dns-seeds -seed=your-seed.example.com -servicebits=1
   ```

3. **Response Size**: Are you returning an appropriate number of addresses?
   ```bash
   ./test-dns-seeds -seed=your-seed.example.com -verbose
   ```

4. **Network-Specific Seeds**: Test each network separately
   ```bash
   ./test-dns-seeds -seed=your-seed.example.com  # mainnet
   ./test-dns-seeds -seed=your-seed.example.com -testnet
   ```

## Troubleshooting

### "DNS lookup failed"
- Check that the hostname is correct and resolvable
- Verify your DNS server is accessible
- Try with `-verbose` for more details

### "Seed responded but returned no addresses"
- Your seeder is online but has no peers to return
- Check your seeder's peer database
- Verify your seeder is actually crawling the network

### Build errors
- Ensure you have built Rincoin Core first (`./configure && make`)
- Check that all dependencies are installed

## Integration with CI/CD

You can integrate this tool into your CI/CD pipeline:

```bash
#!/bin/bash
# Check DNS seeds health
./test-dns-seeds -timeout=10
if [ $? -ne 0 ]; then
    echo "DNS seed check failed!"
    exit 1
fi
```

## Files

- `src/test-dns-seeds.cpp` - Main tool implementation
- `src/Makefile.am` - Build wiring (`noinst_PROGRAMS`)
- `doc/dns-seed-tester.md` - This README

## See Also

- Rincoin DNS Seeder: https://github.com/rincoin/...
- Bitcoin DNS seed protocol: https://github.com/bitcoin/bitcoin/blob/master/doc/dnsseed-policy.md
- BIP 155 (addr v2): https://github.com/bitcoin/bips/blob/master/bip-0155.mediawiki

## License

MIT License - Same as Rincoin Core
