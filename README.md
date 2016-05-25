Live Storage Migration - I/O Mirroring Implemetation of VMWare ESX
===================================================================

Live Storage Migration, I/O Mirroring Implemetation of VMWare ESX based on the paper "[The Design and Evolution of Live Storage Migration in VMware ESX](http://xenon.stanford.edu/~talg/papers/USENIXATC11/atc11-svmotion.pdf)", under the supervision of Prof. [Hussain Almohri](http://www.halmohri.com).

FUSE must be install on your system. After running 'make' two binaries will be generated: "filesystem.o" and "copy_deamon.o". The first one is the filesystem which must contains the virtual disks and the source virtual machine must use the vitural disks which reside there. The second binary is the deamon which is going to start the migration process.

You can find a high-level introduction on Live Storage Migration and I/O Mirroring in my blog: http://www.maastaar.net/virtual%20machines/2016/05/25/live-storage-migration-vmware-implementation/

License: GNU GPL.
