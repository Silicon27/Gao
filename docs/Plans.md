The Gao runtime is a host for Gaolette containers, which are mini environments that are created with `mmap` running in the same process as Gao. Gao acts as a conductor for Gaolettes, monitoring, restricting and managing them.

A Gaolette, when reduced to layman terms is simply a section of memory mapped off with `mmap` that can be freely manipulated by the user via Gao.