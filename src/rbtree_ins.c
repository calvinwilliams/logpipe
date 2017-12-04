#include "logpipe_in.h"

#include "rbtree_tpl.h"

LINK_RBTREENODE_INT( LinkTraceFileWdTreeNode , struct InotifySession , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
QUERY_RBTREENODE_INT( QueryTraceFileWdTreeNode , struct InotifySession , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
UNLINK_RBTREENODE( UnlinkTraceFileWdTreeNode , struct InotifySession , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode )

DESTROY_RBTREE( DestroyTraceFileTree , struct InotifySession , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , FREE_RBTREENODEENTRY_DIRECTLY )

