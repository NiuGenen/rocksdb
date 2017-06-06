#ifndef YWJ_OCSSD_OC_BLK_MNG_H
#define YWJ_OCSSD_OC_BLK_MNG_H

#include "oc_options.h"
#include "oc_gc.h"

//mutex & condition varieble headers
#include "port/port.h"


//liblightnvm headers
#include "oc_lnvm.h"

#include "utils/common.h"
#include "utils/oc_exception.hpp"

#include <cstddef>
#include <cstdint>
#include <rocksdb/status.h>


namespace rocksdb {
namespace ocssd {

class oc_ssd;
class oc_GC;


class oc_block_manager { //allocation is done in a granularity of <Block>
public:
	typedef int BlkState_t;

	oc_block_manager(oc_ssd *ssd) throw(bbt_operation_not_good, erase_block_not_good);
	~oc_block_manager() throw();

	void print_info();
	void print_bbts(bool pr2file = false);

private:

	

	struct oc_blk_mng_descriptor {
		/* 
		 *	This decide the file's allocation unit & extent-inner bimap size:  
		 *		alloc_unit_in_bytes = 1 / <chunk_grain_factor> * <Block_Bytes>, 
		 *  	bit numbers of the extent bitmap = chunk_grain_factor,
		 *	Default: 16 
		 */
		int chunk_grain_factor;
		int alloc_unit_in_bytes;

		/* 
		 *  Default: kFirstFit
		 */
		oc_options::ExtentAllocPolicy extent_alloc_policy;

		/*
		 *	Default: true
		 */
		bool bbt_cached;

		/*
		 *	Default: false
		 */
		bool erase_on_startup;

		//other fields
		oc_ssd *const ssd_;
		const struct nvm_geo *geo_;

		struct nvm_bbt **bbts_;
		int bbts_length_;
		int blks_length_;

		oc_blk_mng_descriptor(oc_ssd *ssd) throw();
		~oc_blk_mng_descriptor() throw();
	};

	friend class oc_GC;

	oc_blk_mng_descriptor *des_;
	rocksdb::Status s;


	void init_bbts() throw(bbt_operation_not_good);
	void flush_bbts() throw(bbt_operation_not_good);
	void erase_on_startup() throw();

};

}
}
#endif
