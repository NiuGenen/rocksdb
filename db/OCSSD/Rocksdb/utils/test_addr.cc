#include "fs-core.h"
#include "../oc_ssd.h"

#include <cstdio>
#include <malloc.h>

void test_addr0()
{
	rocksdb::ocssd::addr::addr_meta am1;
	nvm_geo g;

	g.nchannels = 4;
	g.nluns = 8;
	g.nblocks = 512;
	g.nplanes = 2;
	g.npages = 256;

	am1.ch = 2;
	am1.nluns = 4;
	am1.stlun = 0;

	rocksdb::ocssd::addr::blk_addr_handle blk(&g, &am1);
	blk.PrInfo();
}


void test_addr2()
{
	rocksdb::ocssd::addr::addr_meta am1;
	nvm_geo g;

	g.nchannels = 4;
	g.nluns = 8;
	g.nblocks = 512;
	g.nplanes = 2;
	g.npages = 256;

	int max = 8 * 2 * 512;

	am1.ch = 2;
	am1.nluns = 8;
	am1.stlun = 0;

	rocksdb::ocssd::addr::blk_addr_handle bah(&g, &am1);
	rocksdb::ocssd::addr::blk_addr blk;

	//bah.MakeBlkAddr(2, 0, 0, 0, &blk);
	blk = bah.get_lowest();

	bah.PrBlkAddr(&blk, true, "");

	for (int i = 1; i < max + 30; i++) {
		bah.BlkAddrAdd(1, &blk);
		bah.PrBlkAddr(&blk, false, ""); 
		if (!bah.CalcOK()) {
			printf("oops: %d max: %d\n", i, max);
			break;
		}
	}

	bah.PrInfo();
}





void test_addr1() //each channel a addr_meta for full-lun-cover
{
	rocksdb::ocssd::oc_ssd ssd1;
	rocksdb::ocssd::addr::addr_meta *am;
	rocksdb::ocssd::addr::blk_addr_handle **bah;
	am  = (rocksdb::ocssd::addr::addr_meta *)calloc(sizeof(rocksdb::ocssd::addr::addr_meta), ssd1.Get_geo()->nchannels);
	bah = (rocksdb::ocssd::addr::blk_addr_handle **)calloc(sizeof(rocksdb::ocssd::addr::blk_addr_handle *), ssd1.Get_geo()->nchannels);
	for (int i = 0; i < ssd1.Get_geo()->nchannels; i++) {
		am[i].ch = i;
		am[i].stlun = 0;
		am[i].nluns = ssd1.Get_geo()->nluns;

		bah[i] = new rocksdb::ocssd::addr::blk_addr_handle(ssd1.Get_geo(), am + i);
	}

	if (am) {
		free(am);
	}
	for (int i = 0; i < ssd1.Get_geo()->nchannels; i++) {
		if (bah[i]) {
			delete bah[i];
		}
	}
}
int test0()
{
	int a, b;
	a = 75;
	printf("%d >> 5 = %d, %d\n", a, a >> 5, ((a >> 5) << 5));
	return 0;
}

int main()
{
	test_addr2();
	return 0;
}
