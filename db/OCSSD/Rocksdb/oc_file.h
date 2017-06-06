#ifndef YWJ_OC_FILE_H
#define YWJ_OC_FILE_H

#include "rocksdb/status.h"

//mutex & condition varieble headers
#include "port/port.h"

#include "oc_options.h"
#include "oc_block_manager.h"

//liblightnvm headers
#include "oc_lnvm.h"


namespace rocksdb {
namespace ocssd {

class oc_block_manager;

class oc_ssd;


/*
 * a non-posix file 
 * support: 
 *   - Random Access Read
 *   - Append-only Write
 */
class oc_file {
public:
	oc_file();
	~oc_file();

	rocksdb::Status Append();

	rocksdb::Status RandomReadByOffset(); //Offset is byte-oriented.
	rocksdb::Status Flush();

private:

	// No copying allowed
	oc_file(const oc_file&);
	void operator=(const oc_file&);
}; //oc_file


} //namespace ocssd
} //namespace rocksdb

#endif
