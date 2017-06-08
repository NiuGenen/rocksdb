#ifndef YWJ_OCSSD_OC_OPTIONS_H
#define YWJ_OCSSD_OC_OPTIONS_H

#include <cstddef>  //for size_t

namespace rocksdb {
namespace ocssd {

namespace oc_options{

typedef enum {
	kRaid0 = 0x00,   //No parity ,so Reconstruction_Read is not supported.
	kRaid5 = 0x01    //with parity spread around the stripe.
}ChunkingAlgor;
typedef enum {
	kOneLunSerial = 0x00,    //Erase Operation is emited to one LUN by serializtion
	kAllIn = 0x01
}GCPolicy;
typedef enum {
	kFirstFit = 0x00,
	kBestFit = 0x01
}ExtentAllocPolicy;

extern const char *kDevPath;
extern const char *kOCSSDMetaFileNameSuffix;
extern const ChunkingAlgor kChunkingAlgor;
extern const GCPolicy kGCPolicy;
extern const int kOCFileNodeDegree;
extern const size_t kStChannel;
extern const size_t kEdChannel;
}

}//namespace ocssd
}//namespace rocksdb



#endif
