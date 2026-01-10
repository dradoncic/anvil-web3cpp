// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <web3cpp/ethcore/Common.h>
#include <web3cpp/devcrypto/Common.h>
#include <web3cpp/devcore/RLP.h>
#include <web3cpp/devcore/SHA3.h>
#include <nlohmann/json.hpp>

#include <boost/optional.hpp>

#define GAS_SAFETY_MULTIPLIER 1.2
#define BASE_FEE_MULTIPLIER 2

using json = nlohmann::ordered_json;

namespace dev
{
namespace eth
{

/// Named-boolean type to encode whether a signature be included in the serialisation process.
enum IncludeSignature
{
    WithoutSignature = 0,	///< Do not include a signature.
    WithSignature = 1,		///< Do include a signature.
};

enum class CheckTransaction
{
    None,
    Cheap,
    Everything
};


enum Type
{
    Legacy,
    EIP2930,
    EIP1559,
    EIP4844,
    EIP7702
};

enum FeeLevel
{
    Low,    // 10th percentile
    Medium, // 50th percentile
    High    // 90th percentile
};

/// Encodes a transaction, ready to be exported to or freshly imported from RLP.
class TransactionBase
{
public:
    /// Constructs a null transaction.
    TransactionBase() {}

    /// Constructs a transaction from a transaction skeleton & optional secret.
    TransactionBase(TransactionSkeleton const& _ts);

    /// Constructs an unsigned message-call transaction.
    TransactionBase(u256 const& _value, Address const& _dest, bytes const& _data, uint64_t _chainId, u256 const& _nonce = 0, AccessList _accessList = {}, FeeLevel _feeLevel = Medium): m_function(MessageCall), m_type(EIP1559), m_feeLevel(_feeLevel), m_nonce(_nonce),
                                                                                                                                                                                        m_chainId(_chainId), m_value(_value), m_destination(_dest), m_data(_data), m_accessList(_accessList),
                                                                                                                                                                                        m_gasLimit(Invalid256), m_maxPriorityFeePerGas(Invalid256), m_maxFeePerGas(Invalid256) {}

    /// Constructs an unsigned contract-creation transaction.
    TransactionBase(u256 const& _value, bytes const& _data, uint64_t _chainId, u256 const& _nonce = 0, FeeLevel _feeLevel = Medium): m_function(ContractCreation), m_type(EIP1559), m_feeLevel(_feeLevel), m_nonce(_nonce), m_chainId(_chainId), m_value(_value), m_data(_data),
                                                                                                                                     m_gasLimit(Invalid256), m_maxPriorityFeePerGas(Invalid256), m_maxFeePerGas(Invalid256) {}

    /// Constructs a transaction from the given RLP.
    explicit TransactionBase(bytesConstRef _rlp, CheckTransaction _checkSig);

    /// Constructs a transaction from the given RLP.
    explicit TransactionBase(bytes const& _rlp, CheckTransaction _checkSig): TransactionBase(&_rlp, _checkSig) {}

    /// Checks equality of transactions.
    bool operator==(TransactionBase const& _c) const { return m_type == _c.m_type && (m_function == _c.m_function && (m_function == ContractCreation || m_destination == _c.m_destination) && m_value == _c.m_value && m_data == _c.m_data); }
    /// Checks inequality of transactions.
    bool operator!=(TransactionBase const& _c) const { return !operator==(_c); }

    /// @returns sender of the transaction from the signature (and hash).
    Address const& sender() const;

    Address const& safeSender() const noexcept;

    /// @throws TransactionIsUnsigned if signature was not initialized
    /// @throws InvalidSValue if the signature has an invalid S value.
    void checkLowS() const;

    /// @throws InvalidSignature if the transaction is replay protected
    /// and chain id is not equal to @a _chainId
    void checkChainId(uint64_t _chainId) const;

    /// @returns true if transaction is non-null.
    explicit operator bool() const { return m_function != NullTransaction; }

    /// @returns true if transaction is contract-creation.
    bool isCreation() const { return m_function == ContractCreation; }

    /// Serialises this transaction to an RLPStream.
    /// @throws TransactionIsUnsigned if including signature was requested but it was not initialized
    void streamRLP(RLPStream& _s, IncludeSignature _sig = WithSignature) const;

    /// @returns the RLP serialisation of this transaction.
    bytes rlp(IncludeSignature _sig = WithSignature) const { RLPStream s; streamRLP(s, _sig); return s.out(); }

    /// @returns the SHA3 hash of the RLP serialisation of this transaction.
    h256 sha3(IncludeSignature _sig = WithSignature) const;

    /// @returns the amount of ETH to be transferred by this (message-call) transaction, in Wei. Synonym for endowment().
    u256 value() const { return m_value; }

    // Function m_function = NullTransaction;		///< Is this a contract-creation transaction or a message-call transaction?
    // Type m_type = EIP1559;               /// < Typed-envelope transaction
    // uint64_t m_chainId;
    // u256 m_nonce;						///< The transaction-count of the sender.
    // u256 m_maxPriorityFeePerGas;        ///< The maximum fee per gas sender is willing to give to miners to incentivize them to include their transaction.
    // u256 m_maxFeePerGas;                ///< The maximum fee per gas sender is willing to pay total (covering both priority fee and base fee)
    // u256 m_gasLimit;                    ///< The upper limit of total gas to convert, paid for from sender's account. Any unused gas gets refunded.
    // Address m_destination;              ///< The receiving address of the transaction.
    // u256 m_value;                       ///< The amount of ETH to be transferred by this transaction. Called "endowment" for contract-creation transactions.
    // bytes m_data;                       ///< The data associated with the transaction, or the initializer if it's a creation transaction.
    // AccessList m_accessList;            ///< The list of addresss and storage keys that the transaction plans to access; for more info, https://eips.ethereum.org/EIPS/eip-2930



    /// @returns the max priority fee per gas.
    u256 maxPriorityFeePerGas() const { return m_maxPriorityFeePerGas; }

    /// @returns the max fee per gas and thus the implied exchange rate of ETH to GAS.
    u256 maxFeePerGas() const { return m_maxFeePerGas; }

    /// @param _f eth_feeHistory json objec
    /// @param _l priority feeLevel
    /// @param _m base fee multiplier
    void setFees(const json& _f, uint64_t _m = BASE_FEE_MULTIPLIER);

    /// @returns the total gas to convert, paid for from sender's account. Any unused gas gets refunded once the contract is ended.
    u256 gasLimit() const { return m_gasLimit; }

    /// @param _g estimatedGas    /// @param _m safety  multiplier
    void setGas(u256 _g) { m_gasLimit = _g * 12 / 10; };

    /// @returns the receiving address of the message-call transaction (undefined for contract-creation transactions).
    Address destination() const { return m_destination; }

    /// Synonym for receiveAddress().
    Address to() const { return m_destination; }

    /// Synonym for safeSender().
    Address from() const { return sender(); }

    /// @returns the senders priority fee level
    FeeLevel feeLevel() const { return m_feeLevel; }

    /// Sets the users new priority fee preference. Clears any signature & prior fees.
    void setFeeLevel(FeeLevel _l) { m_feeLevel = _l; clearSignature(); clearFees(); }

    /// @returns the data associated with this (message-call) transaction. Synonym for initCode().
    bytes const& data() const { return m_data; }

    /// @returns the transaction-count of the sender.
    u256 nonce() const { return m_nonce; }

    /// Sets the nonce to the given value. Clears any signature.
    void setNonce(u256 const& _n) { clearGas(); clearFees(); clearSignature(); m_nonce = _n; }

    /// @returns true if the transaction was signed
    bool hasSignature() const { return m_vrs.has_value(); }

    /// @returns true if the transaction was signed with zero signature
    bool hasZeroSignature() const { return m_vrs && isZeroSignature(m_vrs->r, m_vrs->s); }

    /// @returns the signature of the transaction (the signature has the sender encoded in it)
    /// @throws TransactionIsUnsigned if signature was not initialized
    SignatureStruct const& signature() const;

    /// @returns v value of the transaction (has chainID and recoveryID encoded in it)
    /// @throws TransactionIsUnsigned if signature was not initialized
    u256 yParity() const;

    void sign(Secret const& _priv);		///< Sign the transaction.

	json toJson() const;

	bool signable() const { return m_gasLimit != Invalid256 && m_maxFeePerGas != Invalid256 && m_maxPriorityFeePerGas != Invalid256; }

protected:
    /// Type of transaction.
    enum Function
    {
        NullTransaction,				///< Null transaction.
        ContractCreation,				///< Transaction to create contracts - receiveAddress() is ignored.
        MessageCall						///< Transaction to invoke a message call - receiveAddress() is used.
    };

    static bool isZeroSignature(u256 const& _r, u256 const& _s) { return !_r && !_s; }

    /// Clears the signature.
    void clearSignature() { m_vrs = SignatureStruct();  m_hashWith = h256(); }
    void clearGas() { m_gasLimit = Invalid256; }
    void clearFees() { m_maxPriorityFeePerGas = Invalid256; m_maxFeePerGas = Invalid256; }

    Function m_function = NullTransaction;		///< Is this a contract-creation transaction or a message-call transaction?
    Type m_type;                        ///< Typed-envelope transaction
    FeeLevel m_feeLevel;                ///< The priority fee level.
    uint64_t m_chainId;
    u256 m_nonce;						///< The transaction-count of the sender.
    u256 m_maxPriorityFeePerGas;        ///< The maximum fee per gas sender is willing to give to miners to incentivize them to include their transaction.
    u256 m_maxFeePerGas;                ///< The maximum fee per gas sender is willing to pay total (covering both priority fee and base fee)
    u256 m_gasLimit;                    ///< The upper limit of total gas to convert, paid for from sender's account. Any unused gas gets refunded.
    Address m_destination;              ///< The receiving address of the transaction.
    u256 m_value;                       ///< The amount of ETH to be transferred by this transaction. Called "endowment" for contract-creation transactions.
    bytes m_data;                       ///< The data associated with the transaction, or the initializer if it's a creation transaction.
    AccessList m_accessList;            ///< The list of addresss and storage keys that the transaction plans to access; for more info, https://eips.ethereum.org/EIPS/eip-2930
    boost::optional<SignatureStruct> m_vrs;	///< The signature of the transaction. Encodes the sender.
    /// EIP155 value for calculating transaction hash
    /// https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md
    /// We are including chainID as RLP field and implementing yParity signage rather than rawV. (Cannot find which EIP introduced this)

    mutable h256 m_hashWith;			///< Cached hash of transaction with signature.
    mutable boost::optional<Address> m_sender;  ///< Cached sender, determined from signature.
};

/// Nice name for vector of Transaction.
using TransactionBases = std::vector<TransactionBase>;

/// Simple human-readable stream-shift operator.
inline std::ostream& operator<<(std::ostream& _out, TransactionBase const& _t)
{
    _out << _t.sha3().abridged() << "{";
    if (_t.destination())
        _out << _t.destination().abridged();
    else
        _out << "[CREATE]";

    _out << "/" << _t.data().size() << "$" << _t.value() << "+" << _t.gasLimit() << "@" << _t.maxFeePerGas()  << "(" << _t.maxPriorityFeePerGas() << ")";
    _out << "<-" << _t.safeSender().abridged() << " #" << _t.nonce() << "}";
    return _out;
}

}
}
