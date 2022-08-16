# MIT-6.S081-2020-labs
All the labs except "net" are finished. Refer to some blogs on the Internet.

references:
* copyin/copystr: https://www.cnblogs.com/YuanZiming/p/14219005.html  
**Remember to modify kernel pgtbl directly but not alloc a new one**
* cow: https://zhuanlan.zhihu.com/p/301027032  
**When kfree, decrease the refcnt until zero then free it**
* buffer cache: https://zhuanlan.zhihu.com/p/426507542  
**Be careful to maintain the invariant that at most one copy of each block is cached**
* bigfile: https://blog.csdn.net/laplacebh/article/details/118530417  
**Modify NDIRECT to 11 is neccessary(can't use "NDIRECT - 1")**
