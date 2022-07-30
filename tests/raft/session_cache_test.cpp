#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "session_cache.h"

namespace raft {

using google::protobuf::util::MessageDifferencer;

TEST(AddSession, HandleDuplicateSession) {
  auto sc = SessionCache(100);
  sc.AddSession(1);
  EXPECT_THROW(sc.AddSession(1), std::out_of_range);
}

TEST(AddSession, LRUSessionExpiry) {
  auto sc = SessionCache(2);
  sc.AddSession(1);
  sc.AddSession(2);
  sc.AddSession(3);
  sc.AddSession(4);

  std::array<int, 2> expected_sessions = {3, 4};
  for (auto session:expected_sessions) {
    EXPECT_TRUE(sc.SessionExists(session));
  }
  std::array<int, 2> removed_sessions = {1, 2};
  for (auto session:removed_sessions) {
    EXPECT_FALSE(sc.SessionExists(session));
  }
}

TEST(CacheResponse, UpdateNonExistentSession) {
  auto sc = SessionCache(100);
  protocol::raft::ClientRequest_Response reply;
  EXPECT_THROW(sc.CacheResponse(1, 1, reply), std::out_of_range);
}

TEST(CacheResponse, OverwriteCachedResponse) {
  auto sc = SessionCache(100);
  sc.AddSession(1);

  int client_id = 1;
  int sequence_num = 1;
  protocol::raft::ClientRequest_Response expected_reply;
  expected_reply.set_status(true);
  expected_reply.set_leaderhint("test1");
  sc.CacheResponse(client_id, sequence_num, expected_reply);
  
  expected_reply.set_status(false);
  expected_reply.set_leaderhint("test2");
  sc.CacheResponse(client_id, sequence_num, expected_reply);
  
  protocol::raft::ClientRequest_Response got_reply;
  EXPECT_TRUE(sc.GetCachedResponse(client_id, sequence_num, got_reply));
  EXPECT_TRUE(MessageDifferencer::Equals(got_reply, expected_reply));
  EXPECT_EQ(got_reply.status(), expected_reply.status());
  EXPECT_EQ(got_reply.leaderhint(), expected_reply.leaderhint());
}

TEST(CacheResponse, HandleMultipleSequenceNums) {
  auto sc = SessionCache(100);
  sc.AddSession(1);
  sc.AddSession(2);
  sc.AddSession(3);

  std::array<protocol::raft::ClientRequest_Response, 3> expected_replies;
  std::array<int, 3> client_ids = {1, 2, 3};
  std::array<bool, 3> status = {true, true, false};
  std::array<std::string, 3> leader_hints = {"test1", "test2", "test3"};
  int sequence_num = 1;
  for (int i = 0; i < 3; i++) {
    expected_replies[i].set_status(status[i]);
    expected_replies[i].set_leaderhint(leader_hints[i]);
    sc.CacheResponse(client_ids[i], sequence_num, expected_replies[i]);
  }

  for (int i = 0; i < 3; i++) {
    protocol::raft::ClientRequest_Response got_reply;
    EXPECT_TRUE(sc.GetCachedResponse(client_ids[i], sequence_num, got_reply));
    EXPECT_TRUE(MessageDifferencer::Equals(got_reply, expected_replies[i]));
  }
}

TEST(CacheResponse, LRUSessionExpiry) {
  auto sc = SessionCache(2);
  sc.AddSession(1);
  sc.AddSession(2);

  int sequence_num = 1;
  protocol::raft::ClientRequest_Response reply;
  reply.set_status(true);
  reply.set_leaderhint("test1");
  sc.CacheResponse(1, sequence_num, reply);

  sc.AddSession(3);

  protocol::raft::ClientRequest_Response got_reply;
  EXPECT_FALSE(sc.GetCachedResponse(2, sequence_num, got_reply));
  EXPECT_TRUE(sc.GetCachedResponse(1, sequence_num, got_reply));
  EXPECT_TRUE(MessageDifferencer::Equals(got_reply, reply));
}

}

