#ifndef WALLET_H
#define WALLET_H

#include <chrono>
#include <ctime>
#include <future>
#include <string>
#include <thread>
#include <vector>
#include <optional>
#include <utility>

#include "Utils.h"
#include "web3cpp/ethcore/Common.h"

#include <boost/algorithm/string/replace.hpp>
#include <nlohmann/json.hpp>

#include <bip3x/crypto/sha3.h>  // keccak_256()
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <web3cpp/devcore/Common.h>
#include <web3cpp/devcore/FixedHash.h>
#include <web3cpp/devcore/Address.h>
#include <web3cpp/ethcore/TransactionBase.h>
#include <web3cpp/Error.h>
#include <web3cpp/Account.h>
#include <web3cpp/Cipher.h>
#include <web3cpp/Bip39.h>
#include <web3cpp/Provider.h>

using json = nlohmann::ordered_json;

/**
 * Stateless wallet abstraction.
 * This wallet does not manage account state internally.
 * Account state management (storage, persistence) is handled externally.
 */

class Wallet {
  private:

    struct Estimations
    {
      dev::u256 gas = dev::Invalid256;
      json feeHistory = {};
      int errorCode = 0;
    };

    const std::unique_ptr<Provider>& provider;          ///< Pointer to the blockchain provider.

    Estimations fetchEstimations(json& txObj);

  public:
    /**
     * Constructor.
     * @param _provider Pointer to the provider that will be used for blockchain operations.
     */
    Wallet(const std::unique_ptr<Provider>& _provider)
      : provider(_provider)
    {};

    const std::unique_ptr<Provider>& getProvider() const { return this->provider; }

    /**
     * Generate a new account from a seed phrase.
     * The generated account is NOT stored internally; state management is external.
     * @param name A custom human-readable name/label for the account.
     * @param error Error object for error reporting.
     * @param seed (optional) The BIP39 seed phrase to use for generating the account.
     *                        If no seed is passed, a random seed will be generated.
     * @return An Account object with the generated address and private key, or empty on failure.
     */
    Account createAccount(
      std::string name, std::string seed = ""
    );

    /**
     * Load an existing account from address and private key.
     * The account is NOT stored internally; state management is external.
     * @param address The address of the account (will be converted to lowercase).
     * @param name A custom human-readable name/label for the account.
     * @param privateKey The private key for the account.
     * @return An Account object with the provided details.
     */
    std::optional<Account> getAccount(
        std::string address, std::string name,
        std::string privateKey, uint64_t nonce
    );

    /**
     * Sign arbitrary data as an "Ethereum Signed Message".
     * Implements EIP-191 Ethereum signed message standard.
     * @param dataToSign The data to be signed (can be any string or hex data).
     * @param privateKey The private key of the signing account.
     * @return The hex-encoded signature (65 bytes: r + s + v), or empty on failure.
     */
    std::string sign(
      std::string dataToSign, std::string privateKey
    );

    /**
     * Recover the address of the account that signed a given data string.
     * Implements EIP-191 signature recovery.
     * @param signedData The original data that was signed.
     * @param signature The hex-encoded signature from sign().
     * @return The address (checksummed) of the signing account, or empty if recovery fails.
     */
    std::string ecRecover(
      std::string signedData, std::string signature
    );

    /**
     * Build a transaction skeleton ready to be signed.
     * This creates the transaction structure but does NOT sign it.
     * @param from The address that will send the transaction.
     * @param to The destination address (recipient for transfers, contract for calls).
     * @param value The amount of Wei to transfer (0 for contract calls).
     * @param chainID The chain identification number.
     * @param dataHex The encoded transaction data (empty for ETH transfers, ABI-encoded for contract calls).
     * @param accessList The access list of the transation
     * @param nonce The transaction count of the sender (prevents replay attacks).
     * @param error Error object for error reporting.
     * @param creation (optional) Set to true if this is a contract creation transaction. Defaults to false.
     * @return A TransactionSkeleton struct filled with transaction data, ready to be signed.
     */
    dev::eth::TransactionSkeleton buildTransaction(
      std::string from, int nonce, Error &error, std::string to = "",
      std::string dataHex = "", BigNumber value = {}, dev::eth::AccessList accessList = {}
    );

    /**
     * Estimates the gas fields of an transaction.
     * @param txObj The transaction skeleton from buildTransaction.
     * @param feeLevel The priority fee willing to be paid to the miner.
     * @return A Transaction Base struct filled with everything in Transaction Skeleton and gas fields.
     */
    dev::eth::TransactionBase estimateTransaction(
        dev::eth::TransactionSkeleton txObj, dev::eth::FeeLevel feeLevel, Error &error
    );

    /**
     * Estimates the gas fields of an transaction.
     * @param txObj A TransactionBase object..
     * @param feeLevel The priority fee willing to be paid to the miner.
     * @return A Transaction Base struct filled with everything in Transaction Skeleton and gas fields.
     */
    dev::eth::TransactionBase estimateTransaction(
        dev::eth::TransactionBase& txObj, Error &error
    );

    /**
     * Sign a built transaction.
     * @param txObj The transaction base from estimateTransaction().
     * @param privateKey The private key of the transaction sender.
     * @param error Error object for error reporting.
     * @return The RLP-encoded signed transaction (ready to broadcast), or empty on failure.
     */
    std::string signTransaction(
      dev::eth::TransactionBase& txObj, std::string privateKey, Error &error
    );

    /**
     * Broadcast a signed transaction to the blockchain.
     * @param signedTx The RLP-encoded signed transaction from signTransaction().
     * @param error Error object for error reporting.
     * @return A future that resolves to a JSON response containing the transaction hash or error details.
     */
    std::future<json> sendTransaction(std::string signedTx, Error &error);

    /**
     * Drop transaction from the mempool.
     * @param transactionHash The transaction hash
     * @param &err Error object.
     * @return The hash of transaction that was canceled.
     */
    std::future<json> dropTransaction(std::string transactionHash, Error &error);


};

#endif  // WALLET_H
