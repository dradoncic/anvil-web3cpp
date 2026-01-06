#include <web3cpp/Web3.h>

// Default Constructor
Web3::Web3() :
  defaultProvider(std::make_unique<Provider>(Provider(""))),
  wallet(defaultProvider),
  eth(defaultProvider) {}

// Custom provider overload
Web3::Web3(Provider provider) :
  defaultProvider(std::make_unique<Provider>(provider)),
  wallet(defaultProvider),
  eth(defaultProvider) {}
