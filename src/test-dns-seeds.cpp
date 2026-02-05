// Copyright (c) 2025 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * DNS Seed Testing Tool
 * 
 * This standalone utility tests DNS seed servers by querying them using the same
 * methods that the Rincoin Core client uses. It helps verify that DNS seeders are
 * properly configured and returning valid peer addresses.
 */

#include <chainparams.h>
#include <netbase.h>
#include <netaddress.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <logging.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <ctime>

// Service flags that nodes should support (from protocol.h)
static const uint64_t REQUIRED_SERVICE_BITS = 0x0;  // No specific requirements for testing

void PrintUsage()
{
    std::cout << "Usage: test-dns-seeds [options]\n\n"
              << "Test DNS seed servers for the Rincoin network.\n\n"
              << "Options:\n"
              << "  -testnet              Test testnet seeds instead of mainnet\n"
              << "  -regtest              Test regtest seeds instead of mainnet\n"
              << "  -maxips=<n>           Maximum number of IPs to request from each seed (default: 256)\n"
              << "  -seed=<hostname>      Test only a specific seed (can be used multiple times)\n"
              << "  -servicebits=<hex>    Query for specific service bits (default: 0x0)\n"
              << "  -timeout=<n>          Connection timeout in seconds (default: 5)\n"
              << "  -verbose              Show detailed output\n"
              << "  -help                 Display this help message\n"
              << std::endl;
}

void PrintHeader(const std::string& title)
{
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(70, '=') << "\n" << std::endl;
}

void PrintSeedInfo(const std::string& seed, int index, int total)
{
    std::cout << "[" << (index + 1) << "/" << total << "] Testing: " << seed << "\n";
    std::cout << std::string(70, '-') << std::endl;
}

std::string TimeStamp()
{
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

int main(int argc, char* argv[])
{
    // Parse command line arguments
    std::string network = "main";
    unsigned int maxIPs = 256;
    std::vector<std::string> specificSeeds;
    uint64_t serviceBits = REQUIRED_SERVICE_BITS;
    int timeout = DEFAULT_CONNECT_TIMEOUT / 1000;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        
        if (arg == "-help" || arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        }
        else if (arg == "-testnet") {
            network = "test";
        }
        else if (arg == "-regtest") {
            network = "regtest";
        }
        else if (arg.substr(0, 8) == "-maxips=") {
            try {
                maxIPs = std::stoi(arg.substr(8));
            } catch (...) {
                std::cerr << "Error: Invalid maxips value\n";
                return 1;
            }
        }
        else if (arg.substr(0, 6) == "-seed=") {
            specificSeeds.push_back(arg.substr(6));
        }
        else if (arg.substr(0, 13) == "-servicebits=") {
            try {
                serviceBits = std::stoull(arg.substr(13), nullptr, 16);
            } catch (...) {
                std::cerr << "Error: Invalid servicebits value\n";
                return 1;
            }
        }
        else if (arg.substr(0, 9) == "-timeout=") {
            try {
                timeout = std::stoi(arg.substr(9));
            } catch (...) {
                std::cerr << "Error: Invalid timeout value\n";
                return 1;
            }
        }
        else if (arg == "-verbose" || arg == "-v") {
            verbose = true;
        }
        else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    // Set global timeout
    nConnectTimeout = timeout * 1000;

    PrintHeader("Rincoin DNS Seed Tester");
    std::cout << "Start Time: " << TimeStamp() << "\n";
    std::cout << "Network: " << network << "\n";
    std::cout << "Max IPs per seed: " << maxIPs << "\n";
    std::cout << "Service Bits: 0x" << std::hex << serviceBits << std::dec << "\n";
    std::cout << "Timeout: " << timeout << " seconds\n";

    // Select chain parameters
    try {
        SelectParams(network);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    const CChainParams& chainparams = Params();
    std::vector<std::string> seeds;

    // Get DNS seeds
    if (specificSeeds.empty()) {
        seeds = chainparams.DNSSeeds();
        if (seeds.empty()) {
            std::cout << "\nNo DNS seeds configured for " << network << " network.\n";
            return 0;
        }
    } else {
        seeds = specificSeeds;
    }

    std::cout << "\nFound " << seeds.size() << " DNS seed(s) to test:\n";
    for (size_t i = 0; i < seeds.size(); i++) {
        std::cout << "  " << (i + 1) << ". " << seeds[i] << "\n";
    }

    // Statistics
    int totalSeeds = seeds.size();
    int successfulSeeds = 0;
    int totalAddresses = 0;

    PrintHeader("Testing DNS Seeds");

    // Query each seed
    for (size_t i = 0; i < seeds.size(); i++) {
        const std::string& seed = seeds[i];
        PrintSeedInfo(seed, i, totalSeeds);

        // Build the query hostname with service bits (like Bitcoin Core does)
        std::string queryHost;
        if (serviceBits != 0) {
            queryHost = strprintf("x%x.%s", serviceBits, seed);
        } else {
            queryHost = seed;
        }

        std::cout << "Query hostname: " << queryHost << "\n";
        std::cout << "Resolving... " << std::flush;

        std::vector<CNetAddr> vIPs;
        bool success = LookupHost(queryHost, vIPs, maxIPs, true);

        if (!success) {
            std::cout << "FAILED\n";
            std::cerr << "  Error: DNS lookup failed for " << queryHost << "\n";
            std::cout << std::endl;
            continue;
        }

        std::cout << "OK\n";
        std::cout << "Addresses returned: " << vIPs.size() << "\n";

        if (vIPs.empty()) {
            std::cout << "  Warning: Seed responded but returned no addresses\n";
            std::cout << std::endl;
            continue;
        }

        successfulSeeds++;
        totalAddresses += vIPs.size();

        // Display results
        if (verbose) {
            std::cout << "\nReturned addresses:\n";
            for (size_t j = 0; j < vIPs.size(); j++) {
                std::cout << "  " << std::setw(4) << (j + 1) << ". " 
                          << vIPs[j].ToString() << "\n";
            }
        } else {
            // Just show first 5 and last 5 if there are many
            if (vIPs.size() <= 10) {
                std::cout << "\nReturned addresses:\n";
                for (size_t j = 0; j < vIPs.size(); j++) {
                    std::cout << "  " << vIPs[j].ToString() << "\n";
                }
            } else {
                std::cout << "\nFirst 5 addresses:\n";
                for (size_t j = 0; j < 5; j++) {
                    std::cout << "  " << vIPs[j].ToString() << "\n";
                }
                std::cout << "  ... (" << (vIPs.size() - 10) << " more) ...\n";
                std::cout << "Last 5 addresses:\n";
                for (size_t j = vIPs.size() - 5; j < vIPs.size(); j++) {
                    std::cout << "  " << vIPs[j].ToString() << "\n";
                }
            }
        }

        std::cout << std::endl;
    }

    // Print summary
    PrintHeader("Summary");
    std::cout << "Total seeds tested: " << totalSeeds << "\n";
    std::cout << "Successful queries: " << successfulSeeds << " (" 
              << (totalSeeds > 0 ? (successfulSeeds * 100 / totalSeeds) : 0) << "%)\n";
    std::cout << "Failed queries: " << (totalSeeds - successfulSeeds) << "\n";
    std::cout << "Total addresses returned: " << totalAddresses << "\n";
    if (successfulSeeds > 0) {
        std::cout << "Average addresses per seed: " 
                  << (totalAddresses / successfulSeeds) << "\n";
    }
    std::cout << "\nEnd Time: " << TimeStamp() << "\n";

    if (successfulSeeds == 0) {
        std::cerr << "\n⚠️  WARNING: All DNS seed queries failed!\n";
        return 1;
    } else if (successfulSeeds < totalSeeds) {
        std::cerr << "\n⚠️  WARNING: Some DNS seed queries failed!\n";
        return 2;
    } else {
        std::cout << "\n✓ All DNS seeds are working correctly!\n";
        return 0;
    }
}
