#include <web3cpp/Wallet.h>

Account Wallet::createAccount(
  std::string name,
  std::string seed
) {
  // Derive and import the account.
  dev::KeyPair k = dev::KeyPair::create();
  std::string addr = Utils::toLowercaseAddress("0x" + dev::toHex(k.address()));

  Account acc(
      addr,
      name,
      k.secret().hex(),
      provider
  );

  return acc;
}

Account Wallet::getAccount(
  std::string address, std::string name,
  std::string privateKey, uint64_t nonce
) {
 Account acc(
     address, name,
     privateKey, provider,
     nonce
 );

 return acc;
}

std::string Wallet::sign(
  std::string dataToSign, std::string privateKey
) {
  // EIP-712 requires us to hash the message before signing
  address = Utils::toLowercaseAddress(address);
  json accRaw = getAccountRawDetails(address);
  Error decErr;
  std::string dec = Cipher::decrypt(accRaw.dump(), password, decErr);
  if (decErr.getCode() != 0) return decErr.what();
  Secret s(dev::toHex(dec));
  std::string signableData = std::string("\x19") + "Ethereum Signed Message:\n"
    + boost::lexical_cast<std::string>(dataToSign.size()) + dataToSign;
  dev::h256 messageHash(dev::toHex(dev::sha3(signableData, false)));
  dev::h520 signature = dev::sign(s, messageHash);
  return std::string("0x") + dev::toHex(signature);
}

std::string Wallet::ecRecover(
  std::string dataThatWasSigned, std::string signature
) {
  // Aleth does some stupidly arcane magic to make this work, better use it for now
  if (signature.substr(0, 2) == "0x" || signature.substr(0, 2) == "0X") {
    signature = signature.substr(2); // Remove "0x"
  }
  std::string data = std::string("\x19") + "Ethereum Signed Message:\n"
    + boost::lexical_cast<std::string>(dataThatWasSigned.size())
    + dataThatWasSigned;
  dev::h256 messageHash(dev::toHex(dev::sha3(data, false)));
  dev::h520 sig = dev::FixedHash<65>(signature);
  dev::Public p = dev::recover(sig, messageHash);
  return "0x" + dev::toHex(dev::toAddress(p));
}

dev::eth::TransactionSkeleton Wallet::buildTransaction(
  std::string from, std::string to,
  BigNumber value, BigNumber gasLimit, BigNumber gasPrice,
  std::string dataHex, int nonce, Error &error, bool creation
) {
  dev::eth::TransactionSkeleton ret;
  try {
    ret.creation = creation;
    ret.from = dev::eth::toAddress(Utils::toLowercaseAddress(from));
    ret.to = dev::eth::toAddress(Utils::toLowercaseAddress(to));
    ret.value = value;
    if(!dataHex.empty()) { ret.data = dev::fromHex(dataHex); }
    ret.nonce = nonce;
    ret.gas = gasLimit;
    ret.gasPrice = gasPrice;
    ret.chainId = this->provider->getChainId();
  } catch (std::exception &e) {
    error.setCode(11);  // Transaction Build Error
    return ret;
  }
  error.setCode(0);
  return ret;
}


std::string Wallet::signTransaction(
  dev::eth::TransactionSkeleton txObj, std::string password, Error &err
) {
  Error e;
  Secret s(dev::toHex(Cipher::decrypt(
    getAccountRawDetails("0x" + dev::toString(txObj.from)).dump(), password, e
  )));
  if (e.getCode() != 0) { err.setCode(e.getCode()); return ""; }
  try {
    std::stringstream txHexBuffer;
    dev::eth::TransactionBase t(txObj);
    t.setNonce(txObj.nonce);
    t.sign(s);
    txHexBuffer << dev::toHex(t.rlp());
    err.setCode(0);
    return txHexBuffer.str();
  } catch (std::exception &e) {
    err.setCode(12); return ""; // Transaction Sign Error
  }
}

std::future<json> Wallet::sendTransaction(std::string signedTx, Error &err) {
  if (signedTx.substr(0,2) != "0x" && signedTx.substr(0,2) != "0X") {
    signedTx.insert(0, "0x");
  }
  return std::async([this, signedTx, &err]{
    json txResult;
    Error rpcErr;
    std::string rpcStr = RPC::eth_sendRawTransaction(signedTx, rpcErr).dump();
    if (rpcErr.getCode() != 0) {
      err.setCode(rpcErr.getCode());
      txResult["error"] = rpcStr;
      return txResult;
    }

    std::string req = Net::HTTPRequest(
      this->provider, Net::RequestTypes::POST, rpcStr
    );
    json reqJson = json::parse(req);

    txResult["signature"] = signedTx;
    if (reqJson.count("error")) {
      txResult["error"] = reqJson;
      err.setCode(13);  // Transaction Send Error
    } else {
      txResult["result"] = reqJson["result"].get<std::string>();
      err.setCode(0);
    }
    return txResult;
  });
}

void Wallet::storePassword(const std::string& password, unsigned int seconds) {
  this->_password = password;
  if (seconds > 0) {
    this->_passEnd = std::time(nullptr) + seconds;
    this->_passThread = std::thread(std::bind(&Wallet::passHandler, this));
    this->_passThread.detach();
  }
}

void Wallet::clearPassword() {
  this->_password = "";
  this->_passEnd = 0; // This ensures the thread will be terminated
}

bool Wallet::isPasswordStored() {
  return !this->_password.empty();
}

std::vector<std::string> Wallet::getAccounts() {
  std::vector<std::string> ret;
  for (auto const &acc : this->accountList) {
    ret.push_back(acc->address());
  }
  return ret;
}

const std::unique_ptr<Account>& Wallet::getAccountDetails(std::string address) {
  address = Utils::toLowercaseAddress(address);
  for (auto &acc : this->accountList) {
    if (acc->address() == address) {
      return acc;
    }
  }

  return NullAccount;
}

json Wallet::getAccountRawDetails(std::string address) {
  json ret;
  address = Utils::toLowercaseAddress(address);
  std::map<std::string, std::string> accs = this->accountDB.getAllPairs();
  for (std::pair<std::string, std::string> acc : accs) {
    if (acc.first == address) {
      ret = json::parse(acc.second);
      break;
    }
  }
  return ret;
}

std::string Wallet::getSeedPhrase(const std::string& password, Error &err) {
  std::string seedPhrase;
  Error readErr, decErr;
  boost::filesystem::path tmpPath = seedPhraseFile();
  json seedJson = Utils::readJSONFile(tmpPath, readErr);
  if (readErr.getCode() != 0) {
    err.setCode(readErr.getCode());
    return "";
  }
  seedPhrase = Cipher::decrypt(seedJson.dump(), password, decErr);
  if (decErr.getCode() != 0) { err.setCode(decErr.getCode()); return ""; }
  return seedPhrase;
}
