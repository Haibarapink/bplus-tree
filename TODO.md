## TODO 

- [TODO](#todo)
- [Save TreeMeta data in the header page](#save-treemeta-data-in-the-header-page)
- [More tests](#more-tests)
- [Support deletion Options](#support-deletion-options)
- [BufferPool manager the free pages (deleted pages)](#bufferpool-manager-the-free-pages-deleted-pages)

## Save TreeMeta data in the header page
1. Alloc a page from BufferPoolManager, and save the TreeMeta data in the header page, such as root page id, size of the tree, etc.
## More tests
1. Test all functions of BufferPoolManager, InternalNode, LeafNode, BPlusTree.
## Support deletion Options
1. If the brother node only has one key-val, and that key-val is larger than the 3/4 of page size, how to merge the brother node with the current node? 

## BufferPool manager the free pages (deleted pages)
1. How to manage the free pages?
Ans. Use a free list to manage the free pages, just like linked-list, and the last list node's page id will be saved in the header page.
2. Remove page_idx_ (Not nessary) 