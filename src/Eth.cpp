#include "web3cpp/RPC.h"
#include <web3cpp/Eth.h>

// Helper: create a ready future containing an error JSON object.
static std::future<json> makeErrorFuture(Error& err) {
    std::promise<json> p;
    json ret;
    ret["error"]["message"] = err.what();
    p.set_value(ret);
    return p.get_future();
}

// Helper: ensure a string has a "0x" prefix.
static void ensureHexPrefix(std::string& s) {
    if (s.substr(0, 2) != "0x" && s.substr(0, 2) != "0X") {
        s.insert(0, "0x");
    }
}

std::future<json> Eth::getProtocolVersion() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_protocolVersion().dump()
    );
}

std::future<json> Eth::isSyncing() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_syncing().dump()
    );
}

std::future<json> Eth::getCoinbase() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_coinbase().dump()
    );
}

std::future<json> Eth::isMining() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_mining().dump()
    );
}

std::future<json> Eth::getHashrate() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_hashrate().dump()
    );
}

std::future<json> Eth::getGasPrice() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_gasPrice().dump()
    );
}

std::future<json> Eth::getAccounts() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_accounts().dump()
    );
}

std::future<json> Eth::getBlockNumber() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_blockNumber().dump()
    );
}

std::future<json> Eth::getBalance(const std::string& address, const std::string& defaultBlock) {
    Error err;
    std::string rpcStr = RPC::eth_getBalance(
      address,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getStorageAt(
  std::string address, std::string position, const std::string& defaultBlock
) {
    ensureHexPrefix(position);
    Error err;
    std::string rpcStr = RPC::eth_getStorageAt(
      address, position,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getStorageAt(
  const std::string& address, const BigNumber& position, const std::string& defaultBlock
) {
    std::stringstream ss;
    ss << std::hex << position;
    return getStorageAt(address, ss.str(), defaultBlock);
}

std::future<json> Eth::getCode(const std::string& address, const std::string& defaultBlock) {
    Error err;
    std::string rpcStr = RPC::eth_getCode(
      address,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getBlock(
  std::string blockHashOrBlockNumber, bool isHash, bool returnTransactionObjects
) {
    ensureHexPrefix(blockHashOrBlockNumber);
    Error err;
    std::string rpcStr = (isHash)
      ? RPC::eth_getBlockByHash(
          blockHashOrBlockNumber, returnTransactionObjects, err
        ).dump()
      : RPC::eth_getBlockByNumber(
          blockHashOrBlockNumber, returnTransactionObjects, err
        ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getBlockTransactionCount(
  std::string blockHashOrBlockNumber, bool isHash
) {
    ensureHexPrefix(blockHashOrBlockNumber);
    Error err;
    std::string rpcStr = (isHash)
      ? RPC::eth_getBlockTransactionCountByHash(blockHashOrBlockNumber, err).dump()
      : RPC::eth_getBlockTransactionCountByNumber(blockHashOrBlockNumber, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getBlockUncleCount(
  std::string blockHashOrBlockNumber, bool isHash
) {
    ensureHexPrefix(blockHashOrBlockNumber);
    Error err;
    std::string rpcStr = (isHash)
      ? RPC::eth_getUncleCountByBlockHash(blockHashOrBlockNumber, err).dump()
      : RPC::eth_getUncleCountByBlockNumber(blockHashOrBlockNumber, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getUncle(
  std::string blockHashOrBlockNumber, std::string uncleIndex,
  bool isHash, bool returnTransactionObjects
) {
    ensureHexPrefix(blockHashOrBlockNumber);
    ensureHexPrefix(uncleIndex);
    Error err;
    std::string rpcStr = (isHash)
      ? RPC::eth_getUncleByBlockHashAndIndex(
          blockHashOrBlockNumber, uncleIndex, err
        ).dump()
      : RPC::eth_getUncleByBlockNumberAndIndex(
          blockHashOrBlockNumber, uncleIndex, err
        ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getTransaction(const std::string& transactionHash) {
    Error err;
    std::string rpcStr = RPC::eth_getTransactionByHash(transactionHash, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getTransactionFromBlock(
  std::string hashStringOrNumber, bool isHash, std::string indexNumber
) {
    ensureHexPrefix(hashStringOrNumber);
    ensureHexPrefix(indexNumber);
    Error err;
    std::string rpcStr = (isHash)
      ? RPC::eth_getTransactionByBlockHashAndIndex(
          hashStringOrNumber, indexNumber, err
        ).dump()
      : RPC::eth_getTransactionByBlockNumberAndIndex(
          hashStringOrNumber, indexNumber, err
        ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getTransactionReceipt(const std::string& hash) {
    Error err;
    std::string rpcStr = RPC::eth_getTransactionReceipt(hash, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getTransactionCount(
  const std::string& address, std::string defaultBlock
) {
    Error err;
    std::string rpcStr = RPC::eth_getTransactionCount(
      address,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::feeHistory(
    uint64_t blockCount, const std::string& defaultBlock,
    const std::vector<uint64_t>& rewardPercentile
) {
    Error err;
    std::string rpcStr = RPC::eth_feeHistory(
      blockCount,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      rewardPercentile,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::maxPriorityFeePerGas() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_maxPriorityFeePerGas().dump()
    );
}

std::future<json> Eth::sign(std::string dataToSign, const std::string& address) {
    if (!Utils::isHex(dataToSign)) { dataToSign = Utils::utf8ToHex(dataToSign); }
    Error err;
    std::string rpcStr = RPC::eth_sign(address, dataToSign, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::signTransaction(const json& txObj) {
    Error err;
    std::string rpcStr = RPC::eth_signTransaction(txObj, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::call(const json& callObject, const std::string& defaultBlock) {
    Error err;
    std::string rpcStr = RPC::eth_call(
      callObject,
      (!defaultBlock.empty()) ? defaultBlock : this->defaultBlock,
      err
    ).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::estimateGas(const json& callObject) {
    Error err;
    std::string rpcStr = RPC::eth_estimateGas(callObject, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getPastLogs(const json& options) {
    Error err;
    std::string rpcStr = RPC::eth_getLogs(options, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

std::future<json> Eth::getWork() {
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST,
      RPC::eth_getWork().dump()
    );
}

std::future<json> Eth::submitWork(
  std::string nonce, std::string powHash, std::string digest
) {
    Error err;
    std::string rpcStr = RPC::eth_submitWork(nonce, powHash, digest, err).dump();
    if (err.getCode() != 0) {
        return makeErrorFuture(err);
    }
    return this->provider->getTransport()->send(
      Net::RequestTypes::POST, rpcStr
    );
}

uint64_t Eth::getChainId() {
    return this->provider->getChainId();
}

std::string Eth::getNodeInfo() {
    return std::string("web3cpp/") + PROJECT_VERSION;
}