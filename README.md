# numalloc
Numa allocator


# Remove the information of getting callsites. No matter what, we will allocate sequential objects from the interleaved heap. 
Remaining parts: we should add support for analyze the environment variable, in order to know whether this should be closed or not. That is, there is no need to recompile the allocator. 

# Remove the origin-based design. It is not cool, and it is the source of the memory overhead. 
When perthreadheap don't have objects, it will invoke getObjectsFromNode to get many objects from the pernode heap. 
NumaHeap::getInstance().getObjectsFromNode

This will call pernodeheap->allocateObjects() to get the heap. 

Inside PerNodeHeap, if no objects in pernodeheap, then we will call _smallBags[sc].allocate().
Currently, the SIZE_ONE_BAG is 1MB. We should change it to 32KB. Then whenever one bag is allocated, then it will be marked in the shadow memory. 

Right now, getting the size information can be seen at pernodeheap.getSizeFromMbs(). We only need to change this. 
Basically, we will use 32KB as the basic unit. But if an object has the size of larer than 32KB, we only need to store the size on the starting address, as typically we only need to check the size information upon deallocations. For normal cases (without the bug), the first address will be used to store it. 

For big objects, we could use the hashtable to store the size information, as it is more efficient than using the shadow memory. 




# When there are too many objects in the per-thread heap, we should return objects back to the per-node heap. 

# when there are too many objects in the per-node heap, we should return it to the global heap. 

# when the global heap has too many objects, then we should return it back to the os. 

