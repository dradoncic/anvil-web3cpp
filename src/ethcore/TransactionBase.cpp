// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include <boost/throw_exception.hpp>
#include <web3cpp/devcore/vector_ref.h>
#include <web3cpp/devcrypto/Common.h>
#include <web3cpp/ethcore/Exceptions.h>
#include <web3cpp/ethcore/TransactionBase.h>
#include <web3cpp/Utils.h>

using namespace dev;
using namespace dev::eth;

TransactionBase::TransactionBase(TransactionSkeleton const& _ts):
    m_function(_ts.isCreation() ? ContractCreation : MessageCall),
    m_type(EIP1559),
    m_nonce(_ts.nonce),
    m_value(_ts.value),
    m_destination(_ts.to),
    m_gasLimit(_ts.gasLimit),
    m_maxPriorityFeePerGas(_ts.maxPriorityFeePerGas),
    m_maxFeePerGas(_ts.maxFeePerGas),
    m_data(_ts.data),
    m_accessList(_ts.accessList),
    m_sender(_ts.from),
	m_chainId(_ts.chainId)
{}

Address const& TransactionBase::sender() const
{
    if (!m_sender.is_initialized())
    {
        if (hasZeroSignature())
            m_sender = MaxAddress;
        else
        {
            if (!m_vrs) BOOST_THROW_EXCEPTION(TransactionIsUnsigned());

            auto p = recover(*m_vrs, sha3(WithoutSignature));
            if (!p) BOOST_THROW_EXCEPTION(InvalidSignature());
            m_sender = right160(dev::sha3(bytesConstRef(p.data(), sizeof(p))));
        }
    }
    return *m_sender;
}

Address const& TransactionBase::safeSender() const noexcept
{
    try
    {
        return sender();
    }
    catch (...)
    {
        return ZeroAddress;
    }
}

static const u256 c_secp256k1n("115792089237316195423570985008687907852837564279074904382605163141518161494337");

void TransactionBase::checkLowS() const
{
    if (!m_vrs) BOOST_THROW_EXCEPTION(TransactionIsUnsigned());
    if (m_vrs->s > c_secp256k1n / 2) BOOST_THROW_EXCEPTION(InvalidSignature());
}

void TransactionBase::checkChainId(uint64_t _chainId) const
{
    if (m_chainId != _chainId) BOOST_THROW_EXCEPTION(InvalidSignature());
}

void TransactionBase::streamRLP(RLPStream& _s, IncludeSignature _sig) const
{
    if (m_function == NullTransaction || !signable()) return;

    _s.appendRaw(bytes{ static_cast<byte>(m_type)}, 1);

    RLPStream payload;

    switch(m_type)
    {
        case EIP1559:
        {
            payload.appendList(_sig ? 12 : 9);
            payload << m_chainId
                    << m_nonce
                    << m_maxPriorityFeePerGas
                    << m_maxFeePerGas
                    << m_gasLimit
                    << (m_function == MessageCall ? m_destination : Address())
                    << m_value
                    << m_data
                    << m_accessList;
            break;
        }
        default: { BOOST_THROW_EXCEPTION(InvalidTransactionFormat()); }
    }

    if (_sig)
    {
        if (!m_vrs) BOOST_THROW_EXCEPTION(TransactionIsUnsigned());

        payload << yParity()
                << (u256)m_vrs->r
                << (u256)m_vrs->s;
    }

    _s.appendRaw(payload.out());
}

h256 TransactionBase::sha3(IncludeSignature _sig) const
{
    if (_sig == WithSignature && m_hashWith)
        return m_hashWith;

    RLPStream s;
    streamRLP(s, _sig);

    auto ret = dev::sha3(s.out());
    if (_sig == WithSignature)
        m_hashWith = ret;
    return ret;
}

SignatureStruct const& TransactionBase::signature() const
{
    if (!m_vrs) BOOST_THROW_EXCEPTION(TransactionIsUnsigned());
    return *m_vrs;
}

u256 TransactionBase::yParity() const
{
    if (!m_vrs) BOOST_THROW_EXCEPTION(TransactionIsUnsigned());
    return m_vrs->v;
}

void TransactionBase::sign(Secret const& _priv)
{
    if (!signable()) return;
    auto sig = dev::sign(_priv, sha3(WithoutSignature));
    SignatureStruct sigStruct = *(SignatureStruct const*)&sig;
    if (sigStruct.isValid()) m_vrs = sigStruct;
}

json TransactionBase::toJson() const
{
    json j;

    // from is always required
    if (safeSender() != Address())
        j["from"] = "0x" + dev::toHex(safeSender());

    // to only for message-call transactions
    if (m_function == MessageCall && m_destination != Address())
        j["to"] = "0x" + dev::toHex(m_destination);

    // value only if non-zero
    if (m_value != Invalid256)
        j["value"] = Utils::toHex(m_value);

    // gas / fees for typed transactions
    if (m_gasLimit != Invalid256)
        j["gas"] = Utils::toHex(m_gasLimit);

    if (m_maxPriorityFeePerGas != Invalid256)
        j["maxPriorityFeePerGas"] = Utils::toHex(m_maxPriorityFeePerGas);

    if (m_maxFeePerGas != Invalid256)
        j["maxFeePerGas"] = Utils::toHex(m_maxFeePerGas);

    // data if present
    if (!m_data.empty())
        j["data"] = "0x" + dev::toHex(m_data);

    // chainId if set
    if (m_chainId != 0)
        j["chainId"] = Utils::toHex(m_chainId);

    // access list if applicable
    if (!m_accessList.empty())
        j["accessList"] = Utils::toJson(m_accessList);

    return j;
}

TransactionBase::TransactionBase(bytesConstRef _rlpData, CheckTransaction _checkSig)
{
    try
    {
        if (_rlpData.empty())
            BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment("transaction RLP must be a list"));

        byte type = _rlpData[0];
        RLP rlp(_rlpData.cropped(1));
        if (!rlp.isList())
            BOOST_THROW_EXCEPTION(InvalidTransactionFormat()
                << errinfo_comment("typed transaction payload must be a list"));

        switch(type)
        {
            case(EIP1559) :
            {
                if (rlp.itemCount() != 9 && rlp.itemCount() != 12)
                    BOOST_THROW_EXCEPTION(InvalidTransactionFormat()
                        << errinfo_comment("invalid EIP-1559 field count"));

                m_type = EIP1559;
                m_chainId              = rlp[0].toInt<uint64_t>();
                m_nonce                = rlp[1].toInt<u256>();
                m_maxPriorityFeePerGas = rlp[2].toInt<u256>();
                m_maxFeePerGas         = rlp[3].toInt<u256>();
                m_gasLimit             = rlp[4].toInt<u256>();

                if (!rlp[5].isData())
                    BOOST_THROW_EXCEPTION(InvalidTransactionFormat());

                if (rlp[5].isEmpty())
                {
                    m_function = ContractCreation;
                    m_destination = Address();
                }
                else
                {
                    m_function = MessageCall;
                    m_destination = rlp[5].toHash<Address>(RLP::VeryStrict);
                }

                m_value = rlp[6].toInt<u256>();
                m_data  = rlp[7].toBytes();
                m_accessList = rlp[8].toVector<AccessItem>();

                if (rlp.itemCount() == 12)
                {
                    u256 yParity = rlp[9].toInt<u256>();
                    u256 r       = rlp[10].toInt<u256>();
                    u256 s       = rlp[11].toInt<u256>();

                    if (yParity > 1)
                        BOOST_THROW_EXCEPTION(InvalidSignature());

                    m_vrs = SignatureStruct{ r, s, static_cast<byte>(yParity) };

                    if (_checkSig >= CheckTransaction::Cheap)
                    {
                        checkLowS();
                        if (!m_vrs->isValid())
                            BOOST_THROW_EXCEPTION(InvalidSignature());
                    }

                    if (_checkSig == CheckTransaction::Everything)
                        m_sender = sender();
                }

                break;
            }
            default: { BOOST_THROW_EXCEPTION(InvalidTransactionFormat()); }
        }
    }
    catch (Exception& _e)
    {
        _e << errinfo_name("invalid transaction format RLP: " + toHex(_rlpData));
        throw;
    }
}

void TransactionBase::setFees(const json& _f, uint64_t _m)
{
    auto& result = _f.at("result");

    if (!result.contains("baseFeePerGas") || result["baseFeePerGas"].empty())
        BOOST_THROW_EXCEPTION(InvalidFeeHistoryResponse() << errinfo_comment("Missing baseFeePerGas in JSON"));

    u256 nextBaseFee = Utils::toBN(result["baseFeePerGas"].back());
    nextBaseFee *= _m;

    if (!result.contains("reward") || result["reward"].empty())
        BOOST_THROW_EXCEPTION(InvalidFeeHistoryResponse() << errinfo_comment("Missing reward array in JSON"));

    auto& lastRewardArray = result["reward"].back();
    u256 maxPriorityFee = Utils::toBN(lastRewardArray[m_feeLevel]);

    m_maxPriorityFeePerGas = maxPriorityFee;
    m_maxFeePerGas = nextBaseFee + maxPriorityFee;
}
