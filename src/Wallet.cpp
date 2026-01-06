#include "web3cpp/Net.h"
#include "web3cpp/RPC.h"
#include "web3cpp/Utils.h"
#include "web3cpp/ethcore/Common.h"
#include <web3cpp/Wallet.h>

Account Wallet::createAccount(
  std::string name,
  std::string seed
) {
  // Derive and import the account.
  dev::KeyPair k = dev::KeyPair::create();
  std::string addr = Utils::toChecksumAddress("0x" + dev::toHex(k.address()));

  Account acc(
      addr,
      name,
      k.secret().hex(),
      provider
  );

  return acc;
}

std::optional<Account> Wallet::getAccount(
  std::string address, std::string name,
  std::string privateKey, uint64_t nonce
) {
  if (!Utils::isAddress(address))
      return std::nullopt;

  dev::Secret s(dev::fromHex(privateKey));

  if (Utils::toLowercaseAddress(address) != "0x" + toAddress(toPublic(s)).hex())
    return std::nullopt;

  Account acc(
      address, name,
      privateKey, provider,
      nonce
  );

  return acc;
}

std::string Wallet::sign(std::string dataToSign, std::string privateKey)
{
  dev::Secret s(dev::toHex(privateKey));
  std::string signableData = std::string("\x19") + "Ethereum Signed Message:\n"
    + boost::lexical_cast<std::string>(dataToSign.size()) + dataToSign;
  dev::h256 messageHash(dev::toHex(dev::sha3(signableData, false)));
  dev::h520 signature = dev::sign(s, messageHash);
  return std::string("0x") + dev::toHex(signature);
}

std::string Wallet::ecRecover(std::string signedData, std::string signature)
{
  // Aleth does some stupidly arcane magic to make this work, better use it for now
  if (signature.substr(0, 2) == "0x" || signature.substr(0, 2) == "0X") {
    signature = signature.substr(2); // Remove "0x"
  }
  std::string data = std::string("\x19") + "Ethereum Signed Message:\n"
    + boost::lexical_cast<std::string>(signedData.size())
    + signedData;
  dev::h256 messageHash(dev::toHex(dev::sha3(data, false)));
  dev::h520 sig = dev::FixedHash<65>(signature);
  dev::Public p = dev::recover(sig, messageHash);
  return "0x" + dev::toHex(dev::toAddress(p));
}

dev::eth::TransactionSkeleton Wallet::buildTransaction(
    std::string from, int nonce, Error &error, std::string to,
    std::string dataHex, BigNumber value, dev::eth::AccessList accessList
)
{
  dev::eth::TransactionSkeleton tx;
  try {
    tx.from = dev::eth::toAddress(Utils::toLowercaseAddress(from));
    tx.to = to.empty() ? dev::Address() : dev::eth::toAddress(Utils::toLowercaseAddress(to));
    tx.value = value;
    if (!dataHex.empty()) { tx.data = dev::fromHex(dataHex); }
    tx.nonce = nonce;
    tx.chainId = this->provider->getChainId();
    if (!accessList.empty()) { tx.accessList = accessList; }
  } catch (std::exception &e) {
      error.setCode(11);
      return tx;
  }
  error.setCode(0);
  return tx;
}

Wallet::Estimations Wallet::fetchEstimations(json& txObj)
{
    auto estimatedGasFut = std::async(std::launch::async, [this, txObj]() -> std::pair<dev::u256, int> {
        Error rpcErr;
        std::string rpcStr = RPC::eth_estimateGas(txObj, rpcErr).dump();
        if (rpcErr.getCode() != 0) return {dev::Invalid256, rpcErr.getCode()};

        std::string req = Net::HTTPRequest(
            this->provider, Net::RequestTypes::POST, rpcStr
        );
        json reqJson = json::parse(req);

        if (reqJson.count("error")) return {dev::Invalid256, 36};
        return {Utils::toBN(reqJson["result"].get<std::string>()), 0};
    });

    auto feeHistoryFut = std::async(std::launch::async, [this]() -> std::pair<json, int> {
        Error rpcErr;
        std::string rpcStr = RPC::eth_feeHistory(
            5, "latest", {10, 50, 90}, rpcErr
        );
        if (rpcErr.getCode() != 0) return {json{}, 36};

        std::string req = Net::HTTPRequest(
            this->provider, Net::RequestTypes::POST , rpcStr
        );
        json reqJson = json::parse(req);

        if (reqJson.count("error")) return {json{}, 36};
        return {reqJson, 0};
    });

    auto gasRes = estimatedGasFut.get();
    auto feeRes = feeHistoryFut.get();

    Estimations estim;
    estim.gas = gasRes.first;
    estim.feeHistory = feeRes.first;

    if (gasRes.second != 0 || feeRes.second != 0) {
        estim.errorCode = gasRes.second != 0 ? gasRes.second : feeRes.second;
        return estim;
    }
    return estim;
}

dev::eth::TransactionBase Wallet::estimateTransaction(
    dev::eth::TransactionSkeleton txObj, dev::eth::FeeLevel feeLevel, Error &error
)
{
    auto call = txObj.toJson();
    auto res = this->fetchEstimations(call);
    if (res.errorCode != 0){
        error.setCode(res.errorCode);
        return dev::eth::TransactionBase();
    }

    dev::eth::TransactionBase tx(txObj);
    tx.setFeeLevel(feeLevel);
    tx.setGas(res.gas);
    tx.setFees(res.feeHistory);

    error.setCode(0);
    return tx;
}

dev::eth::TransactionBase Wallet::estimateTransaction(
    dev::eth::TransactionBase& txObj, Error &error
)
{
    auto call = txObj.toJson();
    auto res = this->fetchEstimations(call);
    if (res.errorCode != 0){
        error.setCode(res.errorCode);
        return txObj;
    }

    txObj.setGas(res.gas);
    txObj.setFees(res.feeHistory);

    return txObj;
}

std::string Wallet::signTransaction(
    dev::eth::TransactionBase& txObj, std::string privateKey, Error &error
)
{
    try {
        std::stringstream txHexBuffer;
        dev::Secret s(dev::fromHex(privateKey));
        txObj.sign(s);
        txHexBuffer << "0x" + dev::toHex(txObj.rlp());
        error.setCode(0);
        return txHexBuffer.str();
    } catch (std::exception &e) {
        error.setCode(12);
        return "";
    }
}


std::future<json> Wallet::sendTransaction(std::string signedTx, Error &error)
{
    if (signedTx.substr(0,2) != "0x" && signedTx.substr(0,2) != "0X") signedTx.insert(0, "0x");

    return std::async([this, signedTx, &error]{
        json txResult;
        Error rpcErr;
        std::string rpcStr = RPC::eth_sendRawTransaction(signedTx, rpcErr).dump();
        if (rpcErr.getCode() != 0) {
            error.setCode(rpcErr.getCode());
            txResult["error"] = rpcStr;
            return txResult;
        }

        std::string req = Net::HTTPRequest(this->provider, Net::RequestTypes::POST, rpcStr);
        json reqJson = json::parse(req);

        txResult["signature"] = signedTx;
        if (reqJson.count("error")) {
            txResult["error"] = reqJson;
            error.setCode(13);
        } else {
            txResult["result"] = reqJson["result"].get<std::string>();
            error.setCode(0);
        }
        return txResult;
    });
}

std::future<json> Wallet::dropTransaction(std::string transactionHash, Error &error)
{
    if (transactionHash.substr(0,2) != "0x" && transactionHash.substr(0,2) != "0X") transactionHash.insert(0, "0x");

    return std::async([this, transactionHash, &error]{
        json txResult;
        Error rpcErr;
        std::string rpcStr = RPC::anvil_dropTransaction(transactionHash, rpcErr).dump();
        if (rpcErr.getCode() != 0) {
            error.setCode(rpcErr.getCode());
            txResult["error"] = rpcStr;
            return txResult;
        }

        std::string req = Net::HTTPRequest(this->provider, Net::RequestTypes::POST, rpcStr);
        json reqJson = json::parse(req);

        txResult["hash"] = transactionHash;
        if (reqJson.count("error")) {
            txResult["error"] = reqJson;
            error.setCode(37);
        } else {
            txResult["result"] = reqJson["result"].get<std::string>();
            error.setCode(0);
        }
        return txResult;
    });
}
