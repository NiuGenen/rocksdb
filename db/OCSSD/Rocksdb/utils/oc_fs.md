oc-fs design draft 

File Alloction: extent-based Tree
---------------------------------

x86_64: default memory alignment = 8 Bytes

1. extent:(24B)
|-----------------------------------------------|
| addr_st | addr_ed | free_bitmap | junk_bitmap |
| 8B      | 8B      | 4B          | 4B          |
-------------------------------------------------
free_bitmap: bitmap to indicate which slice is usable.(the bitmap can support 32b's bitmap)
junk_bitmap: bitmap to indicate which slice is discard.
when junk_bitmap is full: this extent could be GC.

LIMITATIONs
-----------
block(8M): the meaning of erase unit (`block` in nvm). 
vblk(256KB): 1/32 of a block(8M).


smallest_allocation_unit: ext_alloc_len * vblk
ext_alloc_len_max = 8(the limitation of LUN)
ext_alloc_len is fixed in range {2, 4, 8}
ext_len could be more.(the free blocks)

grain of file allocation: 0.5MB, 1MB, 2MB

free_vblk_sum of extent: at most 522240 Blocks, so 32 bits is enough.
a node's degree could be more than 255, so meta_obj_num should be 16bits

2. leaf_node:(1024B)
| ext_len_2_meta_obj_num | free_vblk_sum_2    | 
| ext_len_4_meta_obj_num | free_vblk_sum_4    |																					
| ext_len_8_meta_obj_num | free_vblk_sum_8    |
| el_none_mobj_num       | free_vblk_sum_none |
| 2B                     | 4B                 |
| meta_obj_2 | meta_obj_2 | ... |  meta_obj_2 | meta_obj_4 | meta_obj_4 | ... |  meta_obj_4 | meta_obj_8 | meta_obj_8 | ... |  meta_obj_8 |
|-------sorted by free_vblk_num---------------|----sorted by free_vblk_num------------------|-------sorted by free_vblk_num---------------|
|-------ext_len_2_meta_obj_array--------------|----ext_len_4_meta_obj_array-----------------|-------ext_len_8_meta_obj_array--------------|
| meta_obj_2/4/8:  
| free_vblk_num | obj_id | reserve | <--------------`reserve` field for cache-friendly purpose.
| 4B            | 2B     | 2B      | 
|---sorted by address-----------|
| extent | extent |....| extent |
| 24B    |                      |
 
node_id:4B can address at max 4TB meta_file_size
a tree_node(leaf or non_leaf)'s size is fixed to 1KB
node_id is inner-addressing of a meta_file

3. non_leaf_node:
| ext_len_2_meta_obj_num | free_vblk_sum_2    | 
| ext_len_4_meta_obj_num | free_vblk_sum_4    |																					
| ext_len_8_meta_obj_num | free_vblk_sum_8    |
| el_none_mobj_num       | free_vblk_sum_none |
| 2B                     | 4B                 |
| meta_obj_2 | meta_obj_2 | ... |  meta_obj_2 | meta_obj_4 | meta_obj_4 | ... |  meta_obj_4 | meta_obj_8 | meta_obj_8 | ... |  meta_obj_8 |
|-------sorted by free_vblk_num---------------|----sorted by free_vblk_num------------------|-------sorted by free_vblk_num---------------|
|-------ext_len_2_meta_obj_array--------------|----ext_len_4_meta_obj_array-----------------|-------ext_len_8_meta_obj_array--------------|
| meta_obj_2/4/8:
| free_vblk_num | obj_id | reserve | <--------------`reserve` field for cache-friendly purpose.
| 4B            | 2B     | 2B      | 
|---sorted by address--------------|
| node_id | node_id |....| node_id |
| 4B      | 4B      |....|         |

each node_id has 3 meta_obj(2/4/8)

4. degree:
node_size = 4KB                                                                     RESERVE + Type
for leaf_node:      24 + 32 * K         max_degree = 127;  24 + 32 * 127  = 4088    8
for non_leaf_node:  24 + 28 * K         max_degree = 145;  24 + 28 * 145 = 4084     12


File Layout
-----------



1. dir_file
ext_descriptor
| addr_st | addr_ed | bitmap | next_ext |
| 8b      | 8B      | 4B     | 8B       |


dir_ext_node


dir_leaf_node






2. ext_file


NAT
---





