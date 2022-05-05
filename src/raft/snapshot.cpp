#include "global_ctx_manager.h"
#include "snapshot.h"

namespace raft {

Snapshot::Snapshot(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
}

}

