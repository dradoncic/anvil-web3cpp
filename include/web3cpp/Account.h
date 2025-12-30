#ifndef ACCOUNTS_H
#define ACCOUNTS_H

#include <future>
#include <string>
#include <vector>
#include <memory>

#include <web3cpp/Net.h>
#include <web3cpp/Provider.h>
#include <web3cpp/Utils.h>
#include <web3cpp/RPC.h>
#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>

using json = nlohmann::ordered_json;

/**
 * Abstraction for a single account.
 * Contains details such as address, nonce, transaction history, etc..
 */

class Account {
  private:
    std::string _address;                                        ///< Address for the account.
    std::string _name;                                           ///< Custom name/label for the account.
    std::string _privateKey;                                     ///< Private key for the account..
    uint64_t _nonce;                                             ///< Current nonce for the account.
    const std::unique_ptr<Provider>& provider;                   ///< Pointer to Web3::defaultProvider.

  public:

    /**
     * Default constructor.
     * @param name Custom name/label for the account.
     * @param __address Address for the account.
     * @param __privateKey Full private key for the account.
     * @param __isLedger Flag to set whether the account comes from a Ledger device or not.
     * @param *_provider Pointer to the provider used by the account.
     */
    Account(
      const std::string& __address, const std::string& __name,
      const std::string& __privateKey, const std::unique_ptr<Provider>& _provider,
      uint64_t nonce = 0
    );

    /// Copy constructor.
    Account(const Account& other) noexcept :
      _address(other._address),
      _name(other._name),
      _privateKey(other._privateKey),
      _nonce(other._nonce),
      provider(other.provider)
    {}

    /// Copy constructor from pointer.
    Account(const std::unique_ptr<Account>& other) noexcept :
      _address(other->_address),
      _name(other->_name),
      _privateKey(other->_privateKey),
      _nonce(other->_nonce),
      provider(other->provider)
    {}

    const std::string& address()        const { return _address; }           ///< Getter for the address.
    const std::string& name()           const { return _name; }              ///< Getter for the custom name/label.
    const std::string& privateKey()     const { return _privateKey; }       ///< Getter for the private key..
    const uint64_t& nonce()             const { return _nonce; }             ///< Getter for the nonce.

    /**
     * Request the account's balance from the network.
     * @return The balance in Wei as a BigNumber, or 0 if the request fails.
     */
    std::future<BigNumber> balance() const;

    /**
     * Add balance to the account (ETH).
     * @param amount The amount of ETH
     * @param $err Error object
     * @return A JSON object with the send results (either "result" or "error")
     */
     std::future<json> addBalance(BigNumber amount, Error &err);

    /**
     * Set balance to the account (ETH).
     * @param amount The amount of ETH
     * @param $err Error object
     * @return A JSON object with the send results (either "result" or "error")
     */
    std::future<json> setBalance(BigNumber amount, Error &err);

    /**
     * Deal ERC-20 to the account.
     * @param tokenAddress The ERC-20 smart contract address
     * @param amount The amount of ERC-20
     * @param $err Error object
     * @return A JSON object with the send results (either "result" or "error")
     */
    std::future<json> dealERC20(const std::string& tokenAddress, BigNumber amount, Error &err);
};

#endif  // ACCOUNTS_H
