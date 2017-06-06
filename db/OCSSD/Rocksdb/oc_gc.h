#ifndef YWJ_OCSSD_OC_GC_H
#define YWJ_OCSSD_OC_GC_H

#include "rocksdb/status.h"

//liblightnvm headers
#include "oc_lnvm.h"


namespace rocksdb {
namespace ocssd {

class oc_block_manager;

class oc_GC{
public:
	static rocksdb::Status EraseByLun(int ch, int lun, oc_block_manager *blk_mng);
	static rocksdb::Status EraseByBlock(int ch, int lun, int blk, oc_block_manager *blk_mng);

private:
	
};

}//namespace ocssd
}//namespace leveldb

#endif
