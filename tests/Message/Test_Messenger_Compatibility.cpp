/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Message/ZilliqaTest.pb.h"
#include "libMessage/Messenger.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ZilliqaTest;
using namespace ZilliqaMessage;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(messenger_protobuf_test)

BOOST_AUTO_TEST_CASE(test_optionalfield) {
  INIT_STDOUT_LOGGER();

  bytes tmp;

  // Serialize a OneField message
  OneField oneField;
  oneField.set_field1(12345);
  BOOST_CHECK(oneField.IsInitialized());
  LOG_GENERAL(INFO, "oneField.field1 = " << oneField.field1());
  tmp.resize(oneField.ByteSize());
  oneField.SerializeToArray(tmp.data(), oneField.ByteSize());

  // Try to deserialize it as a OneField
  OneField oneFieldDeserialized;
  oneFieldDeserialized.ParseFromArray(tmp.data(), tmp.size());
  BOOST_CHECK(oneFieldDeserialized.IsInitialized());
  BOOST_CHECK(oneFieldDeserialized.field1() == oneField.field1());
  LOG_GENERAL(
      INFO, "oneFieldDeserialized.field1 = " << oneFieldDeserialized.field1());

  // Try to deserialize it as a TwoFields
  TwoFields twoFields;
  twoFields.ParseFromArray(tmp.data(), tmp.size());
  BOOST_CHECK(twoFields.IsInitialized());
  BOOST_CHECK(twoFields.has_field1());
  BOOST_CHECK(!twoFields.has_field2());
  BOOST_CHECK(twoFields.field1() == oneField.field1());
  LOG_GENERAL(INFO, "twoFields.field1 = " << twoFields.field1());
}

void SerializableToProtobufByteArray(const Serializable& serializable,
                                     ByteArray& byteArray) {
  bytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

// Temporary function for use by data blocks
void SerializableToProtobufByteArray(const SerializableDataBlock& serializable,
                                     ByteArray& byteArray) {
  bytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

template <class T, size_t S>
void NumberToProtobufByteArray(const T& number, ByteArray& byteArray) {
  bytes tmp;
  Serializable::SetNumber<T>(tmp, 0, number, S);
  byteArray.set_data(tmp.data(), tmp.size());
}

template <class T>
bool SerializeToArray(const T& protoMessage, bytes& dst,
                      const unsigned int offset) {
  if ((offset + protoMessage.ByteSize()) > dst.size()) {
    dst.resize(offset + protoMessage.ByteSize());
  }

  return protoMessage.SerializeToArray(dst.data() + offset,
                                       protoMessage.ByteSize());
}

BOOST_AUTO_TEST_CASE(test_dsblock_header) {
  ProtoDSBlock::OldDSBlockHeader oldProtoDSBlockHeader;

  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      oldProtoDSBlockHeader.mutable_blockheaderbase();

  DSBlockHeader oldDsBlockHeader;

  // version
  protoBlockHeaderBase->set_version(oldDsBlockHeader.GetVersion());
  // committee hash
  protoBlockHeaderBase->set_committeehash(
      oldDsBlockHeader.GetCommitteeHash().data(),
      oldDsBlockHeader.GetCommitteeHash().size);
  protoBlockHeaderBase->set_prevhash(oldDsBlockHeader.GetPrevHash().data(),
                                     oldDsBlockHeader.GetPrevHash().size);
  oldProtoDSBlockHeader.set_dsdifficulty(5);
  oldProtoDSBlockHeader.set_difficulty(3);
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      10000, *oldProtoDSBlockHeader.mutable_gasprice());

  Schnorr& schnorr = Schnorr::GetInstance();

  std::map<PubKey, Peer> oldDSPoWWinners;
  for (int i = 0; i < 10; ++i) {
    auto powdswinner = oldProtoDSBlockHeader.add_dswinners();
    PairOfKey keypair = schnorr.GenKeyPair();
    Peer peer = TestUtils::GenerateRandomPeer();
    oldDSPoWWinners[keypair.second] = peer;
    SerializableToProtobufByteArray(keypair.second,
                                    *powdswinner->mutable_key());
    SerializableToProtobufByteArray(peer, *powdswinner->mutable_val());
  }

  PairOfKey leaderKeypair = schnorr.GenKeyPair();
  SerializableToProtobufByteArray(
      leaderKeypair.second, *oldProtoDSBlockHeader.mutable_leaderpubkey());

  oldProtoDSBlockHeader.set_blocknum(100);
  oldProtoDSBlockHeader.set_epochnum(10000);
  SWInfo swInfo;
  SerializableToProtobufByteArray(swInfo,
                                  *oldProtoDSBlockHeader.mutable_swinfo());

  ZilliqaMessage::ProtoDSBlock::DSBlockHashSet* protoHeaderHash =
      oldProtoDSBlockHeader.mutable_hash();
  protoHeaderHash->set_shardinghash(oldDsBlockHeader.GetShardingHash().data(),
                                    oldDsBlockHeader.GetShardingHash().size);
  protoHeaderHash->set_reservedfield(
      oldDsBlockHeader.GetHashSetReservedField().data(),
      oldDsBlockHeader.GetHashSetReservedField().size());

  if (!oldProtoDSBlockHeader.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoDSBlock::OldDSBlockHeader initialization failed");
    return;
  }

  bytes dst;
  SerializeToArray(oldProtoDSBlockHeader, dst, 0);
  LOG_GENERAL(INFO, "Sizeof serialized oldProtoDSBlockHeader = " << dst.size());

  ProtoDSBlock::DSBlockHeader protoDSBlockHeader;

  protoDSBlockHeader.ParseFromArray(dst.data(), dst.size());

  if (!protoDSBlockHeader.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed");
    return;
  }
  LOG_GENERAL(INFO, "ProtoDSBlock::DSBlockHeader initialization success");

  LOG_GENERAL(INFO, "Size of protoDSBlockHeader.dswinners "
                        << protoDSBlockHeader.dswinners_size());

  DSBlockHeader dsBlockHeader;

  std::map<PubKey, Peer> dsPoWWinners;
  PubKey tempPubKey;
  Peer tempWinnerNetworkInfo;
  for (const auto& dswinner : protoDSBlockHeader.dswinners()) {
    ProtobufByteArrayToSerializable(dswinner.key(), tempPubKey);
    ProtobufByteArrayToSerializable(dswinner.val(), tempWinnerNetworkInfo);
    dsPoWWinners[tempPubKey] = tempWinnerNetworkInfo;
  }

  BOOST_REQUIRE(dsPoWWinners == oldDSPoWWinners);

  LOG_GENERAL(INFO, "Size of protoDSBlockHeader.removeddskey "
                        << protoDSBlockHeader.removeddskey_size());
}

BOOST_AUTO_TEST_SUITE_END()
