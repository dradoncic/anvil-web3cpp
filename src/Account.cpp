#include "web3cpp/Net.h"
#include <stdexcept>
#include <web3cpp/Account.h>
#include <exception>

Account::Account(
  const std::string& __address, const std::string& __name,
  const std::string& __privateKey, const std::unique_ptr<Provider>& __provider,
  uint64_t __nonce
) : _address(__address), _name(__name), _privateKey(__privateKey), provider(__provider)
{
  Error error;
  if (!__nonce)
  {
      std::string nonceRequest = Net::HTTPRequest(
        this->provider, Net::RequestTypes::POST,
        RPC::eth_getTransactionCount(_address, "latest", error).dump()
      );
      json nonceJson = json::parse(nonceRequest);
      _nonce = boost::lexical_cast<HexTo<uint64_t>>(nonceJson["result"].get<std::string>());
      return;
  }
  _nonce = __nonce;
}

std::future<BigNumber> Account::balance() const {
  return std::async([=]{
    Error error;
    BigNumber ret;
    std::string balanceRequestStr = Net::HTTPRequest(
      this->provider, Net::RequestTypes::POST,
      RPC::eth_getBalance(this->_address, "latest", error).dump()
    );
    if (error.getCode() != 0) {
      std::cout << "Error on getting balance for account " << this->_address
        << ": " << error.what() << std::endl;
      return ret;
    }
    json balanceRequest = json::parse(balanceRequestStr);
    ret = Utils::hexToBigNumber(balanceRequest["result"].get<std::string>());
    return ret;
  });
}

std::future<BigNumber> Account::addBalance(BigNumber amount) const
{
  return std::async([=]{
    Error error;
    BigNumber ret;
    std::string addBalanceRequestStr = Net::HTTPRequest(
        this->provider, Net::RequestTypes::POST,
        RPC::anvil_addBalance(_address, amount, error).dump()
    );
    if (error.getCode() != 0) {
        std::cout << "Error on adding balance for account " << _address
          << ": " << error.what() << std::endl;
        return ret;
    }
    std::string balanceRequestStr = Net::HTTPRequest(
      this->provider, Net::RequestTypes::POST,
      RPC::eth_getBalance(_address, "latest", error).dump()
    );
    if (error.getCode() != 0) {
      std::cout << "Error on getting balance for account " << _address
        << ": " << error.what() << std::endl;
      return ret;
    }
    json balanceRequest = json::parse(balanceRequestStr);
    ret = Utils::hexToBigNumber(balanceRequest["result"].get<std::string>());
    return ret;
  });
}

std::future<BigNumber> Account::setBalance(BigNumber amount) const
{
  return std::async([=]{
    Error error;
    BigNumber ret;
    std::string setBalanceRequestStr = Net::HTTPRequest(
        this->provider, Net::RequestTypes::POST,
        RPC::anvil_setBalance(this->_address, amount, error).dump()
    );
    if (error.getCode() !=  0) {
        std::cout << "Error on setting balance for account " << this->_address
          << ": " << error.what() << std::endl;
        return ret;
    }
    std::string balanceRequestStr = Net::HTTPRequest(
      this->provider, Net::RequestTypes::POST,
      RPC::eth_getBalance(this->_address, "latest", error).dump()
    );
    if (error.getCode() != 0) {
      std::cout << "Error on getting balance for account " << this->_address
        << ": " << error.what() << std::endl;
      return ret;
    }
    json balanceRequest = json::parse(balanceRequestStr);
    ret = Utils::hexToBigNumber(balanceRequest["result"].get<std::string>());
    return ret;
  });
}

// bool Account::saveTxToHistory(std::string signedTx) {
//   json txData = Utils::decodeRawTransaction(signedTx);
//   return this->transactionDB.putKeyValue(txData["hash"], txData.dump());
// }
