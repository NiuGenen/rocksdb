#ifndef YWJ_OCSSD_H
#define YWJ_OCSSD_H
#include <string>
//DB headers
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "port/port.h"


//liblightnvm headers
#include "oc_lnvm.h"

#include "utils/oc_exception.hpp"

#include "oc_file.h"

#include <set>


namespace rocksdb {
namespace ocssd {

class oc_file;
class oc_block_manager;
class oc_GC;
class oc_page_pool;

/* 
 * OCSSD device. 
 * The entrance of OCSSD Impl. 
 *  
 * ---------- Refine Note ----------------
 * 1. use exception to handle error. 
 * 2. all resource is impl. following RAII rule.
 * ---------- TODO ------------------------
 */


class oc_ssd {  
public:
	oc_ssd() throw (oc_excpetion); 
	~oc_ssd() throw();

	inline struct nvm_dev *Get_dev(){
		return des_->dev_;
	}
	inline const struct nvm_geo *Get_geo(){
		return des_->geo_;
	}
	inline oc_block_manager* Get_blkmng(){
		return blkmng_;
	}
	inline oc_page_pool* Get_pgpool(){
		return pgp_;
	}


private:

	//wrapper for RAII
	struct oc_ssd_descriptor
	{
		const std::string dev_path_;
		struct nvm_dev *dev_;
		const struct nvm_geo *geo_;
		size_t ch_st_;
		size_t ch_ed_;

		oc_ssd_descriptor(const char *path = oc_options::kDevPath) throw (dev_init_exception)
		 : dev_path_(path), 
		 dev_(NULL), 
		 geo_(NULL),
		 ch_st_(oc_options::kStChannel),
		 ch_ed_(oc_options::kEdChannel)
		{
			dev_ = nvm_dev_open(dev_path_.c_str());
			if (!dev_) {
				throw dev_init_exception("nvm_dev_open:");
			}

			geo_ = nvm_dev_get_geo(dev_);
			if (!geo_) {
				throw dev_init_exception("nvm_dev_get_geo:");
			}
		}

		~oc_ssd_descriptor() throw()
		{
			nvm_dev_close(dev_);
		}
	};

	struct oc_ssd_descriptor *des_;
	oc_block_manager *blkmng_;
	oc_page_pool *pgp_;

	// No copying allowed
	oc_ssd(const oc_ssd&);
	void operator=(const oc_ssd&);

}; //ocssd


} //namespace ocssd
} //namespace rocksdb

#endif
