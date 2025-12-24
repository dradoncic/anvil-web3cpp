#ifndef WALLET_H
#define WALLET_H

#include <chrono>
#include <ctime>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "Utils.h"

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
 * Abstraction for a single wallet.
 */

static const std::unique_ptr<Account> NullAccount = nullptr;

class Wallet {
  private:
    const std::unique_ptr<Provider>& provider;          ///< Pointer to Web3::defaultProvider.
    std::vector<std::unique_ptr<Account>> accountList;  ///< List of accounts loaded in this wallet. vector is modified on constructor (load from DB) and createNew

  public:
    /**
     * Constructor.
     * @param *_provider Pointer to the provider that will be used.
     * @param _path The path for the wallet.
     */
    Wallet(const std::unique_ptr<Provider>& _provider)
      : provider(_provider)
    {};

    const std::unique_ptr<Provider>& getProvider() const { return this->provider; } ///< Getter for provider.

    /**
     * Create a new account. Address is stored as lowercase.
     * @param derivPath The full derivation path of the account (e.g. "m/44'/60'/0'/0").
     * @param &password The wallet's password.
     * @param name A custom human-readable name/label for the account.
     * @param &error Error object.
     * @param seed (optional) The BIP39 seed phrase to use for generating the account.
     *                        If no seed is passed, uses the BIP39 seed from the wallet.
     * @return The checksum address of the new account, or an empty string on failure.
     */
    std::string createAccount(
      std::string derivPath, std::string name,
      Error &error, std::string seed = ""
    );

    /**
     * Delete an existing account.
     * @param address The address of the account to be deleted. Will be converted
     *                to lowercase before attempting to delete.
     * @return `true` if the account was successfully deleted or didn't exist
     *         anyway prior to requesting deletion, `false` otherwise.
     */
    bool deleteAccount(std::string address);

    /**
     * Sign a data string as an "Ethereum Signed Message".
     * [EIP-712](https://eips.ethereum.org/EIPS/eip-712) compliant.
     * @param dataToSign The data to be signed.
     * @param address The address to use for signing.
     * @param password The wallet's password.
     * @return The hex signature which is meant to be used on ecRecover(),
     *         or an error message on failure.
     */
    std::string sign(
      std::string dataToSign, std::string address, std::string password
    );

    /**
     * Recover the account that signed a given data string.
     * @param dataThatWasSigned The unencrypted data string that was signed.
     * @param signature The raw signature that came from sign().
     * @return The original account used to sign the data string.
     */
    std::string ecRecover(
      std::string dataThatWasSigned, std::string signature
    );

    /**
     * Build a transaction from user data.
     * @param from The address that will make the transaction.
     * @param to The address that will receive the transaction. For coin transactions,
     *           this is the destination address. For token transactions, this is
     *           the token contract's address.
     * @param value The value of the transaction (in Wei). For token transactions
     *              this is 0, as the token value would be packed inside `dataHex`.
     * @param gasLimit The gas limit of the transaction (in Wei).
     * @param gasPrice The gas price of the transaction (in Wei).
     * @param dataHex The arbitrary data of the transaction. For coin transactions
     *                this is blank. For token transactions this would be a
     *                packed ABI call.
     * @param nonce The nonce of the `from` address.
     * @param &error Error object.
     * @param creation (optional) Sets whether the transaction is creating a contract
     *                 or not. Defaults to false.
     * @return A struct filled with data for the transaction, ready to be signed,
     *         or an empty/incomplete struct on failure.
     */
    dev::eth::TransactionSkeleton buildTransaction(
      std::string from, std::string to, BigNumber value,
      BigNumber gasLimit, BigNumber gasPrice, std::string dataHex,
      int nonce, Error &error, bool creation = false
    );

    /**
     * Sign a built transaction.
     * @param txObj The transaction struct returned from buildTransaction().
     * @param password The wallet's password.
     * @param &err Error object.
     * @return The raw transaction signature ready to be sent, or an empty string on failure.
     */
    std::string signTransaction(
      dev::eth::TransactionSkeleton txObj, std::string password, Error &err
    );

    /**
     * Send/broadcast a signed transaction to the blockchain.
     * @param signedTx The raw transaction signature returned from signTransaction().
     * @param &err Error object.
     * @return A JSON object with the send results (either "result" or "error")
     *         and the transaction's raw signature.
     */
    std::future<json> sendTransaction(std::string signedTx, Error &err);

    /**
     * Get the accounts stored in the wallet.
     * @return A list of addresses from the wallet.
     */
    std::vector<std::string> getAccounts();

    /**
     * Get the details for a specific account.
     * @param address The address of the account. Will be converted to lowercase.
     * @return A struct with details of the account, or an empty struct
     *         if the address is not found.
     */
    const std::unique_ptr<Account>& getAccountDetails(std::string address);

    /**
     * Get the raw structure of a specific account.
     * @param address The address of the account. Will be converted to lowercase.
     * @return A JSON object with the raw details of the account, or an empty
     *         JSON object if the address is not found.
     */
    json getAccountRawDetails(std::string address);
};

#endif  // WALLET_H
