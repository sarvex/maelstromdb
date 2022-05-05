#ifndef SNAPSHOT_H
#define SNAPSHOT_H

namespace raft {

class GlobalCtxManager;

class Snapshot {
public:
  Snapshot(GlobalCtxManager& ctx);

private:
  GlobalCtxManager& m_ctx;
};

}

#endif

