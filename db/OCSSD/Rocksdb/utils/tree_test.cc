#include "oc_tree.h"
#include <cstdio>

void test1()
{
	printf("leaf_node:%zu\nnon_leaf_node:%zu\n", sizeof(rocksdb::ocssd::ext_leaf_node), 
		sizeof(rocksdb::ocssd::ext_non_leaf_node));
	printf("ext_descriptor:%zu\ndir_ext_node:%zu\n", sizeof(rocksdb::ocssd::ext_descriptor),
		sizeof(rocksdb::ocssd::dir_ext_node));
	
	printf("file_descriptor:%zu\n", sizeof(rocksdb::ocssd::file_descriptor));
	
	printf("dir_leaf_node_entry:%zu\n", sizeof(rocksdb::ocssd::dir_leaf_node_entry));
	printf("dir_leaf_node:%zu\n", sizeof(rocksdb::ocssd::dir_leaf_node));
	printf("dir_non_leaf_node:%zu\n", sizeof(rocksdb::ocssd::dir_non_leaf_node));
	printf("nat_entry:%zu\n", sizeof(rocksdb::ocssd::nat_entry));
	printf("nat_node:%zu\n", sizeof(rocksdb::ocssd::nat_node));
}

void test_addr()
{
	rocksdb::ocssd::addr::tree_meta tm1;
	rocksdb::ocssd::oc_ssd ssd1;
	tm1.ch = 0;
	tm1.nluns = 4;
	tm1.stlun = 0;

	rocksdb::ocssd::addr::blk_addr_handle blk(ssd1.Get_geo(), &tm1); 
	printf("success.\n");
}

int main()
{
	test_addr();
	return 0;
}
