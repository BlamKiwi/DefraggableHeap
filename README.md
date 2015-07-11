# Defraggable Heap

An implementation of defraggable heaps as a professional learning exercise. I wanted to compare the runtime performance of using a linked list compared to a splay heap as the backing data structure. Has some pretty trivial benchmarking code along with it, there is some example benchmark data for the curious. Code has been tested with ICC v15 on Windows.

This code sucks. If you use it in production code, here be dragons. If you really want to make this code production ready, I would change the defraggable pointers and make the backing data structure a hybrid between the splay tree and the linked list. 

Send your rage-mail here: http://www.missingbox.co.nz/contact/

## License

The source code is licensed using the BSD 2-Clause license. A copy of the license can be found in the LICENSE file and in the source code. 

