#include "oc_ssd.h"
#include "oc_block_manager.h"
#include "oc_gc.h"


#include <cstdio>
#include <cassert>
#include <string>

namespace rocksdb {
    namespace ocssd {

#define DEBUG_GC

        static void TEST_Pr_nvm_addr(struct nvm_addr addr) {
            printf("         | %2ld | %3ld | %2ld | %4ld | %3ld | %ld\n",
                   addr.g.ch, addr.g.lun, addr.g.pl,
                   addr.g.blk, addr.g.pg, addr.g.sec);
        }

#define TEST_digestlen 8

        static void TEST_Pr_nvm_addrs(const char *str, struct nvm_addr *addrs, int num) {
            /*Print the caption for debuging*/
            char digest[TEST_digestlen];
            strncpy(digest, str, TEST_digestlen);
            digest[TEST_digestlen - 1] = '\0';

            printf("%8s | %s | %s | %s | %-4s | %3s | %s \n",
                   digest, "ch", "lun", "pl", "blk", "pg", "sec");

            for (int i = 0; i < num; ++i) {
                TEST_Pr_nvm_addr(addrs[i]);
            }
        }


        static inline bool valid_lun(size_t l, const nvm_geo *geo) {
            return l < geo->nluns;
        }

        static inline std::string GCErrMsg(const char *msg1, int lun) {
            char buf[32];
            snprintf(buf, 32, "%s%d", msg1, lun);
            return std::string(buf);
        }

        static inline std::string GCErrMsg(const char *msg1, int lun, size_t blk1, size_t blk2) {
            char buf[64];
            snprintf(buf, 64, "%sL%d,Blk(%ld - %ld)", msg1, lun, blk1, blk2);
            return std::string(buf);
        }


        static inline void setupaddrs_ch_lun(int ch, int lun, struct nvm_addr *addrs, int len) {
            for (int i = 0; i < len; i++) {
                addrs[i].ppa = 0;
                addrs[i].g.ch = ch;
                addrs[i].g.lun = lun;
            }
        }

        static inline void
        setupaddrs_pl_blks(size_t pl_blk_st, struct nvm_addr *addrs, size_t pl_blks_num, size_t pl_num) {
            for (size_t i = 0; i < pl_blks_num; ++i) {
                for (size_t j = 0; j < pl_num; ++j) {
                    addrs[i * pl_num + j].g.blk = pl_blk_st + i;
                    addrs[i * pl_num + j].g.pl = j;
                }
            }
        }

        rocksdb::Status oc_GC::EraseByLun(int ch, int lun, oc_block_manager *blk_mng) {
            assert(ch == 0); //now only support 1 channel(Qemu)
            struct nvm_addr addrs[NVM_NADDR_MAX];
            rocksdb::Status s;
            struct nvm_ret ret;
            const size_t emit_pl_blks_once_max = NVM_NADDR_MAX / blk_mng->des_->geo_->nplanes;
#ifdef DEBUG_GC_EraseByLun
            printf("emit_pl_blks_once_max : %zu\n", emit_pl_blks_once_max);
#endif
            size_t erased_pl_blks = 0, remained_pl_blks = blk_mng->des_->geo_->nblocks;
            size_t emit_pl_blks_num, emit_blks_num;
            size_t pl_blk_st = 0;

            if (!valid_lun(lun, blk_mng->des_->geo_)) {
                s = rocksdb::Status::IOError(GCErrMsg("EraseByLun: Invalid Lun:", lun));
                goto OUT;
            }
            //do a LUN-level Erase
            setupaddrs_ch_lun(ch, lun, addrs, NVM_NADDR_MAX);
            while (erased_pl_blks < remained_pl_blks) {
                emit_pl_blks_num = NVM_MIN(remained_pl_blks - erased_pl_blks, emit_pl_blks_once_max);
#ifdef DEBUG_GC_EraseByLun
                printf("Erase s:%zu e:%zu r:%zu\n", pl_blk_st, pl_blk_st + emit_pl_blks_num - 1, remained_pl_blks);
#endif
                setupaddrs_pl_blks(pl_blk_st, addrs, emit_pl_blks_num, blk_mng->des_->geo_->nplanes);

                emit_blks_num = emit_pl_blks_num * blk_mng->des_->geo_->nplanes;
#ifdef DEBUG_GC_EraseByLunX1
                TEST_Pr_nvm_addrs("Test_for_EraseByLun", addrs, emit_blks_num);
#endif
                if (nvm_addr_erase(blk_mng->des_->ssd_->Get_dev(), addrs, emit_blks_num, NVM_FLAG_PMODE_DUAL, &ret)) {
                    s = rocksdb::Status::IOError(
                            GCErrMsg("EraseByLun, do erase:", lun, pl_blk_st, pl_blk_st + emit_pl_blks_num - 1),
                            strerror(errno));
                    goto OUT;
                }

                pl_blk_st += emit_pl_blks_num;
                erased_pl_blks += emit_pl_blks_num;
            }

            OUT:
            return s;
        }

        rocksdb::Status oc_GC::EraseByBlock(int ch, int lun, int blk, oc_block_manager *blk_mng) {
            rocksdb::Status s;

            return s;
        }


    } //namespace ocssd
} //namespace rocksdb
