#include "oc_options.h"

namespace rocksdb {

    namespace ocssd {

        namespace oc_options {

//TODO - clean:
            const char *kDevPath = "/dev/nvme0n1";
            const char *kOCSSDMetaFileNameSuffix = ".ocssd";
            const ChunkingAlgor kChunkingAlgor = kRaid0;
            const GCPolicy kGCPolicy = kOneLunSerial;

            const int kOCFileNodeDegree = 6;
			const size_t kStChannel = 0;
			const size_t kEdChannel = 0;
        };


    }//namespace ocssd
}//namespace rocksdb


